#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#include "data.h"
#include "worldgen.h"
#include "message.h"

#define PORT 8080
#define MAX_PLAYERS 5
#define MAP_W 80
#define MAP_H 50
#define FAIR_START 1
#define PLAYER_START_ZONE_SIZE 15

#define frand() (float)rand() / MAX_RAND

typedef int playerID;

typedef struct {
    MapSize size; // initialized by user
    GenParameters p; // initialized by user via argv
    
    struct pollfd local_fds[2]; // initialized by networking, local_fds[0] is server_fd and local_fds[1] is STDIN
    
    struct pollfd client_fds[MAX_PLAYERS]; // initialized by lobby
    struct sockaddr_in client_addrs[MAX_PLAYERS]; // lobby
    int player_count; // lobby
    
    Entity* entities; // init
    int* gens; // init
    Tile* tiles; // init
    Fog* fog_per_player[MAX_PLAYERS]; // init
} ServerState;

typedef struct {
    MsgHeader header;
    void* body;
    playerID from;
} InMsg;

static void handle_networking(ServerState* state) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("SERVER: Server socket failed");
        exit(EXIT_FAILURE);
    }
    int flags0 = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags0 | O_NONBLOCK);
    int flags1 = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags1 | O_NONBLOCK);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(server_fd, (struct sockaddr *)&addr, (socklen_t)sizeof(addr)) < 0) {
        perror("SERVER: Bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 5) < 0) {
        perror("SERVER: Listen failed");
        exit(EXIT_FAILURE);
    }
    printf("SERVER: Listening on port %d\n", PORT);

    state->local_fds[0].fd = server_fd;
    state->local_fds[0].events = POLLIN;
    state->local_fds[1].fd = STDIN_FILENO;
    state->local_fds[1].events = POLLIN;
}

