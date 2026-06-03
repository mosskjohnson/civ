#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <time.h>

#include "raylib.h"
#include "rlgl.h"

#include "data.h"
#include "resources.h"
#include "message.h"

#define PORT 8080
#define SERVER_IP "169.231.116.248"

#define WINDOW_W 960
#define WINDOW_H 720
#define MAX_KEYBOARD_KEYS 512

enum Mode {LOBBY, INIT, PLAYING, END};

typedef struct {
    float vis_x;
    float vis_y;
} EntityVisual; // should be used for rendering to allow animations

typedef struct {
    SpritesheetTextures stextures;
    int canvas_w;
    int canvas_h;
    RenderTexture2D tiles_canvas;
    RenderTexture2D entities_canvas;
    RenderTexture2D fog_canvas;
    Shader fog_shader;
    int fog_shader_time_loc;
    int fog_shader_noise_loc;
    Texture2D noise_tex;
} TextureManager;

typedef struct {
    enum Mode mode;

    struct sockaddr_in server_addr; // initialized by networking
    struct pollfd poll_fd[1];

    playerID my_player_id; // initialized by lobby
    int num_players_now;
    int max_players;
    
    MapSize size; // initialized by init
    Entity* entities;
    int* gens;
    EntityRef active_unit;
    Tile* tiles;
    Fog* fog;
    int got_mapsize;
    int got_tiles;
    int got_entities;
    int got_gens;
    int got_fog;
} ClientState;

static const int direction_keys[8] = {KEY_W, KEY_E, KEY_D, KEY_C, KEY_X, KEY_Z, KEY_A, KEY_Q};

static void handle_networking(ClientState* state) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("CLIENT: Socket failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(sock_fd, (struct sockaddr *)&server_addr, (socklen_t)sizeof(server_addr)) < 0) {
        perror("CLIENT: Connection failed");
        exit(EXIT_FAILURE);
    }
    printf("CLIENT: Connected to server %s:%d\n", SERVER_IP, PORT);

    int flags = fcntl(sock_fd, F_GETFL, 0);
    fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);

    state->poll_fd[0].fd = sock_fd;
    state->poll_fd[0].events = POLLIN;
    
    state->server_addr = server_addr;
}

static void handle_lobby(ClientState* state, TextureManager* tm) {
    (void)tm;
    // receive
    int fd = state->poll_fd[0].fd;
    if (state->poll_fd[0].revents & POLLIN) {
        MsgHeader header;
        void* body;
        int r = recv_alloc_msg(fd, &header, &body);
        if (r == 0) {
           switch (header.type) {
                case SM_WELCOME: {
                    SM_Welcome* msgs = (SM_Welcome*)body;
                    state->my_player_id = msgs[0].player_id;
                    state->num_players_now = msgs[0].num_players_now;
                    state->max_players = msgs[0].max_players;
                    break;
                }
                case SM_PLAYER_JOINED_LOBBY: {
                    SM_PlayerJoinedLobby* msgs = (SM_PlayerJoinedLobby*)body;
                    state->num_players_now = msgs[0].num_players_now;
                    state->max_players = msgs[0].max_players;
                    break;
                }
                case SM_GAME_STARTING:
                    state->mode = INIT;
                    break;
                default:
                    break;
            }
            free(body);
        }
     }
    // input
    // send
    // rendering
    BeginDrawing();
    ClearBackground(WHITE);
    DrawText("Waiting for server to start game.", 0, 0, 20, BLACK);
    EndDrawing();
}

