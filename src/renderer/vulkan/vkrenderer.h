#ifndef VK_RENDERER
#define VK_RENDERER

#ifdef NDEBUG
#define VALIDATION_LAYERS_ENABLE 0
#define VK_EXT_DEBUG_UTILS_ENABLE 0
#else
#define VALIDATION_LAYERS_ENABLE 1
#define VK_EXT_DEBUG_UTILS_ENABLE 1
#endif

typedef struct RendererBackendDevice{
    VkDevice         device;
    VkPhysicalDevice gpu;

    VkQueue graphics_queue;
    u32     graphics_queue_idx;

    VkQueue present_queue;
    u32     present_queue_idx;
}RendererBackendDevice;

typedef struct RendererBackendSwapchain{
    VkSwapchainKHR swapchain;
    VkExtent2D     extent;
    VkFormat       image_format;

    VkImage*       images;
    VkImageView*   image_views;
    u32            images_count; // Shared between images and images views
}RendererBackendSwapchain;

typedef struct RendererBackendVk{
    RendererBackendBase      base;
    RendererBackendDevice    device;
    RendererBackendSwapchain swapchain;
    VkInstance               instance;
    VkSurfaceKHR             surface;
    // Aparantly vk expects a malloc like allocator, which I don't intend on make or using for now
    // so our arenas do not work
    // VkAllocationCallbacks        vk_allocator;
// #ifdef VK_EXT_DEBUG_UTILS_ENABLE
    VkDebugUtilsMessengerEXT debug_messenger;
// #endif
}RendererBackendVk;

typedef struct RendererBackendVKSettings{
    struct InstanceVkSettings{
        const char **instance_extension_names;
        const usize  instance_extension_count;
    }instance_settings;
    struct DeviceVkSettings{
        const char **device_extension_names;
        const usize  device_extension_count;
    }device_settings;
    struct LayerVkSettings{
        const char **layer_names;
        const usize  layer_count;
    }layer_settings;
    struct SwapchainVkSettings{
        VkFormat         preferred_format;
        VkColorSpaceKHR  preferred_colorspace;
        VkPresentModeKHR preferred_present_mode;
    }swapchain_settings;
}RendererBackendVKSettings;

internal RendererBackendVk* RendererBackendBase_AsVk(RendererBackendBase* base);
internal RendererBackendVk* RendererBackendVk_Create(Arena* arena);
internal const RendererBackendVKSettings* RendererBackendVk_Settings();

internal void RendererBackendVk_Init(RendererBackendBase* ctx, DOT_Window* window);
internal void RendererBackendVk_Shutdown(RendererBackendBase* ctx);
#endif
