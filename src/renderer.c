#include "renderer.h"
#include "surface.h"
#include "io.h"

#include <volk.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define ERR_CHECK(x, m) do { VkResult r = x; if  (r != VK_SUCCESS) { fprintf(stderr, "\x1b[31m!!!CRITICAL ERROR!!! couldn't create %s: %d = %s; will crash !!!CRITICAL ERROR!!!\x1b[0m\n", m, r, vk_result_to_str(r)); exit(EXIT_FAILURE); } } while(0);

typedef struct QueueFamilies {
    u32 graphics_family;
    u32 present_family;

    u32 found_families;
} QueueFamilies;

/* this structure can only hold constant string *literals* */
typedef struct CStrArr {
    u32 size;
    const char* values[64];
} CStrArr;

typedef struct SurfaceDetails {
    VkSurfaceCapabilitiesKHR caps;

    u32 formats_count;
    VkSurfaceFormatKHR* formats;

    u32 modes_count;
    VkPresentModeKHR* modes;
} SurfaceDetails;

struct VulkanGraphics {
    VkInstance instance;

    VkSurfaceKHR surface;

    VkPhysicalDevice gpu;
    VkDevice device;

    VkQueue graphics_queue;
    VkQueue present_queue;

    VkSwapchainKHR swapchain;
    VkExtent2D swapchain_extent;
    VkSurfaceFormatKHR swapchain_format;

    /* TODO: this could be stored in "stack" (this object is heap allocated anyway) */
    u32 swapchain_images_count;
    u32 swapchain_views_count;

    VkImage* swapchain_images;
    VkImageView* swapchain_views;
    VkFramebuffer* swapchain_framebuffers;

    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline graphics_pipeline;

    VkCommandPool command_pool;
    VkCommandBuffer command_buffers[MAX_FRAMES_IN_FLIGHT];

    VkSemaphore image_available_semaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore render_finished_semaphores[MAX_FRAMES_IN_FLIGHT];
    VkFence in_flight_fences[MAX_FRAMES_IN_FLIGHT];

    /* SPACE OPTIMIZATION... */
    u32 swapchain_framebuffers_count;
    u32 current_frame;

    Surface* render_surface;

    bool frame_resized_recently;

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
    /* IGNORED: status message about validation layer enablement. */
    if (pCallbackData->messageIdNumber == -2016116905)
        return VK_FALSE;

    if (pCallbackData->messageIdNumber == 0xa470b504)
        __debugbreak();

    fprintf(stderr, "\x1b[33mVulkan validation layers: %s\n\x1b[0m", pCallbackData->pMessage);
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

        // if (strcmp(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME, available_extensions[i].extensionName) == 0) {
        //     enabled_extensions.values[enabled_extensions.size++] = VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME;
        // }

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

    static const char* enabled_layers[] = {
        "VK_LAYER_KHRONOS_validation"
    };

    /* TODO: if more layers wanted to be supported: change function entirely. */
    for (u32 i = 0; i < avail; ++i) {
        if (strcmp(available_layers[i].layerName, "VK_LAYER_KHRONOS_validation") == 0) {
            info->enabledLayerCount = 1;
            info->ppEnabledLayerNames = &enabled_layers[0];
            free(available_layers);
            return true;
        }
    }

    free(available_layers);
    return false;
#else
    return false;
#endif
}

