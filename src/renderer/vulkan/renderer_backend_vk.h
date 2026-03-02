#ifndef RENDERER_BACKEND_VK_H
#define RENDERER_BACKEND_VK_H

#ifdef NDEBUG
#define VALIDATION_LAYERS_ENABLE 0
#define VK_EXT_DEBUG_UTILS_ENABLE 0
#else
#define VALIDATION_LAYERS_ENABLE 1
#define VK_EXT_DEBUG_UTILS_ENABLE 1
#endif

typedef struct LayerVkSettings RBVK_LayerSettings;
typedef struct RBVK_InstanceSettings RBVK_InstanceSettings;
typedef struct RBVK_SwapchainSettings RBVK_SwapchainSettings;
typedef struct RBVK_DeviceSettings RBVK_DeviceSettings;
typedef struct RBVK_Settings{
    struct RBVK_InstanceSettings{
        const String8 *instance_extension_names;
        const usize  instance_extension_count;
        String8 application_name;
        u32 application_version;
        String8 engine_name;
        u32 engine_version;
        u32 api_version;
    }instance_settings;
    struct RBVK_DeviceSettings{
        const String8 *device_extension_names;
        const usize    device_extension_count;
        const void*    device_features;
    }device_settings;
    struct LayerVkSettings{
        const String8 *layer_names;
        const usize    layer_count;
    }layer_settings;
    struct RBVK_SwapchainSettings{
        const VkFormat         preferred_format;
        const VkColorSpaceKHR  preferred_colorspace;
        VkPresentModeKHR preferred_present_mode; // Set from renderer settings
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

typedef struct RBVK_SwapchainImageData{
    VkImage image;
    VkImageView image_view;
    VkSemaphore image_semaphore;
}RBVK_SwapchainImageData;

typedef struct RBVK_SwapchainImageDatas RBVK_SwapchainImageDatas;
typedef struct RBVK_Swapchain{
    VkSwapchainKHR swapchain;
    VkExtent2D     extent;
    VkFormat       image_format;

    struct RBVK_SwapchainImageDatas{
        RBVK_SwapchainImageData *data;
        u32 count;
    }image_datas;

    // array(RBVK_SwapchainImageData) image_datas;
    // u32 image_datas_count; // Shared between images, images views and semaphroe
}RBVK_Swapchain;

typedef struct RBVK_FrameData{
    VkCommandPool   command_pool;
    VkCommandBuffer frame_command_buffer;
    VkSemaphore     acquire_semaphore;
    VkFence         render_fence;
    Arena          *frame_arena;
    u32             swapchain_image_idx;
}RBVK_FrameData;

typedef struct RBVK_FrameDatas RBVK_FrameDatas;
typedef struct RendererBackendVk{
    RendererBackend base;
    RBVK_Device     device;
    RBVK_Swapchain  swapchain;
    VkInstance      instance;
    VkSurfaceKHR    surface;

    struct RBVK_FrameDatas{
        RBVK_FrameData *data;
        u8 count;
    }frame_datas2;
    u8                    frame_data_count;
    array(RBVK_FrameData) frame_datas;

    RBVK_Image      draw_image;
    // NOTE: Splitting this from actual draw_image so that we can draw regions?
    VkExtent2D      draw_extent;

    // TODO: Clean up
    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout bindless_layout;
    VkDescriptorSetLayout compute_layout;
    VkDescriptorSet descriptor_sets;

    // NOTE: Should probably cache this and destroy all on exit
    VkPipeline gradient_pipeline;
    VkPipelineLayout gradient_pipeline_layout;

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

internal RendererBackendVk* renderer_backend_vk_create(Arena *arena, RendererBackendConfig *backend_config);
internal void renderer_backend_vk_init(DOT_Window *window);
internal void renderer_backend_vk_shutdown();
internal void renderer_backend_vk_clear_bg(u8 current_frame, vec3 color);
internal void renderer_backend_vk_begin_frame(u8 current_frame);
internal void renderer_backend_vk_end_frame(u8 current_frame);

internal RBVK_Settings* renderer_backend_vk_settings();
internal void renderer_backend_vk_merge_settings(RendererBackendConfig *backend_config);

internal DOT_ShaderModuleHandle renderer_backend_vk_load_shader_from_file_buffer(DOT_FileBuffer file_buffer);
internal void renderer_backend_vk_unload_shader_module(DOT_ShaderModuleHandle shader_module);

// Internal API
internal RendererBackendVk* renderer_backend_as_vk(RendererBackend *base);
internal RBVK_Image         rbvk_create_image(RendererBackendVk *ctx, VkImageCreateInfo *image_info);
internal void               rbvk_destroy_image(RendererBackendVk *ctx, RBVK_Image *image);
internal DOT_ShaderModuleHandle rbvk_dot_shader_module_from_vk_shader_module(VkShaderModule vk_sm);
internal VkShaderModule         rbvk_vk_shader_module_from_dot_shader_module(DOT_ShaderModuleHandle dot_smh);

#endif // !RENDERER_BACKEND_VK_H
