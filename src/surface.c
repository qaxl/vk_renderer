#include "surface.h"
#include "SDL_init.h"
#include "types.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <string.h>
#include <stdlib.h>

#include <volk.h>

struct Surface {
    SDL_Window* window;
};

Surface* surface_create(int width, int height, const char* title) {
    // TODO: error handling
    SDL_Init(SDL_INIT_EVERYTHING);

    Surface* surface = malloc(sizeof(Surface));
    surface->window = SDL_CreateWindow(title, width, height, SDL_WINDOW_VULKAN);

    return surface;
}

void surface_destroy(Surface* surface) {
    SDL_DestroyWindow(surface->window);
    free(surface);
}

const char* surface_get_title(Surface* surface) {
    return SDL_GetWindowTitle(surface->window);
}

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