static inline VkDebugUtilsMessengerCreateInfoEXT vk_init_debug_messenger_info() {
    return (VkDebugUtilsMessengerCreateInfoEXT){
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
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

    int res = vk_check_required_instance_layers(&create_info);

    ERR_CHECK(vkCreateInstance(&create_info, NULL, &graphics->instance), "VkInstance");
    volkLoadInstance(graphics->instance);

#if defined(ZULK_DEBUG)
    if (res && vkCreateDebugUtilsMessengerEXT) {
        printf("\x1b[31menabling layers!\n\x1b[0m");
        debug_messenger = vk_init_debug_messenger_info();
        vkCreateDebugUtilsMessengerEXT(graphics->instance, &debug_messenger, NULL, &graphics->debug_messenger);
    } else {
        graphics->debug_messenger = NULL;
    }
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

/* TODO: these functions do not need to take in the full details */
static VkSurfaceFormatKHR vk_select_best_surface_format(SurfaceDetails* details) {
    for (u32 i = 0; i < details->formats_count; ++i) {
        if (details->formats[i].format == VK_FORMAT_R8G8B8A8_SRGB && details->formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return details->formats[i];
    }

    return details->formats[0];
}

/* TODO: allow setting in engine settings */
static VkPresentModeKHR vk_select_best_present_mode(SurfaceDetails* details) {
    for (u32 i = 0; i < details->modes_count; ++i) {
        if (details->modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
            return details->modes[i];
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D vk_select_best_swapchain_extent(Surface* render_surface, SurfaceDetails* details) {
    if (details->caps.currentExtent.width != UINT32_MAX)
        return details->caps.currentExtent;

    int width, height;
    surface_get_size(render_surface, &width, &height);

    VkExtent2D extent;

    extent.width = ZCLAMP(width, details->caps.minImageExtent.width, details->caps.maxImageExtent.width);
    extent.width = ZCLAMP(height, details->caps.minImageExtent.height, details->caps.maxImageExtent.height);

    return extent;
}

static void vk_create_logical_dev(VulkanGraphics* graphics, GraphicsConfiguration* config) {
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
                .pQueuePriorities = (float[]){1.0f},
            },
            (VkDeviceQueueCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueCount = 1,
                .queueFamilyIndex = graphics->families.present_family,
                /* i have no idea will this work... */
                .pQueuePriorities = (float[]){1.0f, 1.0f},
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

static void vk_create_swapchain(VulkanGraphics* graphics) {
    /* TODO: verify does device have these... */
    SurfaceDetails details = {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(graphics->gpu, graphics->surface, &details.caps);

    vkGetPhysicalDeviceSurfaceFormatsKHR(graphics->gpu, graphics->surface, &details.formats_count, NULL);
    details.formats = malloc(sizeof(*details.formats) * details.formats_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(graphics->gpu, graphics->surface, &details.formats_count, details.formats);


    vkGetPhysicalDeviceSurfacePresentModesKHR(graphics->gpu, graphics->surface, &details.modes_count, NULL);
    details.modes = malloc(sizeof(*details.modes) * details.modes_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(graphics->gpu, graphics->surface, &details.modes_count, details.modes);

    VkSurfaceFormatKHR format = vk_select_best_surface_format(&details);
    VkPresentModeKHR mode = vk_select_best_present_mode(&details);
    VkExtent2D extent = vk_select_best_swapchain_extent(graphics->render_surface, &details);

    u32 image_count = details.caps.minImageCount + 1;
    if (details.caps.maxImageCount > 0 && image_count > details.caps.maxImageCount)
        image_count = details.caps.maxImageCount;

    int exclusive = graphics->families.graphics_family == graphics->families.present_family;
    u32 queue_families[] = { graphics->families.graphics_family, graphics->families.present_family };

    VkSwapchainCreateInfoKHR swapchain_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        
        .surface = graphics->surface,
        .minImageCount = image_count,
        
        .imageFormat = format.format,
        .imageColorSpace = format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        
        .imageSharingMode = exclusive ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
        .queueFamilyIndexCount = exclusive ? 0 : 2,
        .pQueueFamilyIndices = exclusive ? NULL : queue_families,

        .preTransform = details.caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = mode,
        .clipped = VK_TRUE,

        .oldSwapchain = VK_NULL_HANDLE,
    };

    ERR_CHECK(vkCreateSwapchainKHR(graphics->device, &swapchain_info, NULL, &graphics->swapchain), "couldn't create swapchain");

    free(details.formats);
    free(details.modes);

    vkGetSwapchainImagesKHR(graphics->device, graphics->swapchain, &graphics->swapchain_images_count, NULL);
    graphics->swapchain_images = malloc(sizeof(VkImage) * graphics->swapchain_images_count);
    vkGetSwapchainImagesKHR(graphics->device, graphics->swapchain, &graphics->swapchain_images_count, graphics->swapchain_images);

    graphics->swapchain_extent = extent;
    graphics->swapchain_format = format;
}

static void vk_create_image_views(VulkanGraphics* graphics) {
    graphics->swapchain_views_count = graphics->swapchain_images_count;
    graphics->swapchain_views = malloc(sizeof(VkImageView) * graphics->swapchain_views_count);

    for (u32 i = 0; i < graphics->swapchain_views_count; ++i) {
        VkImageViewCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,

            .image = graphics->swapchain_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,

            .format = graphics->swapchain_format.format,
            .components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, },

            .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .subresourceRange.baseMipLevel = 0,
            .subresourceRange.levelCount = 1,
            .subresourceRange.baseArrayLayer = 0,
            .subresourceRange.layerCount = 1,
        };

        ERR_CHECK(vkCreateImageView(graphics->device, &info, NULL, &graphics->swapchain_views[i]), "image view creation");
    }
}

static void vk_create_render_pass(VulkanGraphics* graphics) {
    VkAttachmentDescription color_attachment = {
        .format = graphics->swapchain_format.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,

        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,

        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,

        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference color_attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref,
    };

    VkSubpassDependency dep = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,

        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,

        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dep,
    };

    ERR_CHECK(vkCreateRenderPass(graphics->device, &info, NULL, &graphics->render_pass), "render pass");
}

static VkShaderModule vk_create_shader_module(VulkanGraphics* graphics, FileView* view) {
    VkShaderModuleCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = view->length,
        .pCode = (const u32*)view->data,
    };

    VkShaderModule module;
    ERR_CHECK(vkCreateShaderModule(graphics->device, &info, NULL, &module), "shader module creation");

    return module;
}

static void vk_create_graphics_pipeline(VulkanGraphics* graphics) {
    FileView vertex = file_view_open("shaders/bin/vulkan_triangle_pos.vert.spv");
    FileView fragment = file_view_open("shaders/bin/vulkan_triangle_pos.frag.spv");

    if (vertex.data == NULL || fragment.data == NULL) {
        fprintf(stderr, "couldn't load shaders!");
        exit(EXIT_FAILURE);
    }

    VkShaderModule vertex_mod = vk_create_shader_module(graphics, &vertex);
    VkShaderModule fragment_mod = vk_create_shader_module(graphics, &fragment);

    file_view_close(&vertex);
    file_view_close(&fragment);

    VkPipelineShaderStageCreateInfo vert_shader_stage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertex_mod,
        .pName = "main"
    };

    VkPipelineShaderStageCreateInfo frag_shader_stage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragment_mod,
        .pName = "main"
    };

    VkPipelineShaderStageCreateInfo stages[] = { vert_shader_stage, frag_shader_stage };

    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = ZARRSIZ(dynamic_states),
        .pDynamicStates = dynamic_states,
    };

    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    // VkViewport viewport = {
    //     .x = 0,
    //     .y = 0,

    //     .width = (float)graphics->swapchain_extent.width,
    //     .height = (float)graphics->swapchain_extent.height,

    //     .minDepth = 0,
    //     .maxDepth = 1,
    // };

    // VkRect2D scissor = {
    //     .extent = graphics->swapchain_extent,
    // };

    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
    };

    VkPipelineMultisampleStateCreateInfo multisample = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE,
    };

    VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment,
    };

    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    };

    ERR_CHECK(vkCreatePipelineLayout(graphics->device, &layout_info, NULL, &graphics->pipeline_layout), "pipeline layout creation");

    VkGraphicsPipelineCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = ZARRSIZ(stages),
        .pStages = stages,

        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisample,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_state,

        .layout = graphics->pipeline_layout,
        .renderPass = graphics->render_pass,
        .subpass = 0,
    };

    ERR_CHECK(vkCreateGraphicsPipelines(graphics->device, VK_NULL_HANDLE, 1, &info, NULL, &graphics->graphics_pipeline), "graphics pipeline");

    vkDestroyShaderModule(graphics->device, vertex_mod, NULL);
    vkDestroyShaderModule(graphics->device, fragment_mod, NULL);
}

