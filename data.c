#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "data.h"

UnitTypeInfo unit_type_info[U_COUNT] = { // ORDER OF SPRITESHEET
    [U_SETTLER]    = {"settler",     0,  1,  1, 0, U_LAND | U_PEACEFUL | U_SETTLES,},
    [U_MILITIA]    = {"militia",     1,  1,  1, 0, U_LAND},
    [U_PHALANX]    = {"phalanx",     1,  2,  1, 0, U_LAND},
    [U_LEGION]     = {"legion",      3,  1,  1, 0, U_LAND},
    [U_MUSKETEERS] = {"musketeers",  2,  3,  1, 0, U_LAND},
    [U_RIFLEMEN]   = {"riflemen",    3,  5,  1, 0, U_LAND},
    [U_CAVALRY]    = {"cavalry",     2,  1,  2, 0, U_LAND},
    [U_KNIGHTS]    = {"knights",     4,  2,  2, 0, U_LAND},
    [U_CATAPULT]   = {"catapult",    6,  1,  1, 0, U_LAND},
    [U_CANNON]     = {"cannon",      8,  1,  1, 0, U_LAND},
    [U_CHARIOT]    = {"chariot",     4,  1,  2, 0, U_LAND},
    [U_ARMOR]      = {"armor",      10,  5,  3, 0, U_LAND},
    [U_MECH_INF]   = {"mech_inf",    6,  6,  3, 0, U_LAND},
    [U_ARTILLERY]  = {"artillery",  12,  2,  2, 0, U_LAND},
    [U_FIGHTER]    = {"fighter",     4,  2, 10, 0, U_AIR},
    [U_BOMBER]     = {"bomber",     12,  1,  8, 0, U_AIR},
    [U_TRIREME]    = {"trireme",     1,  1,  3, 2, U_WATER | U_CARRIES},
    [U_SAIL]       = {"sail",        1,  1,  3, 3, U_WATER | U_CARRIES},
    [U_FRIGATE]    = {"frigate",     2,  2,  3, 4, U_WATER | U_CARRIES},
    [U_IRONCLAD]   = {"ironclad",    4,  4,  4, 0, U_WATER},
    [U_CRUISER]    = {"cruiser",     6,  6,  6, 0, U_WATER},
    [U_BATTLESHIP] = {"battleship", 18, 12,  4, 0, U_WATER},
    [U_SUBMARINE]  = {"submarine",   8,  2,  3, 0, U_WATER},
    [U_CARRIER]    = {"carrier",     1, 12,  5, 8, U_WATER | U_CARRIES},
    [U_TRANSPORT]  = {"transport",   0,  3,  4, 8, U_WATER | U_CARRIES},
    [U_NUCLEAR]    = {"nuclear",    99,  0, 16, 0, U_AIR},
    [U_DIPLOMAT]   = {"diplomat",    0,  0,  2, 0, U_LAND | U_PEACEFUL},
    [U_CARAVAN]    = {"caravan",     0,  1,  1, 0, U_LAND | U_PEACEFUL},
};

TileTypeInfo tile_type_info[T_COUNT] = {
    {"nil", 0, 0, 0.0},
    {"desert", T_LAND | T_IRRIGABLE, 1, 1.0},
    {"plains", T_LAND | T_IRRIGABLE, 1, 1.0},
    {"grassland", T_LAND | T_IRRIGABLE, 1, 1.0},
    {"forest", T_LAND, 2, 1.5},
    {"hills", T_LAND | T_IRRIGABLE, 2, 2.0},
    {"mountain", T_LAND, 3, 3.0},
    {"tundra", T_LAND, 1, 1.0},
    {"swamp", T_LAND, 2, 1.5},
    {"jungle", T_LAND, 2, 1.5},
    {"ocean", T_WATER, 1, 1.0},
    {"river", T_LAND, 1, 1.5},
};

Direction4 direction4_opposite(Direction4 d) {
    return (d+2)%4;
}

Direction8 direction8_opposite(Direction8 d) {
    return (d+4)%8;
}

Entity* alloc_entities(void) {
    return calloc(MAX_ENTITIES, sizeof(Entity));
}

void free_entities(Entity* entities) {
    free(entities);
}

static void add_entity(Entity* entities, Entity* e, int slot) {
    entities[slot] = *e;
}

