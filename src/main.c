#include "renderer.h"
#include "surface.h"

// TODO: abstract
#include <SDL3/SDL.h>
#include <time.h>
#include <stdio.h>

int main() {
    Surface* surface = surface_create(1024, 768, "test");

    clock_t start = clock() / (CLOCKS_PER_SEC / 1000);
    Graphics* graphics = graphics_initialize(&(GraphicsConfiguration) {
        .app_name = surface_get_title(surface),
        .power_preference = GRAPHICS_HIGH_PERFORMANCE,

        .version.major = 0,
        .version.minor = 1,
        .version.patch = 0,

        .render_surface = surface
    });
    clock_t end = clock() / (CLOCKS_PER_SEC / 1000);

    printf("took %ldms to initialzie vulkan\n", end - start);

    while (!surface_should_close(surface)) {
        graphics_draw_frame(graphics);
        surface_poll_events(surface);
    }

    graphics_deinitialize(graphics);
    surface_destroy(surface);
}