static void vk_create_framebuffers(VulkanGraphics* graphics) {
    graphics->swapchain_framebuffers_count = graphics->swapchain_images_count;
    graphics->swapchain_framebuffers = malloc(sizeof(VkFramebuffer) * graphics->swapchain_framebuffers_count);

    for (u32 i = 0; i < graphics->swapchain_framebuffers_count; ++i) {
        VkImageView attachments[] = {
            graphics->swapchain_views[i],
        };

        VkFramebufferCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = graphics->render_pass,
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = graphics->swapchain_extent.width,
            .height = graphics->swapchain_extent.height,
            .layers = 1,
        };

        ERR_CHECK(vkCreateFramebuffer(graphics->device, &info, NULL, &graphics->swapchain_framebuffers[i]), "framebuffer creation");
    }
}

static void vk_create_command_pool(VulkanGraphics* graphics) {
    VkCommandPoolCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphics->families.graphics_family,
    };

    ERR_CHECK(vkCreateCommandPool(graphics->device, &info, NULL, &graphics->command_pool), "command pool");
}

static void vk_create_command_buffers(VulkanGraphics* graphics) {
    VkCommandBufferAllocateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = graphics->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = ZARRSIZ(graphics->command_buffers),
    };

    ERR_CHECK(vkAllocateCommandBuffers(graphics->device, &info, graphics->command_buffers), "command buffer");
}

