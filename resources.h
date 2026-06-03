#ifndef RESOURCES_H
#define RESOURCES_H

#include "raylib.h"
#include "data.h"

#define TEXTURE_COUNT 1

#define TILE_W 16
#define TILE_H 16
#define UNIT_W 16
#define UNIT_H 16

typedef struct {
    Texture2D texture;
    Rectangle portion;
} TexturePortion;

typedef struct {
    Texture2D textures[TEXTURE_COUNT];
    TexturePortion tiles[T_COUNT][CONNECTIONS_PERMUTATIONS];
    TexturePortion units[U_COUNT];
} SpritesheetTextures;

void load_textures(SpritesheetTextures* out);
void unload_textures(SpritesheetTextures* in);

#endif
