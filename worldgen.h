#ifndef WORLDGEN_H
#define WORLDGEN_H

#include "data.h"

#define ELEVATION_LEVELS 3
#define TEMPERATURE_LEVELS 3
#define MOISTURE_LEVELS 3

extern TileType biome_matrix[ELEVATION_LEVELS][TEMPERATURE_LEVELS][MOISTURE_LEVELS];

typedef struct {
    int elevation;
    float elevation_norm;
    float clouds;
    float moisture;
    float temperature;
    int river_source;
    int river; //bool
    TileType final_tile_type;
} GenCell;

typedef struct {
    MapSize size;
    unsigned int seed;
    int margin_x;
    int margin_y;
    float desired_land_proportion; // 0..1.0
    float fragmentation; // 0..1.0
    float evaporation_factor; // 0..1.0
    float precipitation_factor; // 0..1.0
    float runoff_factor; // 0..1.0
    float seepage_factor; // 0..1.0
    Direction8 wind_direction;
    float wind_strength; // proportion of 1.0
    int water_cycles;
    float desired_river_proportion;
    float elevation_cutoffs[2];
    float temperature_cutoffs[2];
    float moisture_cutoffs[2];
} GenParameters;

GenParameters default_gen_parameters_medium(void);
GenParameters default_gen_parameters_large(void);


GenCell* alloc_world(GenParameters params);

void free_world(GenCell* world);

void generate_world(GenCell* out, GenParameters params);

void to_tiles(GenCell* in, Tile* tiles, MapSize size);

#endif
