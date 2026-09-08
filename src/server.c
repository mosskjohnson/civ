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
#include <stdbool.h>

#include "data.h"
#include "worldgen.h"
#include "message.h"

#define PORT 8080
#define MAX_PLAYERS 8
#define MAP_W 80
#define MAP_H 50
#define FAIR_START 1
#define PLAYER_START_ZONE_SIZE 15

typedef int playerID;

typedef struct {
    MapSize size; // initialized by user
    GenParameters p; // initialized by user via argv
    float seconds_per_turn_base;
    float seconds_per_turn_per_entity;
    float seconds_per_turn_total;

    struct timespec time_start;
    
    struct pollfd local_fds[2]; // initialized by networking, local_fds[0] is server_fd and local_fds[1] is STDIN
    
    struct pollfd client_fds[MAX_PLAYERS]; // initialized by lobby
    struct sockaddr_in client_addrs[MAX_PLAYERS]; // lobby
    int player_count; // lobby

    int turn;
    float moment_turn_started;
    CivColor colors[MAX_PLAYERS];
    Entity* entities; // init
    Tile* tiles; // init
    Fog* fog_per_player[MAX_PLAYERS]; // init
} ServerState;

static void clock_start(ServerState* state) {
    clock_gettime(CLOCK_MONOTONIC, &state->time_start);
}

static float clock_elapsed(ServerState* state) {
    struct timespec time_curr;
    clock_gettime(CLOCK_MONOTONIC, &time_curr);
    return (time_curr.tv_sec - state->time_start.tv_sec) + (time_curr.tv_nsec - state->time_start.tv_nsec) / 1e9;
}

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

    int reuse = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

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
                    for (int i = 0; i < state->player_count; ++i) {
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

static void recalculate_fog(ServerState* state, playerID id) {
    for (TileID i = 0; i < state->size.width*state->size.height; ++i) { // set to foggy initially
        if (state->fog_per_player[id][i] == F_VISIBLE) {
            state->fog_per_player[id][i] = F_FOGGY;
        }
    }
    for (int i = 0; i < MAX_ENTITIES; ++i) { // overwrite with visible
        Entity* e = &state->entities[i];
        if (e->entity_type != E_NIL && e->owner == id) {
            int neighbor[9][2];
            neighbor_coords_9(state->size, e->x, e->y, neighbor);
            for (Direction9 d = 0; d < 9; ++d) {
                *fog_at(state->fog_per_player[id], state->size, neighbor[d][0], neighbor[d][1]) = F_VISIBLE;
            }
        }
    }
}

static void calculate_filtered_view(ServerState* state, playerID id, Entity* filtered_entities_out, Tile* filtered_tiles_out) {
    for (int i = 0; i < MAX_ENTITIES; ++i) {
        Entity* e = &state->entities[i];
        if (e->owner == id || (*fog_at(state->fog_per_player[id], state->size, e->x, e->y) == F_VISIBLE)) {
            filtered_entities_out[i] = *e;
        }
    }
    for (int x = 0; x < state->size.width; ++x) {
        for (int y = 0; y < state->size.height; ++y) {
            if (*fog_at(state->fog_per_player[id], state->size, x, y) != F_UNDISCOVERED) {
                int neighbor_ids[9];
                neighbor_ids_9(state->size, x, y, neighbor_ids);
                for (Direction9 d = 0; d < 9; ++d) {
                    filtered_tiles_out[neighbor_ids[d]] = state->tiles[neighbor_ids[d]];
                }
            }
        }
    }
}

static void handle_init(ServerState* state) {
    state->entities = alloc_entities();
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
        new_unit(state->entities, state->tiles, state->size, i, x, y, U_SETTLER);
        new_unit(state->entities, state->tiles, state->size, i, x, y, U_MILITIA);
        new_unit(state->entities, state->tiles, state->size, i, x, y, U_CHARIOT);

        // ship, temporary
        int x2 = 0;
        int y2 = 0;
        while (1) {
            x2 = rand()%(state->size.width);
            y2 = rand()%(state->size.height);
            if (tile_at(state->tiles, state->size, x2, y2)->type == T_OCEAN) {
                break;
            }
        }
        new_unit(state->entities, state->tiles, state->size, i, x2, y2, U_FRIGATE);

        recalculate_fog(state, i);
    }
    for (playerID i = 0; i < state->player_count; ++i) {
        Entity* filtered_entities = alloc_entities();
        Tile* filtered_tiles = alloc_tiles(state->size);

        calculate_filtered_view(state, i, filtered_entities, filtered_tiles);

        int fd = state->client_fds[i].fd;
        send_msg(fd, SM_MAPSIZE, &state->size, 1, sizeof(MapSize));
        send_msg(fd, SM_COLORS, state->colors, MAX_PLAYERS, sizeof(CivColor));

        // send_updates(state, i);
        send_msg(fd, SM_ENTITIES, state->entities, MAX_ENTITIES, sizeof(Entity));
        //send_msg(fd, SM_TILES, filtered_tiles, state->size.width*state->size.height, sizeof(Tile));
        send_msg(fd, SM_TILES, state->tiles, state->size.width*state->size.height, sizeof(Tile)); // DEBUG PURPOSES
        send_msg(fd, SM_FOG, state->fog_per_player[i], state->size.width*state->size.height, sizeof(Fog));
        
        send_msg(fd, SM_GAME_STARTING, NULL, 0, 0);

        free_tiles(filtered_tiles);
        free_entities(filtered_entities);
    }
}

