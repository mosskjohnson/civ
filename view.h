#ifndef VIEW_H
#define VIEW_H

#include "raylib.h"

typedef struct {
    float min_zoom;
    float max_zoom;
    float canvas_w;
    float canvas_h;

    float zoom;
    float cx;
    float cy;
    
} View;

void update_view(View* view, float zoom, float cx, float cy);

void draw_tex_with_view(Texture2D tex, View* view, Rectangle dst);

#endif
