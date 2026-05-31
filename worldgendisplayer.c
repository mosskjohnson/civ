#include <stdio.h>
#include <time.h>
#include <assert.h>
#include "raylib.h"
#include "worldgen.h"
#include "data.h"

#define WINDOW_W 960
#define WINDOW_H 720

#define WORLD_W 120
#define WORLD_H 90

enum Mode {
    TILETYPE,
    ELEVATION,
    MOISTURE,
    TEMPERATURE,
    RIVERS,
    COUNT,
};
enum Mode mode = TILETYPE;
const char* const strs[COUNT] = {"TileType","Elevation", "Moisture", "Temperature", "Rivers"};
const int keys[COUNT] = {49, 50, 51, 52, 53};

Color tiletype(GenCell* ref) {
    Color c;
    switch (ref->final_tile_type) {
        case T_DESERT:
            c = YELLOW;
            break;
        case T_PLAINS:
            c = ORANGE;
            break;
        case T_GRASSLAND:
            c = GREEN;
            break;
        case T_FOREST:
            c = LIME;
            break;
        case T_HILLS:
            c = DARKBROWN;
            break;
        case T_MOUNTAIN:
            c = BLACK;
            break;
        case T_TUNDRA:
            c = SKYBLUE;
            break;
        case T_ARCTIC:
            c = WHITE;
            break;
        case T_SWAMP:
            c = LIGHTGRAY;
            break;
        case T_JUNGLE:
            c = DARKGREEN;
            break;
        case T_OCEAN:
            c = DARKBLUE;
            break;
        case T_RIVER:
            c = BLUE;
            break;
        default:
            c = RED;
            break;
    }
    return c;
}

Color elevation_gradient(GenCell* ref) {
    int e = ref->elevation;
    float e_norm = ref->elevation_norm;
    return ((e==0) ? DARKBLUE : ColorFromNormalized((Vector4){e_norm, e_norm, e_norm, 1.0}));
}

Color moisture_gradient(GenCell* ref) {
    float m = ref->moisture;
    return ColorFromNormalized((Vector4){m, m, m, 1.0});
}

Color temperature_gradient(GenCell* ref) {
    float t = ref->temperature;
    int e = ref->elevation;
    return ((e==0) ? BLACK : ColorFromNormalized((Vector4){t, 0.0, (-t+1), 1.0}));
}

Color rivers(GenCell* ref) {
    Color c;
    if (ref->elevation==0) c = BLACK;
    else if (ref->river_source) c = BLUE;
    else if (ref->river) c = GREEN;
    return c;
}

Color (*shader_fps[COUNT])(GenCell*) = {tiletype, elevation_gradient, moisture_gradient, temperature_gradient, rivers};

char* shift(int* argc, char*** argv) {
    if (*argc > 0) {
        (*argc)--;
        return *(*argv)++;
    }
    return NULL;
}

int main(int argc, char** argv) {

    unsigned int seed;
    
    char* _prog_name = shift(&argc, &argv);
    char* seed_str = shift(&argc, &argv);
    if (seed_str != NULL) {
        seed = atoi(seed_str);
    } else {
        seed = time(NULL);
    }

    printf("seed: %d\n", seed);

    GenParameters p = {
        .width = WORLD_W,
        .height = WORLD_H,
        .seed = seed,
        .desired_land_proportion = 0.45,
        .fragmentation = 0.8,
        .evaporation_factor = 0.5,
        .precipitation_factor = 0.25,
        .runoff_factor = 0.125,
        .seepage_factor = 0.125,
        .wind_direction = D_SW,
        .wind_strength = 3.0,
        .water_cycles = 40,
        .desired_river_proportion = 0.08,
        .elevation_cutoffs = {0.18, 0.45},
        .temperature_cutoffs = {0.3, 0.7},
        .moisture_cutoffs = {0.35, 0.6},
    };

    GenCell* world = allocate_world(p);

    generate_world(world, p);

    InitWindow(WINDOW_W, WINDOW_H, "worldgen");

    int CELL_W = WINDOW_W / WORLD_W;
    int CELL_H = WINDOW_H / WORLD_H;
    
    SetTargetFPS(30);
    while (!WindowShouldClose()) {

        for (int i = 0; i < COUNT; ++i) {
            if (IsKeyPressed(keys[i])) mode = i;
        }
    
        BeginDrawing();
            ClearBackground(WHITE);
            for (int x = 0; x < p.width; ++x) {
                for (int y = 0; y < p.height; ++y) {
                    GenCell* ref = world + y*p.width + x; 
                                        
                    Color c = shader_fps[mode](ref);
                    
                    DrawRectangle(x*CELL_W, y*CELL_H, CELL_W, CELL_H, c);    
                }
            }
            DrawText(strs[mode], 10, 10, 20, RED);
        EndDrawing();
        
    }

    CloseWindow();

    free_world(world);
}
