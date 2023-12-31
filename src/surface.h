#pragma once

#include <stdbool.h>

typedef struct Surface Surface;

typedef void (*SurfaceResizeFunc)(Surface*, int, int);

Surface* surface_create(int width, int height, const char* title);
void surface_destroy(Surface* surface);

const char* surface_get_title(Surface* surface);
void surface_get_size(Surface* surface, int* width, int* height);
void surface_poll_events(Surface* surface);
void surface_wait_event(Surface* surface);

void surface_on_resize(Surface* surface, SurfaceResizeFunc function);
int surface_should_close(Surface* surface);

void surface_set_data(Surface* surface, void* data);
void* surface_get_data(Surface* surface);