static void send_updates(ServerState* state, playerID id) {
    Entity* filtered_entities = alloc_entities();
    Tile* filtered_tiles = alloc_tiles(state->size);

    calculate_filtered_view(state, id, filtered_entities, filtered_tiles);
    
    int fd = state->client_fds[id].fd;
    send_msg(fd, SM_ENTITIES, filtered_entities, MAX_ENTITIES, sizeof(Entity));
    send_msg(fd, SM_TILES, filtered_tiles, state->size.width*state->size.height, sizeof(Tile));
    send_msg(fd, SM_FOG, state->fog_per_player[id], state->size.width*state->size.height, sizeof(Fog));

    free_entities(filtered_entities);
    free_tiles(filtered_tiles);
}

static int try_move_unit(ServerState* state, CM_UnitMove move, playerID owner) {
    EntityID e_id = move.id;
    Entity* e = &state->entities[e_id];
    
    if (e->entity_type != E_UNIT) return 0;
    if (move.gen != e->gen) return 0;
    if (owner != e->owner) return 0;
    if (move.x_from != e->x || move.y_from != e->y) return 0;
    if (e->movement_remaining <= 0) return 0;
    
    bool ok = false;
    int possible_coords[8][2];
    neighbor_coords_8(state->size, move.x_from, move.y_from, possible_coords);
    for (Direction8 d = 0; d < 8; ++d) {
        if (possible_coords[d][0] == move.x_to && possible_coords[d][1] == move.y_to) {
            ok = true;
        }
    }
    if (!ok) return 0;

    Tile* t = tile_at(state->tiles, state->size, move.x_to, move.y_to);

    EntityID other_id = t->entity_on_first;
    Entity* other = &state->entities[other_id];    

    bool board_legal_land = other_id != 0 && other->owner == e->owner && has_traits(e, U_LAND) && has_traits(other, U_CARRIES_LAND);
    bool board_legal_air = other_id != 0 && other->owner == e->owner && has_traits(e, U_AIR) && has_traits(other, U_CARRIES_AIR);

    if (board_legal_land || board_legal_air) {
        // board
        move_unit_board(state->entities, state->tiles, state->size, e_id, other_id, move.x_to, move.y_to);
    } else if (other_id == 0 || other->owner == e->owner) {
        // regular move
        if (has_traits(e, U_LAND) && t->type == T_OCEAN) return 0;
        if (has_traits(e, U_WATER) && t->type != T_OCEAN) return 0;

        if (e->carrying_parent == 0) {
            move_unit(state->entities, state->tiles, state->size, e_id, move.x_to, move.y_to);
        } else {
            move_unit_unboard(state->entities, state->tiles, state->size, e_id, e->carrying_parent, move.x_to, move.y_to);
        }

    } else {
        // battle
        if (has_traits(e, U_LAND) && t->type == T_OCEAN) return 0;

        int win = battle(e, other, state->tiles);
        printf("Battle, %s wins\n", win ? "attacker" : "defender");
        
        if (win) rem_unit(state->entities, state->tiles, other_id);
        else rem_unit(state->entities, state->tiles, e_id);
    }

    recalculate_fog(state, owner);
    
    return 1;
}

