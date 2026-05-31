#include <stdlib.h>
#include <string.h>

#include "data.h"

Direction direction_opposite(Direction d) {
    return (d+4)%8;
}

UnitTypeInfo unit_type_info[U_COUNT] = { // DO NOT CHANGE ORDER!
    [U_SETTLER]    = {"settler",    U_LAND | U_SETTLES,   0,  1,  1, 0},
    [U_MILITIA]    = {"militia",    U_LAND,               1,  1,  1, 0},
    [U_PHALANX]    = {"phalanx",    U_LAND,               1,  2,  1, 0},
    [U_LEGION]     = {"legion",     U_LAND,               3,  1,  1, 0},
    [U_MUSKETEERS] = {"musketeers", U_LAND,               2,  3,  1, 0},
    [U_RIFLEMEN]   = {"riflemen",   U_LAND,               3,  5,  1, 0},
    [U_CAVALRY]    = {"cavalry",    U_LAND,               2,  1,  2, 0},
    [U_KNIGHTS]    = {"knights",    U_LAND,               4,  2,  2, 0},
    [U_CATAPULT]   = {"catapult",   U_LAND,               6,  1,  1, 0},
    [U_CANNON]     = {"cannon",     U_LAND,               8,  1,  1, 0},
    [U_CHARIOT]    = {"chariot",    U_LAND,               4,  1,  2, 0},
    [U_ARMOR]      = {"armor",      U_LAND,              10,  5,  3, 0},
    [U_MECH_INF]   = {"mech_inf",   U_LAND,               6,  6,  3, 0},
    [U_ARTILLERY]  = {"artillery",  U_LAND,              12,  2,  2, 0},
    [U_FIGHTER]    = {"fighter",    U_AIR,                4,  2, 10, 0},
    [U_BOMBER]     = {"bomber",     U_AIR,               12,  1,  8, 0},
    [U_TRIREME]    = {"trireme",    U_WATER | U_CARRIES,  1,  1,  3, 2},
    [U_SAIL]       = {"sail",       U_WATER | U_CARRIES,  1,  1,  3, 3},
    [U_FRIGATE]    = {"frigate",    U_WATER | U_CARRIES,  2,  2,  3, 4},
    [U_IRONCLAD]   = {"ironclad",   U_WATER,              4,  4,  4, 0},
    [U_CRUISER]    = {"cruiser",    U_WATER,              6,  6,  6, 0},
    [U_BATTLESHIP] = {"battleship", U_WATER,             18,  12, 4, 0},
    [U_SUBMARINE]  = {"submarine",  U_WATER,              8,  2,  3, 0},
    [U_CARRIER]    = {"carrier",    U_WATER | U_CARRIES,  1,  12, 5, 8},
    [U_TRANSPORT]  = {"transport",  U_WATER | U_CARRIES,  0,  3,  4, 8},
    [U_NUCLEAR]    = {"nuclear",    U_AIR,               99,  0, 16, 0},
    [U_DIPLOMAT]   = {"diplomat",   U_LAND,               0,  0,  2, 0},
    [U_CARAVAN]    = {"caravan",    U_LAND,               0,  1,  1, 0},
};

void new_unit(Entity* e, UnitType unit_type, playerID owner, int x, int y) {
    e->entity_type = E_UNIT;
    e->unit_type = unit_type;
    e->owner = owner;
    e->x = x;
    e->y = y;
}

Entity* alloc_entities(void) {
    return calloc(MAX_ENTITIES, sizeof(Entity));
}

void add_entity(Entity* entities, Entity* e, int slot) {
    entities[slot] = *e;
}

void rem_entity(Entity* entities, int slot) {
    // TODO: add generations
    memset(&entities[slot], 0, sizeof(Entity));
}

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

Tile* alloc_tiles(void) {
    return calloc(TILEMAP_W*TILEMAP_H, sizeof(Tile));
}

Fog* alloc_fog(void) {
    return calloc(TILEMAP_W*TILEMAP_H, sizeof(Fog));
}
