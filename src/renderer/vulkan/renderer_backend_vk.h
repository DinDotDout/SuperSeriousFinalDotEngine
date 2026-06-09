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

typedef PoolHandle RBVK_TextureHandle;
typedef PoolHandle RBVK_BufferHandle;
typedef PoolHandle RBVK_SamplerHandle;

typedef struct RBVK_Buffer{
    VkBuffer            vk_buffer;
    u64                 vk_size;
    VkBufferUsageFlags  vk_buffer_usage_flags;
    VkMemory_Alloc      alloc;

    u32                 global_offset;    // Offset into global constant, if dynamic
    // RBVK_BufferHandle   parent_buffer;
    RenderTypes_ResourceUsageKind resource_usage; // here for now;
    DOT_DEBUG_NAME(name, DOT_DEBUG_NAME_LEN);
}RBVK_Buffer;

enum { DOT_MAX_DESCRIPTOR_SET_LAYOUTS = 16 };

struct DesciptorSetLayout {

    VkDescriptorSetLayout           vk_descriptor_set_layout;

    // VkDescriptorSetLayoutBinding*   vk_binding      = nullptr;
    // DescriptorBinding*              bindings        = nullptr;
    // u16                             num_bindings    = 0;
    // u16                             set_index       = 0;
    //
    // DescriptorSetLayoutHandle       handle;

}; // struct DesciptorSetLayoutVulkan
   //
typedef struct RBVK_Pipeline{
    VkPipeline                      vk_pipeline;
    VkPipelineLayout                vk_pipeline_layout;

    VkPipelineBindPoint             vk_bind_point;

    DOT_ShaderModuleHandle          shader_state;

    // const DesciptorSetLayout*       descriptor_set_layout[DOT_MAX_DESCRIPTOR_SET_LAYOUTS];
    // DescriptorSetLayoutHandle       descriptor_set_layout_handle[DOT_MAX_DESCRIPTOR_SET_LAYOUTS];
    // u32                             num_active_layouts = 0;
    //
    // DepthStencilCreation            depth_stencil;
    // BlendStateCreation              blend_state;
    // RasterizationCreation           rasterization;
    //
    // PipelineHandle                  handle;
    // bool                            graphics_pipeline = true;
}RBVK_Pipeline;

typedef struct RBVK_Sampler{
    VkSampler            vk_sampler;

    VkFilter             vk_min_filter; // = VK_FILTER_NEAREST
    VkFilter             vk_mag_filter; // = VK_FILTER_NEAREST
    VkSamplerMipmapMode  vk_mipmap_filter; // = VK_SAMPLER_MIPMAP_MODE_NEAREST

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
    // u8 flags; // Needed?

    RBVK_SamplerHandle sampler;
    DOT_DEBUG_NAME(name, DOT_DEBUG_NAME_LEN);
}RBVK_Texture;

typedef struct RBVK_SwapchainTexture{
    VkImage vk_image;
    VkImageView vk_image_view;
    VkSemaphore image_semaphore;
}RBVK_SwapchainTexture;

