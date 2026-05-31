#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include "worldgen.h"

#define at(world, params, x, y) world[(y)*params.width + (x)]
#define inbounds(a, bound_begin, bound_end) (a >= bound_begin && a < bound_end)
#define frand() (float)rand()/(float)RAND_MAX

#define POLES_H 8
#define CHANNEL_W 4

TileType biome_matrix[ELEVATION_LEVELS][TEMPERATURE_LEVELS][MOISTURE_LEVELS] = {
    { // elevation 0
        {T_TUNDRA,T_TUNDRA,T_ARCTIC}, // temperature 0
        {T_PLAINS,T_GRASSLAND,T_SWAMP}, // temperature 1
        {T_DESERT,T_GRASSLAND,T_JUNGLE}, // temperature 2
    },
    { // elevation 1
        {T_TUNDRA,T_TUNDRA,T_ARCTIC}, // temperature 0
        {T_HILLS,T_FOREST,T_FOREST}, // temperature 1
        {T_HILLS,T_FOREST,T_JUNGLE}, // temperature 2
    },
    { // elevation 2
        {T_MOUNTAIN,T_MOUNTAIN,T_MOUNTAIN}, // temperature 0
        {T_MOUNTAIN,T_MOUNTAIN,T_MOUNTAIN}, // temperature 1
        {T_MOUNTAIN,T_MOUNTAIN,T_MOUNTAIN}, // temperature 2
    },
};

GenCell* allocate_world(GenParameters params) {
    return calloc(params.width*params.height, sizeof(GenCell));
}

void free_world(GenCell* world) {
    free(world);
}

