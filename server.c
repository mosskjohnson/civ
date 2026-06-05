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

static void random_player_start_positions(ServerState* state, int out[MAX_PLAYERS][2]) {
    for (playerID i = 0; i < state->player_count; ++i) {
        while (1) {
            int x = rand()%(state->size.width);
            int y = rand()%(state->size.height);
            if (tile_at(state->tiles, state->size, x, y)->type != T_OCEAN) {
                out[i][0] = x;
                out[i][1] = y;
                break;
            }
        }
    }
}
static void fair_player_start_positions(ServerState* state, int out[MAX_PLAYERS][2]) {
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
        random_player_start_positions(state, out);
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
                int dx = cx - out[j][0];
                int dy = cy - out[j][1];
                int dist = dx*dx + dy*dy;
                if (dist < min_dist) min_dist = dist;
            }
            if (min_dist > best_min_dist) {
                best_min_dist = min_dist;
                best = idx;
            }
        }
        out[i][0] = cand_xs[best];
        out[i][1] = cand_ys[best];
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
        state->fog_per_player[i] = fog;
    }
    // generate world
    GenCell* gen_world = alloc_world(state->p);
        generate_world(gen_world, state->p);
        to_tiles(gen_world, state->tiles, state->size);
    free_world(gen_world);
    // player start positions
    int start_coords[MAX_PLAYERS][2];
    if (FAIR_START) fair_player_start_positions(state, start_coords);
    else random_player_start_positions(state, start_coords);
    
    for (playerID i = 0; i < state->player_count; ++i) {
        int x = start_coords[i][0];
        int y = start_coords[i][1];
        new_unit(state->entities, i, x, y, U_SETTLER);
        new_unit(state->entities, i, x, y, U_MILITIA);
        
        int neighbor_coords[9][2];
        neighbor_coords_9(state->size, x, y, neighbor_coords);
        for (Direction9 d = 0; d < 9; ++d) {
            int nx = neighbor_coords[d][0];
            int ny = neighbor_coords[d][1];
            *fog_at(state->fog_per_player[i], state->size, nx, ny) = F_VISIBLE;
        }
    }
    for (playerID i = 0; i < state->player_count; ++i) {
        Tile* filtered_tiles = alloc_tiles(state->size);
        Entity* filtered_entities = alloc_entities();
        for (int x = 0; x < state->size.width; ++x) {
            for (int y = 0; y < state->size.height; ++y) {
                if (*fog_at(state->fog_per_player[i], state->size, x, y) == F_VISIBLE) {
                    int neighbor_ids[9];
                    neighbor_ids_9(state->size, x, y, neighbor_ids);
                    for (Direction9 d = 0; d < 9; ++d) {
                        filtered_tiles[neighbor_ids[d]] = state->tiles[neighbor_ids[d]];
                    }
                }
            }
        }
        for (int j = 0; j < MAX_ENTITIES; ++j) {
            Entity* e = &state->entities[j];
            if (*fog_at(state->fog_per_player[i], state->size, e->x, e->y) == F_VISIBLE) {
                filtered_entities[j] = state->entities[j];
            }
        }

        int fd = state->client_fds[i].fd;
        send_msg(fd, SM_GAME_STARTING, NULL, 0, 0);
        send_msg(fd, SM_INIT_MAPSIZE, &state->size, 1, sizeof(MapSize));
        send_msg(fd, SM_INIT_ENTITIES, state->entities, MAX_ENTITIES, sizeof(Entity));
        send_msg(fd, SM_INIT_GENS, state->gens, MAX_ENTITIES, sizeof(int));
        send_msg(fd, SM_INIT_TILES, filtered_tiles, state->size.width*state->size.height, sizeof(Tile));
        //send_msg(fd, SM_INIT_TILES, state->tiles, state->size.width*state->size.height, sizeof(Tile)); // DEBUG PURPOSES
        send_msg(fd, SM_INIT_FOG, state->fog_per_player[i], state->size.width*state->size.height, sizeof(Fog));

        free_tiles(filtered_tiles);
        free_entities(filtered_entities);
    }
}

