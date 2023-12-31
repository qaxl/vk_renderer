#include "renderer.h"
#include "surface.h"

// TODO: abstract
#include <SDL3/SDL.h>

int main() {
    Surface* surface = surface_create(1024, 768, "test");

    Graphics* graphics = graphics_initialize(&(GraphicsConfiguration) {
        .app_name = surface_get_title(surface),
        .power_preference = GRAPHICS_HIGH_PERFORMANCE,

        .version.major = 0,
        .version.minor = 1,
        .version.patch = 0,

        .render_surface = surface
    });

    while (1) {
        SDL_Event event;
        SDL_PollEvent(&event);

        if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            break;
    }

    graphics_deinitialize(graphics);
    surface_destroy(surface);
}
