#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>

#include "resources.h"

static const Rectangle tiles_portion = {0, 0, 256, 256};
static const Rectangle units_portion = {256, 0, 256, 64};
static const Rectangle ocean_corners_portion = {256, 64, 64, 32};
static const Rectangle rivermouths_portion = {256, 96, 64, 16};

void load_textures(SpritesheetTextures* out) {
    int texture_i = 0;
    
    Image tiles_img = LoadImage("resources/images/spritesheet.png");
        Texture2D spritesheet = LoadTextureFromImage(tiles_img);
        assert(texture_i < TEXTURE_COUNT);
        out->textures[texture_i++] = spritesheet;
    UnloadImage(tiles_img);
    
    for (TileType i = T_DESERT; i < T_COUNT; ++i) {
        for (uint8_t j = 0; j < CONNECTIONS_PERMUTATIONS; ++j) {
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
    for (int i = 0; i < 4; ++i) {// 4 corners
        for (int j = 0; j < 8; ++j) {
            out->ocean_corners[i][j] = (TexturePortion){
                spritesheet,
                (Rectangle){ocean_corners_portion.x + OCEAN_CORNER_W*j, ocean_corners_portion.y+OCEAN_CORNER_H*i, OCEAN_CORNER_W, OCEAN_CORNER_H},
            };
        }
    }
    for (int i = 0; i < 4; ++i) {
        out->rivermouths[i] = (TexturePortion){
            spritesheet,
            (Rectangle){rivermouths_portion.x + TILE_W*i, rivermouths_portion.y, TILE_W, TILE_H},
        };
    }
}

void unload_textures(SpritesheetTextures* in) {
    for (int i = 0; i < TEXTURE_COUNT; ++i) {
        UnloadTexture(in->textures[i]);
    }
}

int get_corner_index(int corner, int direction8_on_bools[8]) {
    int i = 0;
    i |= (direction8_on_bools[(2*corner+0)%8] << 0);
    i |= (direction8_on_bools[(2*corner+1)%8] << 1);
    i |= (direction8_on_bools[(2*corner+2)%8] << 2);
    return i;
}

void draw_tile(Tile* tiles, MapSize size, Tile* t, int x, int y, SpritesheetTextures* stextures) {
    uint8_t connections = 0;
    TileID neigh[4];
    neighbor_ids_4(size, x, y, neigh);
    switch (t->type) {
        case T_NIL: break;
        case T_OCEAN:
            for (Direction4 d = 0; d < 4; ++d) {
                if (tiles[neigh[d]].type != T_OCEAN) {
                    connections |= (1 << d);
                }
            }
            break;
        case T_RIVER:
            for (Direction4 d = 0; d < 4; ++d) {
                if (tiles[neigh[d]].type == T_RIVER || tiles[neigh[d]].type == T_OCEAN) {
                    connections |= (1 << d);
                }
            }
            break;
        default:
            for (Direction4 d = 0; d < 4; ++d) {
                if (tiles[neigh[d]].type == t->type) {
                    connections |= (1 << d);
                }
            }
    }
    TexturePortion tp = stextures->tiles[t->type][connections];
    DrawTexturePro(tp.texture, tp.portion, (Rectangle){x*TILE_W, y*TILE_H, TILE_W, TILE_H}, (Vector2){0.0, 0.0}, 0.0f, WHITE);
    if (t->type == T_OCEAN) {
        TileID neigh8[8];
        neighbor_ids_8(size, x, y, neigh8);
        int neigh8_on_bool[8] = {0};
        for (Direction8 d = 0; d < 8; ++d) {
            if (tiles[neigh8[d]].type != T_OCEAN) {
                neigh8_on_bool[d] = 1;
            }
        }
        for (int c = 0; c < 4; ++c) {
            int j = get_corner_index(c, neigh8_on_bool);
            TexturePortion corner_tp = stextures->ocean_corners[c][j];
            int corner_x = x*TILE_W + OCEAN_CORNER_W - (c/2)*OCEAN_CORNER_W;
            int corner_y = y*TILE_H + ((c+1)%4 / 2)*OCEAN_CORNER_H;
            DrawTexturePro(corner_tp.texture, corner_tp.portion, (Rectangle){corner_x, corner_y, OCEAN_CORNER_W, OCEAN_CORNER_H}, (Vector2){0.0,0.0}, 0.0f, WHITE);
        }
        for (Direction4 d = 0; d < 4; ++d) {
            if (tiles[neigh[d]].type == T_RIVER) {
                TexturePortion rivermouth = stextures->rivermouths[d];
                DrawTexturePro(rivermouth.texture, rivermouth.portion, (Rectangle){x*TILE_W, y*TILE_H, TILE_W, TILE_H}, (Vector2){0.0,0.0}, 0.0, WHITE);
            }
        }
    }
}

void draw_entity(Entity* e, SpritesheetTextures* stextures) {
    switch (e->entity_type) {
        case E_NIL: return;
        case E_UNIT: {
            TexturePortion tp = stextures->units[e->unit_type];
            //DrawRectangle(e->x*TILE_W, e->y*TILE_H, UNIT_W, UNIT_H, RED);
            DrawTexturePro(
                tp.texture,
                tp.portion,
                (Rectangle){e->x*TILE_W, e->y*TILE_H, UNIT_W, UNIT_H},
                (Vector2){0.0,0.0},
                0.0,
                WHITE
            );
            break;
        } case E_CITY: {
            assert(0 && "todo");
            break;
        }
    }
}

void draw_fog(Fog* f, int x, int y) {
    Color c;
    switch (*f) {
        case F_UNDISCOVERED: c = (Color){0,0,0,255}; break;
        case F_FOGGY: c = (Color){0,0,0,128}; break;
        case F_VISIBLE: c = (Color){0,0,0,0}; break;
    }
    DrawRectangle(x, y, 1, 1, c);
}

void draw_text(Font font, float fontsize, float spacing, int x, int y, enum FontAlignment x_behavior, enum FontAlignment y_behavior, Color color, const char* format, ...) {
    va_list args;

    va_start(args, format);
    int size = vsnprintf(NULL, 0, format, args) + 1; // +1 for \0
    va_end(args);

    char formatted[size];
    
    va_start(args, format);
    vsnprintf(formatted, size, format, args);
    va_end(args);

    Vector2 formatted_size = MeasureTextEx(font, formatted, fontsize, spacing);
    int formatted_str_w = formatted_size.x;
    int formatted_str_h = formatted_size.y;
    x = x - (formatted_str_w/2)*x_behavior;
    y = y - (formatted_str_h/2)*y_behavior;
    DrawTextEx(
        font, 
        formatted, 
        (Vector2){x, y}, 
        fontsize, 
        spacing, 
        color
    );
}
