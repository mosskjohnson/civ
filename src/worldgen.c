#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include "worldgen.h"
#include "utils.h"

#define at(world, p, x, y) world[(y)*p.size.width + (x)]
#define inrange(a, bound_begin, bound_end) (a >= bound_begin && a < bound_end)

TileType biome_matrix[ELEVATION_LEVELS][TEMPERATURE_LEVELS][MOISTURE_LEVELS] = {
    { // elevation 0
        {T_ARCTIC,T_ARCTIC,T_ARCTIC}, // temperature 0
        {T_PLAINS,T_GRASSLAND,T_SWAMP}, // temperature 1
        {T_DESERT,T_GRASSLAND,T_JUNGLE}, // temperature 2
    },
    { // elevation 1
        {T_TUNDRA,T_TUNDRA,T_TUNDRA}, // temperature 0
        {T_HILLS,T_FOREST,T_FOREST}, // temperature 1
        {T_HILLS,T_FOREST,T_JUNGLE}, // temperature 2
    },
    { // elevation 2
        {T_MOUNTAIN,T_MOUNTAIN,T_MOUNTAIN}, // temperature 0
        {T_MOUNTAIN,T_MOUNTAIN,T_MOUNTAIN}, // temperature 1
        {T_MOUNTAIN,T_MOUNTAIN,T_MOUNTAIN}, // temperature 2
    },
};

GenParameters default_gen_parameters_medium(void) {
    return (GenParameters){
        .size = {80, 50},
        .seed = 0, // filled in later
        .margin_x = 3,
        .margin_y = 5,
        .desired_land_proportion = 0.4,
        .fragmentation = 0.9,
        .evaporation_factor = 0.5,
        .precipitation_factor = 0.25,
        .runoff_factor = 0.125,
        .seepage_factor = 0.125,
        .wind_direction = D_SW8,
        .wind_strength = 3.0,
        .water_cycles = 40,
        .desired_river_proportion = 0.06,
        .elevation_cutoffs = {0.20, 0.5},
        .temperature_cutoffs = {0.3, 0.7},
        .moisture_cutoffs = {0.35, 0.7},
        .randomness = 0.2,
        .resource_frequency = 0.035,
    };
}

GenParameters default_gen_parameters_large(void) {
    return (GenParameters){
        .size = {120, 90},
        .seed = 0, // filled in later
        .margin_x = 4,
        .margin_y = 8,
        .desired_land_proportion = 0.45,
        .fragmentation = 0.8,
        .evaporation_factor = 0.5,
        .precipitation_factor = 0.25,
        .runoff_factor = 0.125,
        .seepage_factor = 0.125,
        .wind_direction = D_SW8,
        .wind_strength = 3.0,
        .water_cycles = 40,
        .desired_river_proportion = 0.08,
        .elevation_cutoffs = {0.18, 0.45},
        .temperature_cutoffs = {0.3, 0.7},
        .moisture_cutoffs = {0.3, 0.6},
        .randomness = 0.2,
        .resource_frequency = 0.035,
    };
}

GenCell* alloc_world(GenParameters p) {
    return calloc(p.size.width*p.size.height, sizeof(GenCell));
}

void free_world(GenCell* world) {
    free(world);
}

