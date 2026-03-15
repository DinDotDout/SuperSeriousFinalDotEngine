#ifndef RENDERER_BACKEND_VK_H
#define RENDERER_BACKEND_VK_H

#ifdef NDEBUG
#define VALIDATION_LAYERS_ENABLE 0
#define VK_EXT_DEBUG_UTILS_ENABLE 0
#else
#define VALIDATION_LAYERS_ENABLE 1
#define VK_EXT_DEBUG_UTILS_ENABLE 1
#endif

typedef struct RBVK_Settings RBVK_Settings;
struct RBVK_Settings{
    struct RBVK_InstanceSettings{
        struct RBVK_InstanceExtensions{
            String8 *data;
            usize    count;
        }instance_extensions;

        String8 application_name;
        u32 application_version;
        String8 engine_name;
        u32 engine_version;
        u32 api_version;
    }instance_settings;

    struct RBVK_DeviceSettings{
        struct RBVK_DeviceExtensions{
            String8 *data;
            usize    count;
        }device_extensions;
        const String8 *device_extension_names;
        const usize    device_extension_count;
        const void    *device_features;
    }device_settings;

    struct RBVK_ValidationLayers{
        String8 *data;
        usize    count;
    }validation_layers;

    struct RBVK_SwapchainSettings{
        const VkFormat         preferred_format;
        const VkColorSpaceKHR  preferred_colorspace;
        VkPresentModeKHR preferred_present_mode; // Set from renderer settings
    }swapchain_settings;

    struct FrameSettings{
        u8 frame_overlap;
    }frame_settings;

} global VK_SETTINGS = {
    .instance_settings = {
        .application_name    = String8Lit("dot_engine"),
        .application_version = VK_MAKE_VERSION(1, 0, 0),

        .engine_name         = String8Lit("dot_engine"),
        .engine_version      = VK_MAKE_VERSION(1, 0, 0),

        .api_version         = VK_API_VERSION_1_4,

        .instance_extensions = SLICE(String8, {
            String8Lit(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME),
            String8Lit(VK_KHR_SURFACE_EXTENSION_NAME),
#ifdef VK_EXT_DEBUG_UTILS_ENABLE
            String8Lit(VK_EXT_DEBUG_UTILS_EXTENSION_NAME),
#endif
            String8Lit(DOT_VK_SURFACE),
        }),
    },
    .validation_layers = SLICE(String8, {
#ifdef VALIDATION_LAYERS_ENABLE
        String8Lit("VK_LAYER_KHRONOS_validation"),
#endif
    }),
    .device_settings = {
        .device_extensions = SLICE(String8, {
            String8Lit(VK_KHR_SWAPCHAIN_EXTENSION_NAME),
            String8Lit(VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME),
            String8Lit(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME),
            String8Lit(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME),
            String8Lit(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME),
        }),
        .device_features = 
            &(VkPhysicalDeviceVulkan12Features){
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
                .bufferDeviceAddress = true,
                .descriptorIndexing = true,
                .descriptorBindingPartiallyBound = true,
                .descriptorBindingVariableDescriptorCount = true,
                .runtimeDescriptorArray = true,
        .pNext =
            &(VkPhysicalDeviceVulkan13Features){
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
                .synchronization2 = VK_TRUE,
                .dynamicRendering = true,
        .pNext =
            &(VkPhysicalDeviceDescriptorBufferFeaturesEXT){
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
                .descriptorBuffer = VK_TRUE,
        .pNext =
            &(VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT){
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT,
                .graphicsPipelineLibrary = VK_TRUE,
        .pNext = NULL,
            }}}},
    },
    .swapchain_settings = {
        .preferred_format       = VK_FORMAT_B8G8R8A8_SRGB,
        .preferred_colorspace   = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    },
};

typedef struct RBVK_Device{
    VkDevice         device;
    VkPhysicalDevice gpu;
    b32              is_integrated_gpu;


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
    Arena          *frame_arena;
    VkCommandPool   command_pool;
    VkCommandBuffer frame_command_buffer;
    VkSemaphore     acquire_semaphore;
    VkFence         render_fence;
    u32             swapchain_image_idx; // Selected swpachain img for a given frame
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
    VkDescriptorSet descriptor_sets[2];
    u32             descriptor_set_count;

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

typedef struct RBVK_Pipeline{
    VkPipeline pipeline;
}RBVK_Pipeline;

#define FN(ret, name, args) internal ret renderer_backend_vk_##name args;
RENDERER_BACKEND_FN_LIST
#undef FN

internal RendererBackendVk* renderer_backend_vk_create(Arena *arena, RendererBackendConfig *backend_config);
internal void renderer_backend_vk_merge_settings(RendererBackendConfig *backend_config);

// Internal API
internal RendererBackendVk* renderer_backend_as_vk(RendererBackend *base);
internal RBVK_Image         rbvk_create_image(RendererBackendVk *ctx, VkImageCreateInfo *image_info);
internal void               rbvk_destroy_image(RendererBackendVk *ctx, RBVK_Image *image);
internal DOT_ShaderModuleHandle rbvk_dot_shader_module_from_vk_shader_module(VkShaderModule vk_sm);
internal VkShaderModule         rbvk_vk_shader_module_from_dot_shader_module(DOT_ShaderModuleHandle dot_smh);

#endif // !RENDERER_BACKEND_VK_H
