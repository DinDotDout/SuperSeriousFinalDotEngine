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
    b8               is_integrated_gpu;


    VkQueue graphics_queue;
    u32     graphics_queue_idx;

    VkQueue present_queue;
    u32     present_queue_idx;
}RBVK_Device;

typedef struct RBVK_Image{
    VkImage     image;
    VkImageView image_view;
    VkFormat    image_format;
    VkExtent3D  extent;
    VkMemory_Alloc alloc;
}RBVK_Image;

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
    u32             swapchain_image_idx;
}RBVK_FrameData;

typedef struct RendererBackendVk{
    RendererBackend base;
    RBVK_Device     device;
    RBVK_Swapchain  swapchain;
    VkInstance      instance;
    VkSurfaceKHR    surface;

    u8              frame_data_count;
    RBVK_FrameData *frame_datas;

    RBVK_Image      draw_image;
    // NOTE: Splitting this from actual draw_image so that we can draw regions?
    VkExtent2D      draw_extent;

    // TODO: Clean up
    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout bindless_layout;
    VkDescriptorSetLayout compute_layout;
    VkDescriptorSet descriptor_sets;
    // VkDescriptorSet bindles_set;
    // VkDescriptorSet compute_set;

    VkMemory_Pools memory_pools;
    // NOTE: vk expects a malloc like allocator, which I don't intend on make or using for now
    // so our push arenas do not work for this :(
    VkAllocationCallbacks    vk_allocator;
    VkDebugUtilsMessengerEXT debug_messenger;
}RendererBackendVk;

typedef struct RBVK_FileBuffer{
    u32 *buff;
    usize size;
}RBVK_FileBuffer;

// Outfacing renderer API
internal const RBVK_Settings* renderer_backend_vk_settings();

internal RendererBackendVk* renderer_backend_vk_create(Arena *arena);
internal void renderer_backend_vk_init(RendererBackend *base_ctx, DOT_Window *window);
internal void renderer_backend_vk_shutdown(RendererBackend* base_ctx);
internal void renderer_backend_vk_clear_bg(RendererBackend *base_ctx, u8 current_frame, vec3 color);
// internal void renderer_backend_vk_draw(RendererBackend *base_ctx, u8 current_frame, u64 frame);

internal void renderer_backend_vk_begin_frame(RendererBackend *base_ctx, u8 current_frame);
internal void renderer_backend_vk_end_frame(RendererBackend *base_ctx, u8 current_frame);

internal DOT_ShaderModuleHandle renderer_backend_vk_load_shader_from_file_buffer(RendererBackend *base_ctx, FileBuffer file_buffer);

// Internal API
internal RendererBackendVk* renderer_backend_as_vk(RendererBackend *base);
internal RBVK_Image rbvk_create_image(RendererBackendVk *ctx, VkImageCreateInfo *image_info);
internal void rbvk_destroy_image(RendererBackendVk *ctx, RBVK_Image *image);

#endif // !RENDERER_BACKEND_VK_H
