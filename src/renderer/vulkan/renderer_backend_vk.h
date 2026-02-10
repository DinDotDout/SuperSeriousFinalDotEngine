#ifndef RENDERER_BACKEND_VK_H
#define RENDERER_BACKEND_VK_H

#ifdef NDEBUG
#define VALIDATION_LAYERS_ENABLE 0
#define VK_EXT_DEBUG_UTILS_ENABLE 0
#else
#define VALIDATION_LAYERS_ENABLE 1
#define VK_EXT_DEBUG_UTILS_ENABLE 1
#endif

typedef struct RBVK_Settings{
    struct InstanceVkSettings{
        const String8 *instance_extension_names;
        const usize  instance_extension_count;
    }instance_settings;
    struct DeviceVkSettings{
        const String8 *device_extension_names;
        const usize    device_extension_count;
        const void*    device_features;
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
}RBVK_Settings;

typedef struct RBVK_Device{
    VkDevice         device;
    VkPhysicalDevice gpu;

    VkQueue graphics_queue;
    u32     graphics_queue_idx;

    VkQueue present_queue;
    u32     present_queue_idx;
}RBVK_Device;

typedef struct RBVK_Swapchain{
    VkSwapchainKHR swapchain;
    VkExtent2D     extent;
    VkFormat       image_format;

    VkImage       *images;
    VkImageView   *image_views;
    u32            images_count; // Shared between images and images views
}RBVK_Swapchain;

typedef struct RBVK_FrameData{
    VkCommandPool   command_pool;
    VkCommandBuffer frame_command_buffer;
    VkSemaphore     swapchain_semaphore, render_semaphore;
    VkFence         render_fence;
    Arena          *frame_arena;
}RBVK_FrameData;

typedef struct RendererBackendVk{
    RendererBackend base;
    RBVK_Device     device;
    RBVK_Swapchain  swapchain;
    VkInstance      instance;
    VkSurfaceKHR    surface;

    u8              frame_data_count;
    RBVK_FrameData *frame_datas;

    // NOTE: vk expects a malloc like allocator, which I don't intend on make or using for now
    // so our push arenas do not work for this :(
    VkAllocationCallbacks    vk_allocator;
    VkDebugUtilsMessengerEXT debug_messenger;
}RendererBackendVk;


internal RendererBackendVk* renderer_backend_as_vk(RendererBackend* base);
internal RendererBackendVk* renderer_backend_vk_create(Arena* arena);
internal const RBVK_Settings* renderer_backend_vk_settings();

internal void renderer_backend_vk_init(RendererBackend* base_ctx, DOT_Window* window);
internal void renderer_backend_vk_shutdown(RendererBackend* base_ctx);
internal void renderer_backend_vk_draw(RendererBackend* base_ctx, u8 current_frame, u64 frame);

#endif // !RENDERER_BACKEND_VK_H
