#ifndef DATA_H
#define DATA_H

#include <stdlib.h>
#include <string.h>

#define MAX_ENTITIES 1024
#define TILEMAP_W 160
#define TILEMAP_H 120
#define CONNECTIONS_PERMUTATIONS 16

typedef int playerID;

typedef enum {
    D_N  = 0,
    D_NE = 1,
    D_E  = 2,
    D_SE = 3,
    D_S  = 4,
    D_SW = 5,
    D_W  = 6,
    D_NW = 7,
} Direction;

Direction direction_opposite(Direction d);

typedef struct {
    int id;
    int gen;
} EntityRef;

typedef enum {
    E_NIL,
    E_UNIT,
    E_CITY,
    E_BUILDING,
} EntityType;

typedef enum {  // DO NOT CHANGE ORDER!
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

typedef struct {
    const char* name;
    UnitTraits traits;
    int attack;
    int defense;
    int movement;
    int carries;
} UnitTypeInfo;
extern UnitTypeInfo unit_type_info[U_COUNT];

typedef struct {
    EntityType entity_type;
    playerID owner;
    int x;
    int y;
    EntityRef entity_on_next;
    union {
        struct { // UNIT
            UnitType unit_type;
            EntityRef carrying_first;
            EntityRef carrying_next;
        };
        struct { // CITY
            int population;
            char name[32];
            EntityRef building_first;
        };
        struct { // BUILDING
            EntityRef building_next;
            
        };
    };
} Entity;

void new_unit(Entity* e, UnitType unit_type, playerID owner, int x, int y);

Entity* alloc_entities(void);

void add_entity(Entity* entities, Entity* e, int slot);

void rem_entity(Entity* entities, int slot);

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

typedef struct {
    const char* name;
    TileTraits traits;
    int movement_cost;
    float defense_multiplier;
} TileTypeInfo;
extern TileTypeInfo tile_type_info[T_COUNT];

typedef enum {
    C_NORTH = (1 << 0),
    C_EAST = (1 << 1),
    C_SOUTH = (1 << 2),
    C_WEST = (1 << 3),
} Connections;

typedef enum {
    IM_IRRIGATION = (1 << 0),
    IM_ROAD       = (1 << 1),
    IM_RAILROAD   = (1 << 2),
} Improvements;

typedef struct {
    TileType type;
    Connections connections;
    Improvements improvements;
    EntityRef entity_on_first;
} Tile;

Tile* alloc_tiles(void);

typedef enum {
    F_UNDISCOVERED = 0,
    F_FOGGY,
    F_VISIBLE,
} Fog;

Fog* alloc_fog(void);

typedef enum {
    SM_GAME_STARTING,
    SM_INIT_DATA,
    SM_TILE_CHANGED,
    SM_UNIT_MOVED,
    SM_UNIT_CHANGED,
    SM_FOG_CHANGED,
} ServerMsgType;

typedef struct {
    size_t len;
    ServerMsgType type;
} ServerMsgHeader;

typedef enum {
    CM_ACTION,
} ClientMsgType;

typedef struct {
    size_t len;
    ClientMsgType type;
} ClientMsgHeader;

/*

Server has:
    playerID 1,2,3
    [fd1, fd2, fd3]
    [addr1, addr2, addr3]
    tiles (true)
    entities (true)
    [fog1, fog2, fog3]
    [turn1, turn2, turn3]

Client has:
    playerID
    tiles (partially filled)
    entities (partially filled)
    fog

Server:
    for id in 1..3 {
        send_init_data(fd[id])
    }
    loop {
        msgs = []
        for id in 1..3 {
            read_for_messages(fd[id])
            msgs.push(msg)
        }
        for msg in msgs {
            bool legal = validate()
            if legal {
                send_message_delta_state()
            } else {
                send_message_action_denied()
            }
        }
    }

Client:
    read_for_init_state()
    tiles = init
    entities = init
    fog = init
    loop {
        read_input()
        action = map_input_to_action()
        send_message_action()

        msgs = []
        read_for_messages()
        for msg in msgs {
            if msg==delta_state {
                tiles += msg.delta
                entities += msg.delta
                fog += msg.delta
            }
        }
        render()
    }
    
*/


#endif
