#include "renderer.h"
#include "surface.h"

#include <volk.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <vulkan/vulkan_core.h>

#define MUST_SUCCEED_OR_CRASH(x, m) do { VkResult r = x; if  (r != VK_SUCCESS) { fprintf(stderr, "\x1b[31m!!!CRITICAL ERROR!!! couldn't create %s: %d = %s; will crash !!!CRITICAL ERROR!!!\x1b[0m\n", m, r, vk_result_to_str(r)); exit(EXIT_FAILURE); } } while(0);

struct VulkanGraphics {
    VkInstance instance;

    VkPhysicalDevice gpu;
    VkDevice device;
};

// Internal functions not defined here...
extern VkSurfaceKHR surface_vk_create(Surface* surface, VkInstance instance);
extern const char* const* surface_vk_get_required_extensions(Surface* surface, u32* count);

static bool load_vulkan() {
    static bool loaded = false;

    if (!loaded) {
        if (volkInitialize() != VK_SUCCESS) {
            fprintf(stderr, "vulkan not available\n");
            return false;
        }

        loaded = true;
        return true;
    }

    return true;
}

static VkBool32 VKAPI_CALL vk_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    fprintf(stderr, "vulkan validation: %s\n", pCallbackData->pMessage);
    return VK_FALSE;
}

static const char* vk_result_to_str(VkResult result) {
    switch (result) {
        case VK_SUCCESS:
            return "Success";
        case VK_NOT_READY:
            return "Not ready";
        case VK_TIMEOUT:
            return "Timeout";
        case VK_EVENT_SET:
            return "Event set";
        case VK_EVENT_RESET:
            return "Event reset";
        case VK_INCOMPLETE:
            return "Incomplete";
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return "Out of host memory";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return "Out of device memory";
        case VK_ERROR_INITIALIZATION_FAILED:
            return "Initialization failed";
        case VK_ERROR_LAYER_NOT_PRESENT:
            return "Layer not present";
        // Add more cases for other error codes as needed
        default:
            return "Unknown error code";
    }
}


static void enable_extension_if_possible(VkExtensionProperties* extension, const char* wanted_extension, const char** enabled_extensions, u32* enabled_count) {
    if (strcmp(wanted_extension, extension->extensionName) == 0) {
        printf("enabling instance extension %s (v: %d)\n", extension->extensionName, extension->specVersion);
        enabled_extensions[*enabled_count++] = extension->extensionName;
    }
}

const char** vk_required_instance_extensions(GraphicsConfiguration* config, u32* count) {
    const char* const* surface_exts = surface_vk_get_required_extensions(config->render_surface, count);
    u32 surface_exts_count = *count;

    const char** extensions = malloc(sizeof(const char*) * (*count + 1000));
    *count = 0;

    u32 avail;
    MUST_SUCCEED_OR_CRASH(vkEnumerateInstanceExtensionProperties(NULL, &avail, NULL), "VkInstanceExtensionProperties phase #1");

    VkExtensionProperties* props = malloc(sizeof(VkExtensionProperties) * avail);
    MUST_SUCCEED_OR_CRASH(vkEnumerateInstanceExtensionProperties(NULL, &avail, props), "VkInstanceExtensionProperties phase #2");

    for (u32 i = 0; i < avail; ++i) {
        printf("vulkan instance-level extension: %s\n", props[i].extensionName);

        enable_extension_if_possible(&props[i], VK_EXT_DEBUG_UTILS_EXTENSION_NAME, extensions, count);

        for (u32 j = 0; j < surface_exts_count; ++j) {
            enable_extension_if_possible(&props[i], surface_exts[j], extensions, count);
        }
    }

    free(props);
    return extensions;
}

