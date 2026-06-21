#ifndef DATA_H
#define DATA_H

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define MAX_ENTITIES 1024
#define CONNECTIONS_PERMUTATIONS 16
#define BATTLE_CURVE_SHARPNESS 0.4

#define FOR_COLORS(DO) \
    DO(0, CIVWHITE, 0xebebebff, 0x8a8a8eff) \
    DO(1, CIVRED, 0xf75555ff, 0x822014ff) \
    DO(2, CIVGREEN, 0x61e365ff, 0x2c7900ff) \
    DO(3, CIVBLUE, 0x798effff, 0x304db2ff) \
    DO(4, CIVYELLOW, 0xffff96ff, 0x61e365ff) \
    DO(5, CIVTEAL, 0x0ce3ebff, 0x00aaaaff) \
    DO(6, CIVPINK, 0xff55ffff, 0x822014ff) \
    DO(7, CIVGRAY, 0x8a8a8eff, 0x4d4d4dff)

#define DEFINE_ENUMERATION(id, name, ...) name = id,
typedef enum {
    FOR_COLORS(DEFINE_ENUMERATION)
    CIVCOLOR_COUNT,
} CivColor;
#undef DEFINE_ENUMERATION

unsigned int color_hex(CivColor c);
unsigned int highlight_hex(CivColor c);

typedef int playerID;
typedef int EntityID;
typedef int TileID;

typedef struct {
    int width;
    int height;
} MapSize;

typedef enum {
    D_N4 = 0,
    D_E4 = 1,
    D_S4 = 2,
    D_W4 = 3,
} Direction4;

typedef enum {
    D_N8  = 0,
    D_NE8 = 1,
    D_E8  = 2,
    D_SE8 = 3,
    D_S8  = 4,
    D_SW8 = 5,
    D_W8  = 6,
    D_NW8 = 7,
} Direction8;

typedef enum {
    D_N9  = 0,
    D_NE9 = 1,
    D_E9  = 2,
    D_SE9 = 3,
    D_S9  = 4,
    D_SW9 = 5,
    D_W9  = 6,
    D_NW9 = 7,
    D_ORIG9 = 8,
} Direction9;

static const int DELTAS_4[4][2] = {
    {0, -1}, {1, 0}, {0, 1}, {-1, 0},
};

static const int DELTAS_8[8][2] = {
    { 0,-1}, { 1,-1}, { 1, 0}, { 1, 1},
    { 0, 1}, {-1, 1}, {-1, 0}, {-1,-1},
};

static const int DELTAS_9[9][2] = {
    { 0,-1}, { 1,-1}, { 1, 0}, { 1, 1},
    { 0, 1}, {-1, 1}, {-1, 0}, {-1,-1},
    { 0, 0}, 
};

static const int DELTAS_R2[21][2] = {
    { 0,-1}, { 1,-1}, { 1, 0}, { 1, 1},
    { 0, 1}, {-1, 1}, {-1, 0}, {-1,-1},
    { 0, 0},
    { 0,-2}, { 1,-2}, { 2,-1}, { 2, 0},
    { 2, 1}, { 1, 2}, { 0, 2}, {-1, 2},
    {-2, 1}, {-2, 0}, {-2,-1}, {-1,-2},
};

typedef enum {
    E_NIL,
    E_UNIT,
    E_CITY,
} EntityType;

typedef enum {  // ORDER OF SPRITESHEET
    U_SETTLER = 0,
    U_MILITIA,
    U_PHALANX,
    U_LEGION,
    U_MUSKETEERS,
    U_RIFLEMEN,
    U_CAVALRY,
    U_KNIGHTS,
    U_CATAPULT,
    U_CANNON,
    U_CHARIOT,
    U_ARMOR,
    U_MECH_INF,
    U_ARTILLERY,
    U_FIGHTER,
    U_BOMBER,
    U_TRIREME,
    U_SAIL,
    U_FRIGATE,
    U_IRONCLAD,
    U_CRUISER,
    U_BATTLESHIP,
    U_SUBMARINE,
    U_CARRIER,
    U_TRANSPORT,
    U_NUCLEAR,
    U_DIPLOMAT,
    U_CARAVAN,
    U_COUNT,
} UnitType;

