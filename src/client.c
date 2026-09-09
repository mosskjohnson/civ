#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>

#include "raylib.h"
#include "rlgl.h"

#include "data.h"
#include "resources.h"
#include "message.h"
#include "utils.h"
#include "camera.h"

#define PORT 8080
// #define SERVER_IP "10.0.0.98"
// #define SERVER_IP "192.168.0.109"
#define SERVER_IP "169.231.116.248"

#define WINDOW_W 960
#define WINDOW_H 720
#define MINIMAP_W_SCALE 6.0
#define MINIMAP_H_SCALE 6.0
#define MAX_KEYBOARD_KEYS 512
#define FOG_SHADER_ON 0

enum Mode {LOBBY, PLAYING, END};

typedef struct {
    Font font;
    SpritesheetTextures stextures;
    int canvas_w;
    int canvas_h;
    RenderTexture2D tiles_canvas;
    RenderTexture2D entities_canvas;
    RenderTexture2D fog_canvas;
    RenderTexture2D final_canvas;
    RenderTexture2D minimap;
    Texture2D noise_tex;
    Shader fog_shader;
    int fog_shader_time_loc;
    int fog_shader_noise_loc;
    Shader unit_color_shader;
    int unit_color_shader_co_loc;
    int unit_color_shader_cho_loc;
    int unit_color_shader_cn_loc;
    int unit_color_shader_chn_loc;
} TextureManager;

typedef struct {
    enum Mode mode;
    int window_w;
    int window_h;

    // initialized by networking
    struct sockaddr_in server_addr;
    struct pollfd poll_fd[1];

    // initialized by lobby
    playerID my_player_id;
    int num_players_now;
    int max_players;
    
    // initialized by init
    MapSize size; 
    Entity* entities;
    EntityID active_unit;
    Tile* tiles;
    Fog* fog;
    bool got_size;
    bool got_entities;
    bool got_tiles;
    bool got_fog;
    CivColor* colors;
    Camera2D cam;
    int turn;
    float seconds_for_this_turn;
    float moment_turn_started;
} ClientState;

// one day will be some kind of keybindings map
static const int direction_keys[8] = {KEY_W, KEY_E, KEY_D, KEY_C, KEY_X, KEY_Z, KEY_A, KEY_Q};

static void handle_networking(ClientState* state) {
    printf("got here\n");
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

    //int flags = fcntl(sock_fd, F_GETFL, 0);
    //fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);

    state->poll_fd[0].fd = sock_fd;
    state->poll_fd[0].events = POLLIN;
    
    state->server_addr = server_addr;
}

