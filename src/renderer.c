#include "renderer.h"
#include "surface.h"

#include <volk.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <vulkan/vulkan_core.h>

#define ERR_CHECK(x, m) do { VkResult r = x; if  (r != VK_SUCCESS) { fprintf(stderr, "\x1b[31m!!!CRITICAL ERROR!!! couldn't create %s: %d = %s; will crash !!!CRITICAL ERROR!!!\x1b[0m\n", m, r, vk_result_to_str(r)); exit(EXIT_FAILURE); } } while(0);

typedef struct QueueFamilies {
    u32 graphics_family;
    u32 present_family;

    u32 found_families;
} QueueFamilies;

struct VulkanGraphics {
    VkInstance instance;

    VkSurfaceKHR surface;

    VkPhysicalDevice gpu;
    VkDevice device;

    VkQueue graphics_queue;
    VkQueue present_queue;

    /* TODO: don't store this here lol */
    QueueFamilies families;

#if defined(ZULK_DEBUG)
    /* TODO: move this into optional structure? */
    VkDebugUtilsMessengerEXT debug_messenger;
#endif
};

#define QUEUE_IS_COMPLETE(x) (x.found_families & 0b1100)
#define QUEUE_FOUND_SET(x, m, v, b) x.m = v; x.found_families |= b

// Internal functions not defined here...
extern VkSurfaceKHR surface_vk_create(Surface* surface, VkInstance instance);
extern const char* const* surface_vk_get_required_extensions(Surface* surface, u32* count);

static inline bool load_vulkan() {
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

static inline const char* vk_result_to_str(VkResult result) {
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
        case VK_ERROR_DEVICE_LOST:
            return "Device lost";
        case VK_ERROR_MEMORY_MAP_FAILED:
            return "Memory map failed";
        case VK_ERROR_LAYER_NOT_PRESENT:
            return "Layer not present";
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            return "Extension not present";
        case VK_ERROR_FEATURE_NOT_PRESENT:
            return "Feature not present";
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            return "Incompatible driver";
        case VK_ERROR_TOO_MANY_OBJECTS:
            return "Too many objects";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            return "Format not supported";
        case VK_ERROR_FRAGMENTED_POOL:
            return "Fragmented pool";
        case VK_ERROR_UNKNOWN:
            return "Unknown error";
        case VK_ERROR_OUT_OF_POOL_MEMORY:
            return "Out of pool memory";
        case VK_ERROR_INVALID_EXTERNAL_HANDLE:
            return "Invalid external handle";
        case VK_ERROR_FRAGMENTATION:
            return "Fragmentation";
        case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
            return "Invalid opaque capture address";
        case VK_ERROR_SURFACE_LOST_KHR:
            return "Surface lost";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
            return "Native window in use";
        case VK_SUBOPTIMAL_KHR:
            return "Suboptimal";
        case VK_ERROR_OUT_OF_DATE_KHR:
            return "Out of date";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
            return "Incompatible display";
        case VK_ERROR_VALIDATION_FAILED_EXT:
            return "Validation failed";
        case VK_ERROR_INVALID_SHADER_NV:
            return "Invalid shader";
        case VK_ERROR_NOT_PERMITTED_EXT:
            return "Not permitted";
        case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
            return "Full screen exclusive mode lost";
        case VK_THREAD_IDLE_KHR:
            return "Thread idle";
        case VK_THREAD_DONE_KHR:
            return "Thread done";
        case VK_OPERATION_DEFERRED_KHR:
            return "Operation deferred";
        case VK_OPERATION_NOT_DEFERRED_KHR:
            return "Operation not deferred";
        case VK_PIPELINE_COMPILE_REQUIRED_EXT:
            return "Pipeline compile required";
        default:
            return "Unknown error code";
    }
}

/* this structure can only hold constant string *literals* */
typedef struct CStrArr {
    u32 size;
    const char* values[64];
} CStrArr;