static void init_tiles_canvas(ClientState* state, TextureManager* tm) {
    tm->tiles_canvas = LoadRenderTexture(tm->canvas_w, tm->canvas_h);
    BeginTextureMode(tm->tiles_canvas);
    ClearBackground(WHITE);
    for (int x = 0; x < state->size.width; ++x) {
        for (int y = 0; y < state->size.height; ++y) {
            Tile* t = tile_at(state->tiles, state->size, x, y);
            Connections connections = 0;
            TileID neigh[4];
            neighbor_ids_4(state->size, x, y, neigh);
            switch (t->type) {
                case T_NIL: break;
                case T_OCEAN:
                    for (TileID n = 0; n < 4; ++n) {
                        if (neigh[n] != -1 && state->tiles[neigh[n]].type != T_OCEAN) {
                            connections |= (1 << n);
                        }
                    }
                    break;
                case T_RIVER:
                    for (TileID n = 0; n < 4; ++n) {
                        if (neigh[n] != -1 && (state->tiles[neigh[n]].type == T_RIVER || state->tiles[neigh[n]].type == T_OCEAN)) {
                            connections |= (1 << n);
                        }
                    }
                    break;
                default:
                    for (TileID n = 0; n < 4; ++n) {
                        if (neigh[n] != -1 && state->tiles[neigh[n]].type == t->type) {
                            connections |= (1 << n);
                        }
                    }
            }
            TexturePortion tp = tm->stextures.tiles[t->type][connections];
            DrawTexturePro(tp.texture, tp.portion, (Rectangle){x*TILE_W, y*TILE_H, TILE_W, TILE_H}, (Vector2){0.0, 0.0}, 0.0f, WHITE);
        }
    }
    EndTextureMode();    
}
static void init_entities_canvas(ClientState* state, TextureManager* tm) {
    tm->entities_canvas = LoadRenderTexture(tm->canvas_w, tm->canvas_h);
    BeginTextureMode(tm->entities_canvas);
    ClearBackground((Color){0,0,0,0});
    for (int i = 0; i < MAX_ENTITIES; ++i) {
        Entity* e = &state->entities[i];
        if (e->entity_type == E_NIL) continue;
        if (e->entity_type == E_UNIT) {
            TexturePortion tp = tm->stextures.units[e->unit_type];
            //DrawRectangle(e->x*TILE_W, e->y*TILE_H, UNIT_W, UNIT_H, RED);
            DrawTexturePro(
                tp.texture,
                tp.portion,
                (Rectangle){e->x*TILE_W, e->y*TILE_H, UNIT_W, UNIT_H},
                (Vector2){0.0,0.0},
                0.0,
                WHITE
            );
        }
    }
    EndTextureMode();
}
static void init_fog_canvas(ClientState* state, TextureManager* tm) {
    tm->fog_canvas = LoadRenderTexture(state->size.width, state->size.height);
    SetTextureFilter(tm->fog_canvas.texture, TEXTURE_FILTER_BILINEAR);
    BeginTextureMode(tm->fog_canvas);
    ClearBackground((Color){0,0,0,0});
    for (int x = 0; x < state->size.width; ++x) {
        for (int y = 0; y < state->size.height; ++y) {
            Fog* f = fog_at(state->fog, state->size, x, y);
            Color c;
            switch (*f) {
                case F_UNDISCOVERED: c = (Color){0,0,0,255}; break;
                case F_FOGGY: c = (Color){0,0,0,128}; break;
                case F_VISIBLE: c = (Color){0,0,0,0}; break;
            }
            DrawRectangle(x, y, 1, 1, c);
        }
    }
    EndTextureMode();
}

