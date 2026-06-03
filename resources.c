#include <assert.h>

#include "resources.h"

static const Rectangle tiles_portion = {0, 0, 256, 208};
static const Rectangle units_portion = {256, 160, 320, 32};

void load_textures(SpritesheetTextures* out) {
    int texture_i = 0;
    
    Image tiles_img = LoadImage("resources/images/spritesheet.png");
        Texture2D spritesheet = LoadTextureFromImage(tiles_img);
        assert(texture_i < TEXTURE_COUNT);
        out->textures[texture_i++] = spritesheet;
    UnloadImage(tiles_img);
    
    for (TileType i = T_DESERT; i < T_COUNT; ++i) {
        for (Connections j = 0; j < CONNECTIONS_PERMUTATIONS; ++j) {
            out->tiles[i][j] = (TexturePortion){
                spritesheet,
                (Rectangle){tiles_portion.x + TILE_W*j, tiles_portion.y + TILE_H*(i-T_DESERT), TILE_W, TILE_H},
            };
        }
    }
    int units_per_row = units_portion.width / UNIT_W;
    for (UnitType i = U_SETTLER; i < U_COUNT; ++i) {
        out->units[i] = (TexturePortion){
            spritesheet,
            (Rectangle){
                units_portion.x + (i%units_per_row)*UNIT_W, 
                units_portion.y + (i/units_per_row)*UNIT_H, 
                UNIT_W, 
                UNIT_H,
            },
        };
    }
}

void unload_textures(SpritesheetTextures* in) {
    for (int i = 0; i < TEXTURE_COUNT; ++i) {
        UnloadTexture(in->textures[i]);
    }
}
