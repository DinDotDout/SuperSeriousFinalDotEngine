#ifndef RENDERER_BACKEND_VK_H
#define RENDERER_BACKEND_VK_H

typedef struct RBVK_Device{
    VkDevice         vk_device;
    VkPhysicalDevice vk_gpu;
    b32              is_integrated_gpu;

    VkQueue graphics_queue;
    u32     graphics_queue_idx;

    VkQueue present_queue;
    u32     present_queue_idx;
}RBVK_Device;

typedef struct RBVK_Buffer{
    VkBuffer        vk_buffer;
    VkDeviceSize    size;

    VkMemory_Alloc alloc;
    DOT_DEBUG_NAME(name, DOT_DEBUG_NAME_LEN);
}RBVK_Buffer;

typedef struct RBVK_Sampler{
    VkSampler            vk_sampler;

    VkFilter             vk_min_filter; // = VK_FILTER_NEAREST
    VkFilter             vk_mag_filter; // = VK_FILTER_NEAREST
    VkSamplerMipmapMode  vk_mip_filter; // = VK_SAMPLER_MIPMAP_MODE_NEAREST

    VkSamplerAddressMode vk_address_mode_u; // = VK_SAMPLER_ADDRESS_MODE_REPEAT
    VkSamplerAddressMode vk_address_mode_v; // = VK_SAMPLER_ADDRESS_MODE_REPEAT
    VkSamplerAddressMode vk_address_mode_w; // = VK_SAMPLER_ADDRESS_MODE_REPEAT

    DOT_DEBUG_NAME(name, DOT_DEBUG_NAME_LEN);
}RBVK_Sampler;

typedef struct RBVK_Texture{
    VkImage     vk_image;
    VkImageView vk_image_view;
    VkExtent3D  vk_extent3d;

    VkFormat    vk_format; // DOT_TEXTURE_FORMAT to vk
    VkImageLayout vk_image_layout;

    // Store layout here for transitioning from one the other and only provide target layout?
    VkMemory_Alloc alloc;
    u8 mip_levels;
    u8 flags; // Needed?

    RBVK_Sampler *sampler;

    DOT_DEBUG_NAME(name, DOT_DEBUG_NAME_LEN);
}RBVK_Texture;

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
    VkCommandBuffer frame_command_buffer; // (jd) NOTE: * parallel recordings in flight
    VkCommandBuffer immediate_command_buffer;
    VkSemaphore     acquire_semaphore;
    VkFence         render_fence;
    u32             swapchain_image_idx; // Selected swpachain img for a given frame
}RBVK_FrameData;


typedef struct ResourceCleanupList{
    // ResourceCleanupList children[10];
    // u32 *children_count;
    // TempArena temp;
    u32 texture_id_count;
    PoolHandle *texture_ids; // RBVK_Texture

    u32 buffer_id_count;
    PoolHandle *buffer_ids; // RBVK_Buffer

    u32 sampler_id_count;
    PoolHandle *sampler_ids; // RBVK_Sampler

    TreeHeader node;
}ResourceCleanupList;

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
    u32 current_frame;
    u32 previous_frame;
    u64 absolute_frame;

    RBVK_Texture      draw_image;
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

    RBVKMemory_Pools memory_pools;
    POOL(RBVK_Texture)  texture_pool;
    POOL(RBVK_Buffer)   buffer_pool;
    POOL(RBVK_Sampler)  sampler_pool;
    TREE_POOL(ResourceCleanupList) resource_cleanup_list;

    // NOTE: vk expects a malloc like allocator, which I don't intend on make or using for now
    // so our push arenas do not work for this :(
    VkAllocationCallbacks    vk_allocator;
    VkDebugUtilsMessengerEXT debug_messenger;
}RendererBackendVk;

typedef struct RBVK_Pipeline{
    VkPipeline pipeline;
}RBVK_Pipeline;

#define FN(ret, name, params) internal ret renderer_backend_vk_##name params;
RENDERER_BACKEND_FN_LIST
#undef FN

internal void               renderer_backend_vk_merge_render_settings(RendererBackendConfig *backend_config);
internal RendererBackendVk* renderer_backend_vk_create(Arena *arena, RendererBackendConfig *backend_config);
internal RendererBackendVk* renderer_backend_as_vk(RendererBackend *base);

// (jd) NOTE: should we allow this and mem copy the resource list?
// If we already know the layout/size and we use a specific arena chunk we can maybe
// work like a pool? tho maybe there is too much memory wasted
internal void renderer_backend_resource_cleanup_list_reparent_at(u32 idx); // reparents children

// Internal API
internal void               rbvk_frame_counters_advance();

internal RBVK_Texture       rbvk_texture_create(VkImageCreateInfo *image_info);
internal void               rbvk_texture_destroy(RBVK_Texture *image);
internal RBVK_Buffer        rbvk_buffer_create(VkDeviceSize size, VkMemory_PoolsKind pool_kind, String8 name);

// Should these be also coming from a pool like RBVK_Resources?
internal DOT_ShaderModuleHandle rbvk_dot_shader_module_from_vk_shader_module(VkShaderModule vk_sm);
internal VkShaderModule         rbvk_vk_shader_module_from_dot_shader_module(DOT_ShaderModuleHandle dot_smh);

#endif // !RENDERER_BACKEND_VK_H
