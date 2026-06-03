#include <stdlib.h>

#include "data.h"

typedef enum {
    // server to client
    SM_WELCOME,
    SM_PLAYER_JOINED_LOBBY,
    SM_PLAYER_LEFT_LOBBY,
    SM_GAME_STARTING,
    
    SM_INIT_MAPSIZE,
    SM_INIT_ENTITIES,
    SM_INIT_GENS,
    SM_INIT_TILES,
    SM_INIT_FOG,

    SM_UPDATE_TILES,
    SM_UPDATE_ENTITIES,
    SM_UPDATE_FOG,
    // client to server
    CM_UNIT_MOVE,
} MsgType;

typedef struct {
    MsgType type;
    int count;
    size_t element_size;
} MsgHeader;

typedef struct {
    playerID player_id;
    int num_players_now;
    int max_players;
} SM_Welcome;

typedef struct {
    int num_players_now;
    int max_players;
} SM_PlayerJoinedLobby;

typedef struct {
    int num_players_now;
    int max_players;
} SM_PlayerLeftLobby;

typedef struct {
    int x;
    int y;
    Tile updated;
} SM_UpdateTile;

typedef struct {
    EntityRef ref;
    Entity updated;
} SM_UpdateEntity;

typedef struct {
    int x;
    int y;
    Fog updated;
} SM_UpdateFog;

typedef struct {
    EntityRef ref;
    int x_from;
    int y_from;
    int x_to;
    int y_to;
} CM_UnitMove;

void send_msg(int fd, MsgType type, const void* body, int count, size_t element_size);
int recv_alloc_msg(int fd, MsgHeader* header_out, void** body_out);
