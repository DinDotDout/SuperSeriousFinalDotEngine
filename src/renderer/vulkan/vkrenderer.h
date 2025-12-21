#ifndef DOT_VK_RENDERER
#define DOT_VK_RENDERER

#ifndef NDEBUG
#define DOT_VALIDATION_LAYERS_ENABLE
#define DOT_VK_EXT_DEBUG_UTILS_ENABLE
#endif

typedef struct DOT_RendererBackendDevice{
    VkDevice device;
    VkPhysicalDevice gpu;

    VkQueue graphics_queue;
    u32 graphics_queue_idx;

    VkQueue present_queue;
    u32 present_queue_idx;
}DOT_RendererBackendDevice;

typedef struct DOT_RendererBackendSwapchain{
    VkSwapchainKHR swapchain;
    VkExtent2D     extent;
    VkFormat       image_format;

    VkImage*       images;
    VkImageView*   image_views;
    u32            images_count; // Shared between images and images views
}DOT_RendererBackendSwapchain;

typedef struct DOT_RendererBackendVk{
    DOT_RendererBackendBase      base;
    DOT_RendererBackendDevice    dot_device;
    DOT_RendererBackendSwapchain dot_swapchain;
    VkInstance                   instance;
    VkSurfaceKHR                 surface;
    // Aparantly vk expects a malloc like allocator, which I don't intend on make or using for now
    // so our arenas do not work
    // VkAllocationCallbacks        vk_allocator;
#ifdef DOT_VK_EXT_DEBUG_UTILS_ENABLE
    VkDebugUtilsMessengerEXT     debug_messenger; // WARN: this trip us up if hot reloading different build types
#endif
}DOT_RendererBackendVk;

// We can make this a big struct for all the needed settings
typedef struct DOT_RendererBackendVKSettings{
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
}DOT_RendererBackendVKSettings;

internal DOT_RendererBackendVk* DOT_RendererBackendBase_AsVk(DOT_RendererBackendBase* base);
internal DOT_RendererBackendVk* DOT_RendererBackendVk_Create(Arena* arena);
internal const DOT_RendererBackendVKSettings* DOT_VKSettings();

internal void DOT_RendererBackendVk_InitVulkan(DOT_RendererBackendBase* ctx, DOT_Window* window);
internal void DOT_RendererBackendVk_ShutdownVulkan(DOT_RendererBackendBase* ctx);
#endif