// returns the total land area generated
int simulate_elevation(GenCell* out, GenParameters p) {

    int area = p.size.width*p.size.height;
    int iter_range = area * (-p.fragmentation+1);
    
    int bound_x_left   = p.margin_x;
    int bound_x_right  = p.size.width - p.margin_x;
    int bound_y_top    = p.margin_y;
    int bound_y_bottom = p.size.height - p.margin_y;

    int total_land_area = 0;

    #define REGION_COUNT 2
    #define CHANNEL_W 4
    int region_bounds[REGION_COUNT][4] = {
        {bound_x_left,bound_x_right/2,bound_y_top,bound_y_bottom}, 
        {bound_x_right/2 + CHANNEL_W, bound_x_right, bound_y_top, bound_y_bottom},
    };
    for (int r = 0; r < REGION_COUNT; ++r) {
        int r_bound_x_left = region_bounds[r][0];
        int r_bound_x_right = region_bounds[r][1];
        int r_bound_y_top = region_bounds[r][2];
        int r_bound_y_bottom = region_bounds[r][3];
       
        int desired_land_area = (int)(area * p.desired_land_proportion / REGION_COUNT);
        int region_land_area = 0;
        #define DELTAS_COUNT 5
        while (region_land_area < desired_land_area) {
            int iterations = 1 + rand()%iter_range;
            int x = r_bound_x_left + rand()%(r_bound_x_right-r_bound_x_left);
            int y = r_bound_y_top + rand()%(r_bound_y_bottom-r_bound_y_top);
            
            int deltas[DELTAS_COUNT][2] = {{0,0}, {1,0}, {0,1}, {-1,0}, {0,-1}}; // + shape
            // int deltas[DELTAS_COUNT][2] = {{0,0}, {1,0}, {0,1}}; // L shape
            for (int i = 0; i < iterations; ++i) {
                if (!inrange(x, r_bound_x_left+1, r_bound_x_right) || !inrange(y, r_bound_y_top+1, r_bound_y_bottom)) {
                    break;
                }
                for (int j = 0; j < DELTAS_COUNT; ++j) {
                    GenCell* ref = out + (y+deltas[j][1])*p.size.width + (x+deltas[j][0]);
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
    for (int x = 0; x < p.size.width; ++x) {
        for (int y = 0; y < p.size.height; ++y) {
            GenCell* ref = out + y*p.size.width + x;
            if (ref->elevation > 0 && ref->elevation < 10) ref->elevation += rand()%5;
            else if (ref->elevation >= 10) ref->elevation -= rand()%5;
        }
    }
    // poles (hard set at elevation 1)
    int ys[4] = {0, 1, p.size.height-1, p.size.height-2};
    for (int i = 0; i < 4; ++i) {
        int y = ys[i];
        for (int x = 0; x < p.size.width; ++x) {
            if (i%2==1) {
                if (FRAND() < 0.4) continue;
            }
            GenCell* ref = out + y*p.size.width + x;
            ref->elevation = 1;
        }
    }
    // elevation normalization
    int e_max = 0;
    for (int x = 0; x < p.size.width; ++x) {
        for (int y = 0; y < p.size.height; ++y) {
            GenCell* ref = out + y*p.size.width + x;
            int e = ref->elevation;
            if (e > e_max) e_max = e;
        }
    }
    for (int x = 0; x < p.size.width; ++x) {
        for (int y = 0; y < p.size.height; ++y) {
            GenCell* ref = out + y*p.size.width + x;
            ref->elevation_norm = ((float)(ref->elevation) / e_max);
        }
    }

    return total_land_area;
}

void simulate_moisture(GenCell* out, GenParameters p) {
    Direction8 dispersal_direction = direction8_opposite(p.wind_direction);
    
    for (int i = 0; i < p.water_cycles; ++i) {
        for (int x = 0; x < p.size.width; ++x) {
            for (int y = 0; y < p.size.height; ++y) {
                GenCell* ref = out + (y)*p.size.width + (x);
                if (ref->elevation == 0) { // water cell
                    ref->moisture = 1.0;
                    ref->clouds += p.evaporation_factor;
                } else {
                    float evaporation = ref->moisture * p.evaporation_factor;
                    ref->moisture -= evaporation;
                    ref->clouds += evaporation;
                }
                float precipitation = ref->clouds * p.precipitation_factor;
                ref->moisture += precipitation;
                ref->clouds -= precipitation;

                float cloud_maximum = 1.0 - ref->elevation_norm;
                if (cloud_maximum < 0.2) cloud_maximum = 0.2;
                if (ref->clouds > cloud_maximum) {
                    ref->moisture += ref->clouds - cloud_maximum;
                    ref->clouds = cloud_maximum;
                }

                float cloud_dispersal = ref->clouds * (1.0 / (7.0+p.wind_strength));
                ref->clouds = 0;
                float runoff = ref->moisture * p.runoff_factor * (1.0 / 8.0);
                float seepage = ref->moisture * p.seepage_factor * (1.0 / 8.0);

                for (Direction8 d = 0; d < 8; ++d) {
                    int nx = x+DELTAS_8[d][0];
                    int ny = y+DELTAS_8[d][1];
                    if (!inrange(nx, 0, p.size.width) || !inrange(ny, 0, p.size.height)) {
                        continue;
                    }
                    
                    GenCell* neigh = out + (ny)*p.size.width + (nx);
                    neigh->clouds += cloud_dispersal * (d==dispersal_direction?p.wind_strength:1);
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
}

void simulate_temperature(GenCell* out, GenParameters p) {
    for (int x = 0; x < p.size.width; ++x) {
        for (int y = 0; y < p.size.height; ++y) {
            GenCell* ref = out + (y)*p.size.width + (x);
            float latitude_factor = abs(y-(p.size.height/2)) / (float)(p.size.height/2);
            float elevation_mean = ref->elevation_norm / 2.0;
            float elevation_delta = ref->elevation_norm - elevation_mean;
            float temperature_jitter = ((FRAND()*2)-1)*0.05;
            float temperature = (-latitude_factor+1) - 0.9*(elevation_delta) + temperature_jitter;
            if (temperature < 0.0) temperature = 0.0;
            if (temperature > 1.0) temperature = 1.0;
            ref->temperature = temperature;
        }
    }
}

void simulate_rivers(GenCell* out, GenParameters p, int total_land_area) {
    int desired_river_area = total_land_area*p.desired_river_proportion;
    int river_area = 0;
    typedef struct {
        int x;
        int y;
        float fitness;
    } RiverCand;
    RiverCand* river_cands = malloc(p.size.width*p.size.height*sizeof(RiverCand));
    int river_cands_count = 0;
    for (int x = 0; x < p.size.width; ++x) {
        for (int y = 0; y < p.size.height; ++y) {
            GenCell* ref = out + (y)*p.size.width + (x);
            if (ref->elevation==0) continue; // water tile
            float fitness = (0.3 + ref->elevation_norm)*(0.2+ref->moisture);
            if (fitness < 0.25) fitness = 0.0;
            river_cands[river_cands_count++] = (RiverCand){x, y, fitness};
        }
    }

    float total = 0; // weighted random selection
    for (int i = 0; i < river_cands_count; ++i) total += river_cands[i].fitness;
    while (river_area < desired_river_area) {
        float r = FRAND() * total;
        for (int i = 0; i < river_cands_count; ++i) {
            r -= (river_cands[i].fitness);
            if (r < 0) {
                // river source
                RiverCand pr = river_cands[i];
                int x = pr.x;
                int y = pr.y;
                
                int stop = 0;
                for (int d = 0; d < 4; ++d) { // dont start river next to preexisting
                    int nx = x + DELTAS_4[d][0];
                    int ny = y + DELTAS_4[d][1];
                    GenCell* neigh = out + ny*p.size.width + nx;
                    if (neigh->river) stop = 1;
                }
                if (stop) break;
                
                at(out, p, x, y).river_source = 1;
                at(out, p, x, y).river = 1;
                total -= pr.fitness;
                river_cands[i] = river_cands[--river_cands_count];
                // river flow
                Direction4 dir_prev = rand()%4;
                while (1) {
                    GenCell* ref = out + y*p.size.width + x;
                    if (ref->elevation == 0) break; // water
                    if (ref->elevation > 1) ref->elevation-=1;
                    if (!ref->river) {
                        ref->river = 1;
                        river_area++;
                    }
                    int found_flag = 0;
                    Direction4 dir_jitter = (floor(FRAND()*1.2))*3;
                    for (int j = 0; j < 4; ++j) {
                        Direction4 d = (dir_prev+dir_jitter+j)%4;
                        int nx = x + DELTAS_4[d][0];
                        int ny = y + DELTAS_4[d][1];
                        GenCell* neigh = out + ny*p.size.width + nx;
                        if (neigh->elevation < ref->elevation) {
                            x = nx;
                            y = ny;
                            dir_prev = d;
                            found_flag = 1;
                            break;
                        }
                    }
                    if (!found_flag) {
                        if (FRAND() < 0.9) {
                            // go in the previous direction
                            Direction4 d = (dir_prev+dir_jitter)%4;
                            int nx = x + DELTAS_4[d][0];
                            int ny = y + DELTAS_4[d][1];
                            x = nx;
                            y = ny;
                            found_flag = 1;
                        } else {
                            ref->elevation = 0; // lake
                            for (Direction4 d = rand()%4+1; d < 4; ++d) {
                                int nx = x + DELTAS_4[d][0];
                                int ny = y + DELTAS_4[d][1];
                                GenCell* neigh = out + ny*p.size.width + nx;
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
}

void simulate_tiletypes(GenCell* out, GenParameters p) {
    for (int i = 0; i < p.size.width*p.size.height; ++i) {
        GenCell* ref = out + i;
        TileType type = T_NIL;
        if (ref->elevation == 0) type = T_OCEAN;
        else if (ref->river) type = T_RIVER;
        else {
            int elevation_level;
            int temperature_level;
            int moisture_level;
            if (ref->elevation_norm < p.elevation_cutoffs[0]) elevation_level = 0;
            else if (ref->elevation_norm < p.elevation_cutoffs[1]) elevation_level = 1;
            else elevation_level = 2;
            
            if (ref->temperature < p.temperature_cutoffs[0]) temperature_level = 0;
            else if (ref->temperature < p.temperature_cutoffs[1]) temperature_level = 1;
            else temperature_level = 2;
            
            if (ref->moisture < p.moisture_cutoffs[0]) moisture_level = 0;
            else if (ref->moisture < p.moisture_cutoffs[1]) moisture_level = 1;
            else moisture_level = 2;
            
            type = biome_matrix[elevation_level][temperature_level][moisture_level];
        }
        // randomization
        if (FRAND() < p.randomness) {
            switch (type) {
                case T_NIL: break;
                case T_DESERT: type = T_PLAINS; break;
                case T_PLAINS: type = T_GRASSLAND; break;
                case T_GRASSLAND: type = T_HILLS; break;
                case T_FOREST: type = T_HILLS; break;
                case T_HILLS: type = T_FOREST; break;
                case T_MOUNTAIN: type = T_HILLS; break;
                case T_TUNDRA: type = T_GRASSLAND; break;
                default: break;
            }
        }
        if (FRAND() < p.randomness) {
            switch (type) {
                case T_NIL: break;
                case T_GRASSLAND: type = T_PLAINS; break;
                case T_HILLS: type = T_MOUNTAIN; break;
                default: break;
            }
        }
        if (FRAND() < p.randomness/1.5) {
            switch (type) {
                case T_NIL: break;
                case T_PLAINS: type = T_DESERT; break;
                case T_FOREST: type = T_GRASSLAND; break;
                case T_HILLS: type = T_GRASSLAND; break;
                default: break;
            }
        }
        ref->final_tile_type = type;
    }
}

void simulate_natural_resources(GenCell* out, GenParameters p) {
    for (int i = 0; i < p.size.width*p.size.height; ++i) {
        float freq = p.resource_frequency;
        if (out[i].final_tile_type == T_GRASSLAND) freq *= 2.0;
        if (out[i].final_tile_type == T_OCEAN) freq /= 2.0;
        
        if (FRAND() < freq) out[i].natural_resource = 1;
    }
}

void generate_world(GenCell* out, GenParameters p) {
    srand(p.seed);

    int total_land_area = simulate_elevation(out, p);

    simulate_moisture(out, p);

    simulate_temperature(out, p);

    simulate_rivers(out, p, total_land_area);

    simulate_tiletypes(out, p);

    simulate_natural_resources(out, p);
}

void to_tiles(GenCell* in, Tile* tiles_out, MapSize size) {
    for (int x = 0; x < size.width; ++x) {
        for (int y = 0; y < size.height; ++y) {
            GenCell* cell = in + y*size.width + x;
            Tile* t = tile_at(tiles_out, size, x, y);
            t->type = cell->final_tile_type;
            t->natural_resource = cell->natural_resource;
        }
    }
}
