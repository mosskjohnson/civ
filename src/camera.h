#ifndef CAMERA_H
#define CAMERA_H

#include "raylib.h"

#define MIN_ZOOM 1.0
#define MAX_ZOOM 8.0

typedef struct {
    int window_w;
    int window_h;
    int canvas_w;
    int canvas_h;
} CameraSizeInfo;

void clamp_camera(Camera2D* c, CameraSizeInfo csi);
void zoom_on_anchor(Camera2D* c, CameraSizeInfo csi, float zoom_factor, Vector2 anchor);
void pan(Camera2D* c, CameraSizeInfo csi, int x, int y);

#endif