static void update_tiles_canvas(const ClientState* state, TextureManager* tm, Tile* tiles_update) {
    BeginTextureMode(tm->tiles_canvas);
    for (int x = 0; x < state->size.width; ++x) {
        for (int y = 0; y < state->size.height; ++y) {
            Tile* old_tile = tile_at(state->tiles, state->size, x, y);
            Tile* new_tile = tile_at(tiles_update, state->size, x, y);
            int neighbors[9];
            neighbor_ids_9(state->size, x, y, neighbors);
            for (Direction9 d = 0; d < 9; ++d) {
                Tile* neighbor_tile = &tiles_update[neighbors[d]];
                if (memcmp(old_tile, neighbor_tile, sizeof(Tile)) != 0) { // only draw if neighbor changed
                    draw_tile(tiles_update, state->size, new_tile, x, y, &tm->stextures);
                    break;
                }
            }
        }
    }
    EndTextureMode();
}
static void update_fog_canvas(ClientState* state, TextureManager* tm, Fog* fog_update) {
    BeginTextureMode(tm->fog_canvas);
    rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM);
    for (int x = 0; x < state->size.width; ++x) {
        for (int y = 0; y < state->size.height; ++y) {   
            Fog* old_fog = fog_at(state->fog, state->size, x, y);
            Fog* new_fog = fog_at(fog_update, state->size, x, y);
            if (!state->got_fog || *old_fog != *new_fog) draw_fog(new_fog, x, y);
        }
    }
    EndBlendMode();
    EndTextureMode();
}
static void draw_entities_onto_canvas(const ClientState* state, TextureManager* tm) {
    BeginTextureMode(tm->entities_canvas);
    ClearBackground((Color){0,0,0,0});
    BeginShaderMode(tm->unit_color_shader);

    assert(state->colors != NULL);

    for (int i = 0; i < MAX_ENTITIES; ++i) {
        Entity* e = &state->entities[i];
        if (e->entity_type == E_NIL || i == state->active_unit) continue;

        rlDrawRenderBatchActive();
        Vector4 cn = ColorNormalize(GetColor(color_hex(state->colors[e->owner])));
        Vector4 chn = ColorNormalize(GetColor(highlight_hex(state->colors[e->owner])));
        SetShaderValue(tm->unit_color_shader, tm->unit_color_shader_cn_loc, &cn, SHADER_UNIFORM_VEC4);
        SetShaderValue(tm->unit_color_shader, tm->unit_color_shader_chn_loc, &chn, SHADER_UNIFORM_VEC4);

        draw_entity(e, &tm->stextures);
    }

    Entity* active = &state->entities[state->active_unit];
    if (fmodf(GetTime(), 1.3) >= 0.3) { // blink
        rlDrawRenderBatchActive();
        Vector4 cn = ColorNormalize(GetColor(color_hex(state->colors[state->my_player_id])));
        Vector4 chn = ColorNormalize(GetColor(highlight_hex(state->colors[state->my_player_id])));
        SetShaderValue(tm->unit_color_shader, tm->unit_color_shader_cn_loc, &cn, SHADER_UNIFORM_VEC4);
        SetShaderValue(tm->unit_color_shader, tm->unit_color_shader_chn_loc, &chn, SHADER_UNIFORM_VEC4);

        draw_entity(active, &tm->stextures);
    }

    EndShaderMode();
    EndTextureMode();
}

static void handle_incoming_messages(ClientState* state, TextureManager* tm) {
    while (1) {
        int r = poll(state->poll_fd, 1, 0);
        if (r <= 0 || !(state->poll_fd[0].revents & POLLIN)) break;
        MsgHeader header;
        void* body;
        if (recv_alloc_msg(state->poll_fd[0].fd, &header, &body) < 0) {
            printf("Server disconnected\n");
            state->mode = END;
            break;
        }
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
            case SM_MAPSIZE: {
                state->size = *(MapSize*)body;
                free(body);
                state->tiles = alloc_tiles(state->size);
                state->entities = alloc_entities();
                state->fog = alloc_fog(state->size);
                tm->canvas_w = state->size.width * TILE_W;
                tm->canvas_h = state->size.height * TILE_H;
                
                state->got_size = true;
                break;
            }
            case SM_TILES: {
                Tile* tiles_update = (Tile*)body;

                if (!state->got_tiles) {
                    tm->tiles_canvas = LoadRenderTexture(tm->canvas_w, tm->canvas_h);
                      BeginTextureMode(tm->tiles_canvas);
                        ClearBackground((Color){0,0,0,0});
                      EndTextureMode();
                }
                update_tiles_canvas(state, tm, tiles_update);
                memcpy(state->tiles, tiles_update, state->size.width*state->size.height*sizeof(Tile));
                free(tiles_update);
                
                state->got_tiles = true;
                break;
            }
            case SM_ENTITIES: {
                Entity* entities_update = (Entity*)body;
                memcpy(state->entities, entities_update, MAX_ENTITIES*sizeof(Entity));
                if (!state->got_entities) {
                    tm->entities_canvas = LoadRenderTexture(tm->canvas_w, tm->canvas_h);
                }
                free(entities_update);

                if (!state->got_entities)
                    for (EntityID i = 0; i < MAX_ENTITIES; ++i)
                        if (state->entities[i].entity_type != E_NIL) printf("e: %d\n", i);
                state->got_entities = true;
                break;
            }
            case SM_FOG: {
                Fog* fog_update = (Fog*)body; 
                if (!state->got_fog) {
                    tm->fog_canvas = LoadRenderTexture(state->size.width, state->size.height);
                      SetTextureFilter(tm->fog_canvas.texture, TEXTURE_FILTER_BILINEAR);
                      BeginTextureMode(tm->fog_canvas);
                        ClearBackground((Color){0,0,0,0});
                      EndTextureMode();
                }
                update_fog_canvas(state, tm, fog_update);
                memcpy(state->fog, fog_update, state->size.width*state->size.height*sizeof(Fog));
                free(fog_update);
                
                state->got_fog = true;
                break;
            }
            case SM_COLORS: {
                state->colors = (CivColor*)body;
                printf("My color: %d\n", state->colors[state->my_player_id]);
                break;
            }
            case SM_GAME_STARTING: {
                if (!(state->got_size && state->got_tiles && state->got_entities && state->got_fog)) {
                    fprintf(stderr, "ERROR: Got game starting message before getting all initial data from server\n");
                    state->mode = END;
                    return;
                }
                tm->final_canvas = LoadRenderTexture(tm->canvas_w, tm->canvas_h);
                  GenTextureMipmaps(&tm->final_canvas.texture);
                  //SetTextureFilter(tm->final_canvas.texture, TEXTURE_FILTER_TRILINEAR);
                tm->minimap = LoadRenderTexture(state->window_w/MINIMAP_W_SCALE, state->window_h/MINIMAP_H_SCALE);
                state->cam = (Camera2D){
                    .zoom = 1.0,
                    .target = (Vector2){tm->canvas_w/2.0, tm->canvas_h/2.0},
                    .offset = (Vector2){state->window_w/2.0, state->window_h/2.0},
                };
                state->mode = PLAYING;
                free(body);
                break;
            }
            case SM_NEXT_TURN: {
                SM_NextTurn* msg = (SM_NextTurn*)body;
                state->turn = msg[0].turn;
                state->seconds_for_this_turn = msg[0].seconds;
                state->moment_turn_started = GetTime();
                break;
            }
            default: free(body); break;
        }
    }
}

