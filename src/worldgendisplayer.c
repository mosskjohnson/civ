#include <stdio.h>
#include <time.h>
#include <assert.h>

#include "raylib.h"
// #define RAYGUI_IMPLEMENTATION
// #include "raygui.h"

#include "worldgen.h"
#include "data.h"
#include "resources.h"
#include "utils.h"

#define WINDOW_W 960
#define WINDOW_H 720

#define WORLD_W 80
#define WORLD_H 50

#define CANVAS_W (WORLD_W * TILE_W)
#define CANVAS_H (WORLD_H * TILE_H)

typedef enum {
    M_TILETYPE,
    M_ELEVATION,
    M_MOISTURE,
    M_TEMPERATURE,
    M_COUNT,
} Mode;
Mode mode = M_TILETYPE;
const char* const mode_names[M_COUNT] = {"TileType", "Elevation", "Moisture", "Temperature"};
const int mode_keys[M_COUNT] = {49, 50, 51, 52};
RenderTexture2D mode_canvases[M_COUNT];

Image encodeWorld(GenCell* world, MapSize size) {
    Image out = GenImageColor(size.width, size.height, BLANK);
    Color* pixels = (Color*)out.data;

    for (int y = 0; y < size.height; ++y) {
        for (int x = 0; x < size.width; ++x) {
            int i = y*size.width + x;
            GenCell* gc = &world[i];
            pixels[i] = (Color){
                (unsigned char)(CLAMP(gc->elevation_norm, 0, 1) * 255),
                (unsigned char)(CLAMP(gc->moisture, 0, 1) * 255),
                (unsigned char)(CLAMP(gc->temperature, 0, 1) * 255),
                (unsigned char)gc->final_tile_type,
            };
        }
    }

    return out;
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
    (void)_prog_name;
    char* seed_str = shift(&argc, &argv);

    unsigned int seed;
    if (seed_str != NULL) {
        seed = atoi(seed_str);
    } else {
        seed = time(NULL);
    }
    printf("seed: %d\n", seed);

    MapSize size = (MapSize){WORLD_W, WORLD_H};

    GenParameters p = default_gen_parameters_medium();
    p.seed = seed;
    p.size = size;

    GenCell* world = alloc_world(p);

    generate_world(world, p);

    Tile* tiles = alloc_tiles(size);

    to_tiles(world, tiles, size);

    InitWindow(WINDOW_W, WINDOW_H, "worldgen");

    SpritesheetTextures stextures;
    load_textures(&stextures);

    Image encodedImage = encodeWorld(world, size);
    Texture2D encodedTexture = LoadTextureFromImage(encodedImage);
    SetTextureFilter(encodedTexture, TEXTURE_FILTER_POINT);
    SetTextureWrap(encodedTexture, TEXTURE_WRAP_CLAMP);

    Shader gradient_shader = LoadShader(0, "resources/shaders/worldgendisplayer/gradient.fs");
    int gradient_shader_channel_loc = GetShaderLocation(gradient_shader, "channel");

    // init canvases
    for (int i = 0; i < M_COUNT; ++i) {
        mode_canvases[i] = LoadRenderTexture(CANVAS_W, CANVAS_H);
    }

    // draw on canvases. this is seperate from init because later this may be done again during the game loop.
    for (int i = 0; i < M_COUNT; ++i) {
        BeginTextureMode(mode_canvases[i]);
            if (i == M_TILETYPE) {
                for (int y = 0; y < size.height; y++) {
                    for (int x = 0; x < size.width; x++) {
                        Tile *t = &tiles[y * size.width + x];
                        draw_tile(tiles, size, t, x, y, &stextures);
                    }
                }
            } else {
                int channel = i - M_ELEVATION; // M_ELEVATION=0, M_MOISTURE=1, M_TEMPERATURE=2
                SetShaderValue(gradient_shader, gradient_shader_channel_loc, &channel, SHADER_UNIFORM_INT);

                BeginShaderMode(gradient_shader);
                    DrawTexturePro(encodedTexture,
                        (Rectangle){0, 0, size.width, size.height},
                        (Rectangle){0, 0, CANVAS_W, CANVAS_H},
                        (Vector2){0, 0}, 0.0f, WHITE);
                EndShaderMode();
            }
        EndTextureMode();
    }
    
    SetTargetFPS(30);
    while (!WindowShouldClose()) {

        for (int i = 0; i < M_COUNT; ++i) {
            if (IsKeyPressed(mode_keys[i])) mode = i;
        }

        BeginDrawing();
            ClearBackground(WHITE);
            DrawTexturePro(
                mode_canvases[mode].texture,
                (Rectangle){0, 0, CANVAS_W, -CANVAS_H},
                (Rectangle){0, 0, WINDOW_W, WINDOW_H},
                (Vector2){0, 0}, 0.0f, WHITE
            );
            draw_text(GetFontDefault(), 20, 4, 0, 0, FA_START, FA_START, RED, "Displaying: %s", mode_names[mode]);
            draw_text(GetFontDefault(), 20, 4, WINDOW_W, 0, FA_END, FA_START, RED, "Legend: Black=Low White=High");
        EndDrawing();

    }

    free_world(world);
    free_tiles(tiles);
    unload_textures(&stextures);
    CloseWindow();

}