static void vk_create_instance(VulkanGraphics* graphics, GraphicsConfiguration* config) {
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,

        .pApplicationName = config->app_name,
        .applicationVersion = VK_MAKE_API_VERSION(0, config->version.major, config->version.minor, config->version.patch),

        .pEngineName = "simple vulkan c engine",
        .engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),

        .apiVersion = VK_API_VERSION_1_3,
    };

    u32 extension_count = 0;
    const char** extensions = vk_required_instance_extensions(config, &extension_count);

    const char* layers[] = { "VK_LAYER_KHRONOS_validation" };

    uint32_t count;
    vkEnumerateInstanceLayerProperties(&count, NULL);

    VkLayerProperties* props = calloc(sizeof(VkLayerProperties), count);
    vkEnumerateInstanceLayerProperties(&count, props);

    for (u32 i = 0; i < count; ++i) {
        printf("layer %s\n", props[i].layerName);
    }

    VkDebugUtilsMessengerCreateInfoEXT debug_messenger = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
        .pfnUserCallback = NULL,
    };

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,

        .pApplicationInfo = &app_info,

        .enabledExtensionCount = extension_count,
        .ppEnabledExtensionNames = extensions,

        .enabledLayerCount = 1,
        .ppEnabledLayerNames = layers,
    };

    MUST_SUCCEED_OR_CRASH(vkCreateInstance(&create_info, NULL, &graphics->instance), "VkInstance");
    volkLoadInstance(graphics->instance);

    free(extensions);
}

static void vk_select_physical_dev(VulkanGraphics* graphics, GraphicsConfiguration* config) {
    u32 count = MAX_ACCEPTED_PHYSICAL_DEVICE_COUNT;
    VkPhysicalDevice devices[MAX_ACCEPTED_PHYSICAL_DEVICE_COUNT];

    VkResult result = vkEnumeratePhysicalDevices(graphics->instance, &count, devices);
    if (result == VK_INCOMPLETE) {
        // TODO: this could be implemented, but is it really worth it? no.
        fprintf(stderr, "too many vulkan devices, truncating to %d (of %d) devices", MAX_ACCEPTED_PHYSICAL_DEVICE_COUNT, count);

        result = VK_SUCCESS;
    }

    MUST_SUCCEED_OR_CRASH(result, "VkPhysicalDevice");
    printf("Using %s preference to select a GPU.\n", config->power_preference == GRAPHICS_HIGH_PERFORMANCE ? "high perf" : "low power");

    // TODO: vvv
    // if (count == 1) {
    //     printf("This system only has one GPU, skipping the selection phase.\n");
    // }

    uint32_t ranks_of_devices[MAX_ACCEPTED_PHYSICAL_DEVICE_COUNT] = { 0 };

    for (u32 i = 0; i < count; ++i) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[i], &props);

        VkPhysicalDeviceMemoryProperties memory_props;
        vkGetPhysicalDeviceMemoryProperties(devices[i], &memory_props);

        printf("Found an GPU \"%s\"\n", props.deviceName);

        ranks_of_devices[i] += props.limits.maxImageDimension3D;

        u64 vram = 0;
        for (u32 x = 0; x < memory_props.memoryHeapCount; ++x) {
            // ignore shared memory, for now, for simplicity?
            if (memory_props.memoryHeaps[x].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                printf("\tmemory heap %d has flags: %d\n", x, memory_props.memoryHeaps[x].flags);
                ranks_of_devices[i] = vram += memory_props.memoryHeaps[x].size;
            }
        }

        printf("\thas %d memory heaps in total of %lf GiB of VRAM\n", memory_props.memoryHeapCount, (double)vram / 1024 / 1024 / 1024);
    }

    u32 best_device = 0;
    for (u32 i = 1; i < count; ++i) {
        if (ranks_of_devices[i] > ranks_of_devices[i - 1]) {
            best_device = i;
        } else {
            best_device = i - 1;
        }
    }

    printf("Selected GPU at index %d to be the rendering GPU.\n", best_device);
    graphics->gpu = devices[best_device];
}

static void vk_create_logical_dev(VulkanGraphics* graphics) {
    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    };

    MUST_SUCCEED_OR_CRASH(vkCreateDevice(graphics->gpu, &create_info, NULL, &graphics->device), "VkDevice");
    volkLoadDevice(graphics->device);
}


VulkanGraphics* graphics_initialize(GraphicsConfiguration* config) {
    if (!load_vulkan()) {
        return NULL;
    }

    VulkanGraphics* graphics = malloc(sizeof(VulkanGraphics));
    vk_create_instance(graphics, config);
    vk_select_physical_dev(graphics, config);
    vk_create_logical_dev(graphics);

    return graphics;
}

void graphics_deinitialize(VulkanGraphics* graphics) {
    vkDestroyDevice(graphics->device, NULL);
    vkDestroyInstance(graphics->instance, NULL);

    free(graphics);
}