static void handle_lobby(ServerState* state) {
    char buf[128];
    while (1) {
        poll(state->local_fds, 2, 100); // 100 ms timeout
        if (state->local_fds[0].revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t client_addr_len = sizeof(client_addr);
            int client_fd = accept(state->local_fds[0].fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
            if (client_fd >= 0) {
                if (state->player_count < MAX_PLAYERS) {
                    int id = state->player_count;
                    printf("SERVER: Player %d connected\n", id);
                    state->client_fds[id].fd = client_fd;
                    state->client_fds[id].events = POLLIN;
                    state->client_addrs[id] = client_addr;
                    state->player_count++;
                    SM_Welcome msg = {id, state->player_count, MAX_PLAYERS};
                    send_msg(client_fd, SM_WELCOME, &msg, 1, sizeof(SM_Welcome));
                    for (int i = 0; i < state->player_count-1; ++i) {
                        SM_PlayerJoinedLobby msg2 = {state->player_count, MAX_PLAYERS};
                        send_msg(state->client_fds[i].fd, SM_PLAYER_JOINED_LOBBY, &msg2, 1, sizeof(SM_PlayerJoinedLobby));
                    }
                } else {
                    printf("SERVER: Server full, rejecting connection\n");
                    close(client_fd);
                }
            }
        }
        if (state->local_fds[1].revents & POLLIN) {
            ssize_t n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
            if (n > 0) {
                buf[n] = '\0';
                if (strcmp("start\n", buf) == 0) {
                    printf("SERVER: Starting game\n");
                    break;
                }
            }
        }
    }
}

static void random_player_start_positions(ServerState* state, int xs_out[MAX_PLAYERS], int ys_out[MAX_PLAYERS]) {
    for (playerID i = 0; i < state->player_count; ++i) {
        while (1) {
            int x = rand()%(state->size.width);
            int y = rand()%(state->size.height);
            if (tile_at(state->tiles, state->size, x, y)->type != T_OCEAN) {
                xs_out[i] = x;
                ys_out[i] = y;
                break;
            }
        }
    }
}
static void fair_player_start_positions(ServerState* state, int xs_out[MAX_PLAYERS], int ys_out[MAX_PLAYERS]) {
    int* cand_xs = malloc(state->size.width * state->size.height * sizeof(int));
    int* cand_ys = malloc(state->size.width * state->size.height * sizeof(int));
    int cand_count = 0;
    for (int x = 0; x < state->size.width; ++x) {
        for (int y = 0; y < state->size.height; ++y) {
            Tile* t = tile_at(state->tiles, state->size, x, y);
            if (t->type == T_GRASSLAND)  {
                cand_xs[cand_count] = x;
                cand_ys[cand_count] = y;
                cand_count++;
            }
        }
    }
    if (cand_count < state->player_count) {
        printf("SERVER: warning: not enough fair starting positions found. Defaulting to random\n");
        free(cand_xs); free(cand_ys);
        random_player_start_positions(state, xs_out, ys_out);
        return;
    }
    for (playerID i = 0; i < state->player_count; ++i) {
        int best = -1;
        int best_min_dist = -1;
        int trials = 20;
        for (int t = 0; t < trials; ++t) {
            int idx = rand() % cand_count;
            int cx = cand_xs[idx];
            int cy = cand_ys[idx];
            int min_dist = INT_MAX;
            for (playerID j = 0; j < i; ++j) {
                int dx = cx - xs_out[j];
                int dy = cy - ys_out[j];
                int dist = dx*dx + dy*dy;
                if (dist < min_dist) min_dist = dist;
            }
            if (min_dist > best_min_dist) {
                best_min_dist = min_dist;
                best = idx;
            }
        }
        xs_out[i] = cand_xs[best];
        ys_out[i] = cand_ys[best];
    }
    free(cand_xs);
    free(cand_ys);
}

static void handle_init(ServerState* state) {
    state->entities = alloc_entities();
    state->gens = calloc(MAX_ENTITIES, sizeof(int));
    state->tiles = alloc_tiles(state->size);
    for (playerID i = 0; i < state->player_count; ++i) {
        Fog* fog = alloc_fog(state->size);
        // for (int x = state->size.width/8; x < state->size.width/8*7; ++x) { // TODO: DEBUG PURPOSES
            // for (int y = state->size.height/8; y < state->size.height/8*7; ++y) {
                // *fog_at(fog, state->size, x, y) = F_FOGGY;
            // }
        // }
        // for (int x = state->size.width/4; x < state->size.width/4*3; ++x) {
            // for (int y = state->size.height/4; y < state->size.height/4*3; ++y) {
                // *fog_at(fog, state->size, x, y) = F_VISIBLE;
            // }
        // }
        state->fog_per_player[i] = fog;
    }
    // generate world
    GenCell* gen_world = alloc_world(state->p);
        generate_world(gen_world, state->p);
        to_tiles(gen_world, state->tiles, state->size);
    free_world(gen_world);
    // player start positions
    int xs_start[MAX_PLAYERS];
    int ys_start[MAX_PLAYERS];
    if (FAIR_START) fair_player_start_positions(state, xs_start, ys_start);
    else random_player_start_positions(state, xs_start, ys_start);
    
    for (playerID i = 0; i < state->player_count; ++i) {
        int x = xs_start[i];
        int y = ys_start[i];
        new_unit(state->entities, i, x, y, U_SETTLER);
        TileID neighs[9];
        neighbor_ids_9(state->size, x, y, neighs);
        for (int d = 0; d < 9; ++d) {
            state->fog_per_player[i][neighs[d]] = F_VISIBLE;
        }
    }
    
    // send start signal and init data
    for (playerID i = 0; i < state->player_count; ++i) {
        int fd = state->client_fds[i].fd;
        send_msg(fd, SM_GAME_STARTING, NULL, 0, 0);
        send_msg(fd, SM_INIT_MAPSIZE, &state->size, 1, sizeof(MapSize));
        send_msg(fd, SM_INIT_ENTITIES, state->entities, MAX_ENTITIES, sizeof(Entity));
        send_msg(fd, SM_INIT_GENS, state->gens, MAX_ENTITIES, sizeof(int));
        send_msg(fd, SM_INIT_TILES, state->tiles, state->size.width*state->size.height, sizeof(Tile));
        send_msg(fd, SM_INIT_FOG, state->fog_per_player[i], state->size.width*state->size.height, sizeof(Fog));
    }
}

static int try_move_unit(ServerState* state, CM_UnitMove move, playerID owner, SM_UpdateTile* tiles_out, int* tiles_out_count, SM_UpdateEntity* entities_out, int* entities_out_count, SM_UpdateFog* fog_out, int* fog_out_count) {
    printf("try_move_unit: ref.id=%d ref.gen=%d\n", move.ref.id, move.ref.gen);
    if (state->gens[move.ref.id] != move.ref.gen) return 0;
    printf("x_from=%d y_from=%d x_to=%d y_to=%d\n", move.x_from, move.y_from, move.x_to, move.y_to);
    if (!inbounds(state->size, move.x_to, move.y_to)) return 0;
    for (Direction8 d = 0; d < 8; ++d) {
        if (move.x_from + DELTAS_8[d][0] == move.x_to && move.y_from + DELTAS_8[d][1] == move.y_to) {
            if (tile_at(state->tiles, state->size, move.x_to, move.y_to)->type == T_OCEAN) return 0;

            state->entities[move.ref.id].x = move.x_to;
            state->entities[move.ref.id].y = move.y_to;
            entities_out[(*entities_out_count)++] = (SM_UpdateEntity){move.ref, state->entities[move.ref.id]};
            
            // need to decide if tiles store the entities that are on top of them
            // TODO: server should not send full tilemap at the start and should send an SM_UpdateTile here instead
            (void)tiles_out;
            (void)tiles_out_count;

            for (int i = 0; i < 9; ++i) {
                int nx = move.x_from + DELTAS_9[i][0];
                int ny = move.y_from + DELTAS_9[i][1];
                Fog* f = fog_at(state->fog_per_player[owner], state->size, nx, ny);
                *f = F_FOGGY;
                fog_out[(*fog_out_count)++] = (SM_UpdateFog){nx, ny, *f};
            }
            for (int i = 0; i < 9; ++i) {
                int nx = move.x_to + DELTAS_9[i][0];
                int ny = move.y_to + DELTAS_9[i][1];
                Fog* f = fog_at(state->fog_per_player[owner], state->size, nx, ny);
                *f = F_VISIBLE;
                fog_out[(*fog_out_count)++] = (SM_UpdateFog){nx, ny, *f};
            }
            return 1;
        }
    }
    return 0;
}

static void handle_playing(ServerState* state) {
    while (1) {
        InMsg in_msgs[MAX_PLAYERS];
        int in_msgs_len = 0;
        poll(state->client_fds, state->player_count, 0);
        for (playerID i = 0; i < state->player_count; ++i) {
            if (state->client_fds[i].revents & POLLIN) {
                MsgHeader header;
                void* body = NULL;
                int r = recv_alloc_msg(state->client_fds[i].fd, &header, &body);
                if (r == -2) continue;
                if (r == -1) {
                    printf("Client disconnected\n");
                    return;
                    break;
                }
                in_msgs[in_msgs_len] = (InMsg){header, body, i};
                in_msgs_len++;
            }
        }
        for (int i = 0; i < in_msgs_len; ++i) {
            InMsg in_msg = in_msgs[i];
            switch (in_msg.header.type) {
                case CM_UNIT_MOVE: {
                    printf("unit moves received from player %d\n", in_msg.from);

                    CM_UnitMove* moves = (CM_UnitMove*)in_msg.body;
                    for (int m = 0; m < in_msg.header.count; ++m) {
                        CM_UnitMove move = moves[m];

                        SM_UpdateTile* tile_updates = malloc(state->size.width * state->size.height * sizeof(SM_UpdateTile));
                        SM_UpdateEntity* entity_updates = malloc(MAX_ENTITIES * sizeof(SM_UpdateEntity));
                        SM_UpdateFog* fog_updates = malloc(state->size.width * state->size.height * sizeof(SM_UpdateFog));
                        int tile_updates_count = 0, entity_updates_count = 0, fog_updates_count = 0;
                        
                        try_move_unit(state, move, in_msg.from, tile_updates, &tile_updates_count, entity_updates, &entity_updates_count, fog_updates, &fog_updates_count);

                        if (tile_updates_count > 0) send_msg(state->client_fds[in_msg.from].fd, SM_UPDATE_TILES, tile_updates, tile_updates_count, sizeof(SM_UpdateTile));
                        if (entity_updates_count > 0) send_msg(state->client_fds[in_msg.from].fd, SM_UPDATE_ENTITIES, entity_updates, entity_updates_count, sizeof(SM_UpdateEntity));
                        if (fog_updates_count > 0) send_msg(state->client_fds[in_msg.from].fd, SM_UPDATE_FOG, fog_updates, fog_updates_count, sizeof(SM_UpdateFog));
                        
                        free(tile_updates);
                        free(entity_updates);
                        free(fog_updates);
                    }

                    break;
                }
                default:
                    //printf("cant handle that messge from player %d\n", in_msg.from);
                    break;
            }
            free(in_msg.body);
        }
        // in_addr_t addr = client_addrs[i].sin_addr.s_addr;
    }
}

int main(void) {

    // read parameters
    // TODO: for now, this is hard coded, later it will be based on argv
    MapSize size = {MAP_W, MAP_H};
    GenParameters p = default_gen_parameters();
    p.size = size;
    p.seed = time(NULL);

    ServerState state = {0};
    state.p = p;
    state.size = size;

    handle_networking(&state);
    handle_lobby(&state);
    handle_init(&state);
    handle_playing(&state);

    free_entities(state.entities);
    free(state.gens);
    free_tiles(state.tiles);
    for (playerID i = 0; i < state.player_count; ++i) {
        free_fog(state.fog_per_player[i]);
        
    }

    close(state.local_fds[0].fd);
}
