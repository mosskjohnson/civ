#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdlib.h>

#include "data.h"

typedef enum {
    // server to client
    SM_WELCOME,
    SM_PLAYER_JOINED_LOBBY,
    SM_PLAYER_LEFT_LOBBY,
    SM_GAME_STARTING,
    
    SM_COLORS, // eventually the client should choose
    SM_MAPSIZE,
    SM_ENTITIES,
    SM_TILES,
    SM_FOG,

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
    EntityID id;
    int gen;
    int x_from;
    int y_from;
    int x_to;
    int y_to;
} CM_UnitMove;

void send_msg(int fd, MsgType type, const void* body, int count, size_t element_size);
int recv_alloc_msg(int fd, MsgHeader* header_out, void** body_out);

#endif
