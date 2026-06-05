#ifndef RESOURCES_H
#define RESOURCES_H

#include "raylib.h"
#include "data.h"

#define TEXTURE_COUNT 1

#define TILE_W 16
#define TILE_H 16
#define UNIT_W 16
#define UNIT_H 16
#define OCEAN_CORNER_W 8
#define OCEAN_CORNER_H 8

typedef struct {
    Texture2D texture;
    Rectangle portion;
} TexturePortion;

typedef struct {
    Texture2D textures[TEXTURE_COUNT];
    TexturePortion tiles[T_COUNT][CONNECTIONS_PERMUTATIONS];
    TexturePortion units[U_COUNT];
    TexturePortion ocean_corners[4][8];
    TexturePortion rivermouths[4];
} SpritesheetTextures;

void load_textures(SpritesheetTextures* out);
void unload_textures(SpritesheetTextures* in);

int get_corner_index(int corner, int direction8_on_bools[8]);

void draw_tile(Tile* tiles, MapSize size, Tile* t, int x, int y, SpritesheetTextures* stextures);
void draw_entity(Entity* e, SpritesheetTextures* stextures);
void draw_fog(Fog* f, int x, int y);

// behavior 0: start. behavior 1: middle. behavior 2: end.
void draw_text(Font font, float fontsize, float spacing, int x, int y, int x_behavior, int y_behavior, Color color, const char* format, ...) __attribute__((format(printf, 9, 10)));

#endif