typedef struct RBVK_Swapchain{
    VkSwapchainKHR swapchain;
    VkExtent2D     extent;
    VkFormat       image_format;

    // ARRAY(RBVK_TextureHandle, RBVK_MAX_SWAPCHAIN_TEXTURES) image_datas;
    // ARRAY(VkSemaphore, RBVK_MAX_SWAPCHAIN_TEXTURES) image_semaphores;
    SLICE(RBVK_SwapchainTexture) image_datas;
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

// (jd) TODO: Move this to renderer and use DOT_TextureHandles and so on
typedef struct RBVK_ResourceCleanupCtx{
    TreeHeader node;
    ARRAY(RBVK_SamplerHandle, RBVK_RESOURCE_CLEANUP_CTX_TEXTURES)   texture_ids;
    ARRAY(RBVK_SamplerHandle, RBVK_RESOURCE_CLEANUP_CTX_BUFFERS)    buffer_ids;
    ARRAY(RBVK_SamplerHandle, RBVK_RESOURCE_CLEANUP_CTX_SAMPLERS)   sampler_ids;
    SLICE(Arena *) temp_arenas;
}RBVK_ResourceCleanupCtx;
typedef TREE_POOL(RBVK_ResourceCleanupCtx) ResourceCleanupListTree;

typedef struct RBVK_AttachmentOps {
    VkImageLayout initial_layout;
    VkImageLayout final_layout;
    VkAttachmentLoadOp load_op;
    VkAttachmentStoreOp store_op;
    VkClearValue clear_value;
}RBVK_AttachmentOps;


typedef struct RBVK_RenderingAttachments {
    RBVK_TextureHandle color[RENDER_TYPES_IMAGE_OUTPUTS_MAX];
    RBVK_TextureHandle depth_stencil;

    u32 num_color;
    u16 width, height;

    f32 scale_x, scale_y;
    u8 resize;

    DOT_DEBUG_NAME(name, DOT_DEBUG_NAME_LEN);
}RBVK_RenderingAttachments;

typedef struct RBVK_RenderPassOutput{
    VkFormat depth_stencil_format;
    VkFormat color_formats[RENDER_TYPES_IMAGE_OUTPUTS_MAX];
}RBVK_RenderPassOutput;

typedef struct RendererBackendVk{
    RendererBackend base;

    RBVK_Device     device;
    RBVK_Swapchain  swapchain;
    RBVK_RenderPassOutput swapchain_output;
    VkInstance      instance;
    VkSurfaceKHR    surface;


    u32 current_frame;
    u32 previous_frame;
    u64 absolute_frame;
    SLICE(RBVK_FrameData) frame_datas;
    RBVK_TextureHandle     draw_image;

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
    ResourceCleanupListTree resource_cleanup_list_tree;

    // NOTE: vk expects a malloc like allocator, which I don't intend on make or using for now
    // so our push arenas do not work for this :(
    VkAllocationCallbacks    vk_allocator;
    VkDebugUtilsMessengerEXT debug_messenger;
}RendererBackendVk;

// typedef struct RBVK_Pipeline{
//     VkPipeline pipeline;
// }RBVK_Pipeline;

#define FN(ret, name, params) internal ret renderer_backend_vk_##name params;
RENDERER_BACKEND_FN_LIST
#undef FN

internal void               renderer_backend_vk_merge_render_settings(RendererBackendConfig *backend_config);
internal RendererBackendVk *renderer_backend_vk_create(Arena *arena, RendererBackendConfig *backend_config);
internal RendererBackendVk *renderer_backend_as_vk(RendererBackend *base);

internal void renderer_backend_vk_resource_cleanup_list_push_rbvk_sampler(RBVK_SamplerHandle sampler_id);
internal void renderer_backend_vk_resource_cleanup_list_push_rbvk_texture(PoolHandle texture_id);
internal void renderer_backend_vk_resource_cleanup_list_push_rbvk_buffer(RBVK_BufferHandle buffer_id);

// (jd) NOTE: should we allow this and mem copy the resource list?
// If we already know the layout/size and we use a specific arena chunk we can maybe
// work like a pool? tho maybe there is too much memory wasted
internal void renderer_backend_resource_cleanup_list_reparent_at(u32 idx); // reparents children

// Internal API
internal void               rbvk_frame_counters_advance();

internal RBVK_TextureHandle rbvk_texture_create(const RenderTypes_TextureDesc *desc, void *data, String8 debug_name);
internal RBVK_SamplerHandle rbvk_sampler_create(const RenderTypes_SamplerDesc *desc, String8 debug_name);
internal RBVK_BufferHandle  rbvk_buffer_create(const RenderTypes_BufferDesc *create_info, u8 *data, String8 debug_name);

internal void               rbvk_texture_destroy(RBVK_Texture *image);
internal RBVK_Buffer        rbvk_buffer_create2(VkDeviceSize size, VkMemory_PoolsKind pool_kind, String8 name);
internal RBVK_FrameData    *rbvk_frame_data_get_current();

// Should these be also coming from a pool like RBVK_Resources?
internal DOT_ShaderModuleHandle rbvk_dot_shader_module_from_vk_shader_module(VkShaderModule vk_sm);
internal VkShaderModule         rbvk_vk_shader_module_render_types_shader_module(DOT_ShaderModuleHandle dot_smh);

#endif // !RENDERER_BACKEND_VK_H
