#ifndef RESOURCES_H
#define RESOURCES_H

#include "raylib.h"
#include "data.h"

#define TEXTURE_COUNT 2
#define TILE_WIDTH 16
#define TILE_HEIGHT 16
#define UNIT_WIDTH 16
#define UNIT_HEIGHT 16

typedef struct {
    Texture2D texture;
    Rectangle portion;
} TexturePortion;

typedef struct {
    Texture2D textures[TEXTURE_COUNT];
    TexturePortion tiles[T_COUNT][CONNECTIONS_PERMUTATIONS];
    TexturePortion units[U_COUNT];
} TextureManager;

void load_textures(TextureManager* out);

void unload_textures(TextureManager* tm);

#endif