void generate_world(GenCell* out, GenParameters params) {
    srand(params.seed);

    int area = params.width*params.height;
    int iter_range = area * (-params.fragmentation+1);
    
    int bound_x_left   = CHANNEL_W;
    int bound_x_right  = params.width-CHANNEL_W;
    int bound_y_top    = POLES_H;
    int bound_y_bottom = params.height - POLES_H;

    int total_land_area = 0;

    // elevation
    #define REGION_COUNT 2
    int region_bounds[REGION_COUNT][4] = {
        {bound_x_left,bound_x_right/2,bound_y_top,bound_y_bottom}, 
        {bound_x_right/2 + CHANNEL_W, bound_x_right, bound_y_top, bound_y_bottom},
    };
    for (int r = 0; r < REGION_COUNT; ++r) {
        int r_bound_x_left = region_bounds[r][0];
        int r_bound_x_right = region_bounds[r][1];
        int r_bound_y_top = region_bounds[r][2];
        int r_bound_y_bottom = region_bounds[r][3];
       
        int desired_land_area = (int)(area * params.desired_land_proportion / REGION_COUNT);
        int region_land_area = 0;
        #define DELTAS_COUNT 5
        while (region_land_area < desired_land_area) {
            int iterations = 1 + rand()%iter_range;
            int x = r_bound_x_left + rand()%(r_bound_x_right-r_bound_x_left);
            int y = r_bound_y_top + rand()%(r_bound_y_bottom-r_bound_y_top);
            
            int deltas[DELTAS_COUNT][2] = {{0,0}, {1,0}, {0,1}, {-1,0}, {0,-1}}; // + shape
            // int deltas[DELTAS_COUNT][2] = {{0,0}, {1,0}, {0,1}}; // L shape
            for (int i = 0; i < iterations; ++i) {
                if (!inbounds(x, r_bound_x_left+1, r_bound_x_right) || !inbounds(y, r_bound_y_top+1, r_bound_y_bottom)) {
                    break;
                }
                for (int j = 0; j < DELTAS_COUNT; ++j) {
                    GenCell* ref = out + (y+deltas[j][1])*params.width + (x+deltas[j][0]);
                    if (ref->elevation == 0) {
                        total_land_area++;
                        region_land_area++;
                    }
                    ref->elevation++;
                }
                x += (-1 + rand()%3);
                y += (-1 + rand()%3);
            }
        }
    }
    // elevation randomization
    for (int x = 0; x < params.width; ++x) {
        for (int y = 0; y < params.height; ++y) {
            GenCell* ref = out + y*params.width + x;
            if (ref->elevation > 0 && ref->elevation < 10) ref->elevation += rand()%5;
            else if (ref->elevation >= 10) ref->elevation -= rand()%5;
        }
    }
    // elevation normalization
    int e_max = 0;
    for (int x = 0; x < params.width; ++x) {
        for (int y = 0; y < params.height; ++y) {
            GenCell* ref = out + y*params.width + x;
            int e = ref->elevation;
            if (e > e_max) e_max = e;
        }
    }
    for (int x = 0; x < params.width; ++x) {
        for (int y = 0; y < params.height; ++y) {
            GenCell* ref = out + y*params.width + x;
            ref->elevation_norm = ((float)(ref->elevation) / e_max);
        }
    }
    // water cycle
    
    Direction dispersal_direction = direction_opposite(params.wind_direction);
    
    for (int i = 0; i < params.water_cycles; ++i) {
        for (int x = 0; x < params.width; ++x) {
            for (int y = 0; y < params.height; ++y) {
                GenCell* ref = out + (y)*params.width + (x);
                if (ref->elevation == 0) { // water cell
                    ref->moisture = 1.0;
                    ref->clouds += params.evaporation_factor;
                } else {
                    float evaporation = ref->moisture * params.evaporation_factor;
                    ref->moisture -= evaporation;
                    ref->clouds += evaporation;
                }
                float precipitation = ref->clouds * params.precipitation_factor;
                ref->moisture += precipitation;
                ref->clouds -= precipitation;

                float cloud_maximum = 1.0 - ref->elevation_norm;
                if (cloud_maximum < 0.2) cloud_maximum = 0.2;
                if (ref->clouds > cloud_maximum) {
                    ref->moisture += ref->clouds - cloud_maximum;
                    ref->clouds = cloud_maximum;
                }

                float cloud_dispersal = ref->clouds * (1.0 / (7.0+params.wind_strength));
                ref->clouds = 0;
                float runoff = ref->moisture * params.runoff_factor * (1.0 / 8.0);
                float seepage = ref->moisture * params.seepage_factor * (1.0 / 8.0);

                int neighbors_delta[9][2] = {
                    { 0,-1}, // D_N
                    { 1,-1}, // D_NE
                    { 1, 0}, // D_E
                    { 1, 1}, // D_SE
                    { 0, 1}, // D_S
                    {-1, 1}, // D_SW
                    {-1, 0}, // D_W
                    {-1,-1}, // D_NW
                };
                for (Direction d = 0; d < 8; ++d) {
                    int nx = x+neighbors_delta[d][0];
                    int ny = y+neighbors_delta[d][1];
                    if (!inbounds(nx, 0, params.width) || !inbounds(ny, 0, params.height)) {
                        continue;
                    }
                    
                    GenCell* neigh = out + (ny)*params.width + (nx);
                    neigh->clouds += cloud_dispersal * (d==dispersal_direction?params.wind_strength:1);
                    int elevation_delta = neigh->elevation - ref->elevation;
                    if (elevation_delta < 0) { // runoff
                        ref->moisture -= runoff;
                        neigh->moisture += runoff;
                    } else if (elevation_delta == 0) { // seepage
                        ref->moisture -= seepage;
                        neigh->moisture += seepage;
                    }
                }
                if (ref->moisture > 1.0) ref->moisture = 1.0;
            }
        }
    }
    // temperature
    for (int x = 0; x < params.width; ++x) {
        for (int y = 0; y < params.height; ++y) {
            GenCell* ref = out + (y)*params.width + (x);
            float latitude_factor = abs(y-(params.height/2)) / (float)(params.height/2);
            float elevation_mean = ref->elevation_norm / 2.0;
            float elevation_delta = ref->elevation_norm - elevation_mean;
            float temperature_jitter = ((frand()*2)-1)*0.05;
            float temperature = (-latitude_factor+1) - 0.9*(elevation_delta) + temperature_jitter;
            if (temperature < 0.0) temperature = 0.0;
            if (temperature > 1.0) temperature = 1.0;
            ref->temperature = temperature;
        }
    }
    // rivers
    int desired_river_area = total_land_area*params.desired_river_proportion;
    int river_area = 0;
    typedef struct {
        int x;
        int y;
        float fitness;
    } RiverCand;
    RiverCand* river_cands = malloc(area*sizeof(RiverCand));
    int river_cands_count = 0;
    for (int x = 0; x < params.width; ++x) {
        for (int y = 0; y < params.height; ++y) {
            GenCell* ref = out + (y)*params.width + (x);
            if (ref->elevation==0) continue; // water tile
            float fitness = (0.3 + ref->elevation_norm)*(0.2+ref->moisture);
            if (fitness < 0.25) fitness = 0.0;
            river_cands[river_cands_count++] = (RiverCand){x, y, fitness};
        }
    }

    //int rivers = 0;
    float total = 0;
    int card_neighbor_deltas[4][2] = {{1,0}, {0,1}, {-1,0}, {0,-1}}; // + shape
    for (int i = 0; i < river_cands_count; ++i) total += river_cands[i].fitness;
    while (river_area < desired_river_area) {
        float r = frand() * total;
        for (int i = 0; i < river_cands_count; ++i) {
            r -= (river_cands[i].fitness);
            if (r < 0) {
                // river source
                RiverCand pr = river_cands[i];
                int x = pr.x;
                int y = pr.y;
                int stop = 0;
                for (int d = 0; d < 4; ++d) { // dont start river next to preexisting
                    int nx = x + card_neighbor_deltas[d][0];
                    int ny = y + card_neighbor_deltas[d][1];
                    GenCell* neigh = out + ny*params.width + nx;
                    if (neigh->river) stop = 1;
                }
                if (stop) break;
                at(out, params, x, y).river_source = 1;
                at(out, params, x, y).river = 1;
                total -= pr.fitness;
                river_cands[i] = river_cands[river_cands_count-1];
                river_cands_count--;

                // river flow
                
                Direction dir_prev = rand()%4;
                while (1) {
                    GenCell* ref = out + y*params.width + x;
                    if (ref->elevation == 0) break; // water
                    if (ref->elevation > 1) ref->elevation-=1;
                    if (!ref->river) {
                        ref->river = 1;
                        river_area++;
                    }
                    int found_flag = 0;
                    Direction dir_jitter = (floor(frand()*1.2))*3;
                    for (int j = 0; j < 4; ++j) {
                        Direction d = (dir_prev+dir_jitter+j)%4;
                        int nx = x + card_neighbor_deltas[d][0];
                        int ny = y + card_neighbor_deltas[d][1];
                        GenCell* neigh = out + ny*params.width + nx;
                        if (neigh->elevation < ref->elevation) {
                            x = nx;
                            y = ny;
                            dir_prev = d;
                            found_flag = 1;
                            break;
                        }
                    }
                    if (!found_flag) {
                        if (frand() < 0.9) {
                            // go in the previous direction
                            Direction d = (dir_prev+dir_jitter)%4;
                            int nx = x + card_neighbor_deltas[d][0];
                            int ny = y + card_neighbor_deltas[d][1];
                            x = nx;
                            y = ny;
                            found_flag = 1;
                        } else {
                            ref->elevation = 0; // lake
                            for (Direction d = rand()%4+1; d < 4; ++d) {
                                int nx = x + card_neighbor_deltas[d][0];
                                int ny = y + card_neighbor_deltas[d][1];
                                GenCell* neigh = out + ny*params.width + nx;
                                neigh->elevation = 0;
                            }    
                            break;
                        }
                    }
                }
                break;
            }
        }
    }
    free(river_cands);
    // poles
    int ys[4] = {0, 1, params.height-1, params.height-2};
    for (int i = 0; i < 4; ++i) {
        int y = ys[i];
        for (int x = 0; x < params.width; ++x) {
            if (i%2==1) {
                if (frand() < 0.4) continue;
            }
            GenCell* ref = out + y*params.width + x;
            ref->elevation = 1;
            ref->elevation_norm = 1.0 / e_max;
            ref->clouds = 0.0;
            ref->moisture = 1.0;
            ref->temperature = 0.0;
        }
    }
    // DEBUG: make sure moisture and temperature are between 0 and 1.0
    float m_max = 0;
    float t_max = 0;
    for (int x = 0; x < params.width; ++x) {
        for (int y = 0; y < params.height; ++y) {
            GenCell* ref = out + y*params.width + x;
            float m = ref->moisture;
            float t = ref->temperature;
            if (m > m_max) m_max = m;
            if (t > t_max) t_max = t;
        }
    }
    printf("m_max: %f, t_max: %f\n", m_max, t_max);
    // determine final tile types
    for (int i = 0; i < params.width*params.height; ++i) {
        GenCell* ref = out + i;
        TileType type = T_NIL;
        if (ref->elevation == 0) type = T_OCEAN;
        else if (ref->river) type = T_RIVER;
        else {
            int elevation_level;
            int temperature_level;
            int moisture_level;
            if (ref->elevation_norm < params.elevation_cutoffs[0]) elevation_level = 0;
            else if (ref->elevation_norm < params.elevation_cutoffs[1]) elevation_level = 1;
            else elevation_level = 2;
            
            if (ref->temperature < params.temperature_cutoffs[0]) temperature_level = 0;
            else if (ref->temperature < params.temperature_cutoffs[1]) temperature_level = 1;
            else temperature_level = 2;
            
            if (ref->moisture < params.moisture_cutoffs[0]) moisture_level = 0;
            else if (ref->moisture < params.moisture_cutoffs[1]) moisture_level = 1;
            else moisture_level = 2;
            
            type = biome_matrix[elevation_level][temperature_level][moisture_level];
        }
        ref->final_tile_type = type;
    }
    
}

void to_tiles(GenCell* in, GenParameters params, Tile* tiles) {
    assert(params.width == TILEMAP_W && params.height == TILEMAP_H);
    for (int x = 0; x < TILEMAP_W; ++x) {
        for (int y = 0; y < TILEMAP_H; ++y) {
            GenCell* cell = in + y*params.width + x;
            tiles[y*TILEMAP_W + x].type = cell->final_tile_type;
        }
    }
}
