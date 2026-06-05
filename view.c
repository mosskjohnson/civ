#include "view.h"
#include "utils.h"
#include "math.h"

void update_view(View* view, float zoom, float cx, float cy) {
    zoom = CLAMP(zoom, view->min_zoom, view->max_zoom);

    float view_w = view->canvas_w / zoom;
    float view_h = view->canvas_h / zoom;

    cx = fmodf(cx, view->canvas_w);
    if (cx < 0) cx += view->canvas_w;

    float half_view_h = view_h / 2.0;
    cy = CLAMP(cy, half_view_h, view->canvas_h-half_view_h);

    view->zoom = zoom;
    view->cx = cx;
    view->cy = cy;
}

void draw_tex_with_view(Texture2D tex, View* view, Rectangle dst) {
    float view_w = view->canvas_w / view->zoom;
    float view_h = view->canvas_h / view->zoom;
    Rectangle view_rect = (Rectangle){
        view->cx - (view_w/2.0), 
        -(view->cy - (view_h/2.0)), 
        view_w, 
        -view_h,
    };
    if (view_rect.x < 0) {
        float left_w = -view_rect.x;
        float right_w = view_rect.width - left_w;
        float left_frac = left_w / view_rect.width;
        Rectangle src_left  = {
            view->canvas_w - left_w, 
            view_rect.y, 
            left_w,  
            view_rect.height
        };
        Rectangle src_right = {
            0,
            view_rect.y, 
            right_w, 
            view_rect.height
        };
        Rectangle dst_left  = {
            dst.x,
            dst.y, 
            dst.width * left_frac,
            dst.height
        };
        Rectangle dst_right = {
            dst.x + dst.width * left_frac,
            dst.y,
            dst.width * (1 - left_frac), 
            dst.height
        };
        DrawTexturePro(tex, src_left,  dst_left,  (Vector2){0,0}, 0, WHITE);
        DrawTexturePro(tex, src_right, dst_right, (Vector2){0,0}, 0, WHITE);
    } else if (view_rect.x + view_rect.width > view->canvas_w) {
        float right_w = (view_rect.x + view_rect.width) - view->canvas_w;
        float left_w = view_rect.width - right_w;
        float left_frac = left_w / view_rect.width;
        Rectangle src_left  = {
            view_rect.x, 
            view_rect.y, 
            left_w,
            view_rect.height
        };
        Rectangle src_right = {
            0,
            view_rect.y, 
            right_w,  
            view_rect.height
        };
        Rectangle dst_left  = {
            dst.x,
            dst.y, 
            dst.width * left_frac,
            dst.height
        };
        Rectangle dst_right = {
            dst.x + dst.width * left_frac, 
            dst.y, 
            dst.width * (1 - left_frac), 
            dst.height
        };
        DrawTexturePro(tex, src_left,  dst_left,  (Vector2){0,0}, 0, WHITE);
        DrawTexturePro(tex, src_right, dst_right, (Vector2){0,0}, 0, WHITE);
    } else {
        DrawTexturePro(tex, view_rect, dst, (Vector2){0,0}, 0, WHITE);
    }
}
