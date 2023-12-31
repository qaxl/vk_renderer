#pragma once

typedef struct Surface Surface;

Surface* surface_create(int width, int height, const char* title);
void surface_destroy(Surface* surface);

const char* surface_get_title(Surface* surface);
