#include <assert.h>

#include "resources.h"

void load_textures(TextureManager* out) {

    int texture_i = 0;
    
    Image tiles_img = LoadImage("images/tiles.png");
        Texture2D tiles = LoadTextureFromImage(tiles_img);
        assert(texture_i < TEXTURE_COUNT);
        out->textures[texture_i++] = tiles;
    UnloadImage(tiles_img);
    
    for (TileType i = T_DESERT; i < T_COUNT; ++i) {
        for (Connections j = 0; j < CONNECTIONS_PERMUTATIONS; ++j) {
            out->tiles[i][j] = (TexturePortion){
                tiles,
                (Rectangle){TILE_WIDTH*j, TILE_HEIGHT*(i-T_DESERT) ,TILE_WIDTH,TILE_HEIGHT},
            };
        }
    }
    
    Image things_img = LoadImage("images/things.png");
        Texture2D things = LoadTextureFromImage(things_img);
        assert(texture_i < TEXTURE_COUNT);
        out->textures[texture_i++] = things;
    UnloadImage(things_img);

    int units_per_row = things.width / UNIT_WIDTH;
    for (UnitType i = U_SETTLER; i < U_COUNT; ++i) {
        out->units[i] = (TexturePortion){
            things,
            (Rectangle){
                0  +(i%units_per_row)*UNIT_WIDTH, 
                160+(i/units_per_row)*UNIT_HEIGHT, 
                UNIT_WIDTH, 
                UNIT_HEIGHT,
            },
        };
    }
}

void unload_textures(TextureManager* tm) {
    for (int i = 0; i < TEXTURE_COUNT; ++i) {
        UnloadTexture(tm->textures[i]);
    }
}