void send_updates_to_player(ServerState* state, playerID id, 
                            SM_UpdateTile* global_tile_updates, int global_tile_updates_count,
                            SM_UpdateEntity* global_entity_updates, int global_entity_updates_count) 
{
    int total_tiles = state->size.width * state->size.height;
    
    Fog* working_fog = alloc_fog(state->size);
    for (int i = 0; i < total_tiles; ++i) {
        working_fog[i] = state->fog_per_player[id][i];
        if (working_fog[i] == F_VISIBLE) {
            working_fog[i] = F_FOGGY;
        }
    }
    for (int i = 0; i < MAX_ENTITIES; ++i) {
        Entity* e = &state->entities[i];
        if (e->entity_type != E_NIL && e->owner == id) {
            int neighbors[9][2];
            neighbor_coords_9(state->size, e->x, e->y, neighbors);
            for (Direction9 d = 0; d < 9; ++d) {
                TileID t = tile_id_at(state->size, neighbors[d][0], neighbors[d][1]);
                working_fog[t] = F_VISIBLE;
            }
        }
    }

    SM_UpdateFog* local_fog_updates = malloc(total_tiles * sizeof(SM_UpdateFog));
    SM_UpdateTile* local_tile_updates = malloc((total_tiles*9 + global_tile_updates_count) * sizeof(SM_UpdateTile));
    int fog_count = 0;
    int tile_count = 0;
    for (int y = 0; y < state->size.height; ++y) {
        for (int x = 0; x < state->size.width; ++x) {
            TileID t = tile_id_at(state->size, x, y);
            Fog old_fog = state->fog_per_player[id][t];
            Fog new_fog = working_fog[t];
            if (old_fog != new_fog) {
                state->fog_per_player[id][t] = new_fog;
                local_fog_updates[fog_count++] = (SM_UpdateFog){x, y, new_fog};
            }
            if (new_fog == F_VISIBLE && old_fog != F_VISIBLE) {
                int neighbor_coords[9][2];
                neighbor_coords_9(state->size, x, y, neighbor_coords);
                
                for (Direction9 d = 0; d < 9; ++d) {
                    int nx = neighbor_coords[d][0];
                    int ny = neighbor_coords[d][1];
                    TileID nt = tile_id_at(state->size, nx, ny);

                    int already_added = 0;
                    for (int j = 0; j < tile_count; ++j) {
                        if (local_tile_updates[j].x == nx && local_tile_updates[j].y == ny) {
                            already_added = 1;
                            break;
                        }
                    }
                    if (!already_added) {
                        local_tile_updates[tile_count++] = (SM_UpdateTile){nx, ny, state->tiles[nt]};
                    }
                }
            }
        }
    }
    free_fog(working_fog);

    for (int i = 0; i < global_tile_updates_count; ++i) {
        TileID t = tile_id_at(state->size, global_tile_updates[i].x, global_tile_updates[i].y);
        if (state->fog_per_player[id][t] == F_VISIBLE) {
            local_tile_updates[tile_count++] = global_tile_updates[i];
        }
    }

    SM_UpdateEntity* filtered_entities = malloc(global_entity_updates_count * sizeof(SM_UpdateEntity));
    int filtered_entities_count = 0;
    for (int i = 0; i < global_entity_updates_count; ++i) {
        Entity* e = &state->entities[global_entity_updates[i].ref.id];
        TileID t = tile_id_at(state->size, e->x, e->y);
        if (state->fog_per_player[id][t] == F_VISIBLE) {
            filtered_entities[filtered_entities_count++] = global_entity_updates[i];
        }
    }

    if (tile_count > 0)
        send_msg(state->client_fds[id].fd, SM_UPDATE_TILES, local_tile_updates, tile_count, sizeof(SM_UpdateTile));
    if (filtered_entities_count > 0)
        send_msg(state->client_fds[id].fd, SM_UPDATE_ENTITIES, filtered_entities, filtered_entities_count, sizeof(SM_UpdateEntity));
    if (fog_count > 0)
        send_msg(state->client_fds[id].fd, SM_UPDATE_FOG, local_fog_updates, fog_count, sizeof(SM_UpdateFog));

    free(local_fog_updates);
    free(local_tile_updates);
    free(filtered_entities);
}

static int try_move_unit(ServerState* state, CM_UnitMove move, playerID owner, SM_UpdateEntity* entities_out, int* entities_out_count) {
    if (state->gens[move.ref.id] != move.ref.gen) return 0;
    if (state->entities[move.ref.id].owner != owner) return 0;
    if (move.x_from != state->entities[move.ref.id].x || move.y_from != state->entities[move.ref.id].y) return 0;
    
    int possible_move_spots[8][2];
    neighbor_coords_8(state->size, move.x_from, move.y_from, possible_move_spots);
    for (Direction8 d = 0; d < 8; ++d) {
        if (possible_move_spots[d][0] == move.x_to && possible_move_spots[d][1] == move.y_to) {
            if (tile_at(state->tiles, state->size, move.x_to, move.y_to)->type == T_OCEAN) return 0;

            state->entities[move.ref.id].x = move.x_to;
            state->entities[move.ref.id].y = move.y_to;
            entities_out[(*entities_out_count)++] = (SM_UpdateEntity){move.ref, state->entities[move.ref.id]};
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

                    SM_UpdateTile* tile_updates = malloc(state->size.width * state->size.height * sizeof(SM_UpdateTile));
                    SM_UpdateEntity* entity_updates = malloc(MAX_ENTITIES * sizeof(SM_UpdateEntity));
                    SM_UpdateFog* fog_updates = malloc(state->size.width * state->size.height * sizeof(SM_UpdateFog));
                    int tile_updates_count = 0, entity_updates_count = 0, fog_updates_count = 0;

                    CM_UnitMove* moves = (CM_UnitMove*)in_msg.body;
                    for (int m = 0; m < in_msg.header.count; ++m) {
                        CM_UnitMove move = moves[m];
                        try_move_unit(state, move, in_msg.from, entity_updates, &entity_updates_count);    
                    }
                    for (playerID id = 0; id < state->player_count; ++id) {
                        send_updates_to_player(state, id, tile_updates, tile_updates_count, entity_updates, entity_updates_count); 
                    }
                    free(tile_updates);
                    free(entity_updates);
                    free(fog_updates);
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
    GenParameters p = default_gen_parameters_medium();
    p.size = size;
    p.margin_x = 0;
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
