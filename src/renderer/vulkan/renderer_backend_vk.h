#ifndef VK_RENDERER
#define VK_RENDERER

#ifdef NDEBUG
#define VALIDATION_LAYERS_ENABLE 0
#define VK_EXT_DEBUG_UTILS_ENABLE 0
#else
#define VALIDATION_LAYERS_ENABLE 1
#define VK_EXT_DEBUG_UTILS_ENABLE 1
#endif

typedef struct RendererBackendVk_Settings{
    struct InstanceVkSettings{
        const String8 *instance_extension_names;
        const usize  instance_extension_count;
    }instance_settings;
    struct DeviceVkSettings{
        const String8 *device_extension_names;
        const usize    device_extension_count;
    }device_settings;
    struct LayerVkSettings{
        const String8 *layer_names;
        const usize    layer_count;
    }layer_settings;
    struct SwapchainVkSettings{
        VkFormat         preferred_format;
        VkColorSpaceKHR  preferred_colorspace;
        VkPresentModeKHR preferred_present_mode;
    }swapchain_settings;
    struct FrameSettings{
        u8 frame_overlap;
    }frame_settings;
}RendererBackendVk_Settings;

typedef struct RendererBackendVk_Device{
    VkDevice         device;
    VkPhysicalDevice gpu;

    VkQueue graphics_queue;
    u32     graphics_queue_idx;

    VkQueue present_queue;
    u32     present_queue_idx;
}RendererBackendVk_Device;

typedef struct RendererBackendVk_Swapchain{
    VkSwapchainKHR swapchain;
    VkExtent2D     extent;
    VkFormat       image_format;

    VkImage*       images;
    VkImageView*   image_views;
    u32            images_count; // Shared between images and images views
}RendererBackendVk_Swapchain;


#define RENDERER_BACKEND_VK_FRAME_OVERLAP 2
typedef struct RendererBackendVk_FrameData{
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
}RendererBackendVk_FrameData;

typedef struct RendererBackendVk{
    RendererBackend               base;
    RendererBackendVk_Device      device;
    RendererBackendVk_Swapchain   swapchain;
    VkInstance                    instance;
    VkSurfaceKHR                  surface;

    u8                            frame_count;
    RendererBackendVk_FrameData*  frames;

    // NOTE: Aparantly vk expects a malloc like allocator, which I don't intend on make or using for now
    // so our push arenas do not work
    VkAllocationCallbacks    vk_allocator; // Unused for now
    VkDebugUtilsMessengerEXT debug_messenger;
}RendererBackendVk;


internal RendererBackendVk* renderer_backend_as_vk(RendererBackend* base);
internal RendererBackendVk* renderer_backend_vk_create(Arena* arena);
internal const RendererBackendVk_Settings* renderer_backend_vk_settings();

internal void renderer_backend_vk_init(RendererBackend* ctx, DOT_Window* window);
internal void renderer_backend_vk_shutdown(RendererBackend* ctx);
#endif