CStrArr vk_required_instance_extensions(GraphicsConfiguration* config) {
    u32 surface_exts_count;
    const char* const* surface_exts = surface_vk_get_required_extensions(config->render_surface, &surface_exts_count);

    u32 avail;
    ERR_CHECK(vkEnumerateInstanceExtensionProperties(NULL, &avail, NULL), "VkInstanceExtensionProperties phase #1");

    VkExtensionProperties* available_extensions = malloc(sizeof(VkExtensionProperties) * avail);
    ERR_CHECK(vkEnumerateInstanceExtensionProperties(NULL, &avail, available_extensions), "VkInstanceExtensionProperties phase #2");

    CStrArr enabled_extensions;
    // enabled_extensions.capacity = 64;
    enabled_extensions.size = 0;

    const u32 required_exts = surface_exts_count;
    u32 required_exts_found = 0;

    for (u32 i = 0; i < avail; ++i) {
        if (strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, available_extensions[i].extensionName) == 0) {
            enabled_extensions.values[enabled_extensions.size++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        }

        for (u32 j = 0; j < surface_exts_count; ++j) {
            if (strcmp(surface_exts[j], available_extensions[i].extensionName) == 0) {
                /* safety: surface_exts is a static array pointer, so this SHOULD be safe in most cases. */
                enabled_extensions.values[enabled_extensions.size++] = surface_exts[j];
                required_exts_found++;
            }
        }
    }

    if (required_exts_found != required_exts) {
        fprintf(stderr, "all required extensions not available; required %d extensions, but got only %d. required extensions: ", required_exts_found, required_exts);
        for (u32 i = 0; i < required_exts; ++i) {
            if (i < surface_exts_count) {
                fprintf(stderr, "%s, ", surface_exts[i]);
            } else {
                /* here others... */
            }
        }

        fprintf(stderr, "\n");
        exit(EXIT_FAILURE);
    }

    free(available_extensions);
    return enabled_extensions;
}

static int vk_check_required_instance_layers(VkInstanceCreateInfo* info) {
#if defined(ZULK_DEBUG)
    u32 avail;
    ERR_CHECK(vkEnumerateInstanceLayerProperties(&avail, NULL), "VkInstanceLayerProperties phase #1");

    VkLayerProperties* available_layers = malloc(sizeof(VkLayerProperties) * avail);
    ERR_CHECK(vkEnumerateInstanceLayerProperties(&avail, available_layers), "VkInstanceLayerProperties phase #2");

    int all_found = false;

    static const char* enabled_layers[] = {
        "VK_LAYER_KHRONOS_validation"
    };

    /* TODO: if more layers wanted to be supported: change function entirely. */
    for (u32 i = 0; i < avail; ++i) {
        if (strcmp(available_layers[i].layerName, "VK_LAYER_KHRONOS_validation") == 0) {
            info->enabledLayerCount = 1;
            info->ppEnabledLayerNames = &enabled_layers[0];
            return true;
        }
    }

    return false;
#else
    *count = 0;
#endif
}

static inline VkDebugUtilsMessengerCreateInfoEXT vk_init_debug_messenger_info() {
    return (VkDebugUtilsMessengerCreateInfoEXT){
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,                                                                                             /*   vvv  intel doesn't have this extension? vvv */
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT /* | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT */,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
        .pfnUserCallback = vk_debug_callback,
    };
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

    CStrArr extensions = vk_required_instance_extensions(config);
    VkDebugUtilsMessengerCreateInfoEXT debug_messenger = vk_init_debug_messenger_info();

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,

        .pNext = &debug_messenger,
        .pApplicationInfo = &app_info,

        .enabledExtensionCount = extensions.size,
        .ppEnabledExtensionNames = extensions.values,
    };

    vk_check_required_instance_layers(&create_info);

    ERR_CHECK(vkCreateInstance(&create_info, NULL, &graphics->instance), "VkInstance");
    volkLoadInstance(graphics->instance);

#if defined(ZULK_DEBUG)
    if (vkCreateDebugUtilsMessengerEXT)
        vkCreateDebugUtilsMessengerEXT(graphics->instance, &debug_messenger, NULL, &graphics->debug_messenger);
#endif
}