static void handle_lobby(ClientState* state, TextureManager* tm) {
    // input
    // send
    // rendering
    (void)tm;
    BeginDrawing();
    ClearBackground(WHITE);
    draw_text(tm->font, 16.0, 2.0, state->window_w/2, state->window_h/2, FA_MIDDLE, FA_MIDDLE, BLACK, "Waiting for server to start game. \nPlayers: %d/%d", state->num_players_now, state->max_players);
    EndDrawing();
}

static EntityID get_next_eligible_unit(ClientState* state, EntityID e_old) {
    for (EntityID i = (e_old+1)%MAX_ENTITIES; i != e_old; i = (i+1)%MAX_ENTITIES) {
        if (i == 0) continue;
        if (state->entities[i].owner == state->my_player_id && state->entities[i].entity_type == E_UNIT && state->entities[i].movement_remaining > 0) {
            return i;
        }
    }
    return 0;
}

static void handle_playing(ClientState* state, TextureManager* tm) {
    if (state->active_unit == 0 || state->entities[state->active_unit].entity_type == E_NIL || state->entities[state->active_unit].movement_remaining <= 0) {
        state->active_unit = get_next_eligible_unit(state, state->active_unit);
    }

    CM_UnitMove* unit_moves = malloc(MAX_ENTITIES * sizeof(CM_UnitMove));
    int unit_moves_count = 0;

    // input
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        Vector2 mouse = GetMousePosition();
        float zoom_factor = (1.0f + wheel * 0.2f);
        zoom_on_anchor(&state->cam, (CameraSizeInfo){state->window_w, state->window_h, tm->canvas_w, tm->canvas_h}, zoom_factor, mouse);
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 delta = GetMouseDelta();
        pan(&state->cam, (CameraSizeInfo){state->window_w, state->window_h, tm->canvas_w, tm->canvas_h}, delta.x, delta.y);
    }
    
    int last_key_pressed = 0;
    int k = GetKeyPressed();
    while (k > 0) {
        last_key_pressed = k;
        k = GetKeyPressed();
    }
    for (int dk = 0; dk < 8; ++dk) {
        if (last_key_pressed == direction_keys[dk]) {
            if (state->active_unit == 0) break;
            int id = state->active_unit;
            int gen = state->entities[id].gen;
            int x_from = state->entities[state->active_unit].x;
            int y_from = state->entities[state->active_unit].y;
            int x_to = wrapped_x(state->size, x_from + DELTAS_8[dk][0]);
            int y_to = wrapped_y(state->size, y_from + DELTAS_8[dk][1]);
            unit_moves[unit_moves_count++] = (CM_UnitMove){id, gen, x_from, y_from, x_to, y_to};
        }
    }
    if (last_key_pressed == KEY_SPACE) {
        state->active_unit = get_next_eligible_unit(state, state->active_unit);
        printf("switched to entity %d\n", state->active_unit);
    }
    
    // send
    if (unit_moves_count > 0) {
        send_msg(state->poll_fd[0].fd, CM_UNIT_MOVE, unit_moves, unit_moves_count, sizeof(CM_UnitMove));
    }
    free(unit_moves);
    
    // rendering
    draw_entities_onto_canvas(state, tm);
    
    BeginTextureMode(tm->final_canvas);
    ClearBackground((Color){0,0,0,0});
    DrawTexturePro(
            tm->tiles_canvas.texture,
            (Rectangle){0,0,tm->tiles_canvas.texture.width, -tm->tiles_canvas.texture.height},
            (Rectangle){0,0,tm->canvas_w,tm->canvas_h},
            (Vector2){0.0,0.0}, 0.0, WHITE
        );
    DrawTexturePro(
            tm->entities_canvas.texture,
            (Rectangle){0,0,tm->entities_canvas.texture.width, -tm->entities_canvas.texture.height},
            (Rectangle){0,0,tm->canvas_w,tm->canvas_h},
            (Vector2){0.0,0.0}, 0.0, WHITE
        );
    if (FOG_SHADER_ON) {
        BeginShaderMode(tm->fog_shader);
        float f = GetTime();
        SetShaderValue(tm->fog_shader, tm->fog_shader_time_loc, &f, SHADER_UNIFORM_FLOAT);
    }
    DrawTexturePro(
        tm->fog_canvas.texture,
        (Rectangle){0,0,tm->fog_canvas.texture.width, -tm->fog_canvas.texture.height},
        (Rectangle){0,0,tm->canvas_w,tm->canvas_h},
        (Vector2){0.0,0.0}, 0.0, WHITE
    );
    if (FOG_SHADER_ON) {
        EndShaderMode();
    }
    EndTextureMode();

    BeginTextureMode(tm->minimap);
    ClearBackground(BLACK);
    int mini_w = tm->minimap.texture.width;
    int mini_h = tm->minimap.texture.height;
    DrawTexturePro(
        tm->final_canvas.texture,
        (Rectangle){0, 0, tm->canvas_w, -tm->canvas_h},
        (Rectangle){0, 0, tm->minimap.texture.width, tm->minimap.texture.height},
        (Vector2){0, 0}, 0.0f, WHITE
    );
    float scale_x = (float)mini_w / tm->canvas_w;
    float scale_y = (float)mini_h / tm->canvas_h;

    float view_w = (state->window_w / state->cam.zoom) * scale_x;
    float view_h = (state->window_h / state->cam.zoom) * scale_y;
    float view_x = state->cam.target.x * scale_x - view_w / 2.0f;
    float view_y = state->cam.target.y * scale_y - view_h / 2.0f;
    DrawRectangleLines((int)view_x, (int)view_y, (int)view_w, (int)view_h, WHITE);
    DrawRectangleLines(0, 0, mini_w, mini_h, WHITE);
    EndTextureMode();
    
    BeginDrawing();
    ClearBackground((Color){0,0,0,0});
    BeginMode2D(state->cam);
    DrawTexturePro(
        tm->final_canvas.texture,
        (Rectangle){0, 0, tm->canvas_w, -tm->canvas_h},
        (Rectangle){0, 0, tm->canvas_w, tm->canvas_h},
        (Vector2){0, 0}, 0.0f, WHITE
    );
    float half_screen_w = (state->window_w / 2.0f) / state->cam.zoom;
    // wrap left
    if (state->cam.target.x - half_screen_w < 0) {
        DrawTexturePro(
            tm->final_canvas.texture,
            (Rectangle){0, 0, tm->canvas_w, -tm->canvas_h},
            (Rectangle){-tm->canvas_w, 0, tm->canvas_w, tm->canvas_h},
            (Vector2){0, 0}, 0.0f, WHITE
        );
    }
    // wrap right
    if (state->cam.target.x + half_screen_w > tm->canvas_w) {
        DrawTexturePro(
            tm->final_canvas.texture,
            (Rectangle){0, 0, tm->canvas_w, -tm->canvas_h},
            (Rectangle){tm->canvas_w, 0, tm->canvas_w, tm->canvas_h},
            (Vector2){0, 0}, 0.0f, WHITE
        );
    }
    EndMode2D();
    DrawTexturePro(
        tm->minimap.texture,
        (Rectangle){0, 0, mini_w, -mini_h},
        (Rectangle){0, 0, state->window_w/6.0, state->window_h/6.0},
        (Vector2){0, 0}, 0.0f, WHITE
    );
    float seconds_since_turn_started = GetTime() - state->moment_turn_started;
    draw_text(tm->font, 16.0, 2.0, state->window_w/2, 0, FA_MIDDLE, FA_START, WHITE, "TURN %d TIME: %.0f", state->turn, state->seconds_for_this_turn - seconds_since_turn_started);
    draw_text(tm->font, 16.0, 2.0, state->window_w, 0, FA_END, FA_START, WHITE, "%d FPS", GetFPS());
    EndDrawing();
}

