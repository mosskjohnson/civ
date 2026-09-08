#include <math.h>

#include "utils.h"
#include "camera.h"

void clamp_camera(Camera2D* c, CameraSizeInfo csi) {
    float half_camera_h = (csi.window_h / 2.0f) / c->zoom;
    c->target.y = CLAMP(c->target.y, half_camera_h, csi.canvas_h - half_camera_h);
    
    float canvas_w = csi.canvas_w;
    c->target.x = fmodf(c->target.x, canvas_w);
      if (c->target.x < 0) c->target.x += canvas_w;
}

void zoom_on_anchor(Camera2D* c, CameraSizeInfo csi, float zoom_factor, Vector2 anchor) {
    Vector2 world_before = GetScreenToWorld2D(anchor, *c);
    
    c->zoom *= zoom_factor;
    c->zoom = CLAMP(c->zoom, MIN_ZOOM, MAX_ZOOM);
    
    Vector2 world_after = GetScreenToWorld2D(anchor, *c);
    
    c->target.x += world_before.x - world_after.x;
    c->target.y += world_before.y - world_after.y;

    clamp_camera(c, csi);
}

void pan(Camera2D* c, CameraSizeInfo csi, int x, int y) {
    c->target.x -= x / c->zoom;
    c->target.y -= y / c->zoom;

    clamp_camera(c, csi);
}