static void handle_init(ClientState* state, TextureManager* tm) {
    if (state->poll_fd[0].revents & POLLIN) {
        int fd = state->poll_fd[0].fd;
        MsgHeader header;
        void* body;
        int r = recv_alloc_msg(fd, &header, &body);
        if (r == 0) {
            switch (header.type) {
                case SM_INIT_MAPSIZE:
                    state->size = *(MapSize*)body;
                    state->got_mapsize = 1;
                    free(body);
                    break;
                case SM_INIT_TILES:
                    state->tiles = (Tile*)body;
                    state->got_tiles = 1;
                    break;
                case SM_INIT_ENTITIES:
                    state->entities = (Entity*)body;
                    state->got_entities = 1;
                    break;
                case SM_INIT_GENS:
                    state->gens = (int*)body;
                    state->got_gens = 1;
                    printf("got gens\n");
                    break;
                case SM_INIT_FOG:
                    state->fog = (Fog*)body;
                    state->got_fog = 1;
                    break;
                default:
                    free(body);
                    break;
            }
        }
        if (state->got_mapsize && state->got_tiles && state->got_entities && state->got_gens && state->got_fog) {
            printf("got initial data from server\n");
            tm->canvas_w = state->size.width * TILE_W;
            tm->canvas_h = state->size.height * TILE_H;
            init_tiles_canvas(state, tm);
            init_entities_canvas(state, tm);
            init_fog_canvas(state, tm);
            state->mode = PLAYING;
            return;
        }
    }
    // render
    BeginDrawing();
    ClearBackground(WHITE);
    DrawText("Loading...", 0, 0, 20, BLACK);
    EndDrawing();
}

static void update_tiles_canvas(const ClientState* state, SM_UpdateTile* updates, int count, TextureManager* tm) {
    BeginTextureMode(tm->tiles_canvas);
    for (int i = 0; i < count; ++i) {
        int x = updates[i].x;
        int y = updates[i].y;
        Tile* t = tile_at(state->tiles, state->size, x, y);
        Connections connections = 0;
        TileID neigh[4];
        neighbor_ids_4(state->size, x, y, neigh);
        switch (t->type) {
            case T_NIL: break;
            case T_OCEAN:
                for (TileID n = 0; n < 4; ++n) {
                    if (neigh[n] != -1 && state->tiles[neigh[n]].type != T_OCEAN) {
                        connections |= (1 << n);
                    }
                }
                break;
            case T_RIVER:
                for (TileID n = 0; n < 4; ++n) {
                    if (neigh[n] != -1 && (state->tiles[neigh[n]].type == T_RIVER || state->tiles[neigh[n]].type == T_OCEAN)) {
                        connections |= (1 << n);
                    }
                }
                break;
            default:
                for (TileID n = 0; n < 4; ++n) {
                    if (neigh[n] != -1 && state->tiles[neigh[n]].type == t->type) {
                        connections |= (1 << n);
                    }
                }
        }
        TexturePortion tp = tm->stextures.tiles[t->type][connections];
        DrawTexturePro(tp.texture, tp.portion, (Rectangle){x*TILE_W, y*TILE_H, TILE_W, TILE_H}, (Vector2){0.0, 0.0}, 0.0f, WHITE);
    }
    EndTextureMode();   
}
static void update_entities_canvas(const ClientState* state, TextureManager* tm) {
    BeginTextureMode(tm->entities_canvas);
    ClearBackground((Color){0,0,0,0});
    for (int i = 0; i < MAX_ENTITIES; ++i) {
        Entity* e = &state->entities[i];
        if (e->entity_type == E_NIL) continue;
        if (e->entity_type == E_UNIT) {
            TexturePortion tp = tm->stextures.units[e->unit_type];
            //DrawRectangle(e->x*TILE_W, e->y*TILE_H, UNIT_W, UNIT_H, RED);
            DrawTexturePro(
                tp.texture,
                tp.portion,
                (Rectangle){e->x*TILE_W, e->y*TILE_H, UNIT_W, UNIT_H},
                (Vector2){0.0,0.0},
                0.0,
                WHITE
            );
        }
    }
    EndTextureMode();
}
static void update_fog_canvas(ClientState* state, SM_UpdateFog* updates, int count, TextureManager* tm) {
    BeginTextureMode(tm->fog_canvas);
    rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM);
    for (int i = 0; i < count; ++i) {
        int x = updates[i].x;
        int y = updates[i].y;
        Fog* f = fog_at(state->fog, state->size, x, y);
        Color c;
        switch (*f) {
            case F_UNDISCOVERED: c = (Color){0,0,0,255}; break;
            case F_FOGGY: c = (Color){0,0,0,128}; break;
            case F_VISIBLE: c = (Color){0,0,0,0}; break;
        }
        DrawRectangle(x, y, 1, 1, c);
    }
    EndBlendMode();
    EndTextureMode();
}