int main(void) {
    ClientState state = {0};
    state.mode = LOBBY;
    state.window_w = WINDOW_W;
    state.window_h = WINDOW_H;

    handle_networking(&state);

    //SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(state.window_w, state.window_h, "Civ");

    TextureManager tm = {0};

    int codepoints[256];
    for (int i = 0; i < 256; ++i) codepoints[i] = i;
    tm.font = LoadFontEx("resources/fonts/civ0.ttf", 32, codepoints, 256);

    load_textures(&tm.stextures);

    tm.fog_shader = LoadShader(0, "resources/shaders/fog.fs");
    tm.fog_shader_time_loc = GetShaderLocation(tm.fog_shader, "time");
    tm.fog_shader_noise_loc = GetShaderLocation(tm.fog_shader, "noise");
    tm.noise_tex = LoadTexture("resources/images/noise32.png");
    SetTextureWrap(tm.noise_tex, TEXTURE_WRAP_REPEAT);
    SetShaderValueTexture(tm.fog_shader, tm.fog_shader_noise_loc, tm.noise_tex);

    tm.unit_color_shader = LoadShader(0, "resources/shaders/unit_color.fs");
    tm.unit_color_shader_co_loc = GetShaderLocation(tm.unit_color_shader, "color_old");
    tm.unit_color_shader_cho_loc = GetShaderLocation(tm.unit_color_shader, "color_highlight_old");
    tm.unit_color_shader_cn_loc = GetShaderLocation(tm.unit_color_shader, "color_new");
    tm.unit_color_shader_chn_loc = GetShaderLocation(tm.unit_color_shader, "color_highlight_new");
    Vector4 co = UNIT_REPLACE_COLOR;
    Vector4 cho = UNIT_REPLACE_COLOR_HIGHLIGHT;
    SetShaderValue(tm.unit_color_shader, tm.unit_color_shader_co_loc, &co, SHADER_UNIFORM_VEC4);
    SetShaderValue(tm.unit_color_shader, tm.unit_color_shader_cho_loc, &cho, SHADER_UNIFORM_VEC4);
    
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        if (IsWindowResized()) {
            state.window_w = GetScreenWidth();
            state.window_h = GetScreenHeight();
        }
        handle_incoming_messages(&state, &tm);
        switch (state.mode) {
            case LOBBY: handle_lobby(&state, &tm); break;
            case PLAYING: handle_playing(&state, &tm); break;
            case END: goto cleanup;
        }        
    }

    cleanup:

    free_entities(state.entities);
    free_tiles(state.tiles);
    free_fog(state.fog);
    free(state.colors);
    
    unload_textures(&tm.stextures);
    UnloadRenderTexture(tm.tiles_canvas);
    UnloadRenderTexture(tm.fog_canvas);
    UnloadShader(tm.fog_shader);
    UnloadTexture(tm.noise_tex);

    close(state.poll_fd[0].fd);
    CloseWindow();
}
