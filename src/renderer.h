#pragma once

#include "types.h"

typedef struct VulkanGraphics VulkanGraphics;
// In the future possibly more APIs might be supported?
typedef VulkanGraphics Graphics;

enum GPUPowerPreference {
    GRAPHICS_LOW_POWER,
    GRAPHICS_HIGH_PERFORMANCE,
};

typedef struct GraphicsConfiguration {
    // Specifies the application name; drivers can use this to determine optimizations for 
    // popular games and apps.
    const char* app_name;

    // Application version.
    struct {
        // Technically, in Vulkan last 3 bits are used for variant;
        // Here it is treated as the major version.
        u16 major: 10;
        u16 minor: 10;
        u16 patch: 12;
    } version;

    // High performance prefers dGPUs and low power prefers integrated graphics.
    // But some systems may only have one GPU, so then this option doesn't do anything.
    enum GPUPowerPreference power_preference;

    struct Surface* render_surface;
} GraphicsConfiguration;

#define MAX_ACCEPTED_PHYSICAL_DEVICE_COUNT (u32)32
#define MAX_INSTANCE_EXTENSIONS_LOADED (u32)4096

Graphics* graphics_initialize(GraphicsConfiguration* config);
void graphics_deinitialize(Graphics* graphics);