static void rem_entity(Entity* entities, EntityRef ref) {
    // TODO: add generations
    memset(&entities[ref.id], 0, sizeof(Entity));
}

void new_unit(Entity* entities, playerID owner, int x, int y, UnitType unit_type) {
    for (int i = 1; i < MAX_ENTITIES; ++i) {
        if (entities[i].entity_type == E_NIL) {
            Entity* e = &entities[i];
            e->entity_type = E_UNIT;
            e->owner = owner;
            e->x = x;
            e->y = y;
            e->unit_type = unit_type;
            return;
        }
    }
    assert(0 && "Entity limit reached");
}

Tile* alloc_tiles(MapSize size) {
    return calloc(size.width*size.height, sizeof(Tile));
}

void free_tiles(Tile* tiles) {
    free(tiles);
}

TileID tile_id_at(MapSize size, int x, int y) {
    // assert inbounds
    return y*size.width+x;
}

Tile* tile_at(Tile* tiles, MapSize size, int x, int y) {
    return &tiles[y*size.width + x];
}

void neighbor_ids_4(MapSize size, int x, int y, TileID out[4]) {
    for (Direction4 d = 0; d < 4; ++d) {
        int nx = x+DELTAS_4[d][0];
        int ny = y+DELTAS_4[d][1];
        if (!inbounds(size, nx, ny)) {
            out[d] = -1;
        } else {
            out[d] = tile_id_at(size, nx, ny);
        }
    }
}
void neighbor_ids_8(MapSize size, int x, int y, TileID out[8]) {
    for (Direction8 d = 0; d < 8; ++d) {
        int nx = x+DELTAS_8[d][0];
        int ny = y+DELTAS_8[d][1];
        if (!inbounds(size, nx, ny)) {
            out[d] = -1;
        } else {
            out[d] = tile_id_at(size, nx, ny);
        }
    }
}
void neighbor_ids_9(MapSize size, int x, int y, TileID out[9]) {
    for (Direction9 d = 0; d < 9; ++d) {
        int nx = x+DELTAS_9[d][0];
        int ny = y+DELTAS_9[d][1];
        if (!inbounds(size, nx, ny)) {
            out[d] = -1;
        } else {
            out[d] = tile_id_at(size, nx, ny);
        }
    }
}

void neighbor_coords_4(MapSize size, int x, int y, int xs_out[4], int ys_out[4]) {
    for (Direction4 d = 0; d < 4; ++d) {
        int nx = x+DELTAS_4[d][0];
        int ny = y+DELTAS_4[d][1];
        if (!inbounds(size, nx, ny)) {
            xs_out[d] = -1;
            ys_out[d] = -1;
        } else {
            xs_out[d] = nx;
            ys_out[d] = ny;
        }
    }
}
void neighbor_coords_8(MapSize size, int x, int y, int xs_out[8], int ys_out[8]) {
    for (Direction8 d = 0; d < 8; ++d) {
        int nx = x+DELTAS_8[d][0];
        int ny = y+DELTAS_8[d][1];
        if (!inbounds(size, nx, ny)) {
            xs_out[d] = -1;
            ys_out[d] = -1;
        } else {
            xs_out[d] = nx;
            ys_out[d] = ny;
        }
    }
}
void neighbor_coords_9(MapSize size, int x, int y, int xs_out[9], int ys_out[9]) {
    for (Direction9 d = 0; d < 9; ++d) {
        int nx = x+DELTAS_9[d][0];
        int ny = y+DELTAS_9[d][1];
        if (!inbounds(size, nx, ny)) {
            xs_out[d] = -1;
            ys_out[d] = -1;
        } else {
            xs_out[d] = nx;
            ys_out[d] = ny;
        }
    }
}


Fog* alloc_fog(MapSize size) {
    return calloc(size.width*size.height, sizeof(Fog));
}

void free_fog(Fog* fog) {
    free(fog);
}

TileID fog_id_at(MapSize size, int x, int y) {
    return y*size.width+x;
}

Fog* fog_at(Fog* fog, MapSize size, int x, int y) {
    return &fog[y*size.width+x];
}

int inbounds(MapSize size, int x, int y) {
    return (x >= 0 && x < size.width && y >= 0 && y < size.height);
}