static void vk_create_sync_objects(VulkanGraphics* graphics) {
    VkSemaphoreCreateInfo sem_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fence_info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        ERR_CHECK(vkCreateSemaphore(graphics->device, &sem_info, NULL, &graphics->image_available_semaphores[i]), "img sem");
        ERR_CHECK(vkCreateSemaphore(graphics->device, &sem_info, NULL, &graphics->render_finished_semaphores[i]), "rfinish sem");
        ERR_CHECK(vkCreateFence(graphics->device, &fence_info, NULL, &graphics->in_flight_fences[i]), "infly fence");
    }
}

static void vk_cleanup_swapchain(VulkanGraphics* graphics) {
    for (u32 i = 0; i < graphics->swapchain_framebuffers_count; ++i) {
        vkDestroyFramebuffer(graphics->device, graphics->swapchain_framebuffers[i], NULL);
    }

    for (u32 i = 0; i < graphics->swapchain_views_count; ++i) {
        vkDestroyImageView(graphics->device, graphics->swapchain_views[i], NULL);
    }

    free(graphics->swapchain_views);
    free(graphics->swapchain_images);

    vkDestroySwapchainKHR(graphics->device, graphics->swapchain, NULL);
}

static void vk_recreate_swapchain(VulkanGraphics* graphics) {
    int width = 0, height = 0;
    while (width == 0 || height == 0) {
        surface_get_size(graphics->render_surface, &width, &height);
        surface_wait_event(graphics->render_surface);
    }

    vkDeviceWaitIdle(graphics->device);

    vk_cleanup_swapchain(graphics);

    vk_create_swapchain(graphics);
    vk_create_image_views(graphics);
    vk_create_framebuffers(graphics);
}


static void vk_record_command_buffer(VulkanGraphics* graphics, VkCommandBuffer command_buffer, u32 image_index) {
    VkCommandBufferBeginInfo begin = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    ERR_CHECK(vkBeginCommandBuffer(command_buffer, &begin), "failed to (begin) record command buffer");

    VkRenderPassBeginInfo render_pass = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = graphics->render_pass,
        .framebuffer = graphics->swapchain_framebuffers[image_index],

        .renderArea.offset = { 0, 0 },
        .renderArea.extent = graphics->swapchain_extent,

        .clearValueCount = 1,
        .pClearValues = (VkClearValue[]){ { { { 0, 0, 0, 1 } } } }
    };

    vkCmdBeginRenderPass(command_buffer, &render_pass, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics->graphics_pipeline);

    VkViewport viewport = {
        .width = (float)graphics->swapchain_extent.width,
        .height = (float)graphics->swapchain_extent.height,
        .minDepth = 0,
        .maxDepth = 1,
    };
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D scissor = {
        .extent = graphics->swapchain_extent
    };
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    vkCmdDraw(command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);
    ERR_CHECK(vkEndCommandBuffer(command_buffer), "failed to (end) record command buffer");
}

static void vk_surface_on_resize(Surface* surface, int width, int height) {
    Graphics* graphics = surface_get_data(surface);
    graphics->frame_resized_recently = true;
}