typedef enum {
    U_LAND     = (1 << 0),
    U_WATER    = (1 << 1),
    U_AIR      = (1 << 2),
    U_CARRIES  = (1 << 3),
    U_SETTLES  = (1 << 4),
    U_PEACEFUL = (1 << 5),
} UnitTraits;

typedef enum {
    B_NIL = 0,
    B_BARRACKS,
    B_TEMPLE,
} Building;

typedef struct {
    EntityType entity_type;
    playerID owner;
    int gen;
    int x;
    int y;
    EntityID entity_on_next;
    TileID parent;
    union {
        struct { // UNIT
            UnitType unit_type;
            int movement_remaining;
            EntityID carrying_first;
            EntityID carrying_next;
            EntityID carrying_parent;
        };
        struct { // CITY
            int population;
            char name[32];
            Building buildings[64];
        };
    };
} Entity;

typedef enum { // DO NOT CHANGE ORDER!
    T_NIL = 0,
    T_DESERT,
    T_PLAINS,
    T_GRASSLAND,
    T_FOREST,
    T_HILLS,
    T_MOUNTAIN,
    T_TUNDRA,
    T_ARCTIC,
    T_SWAMP,
    T_JUNGLE,
    T_OCEAN,
    T_RIVER,
    T_COUNT,
} TileType;

typedef enum {
    T_LAND      = (1 << 0),
    T_WATER     = (1 << 1),
    T_IRRIGABLE = (1 << 2),
} TileTraits;

typedef enum {
    IM_IRRIGATION = (1 << 0),
    IM_ROAD       = (1 << 1),
    IM_RAILROAD   = (1 << 2),
} Improvements;

typedef struct {
    TileType type;
    Improvements improvements;
    EntityID entity_on_first;
} Tile;

typedef enum {
    F_UNDISCOVERED = 0,
    F_FOGGY,
    F_VISIBLE,
} Fog;

typedef struct {
    const char* name;
    int attack;
    int defense;
    int movement;
    int carries;
    UnitTraits traits;
} UnitTypeInfo;
extern UnitTypeInfo unit_type_info[U_COUNT];

typedef struct {
    const char* name;
    TileTraits traits;
    int movement_cost;
    float defense_bonus;
} TileTypeInfo;
extern TileTypeInfo tile_type_info[T_COUNT];

Direction4 direction4_opposite(Direction4 d);
Direction8 direction8_opposite(Direction8 d);

Entity* alloc_entities(void);
void free_entities(Entity* entities);
void new_unit(Entity* entities, Tile* tiles, MapSize size, playerID owner, int x, int y, UnitType unit_type);
void rem_unit(Entity* entities, Tile* tiles, EntityID e);
void move_unit(Entity* entities, Tile* tiles, MapSize size, EntityID i, int x_to, int y_to);
int battle(Entity* attacker, Entity* defender, Tile* tiles);
int has_traits(Entity* e, UnitTraits traits);

Tile* alloc_tiles(MapSize size);
void free_tiles(Tile* tiles);
TileID tile_id_at(MapSize size, int x, int y);
Tile* tile_at(Tile* tiles, MapSize size, int x, int y);
void neighbor_ids_4(MapSize size, int x, int y, TileID out[4]);
void neighbor_ids_8(MapSize size, int x, int y, TileID out[8]);
void neighbor_ids_9(MapSize size, int x, int y, TileID out[9]);
void neighbor_coords_4(MapSize size, int x, int y, int out[4][2]);
void neighbor_coords_8(MapSize size, int x, int y, int out[8][2]);
void neighbor_coords_9(MapSize size, int x, int y, int out[9][2]);
int wrapped_x(MapSize size, int x);
int wrapped_y(MapSize size, int y);

Fog* alloc_fog(MapSize size);
void free_fog(Fog* fog);
TileID fog_id_at(MapSize size, int x, int y);
Fog* fog_at(Fog* fog, MapSize size, int x, int y);

int inbounds(MapSize size, int x, int y);

#endif
