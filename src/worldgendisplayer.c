#include <stdio.h>
#include <time.h>
#include <assert.h>

#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include "worldgen.h"
#include "data.h"
#include "resources.h"

#define WINDOW_W 960
#define WINDOW_H 720

#define WORLD_W 80
#define WORLD_H 50

#define CANVAS_W (WORLD_W * TILE_W)
#define CANVAS_H (WORLD_H * TILE_H)

enum Mode {
    TILETYPE,
    ELEVATION,
    MOISTURE,
    TEMPERATURE,
    RIVERS,
    COUNT,
};
enum Mode mode = TILETYPE;
const char* const mode_names[COUNT] = {"TileType", "Elevation", "Moisture", "Temperature", "Rivers"};
const int mode_keys[COUNT] = {49, 50, 51, 52, 53};
RenderTexture2D mode_canvases[COUNT];

void draw_tiletype(GenCell* world, MapSize size, SpritesheetTextures* stextures) {
    Tile* tiles = alloc_tiles(size);
    to_tiles(world, tiles, size);
    
    for (int x = 0; x < size.width; ++x) {
        for (int y = 0; y < size.height; ++y) {
            draw_tile(tiles, size, tile_at(tiles, size, x, y), x, y, stextures);
        }
    }

    free_tiles(tiles);
}

void draw_elevation(GenCell* world, MapSize size, SpritesheetTextures* stextures) {
    for (int x = 0; x < size.width; ++x) {
        for (int y = 0; y < size.height; ++y) {
            GenCell* ref = world + WORLD_W*y + x;

            int e = ref->elevation;
            float e_norm = ref->elevation_norm;
            Color c = ((e==0) ? DARKBLUE : ColorFromNormalized((Vector4){e_norm, e_norm, e_norm, 1.0}));

            DrawRectangle(x*TILE_W, y*TILE_H, TILE_W, TILE_H, c);
        }
    }
}

void draw_moisture(GenCell* world, MapSize size, SpritesheetTextures* stextures) {
    for (int x = 0; x < size.width; ++x) {
        for (int y = 0; y < size.height; ++y) {
            GenCell* ref = world + WORLD_W*y + x;

            float m = ref->moisture;
            Color c = ColorFromNormalized((Vector4){m, m, m, 1.0});

            DrawRectangle(x*TILE_W, y*TILE_H, TILE_W, TILE_H, c);
        }
    }
}

void draw_temperature(GenCell* world, MapSize size, SpritesheetTextures* stextures) {
    for (int x = 0; x < size.width; ++x) {
        for (int y = 0; y < size.height; ++y) {
            GenCell* ref = world + WORLD_W*y + x;

            float t = ref->temperature;
            int e = ref->elevation;
            Color c = ((e==0) ? BLACK : ColorFromNormalized((Vector4){t, 0.0, (-t+1), 1.0}));

            DrawRectangle(x*TILE_W, y*TILE_H, TILE_W, TILE_H, c);
        }
    }
}

void draw_rivers(GenCell* world, MapSize size, SpritesheetTextures* stextures) {
    for (int x = 0; x < size.width; ++x) {
        for (int y = 0; y < size.height; ++y) {
            GenCell* ref = world + WORLD_W*y + x;

            Color c;
            if (ref->elevation==0) c = BLACK;
            else if (ref->river_source) c = BLUE;
            else if (ref->river) c = GREEN;
            else c = WHITE;

            DrawRectangle(x*TILE_W, y*TILE_H, TILE_W, TILE_H, c);
        }
    }
}

void (*mode_draw_functions[COUNT])(GenCell*, MapSize, SpritesheetTextures*) = {draw_tiletype, draw_elevation, draw_moisture, draw_temperature, draw_rivers};

void draw_all_modes(GenCell* world, MapSize size, SpritesheetTextures* stextures) {
    for (int i = 0; i < COUNT; ++i) {
        BeginTextureMode(mode_canvases[i]);
        mode_draw_functions[i](world, size, stextures);
        EndTextureMode();
    }
}

char* shift(int* argc, char*** argv) {
    if (*argc > 0) {
        (*argc)--;
        return *(*argv)++;
    }
    return NULL;
}

int main(int argc, char** argv) {
    
    char* _prog_name = shift(&argc, &argv);
    char* seed_str = shift(&argc, &argv);

    unsigned int seed;
    if (seed_str != NULL) {
        seed = atoi(seed_str);
    } else {
        seed = time(NULL);
    }
    printf("seed: %d\n", seed);

    GenParameters p = default_gen_parameters_medium();
    p.size = (MapSize){WORLD_W, WORLD_H};
    p.seed = seed;

    GenCell* world = alloc_world(p);

    generate_world(world, p);

    InitWindow(WINDOW_W, WINDOW_H, "worldgen");

    SpritesheetTextures stextures;
    load_textures(&stextures);

    for (int i = 0; i < COUNT; ++i) {
        mode_canvases[i] = LoadRenderTexture(CANVAS_W, CANVAS_H);
    }

    draw_all_modes(world, p.size, &stextures);
    
    SetTargetFPS(30);
    while (!WindowShouldClose()) {

        for (int i = 0; i < COUNT; ++i) {
            if (IsKeyPressed(mode_keys[i])) mode = i;
        }

        RenderTexture2D canvas = mode_canvases[mode];
    
        BeginDrawing();
            ClearBackground(WHITE);
            DrawTexturePro(
                canvas.texture,
                (Rectangle){0, 0, CANVAS_W, -CANVAS_H},
                (Rectangle){0, 0, WINDOW_W, WINDOW_H},
                (Vector2){0, 0}, 0.0f, WHITE
            );
            DrawText(mode_names[mode], 10, 10, 20, RED);
        EndDrawing();
        
    }

    CloseWindow();

    free_world(world);

    unload_textures(&stextures);
}