static void vk_create_surface(VulkanGraphics* graphics, GraphicsConfiguration* config) {
    graphics->surface = surface_vk_create(config->render_surface, graphics->instance);
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

    ERR_CHECK(result, "VkPhysicalDevice");
    printf("Using %s preference to select a GPU.\n", config->power_preference == GRAPHICS_HIGH_PERFORMANCE ? "high perf" : "low power");

    // TODO: vvv
    // if (count == 1) {
    //     printf("This system only has one GPU, skipping the selection phase.\n");
    // }

    uint64_t ranks_of_devices[MAX_ACCEPTED_PHYSICAL_DEVICE_COUNT] = { 0 };

    for (u32 i = 0; i < count; ++i) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[i], &props);

        VkPhysicalDeviceMemoryProperties memory_props;
        vkGetPhysicalDeviceMemoryProperties(devices[i], &memory_props);

        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(devices[i], &features);

        printf("Found an GPU \"%s\"\n", props.deviceName);

        if (config->power_preference == GRAPHICS_HIGH_PERFORMANCE) {
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                ranks_of_devices[i] += 10000000;
        } else {
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
                ranks_of_devices[i] += 10000000;
        }

        ranks_of_devices[i] += props.limits.maxImageDimension3D;

        u64 vram = 0;
        for (u32 x = 0; x < memory_props.memoryHeapCount; ++x) {
            if (memory_props.memoryHeaps[x].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                ranks_of_devices[i] = vram += memory_props.memoryHeaps[x].size;
            }
        }
    }

    u32 best_device = 0;
    for (u32 i = 1; i < count; ++i) {
        if (ranks_of_devices[i] > ranks_of_devices[i - 1]) {
            best_device = i;
        } else {
            best_device = i - 1;
        }
    }

    // printf("Selected GPU at index %d to be the rendering GPU.\n", best_device);
    graphics->gpu = devices[best_device];

    /* TODO: move to device selection, perhaps own function */
    count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(graphics->gpu, &count, NULL);

    VkQueueFamilyProperties* queue_families = malloc(sizeof(VkQueueFamilyProperties) * count);
    vkGetPhysicalDeviceQueueFamilyProperties(graphics->gpu, &count, queue_families);


    QueueFamilies families;
    for (u32 i = 0; i < count; ++i) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            QUEUE_FOUND_SET(families, graphics_family, i, 0b1000);
        }

        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(graphics->gpu, i, graphics->surface, &present_support);

        if (present_support) {
            QUEUE_FOUND_SET(families, present_family, i, 0b0100);
        }

        if (QUEUE_IS_COMPLETE(families)) {
            break;
        }
    }

    graphics->families = families;
    free(queue_families);
}

static void vk_create_logical_dev(VulkanGraphics* graphics) {
    VkPhysicalDeviceFeatures enabled_features = {};

    /* TODO: check for extension support */
    const char* extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        
        .pEnabledFeatures = &enabled_features,

        .enabledExtensionCount = ZARRSIZ(extensions),
        .ppEnabledExtensionNames = extensions,
    };

    if (graphics->families.graphics_family == graphics->families.present_family) {
        VkDeviceQueueCreateInfo queue_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueCount = 1,
            .queueFamilyIndex = graphics->families.graphics_family,
            /* i have no idea will this work... */
            .pQueuePriorities = &(float){1.0f},
        };

        create_info.queueCreateInfoCount = 1;
        create_info.pQueueCreateInfos = &queue_info;
    } else {
        VkDeviceQueueCreateInfo queue_infos[] = {
            (VkDeviceQueueCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueCount = 1,
                .queueFamilyIndex = graphics->families.graphics_family,
                /* i have no idea will this work... */
                .pQueuePriorities = &(float){1.0f},
            },
            (VkDeviceQueueCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueCount = 1,
                .queueFamilyIndex = graphics->families.present_family,
                /* i have no idea will this work... */
                .pQueuePriorities = &(float){1.0f},
            },
        };

        create_info.queueCreateInfoCount = 2;
        create_info.pQueueCreateInfos = queue_infos;
    }

    ERR_CHECK(vkCreateDevice(graphics->gpu, &create_info, NULL, &graphics->device), "VkDevice");
    volkLoadDevice(graphics->device);

    vkGetDeviceQueue(graphics->device, graphics->families.graphics_family, 0, &graphics->graphics_queue);
    vkGetDeviceQueue(graphics->device, graphics->families.present_family, 0, &graphics->present_queue);
}


VulkanGraphics* graphics_initialize(GraphicsConfiguration* config) {
    if (!load_vulkan()) {
        return NULL;
    }

    VulkanGraphics* graphics = malloc(sizeof(VulkanGraphics));
    vk_create_instance(graphics, config);
    vk_create_surface(graphics, config);
    vk_select_physical_dev(graphics, config);
    vk_create_logical_dev(graphics);

    return graphics;
}

void graphics_deinitialize(VulkanGraphics* graphics) {
    vkDestroyDevice(graphics->device, NULL);

    vkDestroySurfaceKHR(graphics->instance, graphics->surface, NULL);

#if defined(ZULK_DEBUG)
    if (vkDestroyDebugUtilsMessengerEXT)
        vkDestroyDebugUtilsMessengerEXT(graphics->instance, graphics->debug_messenger, NULL);
#endif

    vkDestroyInstance(graphics->instance, NULL);

    free(graphics);
}