VulkanGraphics* graphics_initialize(GraphicsConfiguration* config) {
    if (!load_vulkan()) {
        return NULL;
    }

    VulkanGraphics* graphics = malloc(sizeof(VulkanGraphics));
    graphics->current_frame = 0;
    graphics->render_surface = config->render_surface;

    vk_create_instance(graphics, config);
    vk_create_surface(graphics, config);
    vk_select_physical_dev(graphics, config);
    vk_create_logical_dev(graphics, config);

    vk_create_swapchain(graphics);
    vk_create_image_views(graphics);
    vk_create_render_pass(graphics);

    vk_create_graphics_pipeline(graphics);
    vk_create_framebuffers(graphics);
    vk_create_command_pool(graphics);
    vk_create_command_buffers(graphics);
    vk_create_sync_objects(graphics);

    surface_set_data(graphics->render_surface, graphics);
    surface_on_resize(graphics->render_surface, vk_surface_on_resize);

    return graphics;
}

void graphics_deinitialize(Graphics* graphics) {
    vkDeviceWaitIdle(graphics->device);

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroySemaphore(graphics->device, graphics->image_available_semaphores[i], NULL);
        vkDestroySemaphore(graphics->device, graphics->render_finished_semaphores[i], NULL);
        vkDestroyFence(graphics->device, graphics->in_flight_fences[i], NULL);
    }

    vkDestroyCommandPool(graphics->device, graphics->command_pool, NULL);

    vkDestroyPipeline(graphics->device, graphics->graphics_pipeline, NULL);
    vkDestroyPipelineLayout(graphics->device, graphics->pipeline_layout, NULL);
    vkDestroyRenderPass(graphics->device, graphics->render_pass, NULL);

    vk_cleanup_swapchain(graphics);
    
    vkDestroyDevice(graphics->device, NULL);
    vkDestroySurfaceKHR(graphics->instance, graphics->surface, NULL);

#if defined(ZULK_DEBUG)
    if (graphics->debug_messenger && vkDestroyDebugUtilsMessengerEXT)
        vkDestroyDebugUtilsMessengerEXT(graphics->instance, graphics->debug_messenger, NULL);
#endif

    vkDestroyInstance(graphics->instance, NULL);

    free(graphics);
}

void graphics_draw_frame(Graphics* graphics) {
    vkWaitForFences(graphics->device, 1, &graphics->in_flight_fences[graphics->current_frame], VK_TRUE, UINT64_MAX);

    u32 img_index;
    VkResult res = vkAcquireNextImageKHR(graphics->device, graphics->swapchain, UINT64_MAX, graphics->image_available_semaphores[graphics->current_frame], VK_NULL_HANDLE, &img_index);

    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        vk_recreate_swapchain(graphics);
        return;
    } else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        ERR_CHECK(res, "swapchain acquirement failure");
    }

    vkResetFences(graphics->device, 1, &graphics->in_flight_fences[graphics->current_frame]);
    vkResetCommandBuffer(graphics->command_buffers[graphics->current_frame], 0);
    vk_record_command_buffer(graphics, graphics->command_buffers[graphics->current_frame], img_index);

    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = (VkSemaphore[]){ graphics->image_available_semaphores[graphics->current_frame] },
        .pWaitDstStageMask = (VkPipelineStageFlags[]){ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
        .commandBufferCount = 1,
        .pCommandBuffers = &graphics->command_buffers[graphics->current_frame],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = (VkSemaphore[]){ graphics->render_finished_semaphores[graphics->current_frame] },
    };

    ERR_CHECK(vkQueueSubmit(graphics->graphics_queue, 1, &submit, graphics->in_flight_fences[graphics->current_frame]), "subm draw cmd buf");

    VkPresentInfoKHR present = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = (VkSemaphore[]){ graphics->render_finished_semaphores[graphics->current_frame] },

        .swapchainCount = 1,
        .pSwapchains = (VkSwapchainKHR[]){ graphics->swapchain },
        .pImageIndices = &img_index,
    };

    res = vkQueuePresentKHR(graphics->present_queue, &present);

    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || graphics->frame_resized_recently) {
        graphics->frame_resized_recently = false;
        vk_recreate_swapchain(graphics);
    } else if (res != VK_SUCCESS) {
        ERR_CHECK(res, "swapchain presentation failure");
    }

    graphics->current_frame += 1;
    graphics->current_frame %= MAX_FRAMES_IN_FLIGHT;
}
