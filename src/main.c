#include "renderer.h"
#include "surface.h"
#include "types.h"

#include <stdio.h>

#include <SDL3/SDL.h>

#ifdef VK_USE_PLATFORM_WAYLAND_KHR
#include <gtk/gtk.h>
void temp_sdl_titlebar_wayland_workaround() {
    gtk_init_check(NULL, NULL);
}
#else
#define temp_sdl_titlebar_wayland_workaround() 
#endif

int main() {
    temp_sdl_titlebar_wayland_workaround();

    Surface* surface = surface_create(1024, 768, "test");

    Graphics* graphics = graphics_initialize(&(GraphicsConfiguration) {
        .app_name = surface_get_title(surface),
        .power_preference = GRAPHICS_HIGH_PERFORMANCE,

        .version.major = 0,
        .version.minor = 1,
        .version.patch = 0,

        .render_surface = surface
    });

    SDL_Renderer* render = SDL_CreateRenderer(*(SDL_Window**)surface, NULL, SDL_RENDERER_ACCELERATED);
    SDL_SetRenderDrawColor(render, 100, 100, 100, 255);

    while (1) {
        SDL_Event event;
        SDL_PollEvent(&event);

        if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            break;

        SDL_RenderClear(render);
        SDL_RenderPresent(render);
    }

    graphics_deinitialize(graphics);
    surface_destroy(surface);
}