void next_turn(ServerState* state, float moment_now) {
    state->turn++;
    state->moment_turn_started = moment_now;
    
    int entities_count = 0;
    for (int i = 0; i < MAX_ENTITIES; ++i) {
        Entity* e = &state->entities[i];
        switch (e->entity_type) {
            case E_NIL: break;
            case E_UNIT: {
                e->movement_remaining = unit_type_info[e->unit_type].movement;
                entities_count++;
                break;
            }
            case E_CITY: {
                entities_count++;
                break;
            }
        }
    }
    state->seconds_per_turn_total = state->seconds_per_turn_base + (entities_count*state->seconds_per_turn_per_entity); 
    for (playerID id = 0; id < state->player_count; ++id) {
        send_updates(state, id);

        SM_NextTurn msg = {
            .turn = state->turn,
            .seconds = state->seconds_per_turn_total,
        };
        send_msg(state->client_fds[id].fd, SM_NEXT_TURN, &msg, 1, sizeof(msg));
    }
    printf("turn %d\n", state->turn);
}

static void handle_playing(ServerState* state) {
    while (1) {
        poll(state->client_fds, state->player_count, 0);
        for (playerID id = 0; id < state->player_count; ++id) {
            if (state->client_fds[id].revents & POLLIN) {
                MsgHeader header;
                void* body = NULL;
                int r = recv_alloc_msg(state->client_fds[id].fd, &header, &body);
                if (r == -1) {
                    printf("Client disconnected\n");
                    return;
                    break;
                }
                switch (header.type) {
                   case CM_UNIT_MOVE: {
                       printf("unit moves received from player %d\n", id);
   
                       CM_UnitMove* moves = (CM_UnitMove*)body;
                       for (int m = 0; m < header.count; ++m) {
                           CM_UnitMove move = moves[m];
                           try_move_unit(state, move, id);    
                       }
                       for (playerID id2 = 0; id2 < state->player_count; ++id2) {
                           send_updates(state, id2);
                       }
                       break;
                   }
                   default:
                       //printf("cant handle that messge from player %d\n", in_msg.from);
                       break;
               }
               free(body);
            }
        }
        // timing
        float moment_now = clock_elapsed(state);
        if (moment_now - state->moment_turn_started >= state->seconds_per_turn_total) {
            next_turn(state, moment_now);
        }
    }
}

int main(void) {
    srand(time(NULL));
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
    state.seconds_per_turn_base = 4;
    state.seconds_per_turn_per_entity = 1;

    // TODO: players should choose colors
    int c = rand()%CIVCOLOR_COUNT;
    for (int i = 0; i < CIVCOLOR_COUNT; ++i) {
        int offsetted = (i + c) % CIVCOLOR_COUNT;
        state.colors[i] = (CivColor)offsetted;
        printf("Color%d: %d\n", i, offsetted);
    }

    clock_start(&state);

    handle_networking(&state);
    handle_lobby(&state);
    handle_init(&state);
    handle_playing(&state);

    free_entities(state.entities);
    free_tiles(state.tiles);
    for (playerID i = 0; i < state.player_count; ++i) {
        free_fog(state.fog_per_player[i]);
    }

    close(state.local_fds[0].fd);
}