static void update_tiles(ClientState* state, SM_UpdateTile* updates, int count) {
    for (int i = 0; i < count; ++i) {
        SM_UpdateTile update = updates[i];
        *tile_at(state->tiles, state->size, update.x, update.y) = update.updated;
    }
}
static void update_entities(ClientState* state, SM_UpdateEntity* updates, int count) {
    for (int i = 0; i < count; ++i) {
        SM_UpdateEntity update = updates[i];
        state->entities[update.ref.id] = update.updated;
    }
}
static void update_fog(ClientState* state, SM_UpdateFog* updates, int count) {
    for (int i = 0; i < count; ++i) {
        SM_UpdateFog update = updates[i];
        printf("fog update: %d,%d -> %d\n", update.x, update.y, update.updated);
        *fog_at(state->fog, state->size, update.x, update.y) = update.updated;
    }
}

static void handle_playing(ClientState* state, TextureManager* tm) {
    // receive
    SM_UpdateTile* tile_updates = malloc(state->size.width * state->size.height * sizeof(SM_UpdateTile));
    SM_UpdateEntity* entity_updates = malloc(MAX_ENTITIES * sizeof(SM_UpdateEntity));
    SM_UpdateFog* fog_updates = malloc(state->size.width * state->size.height * sizeof(SM_UpdateFog));
    int tile_updates_count = 0, entity_updates_count = 0, fog_updates_count = 0;
    while (state->poll_fd[0].revents & POLLIN) {
        int fd = state->poll_fd[0].fd;
        MsgHeader header;
        void* body;
        int r = recv_alloc_msg(fd, &header, &body);
        if (r == -2) break;
        if (r == -1) {
            printf("Server disconnected\n");
            state->mode = END;
            break;
        }
        switch (header.type) {
            case SM_UPDATE_TILES: {
                SM_UpdateTile* buf = (SM_UpdateTile*)body;
                for (int i = 0; i < header.count; ++i) {
                    tile_updates[tile_updates_count] = buf[i];
                    tile_updates_count += 1;
                }
                break;
            }
            case SM_UPDATE_ENTITIES: {
                SM_UpdateEntity* buf = (SM_UpdateEntity*)body;
                for (int i = 0; i < header.count; ++i) {
                    entity_updates[entity_updates_count] = buf[i];
                    entity_updates_count += 1;
                }
                break;
            }
            case SM_UPDATE_FOG: {
                SM_UpdateFog* buf = (SM_UpdateFog*)body;
                for (int i = 0; i < header.count; ++i) {
                    fog_updates[fog_updates_count] = buf[i];
                    fog_updates_count += 1;
                }
                break;
            }
            default: break;
        }
        free(body);
    }    
    // update
    update_tiles(state, tile_updates, tile_updates_count);
    update_tiles_canvas(state, tile_updates, tile_updates_count, tm);
    update_entities(state, entity_updates, entity_updates_count);
    update_entities_canvas(state, tm);
    update_fog(state, fog_updates, fog_updates_count);
    update_fog_canvas(state, fog_updates, fog_updates_count, tm);
    free(tile_updates);
    free(entity_updates);
    free(fog_updates);
    // input
    if (state->active_unit.id == 0) {
        for (int i = 1; i < MAX_ENTITIES; ++i) {
            if (state->entities[i].owner == state->my_player_id && state->entities[i].entity_type == E_UNIT) {
                state->active_unit.id = i;
                state->active_unit.gen = state->gens[i];
            }
        }
    }
    CM_UnitMove* unit_moves = malloc(MAX_ENTITIES * sizeof(CM_UnitMove));
    int unit_moves_count = 0;
    
    int keys_pressed[MAX_KEYBOARD_KEYS];
    int keys_pressed_count = 0;
    int k = GetKeyPressed();
    while (k > 0) {
        keys_pressed[keys_pressed_count++] = k;
        k = GetKeyPressed();
    }
    for (int i = 0; i < keys_pressed_count; ++i) {
        int key_pressed = keys_pressed[i];
        for (int dk = 0; dk < 8; ++dk) {
            if (key_pressed == direction_keys[dk]) {
                int x = state->entities[state->active_unit.id].x;
                int y = state->entities[state->active_unit.id].y;
                int dx = DELTAS_8[dk][0];
                int dy = DELTAS_8[dk][1];
                unit_moves[unit_moves_count++] = (CM_UnitMove){state->active_unit, x, y, x+dx, y+dy};
            }
        }
    }
    // send
    if (unit_moves_count > 0) {
        send_msg(state->poll_fd[0].fd, CM_UNIT_MOVE, unit_moves, unit_moves_count, sizeof(CM_UnitMove));
    }
    free(unit_moves);
    // rendering
    float f = GetTime();
    SetShaderValue(tm->fog_shader, tm->fog_shader_time_loc, &f, SHADER_UNIFORM_FLOAT);
    
    BeginDrawing();
    ClearBackground(WHITE);
    DrawTexturePro(
        tm->tiles_canvas.texture, 
        (Rectangle){0,0,tm->canvas_w, -tm->canvas_h}, 
        (Rectangle){0,0,WINDOW_W,WINDOW_H}, 
        (Vector2){0.0,0.0}, 
        0.0, 
        WHITE
    );
    DrawTexturePro(
        tm->entities_canvas.texture,
        (Rectangle){0,0,tm->canvas_w, -tm->canvas_h},
        (Rectangle){0,0,WINDOW_W,WINDOW_H},
        (Vector2){0.0,0.0},
        0.0,
        WHITE
    );
    BeginShaderMode(tm->fog_shader);
    DrawTexturePro(
        tm->fog_canvas.texture,
        (Rectangle){0,0,tm->fog_canvas.texture.width, -tm->fog_canvas.texture.height},
        (Rectangle){0,0,WINDOW_W,WINDOW_H},
        (Vector2){0.0,0.0},
        0.0,
        WHITE
    );
    EndShaderMode();
    DrawFPS(0,0);
    EndDrawing();
}

