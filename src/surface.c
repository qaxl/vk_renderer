#include "surface.h"
#include "SDL_events.h"
#include "SDL_init.h"
#include "SDL_video.h"
#include "types.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <string.h>
#include <stdlib.h>

#include <volk.h>

#ifdef VK_USE_PLATFORM_WAYLAND_KHR
#include <gtk/gtk.h>
void sdl_wayland_titlebar_workaround() {
    gtk_init_check(NULL, NULL);
}
#else
#define sdl_wayland_titlebar_workaround() do {} while(0)
#endif

#define CALL_LISTENERS(event, ...) for (u32 i = 0; i < ZARRSIZ(event); ++i) { if (event[i] != NULL) event[i](__VA_ARGS__); }

struct Surface {
    SDL_Window* window;
    SDL_Event event;

    int has_received_close;
    int width, height;

    SurfaceResizeFunc on_resize_callbacks[32];

    void* data;
};

Surface* surface_create(int width, int height, const char* title) {
    sdl_wayland_titlebar_workaround();

    // TODO: error handling
    SDL_Init(SDL_INIT_EVERYTHING);

    Surface* surface = malloc(sizeof(Surface));
    memset(surface, 0, sizeof(Surface));

    surface->window = SDL_CreateWindow(title, width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    surface->data = NULL;

    return surface;
}

void surface_destroy(Surface* surface) {
    SDL_DestroyWindow(surface->window);
    free(surface);
}

const char* surface_get_title(Surface* surface) {
    return SDL_GetWindowTitle(surface->window);
}

void surface_get_size(Surface* surface, int* width, int* height) {
    *width = surface->width;
    *height = surface->height;
}

static inline void surface_event_handler(Surface* surface) {
    switch (surface->event.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            surface->has_received_close = true;
            break;

        case SDL_EVENT_WINDOW_RESIZED:
            surface->width = surface->event.window.data1;
            surface->height = surface->event.window.data2;
            CALL_LISTENERS(surface->on_resize_callbacks, surface, surface->width, surface->height);
            break;

        case SDL_EVENT_WINDOW_MINIMIZED:
        case SDL_EVENT_WINDOW_RESTORED:
            break;
    }
}

void surface_poll_events(Surface* surface) {
    while (SDL_PollEvent(&surface->event))
        surface_event_handler(surface);
}

void surface_wait_event(Surface* surface) {
    SDL_WaitEvent(&surface->event);
    surface_event_handler(surface);
}

/* used internally by renderer.c */
VkSurfaceKHR surface_vk_create(Surface* surface, VkInstance instance) {
    VkSurfaceKHR vk_surface;
    if (SDL_Vulkan_CreateSurface(surface->window, instance, NULL, &vk_surface) == SDL_FALSE) {
        // TODO: error handling
        return NULL;
    }

    return vk_surface;
}

const char* const* surface_vk_get_required_extensions(Surface* surface, u32* count) {
    return SDL_Vulkan_GetInstanceExtensions(count);
}

void surface_on_resize(Surface* surface, SurfaceResizeFunc function) {
    for (u32 i = 0; i < ZARRSIZ(surface->on_resize_callbacks); ++i) {
        if (surface->on_resize_callbacks[i] == NULL) {
            surface->on_resize_callbacks[i] = function;
            break;
        }
    }
}

int surface_should_close(Surface* surface) {
    return surface->has_received_close;
}

void surface_set_data(Surface* surface, void* data) {
    surface->data = data;
}

void* surface_get_data(Surface* surface) {
    return surface->data;
}