int main(void) {
    ClientState state = {0};
    state.mode = LOBBY;

    handle_networking(&state);

    InitWindow(WINDOW_W, WINDOW_H, "Civ");

    TextureManager tm = {0};
    load_textures(&tm.stextures);
    tm.fog_shader = LoadShader(0, "resources/shaders/fog.fs");
    tm.fog_shader_time_loc = GetShaderLocation(tm.fog_shader, "time");
    tm.fog_shader_noise_loc = GetShaderLocation(tm.fog_shader, "noise");
    tm.noise_tex = LoadTexture("resources/images/noise32.png");
    SetTextureWrap(tm.noise_tex, TEXTURE_WRAP_REPEAT);
    SetShaderValueTexture(tm.fog_shader, tm.fog_shader_noise_loc, tm.noise_tex);
    
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        poll(state.poll_fd, 1, 0);
        switch (state.mode) {
            case LOBBY:   handle_lobby(&state, &tm);   break;
            case INIT:    handle_init(&state, &tm);    break; 
            case PLAYING: handle_playing(&state, &tm); break;
            case END: goto cleanup;
        }        
    }

    cleanup:

    free_entities(state.entities);
    free_tiles(state.tiles);
    free_fog(state.fog);
    
    unload_textures(&tm.stextures);
    UnloadRenderTexture(tm.tiles_canvas);
    UnloadRenderTexture(tm.fog_canvas);
    UnloadShader(tm.fog_shader);
    UnloadTexture(tm.noise_tex);

    close(state.poll_fd[0].fd);
    CloseWindow();
}
