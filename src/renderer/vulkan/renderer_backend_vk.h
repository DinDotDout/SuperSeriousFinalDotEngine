#ifndef rn_vk_H
#define rn_vk_H

typedef struct RN_VK_Device{
    VkDevice         vk_device;
    VkPhysicalDevice vk_gpu;
    b32              is_integrated_gpu;

    VkQueue graphics_queue;
    u32     graphics_queue_idx;

    VkQueue present_queue;
    u32     present_queue_idx;
}RN_VK_Device;

typedef PoolHandle RN_VK_TextureHandle;
typedef PoolHandle RN_VK_BufferHandle;
typedef PoolHandle RN_VK_SamplerHandle;
typedef PoolHandle RN_VK_ShaderResourceLayoutHandle;

typedef struct RN_VK_Buffer{
    VkBuffer            vk_buffer;
    u64                 vk_size;
    VkBufferUsageFlags  vk_buffer_usage_flags;
    RN_VK_MemoryAlloc      alloc;

    u32                 global_offset;    // Offset into global constant, if dynamic
    // RN_VK_BufferHandle   parent_buffer;
    RN_ResourceUsageKind resource_usage; // here for now;
    DOT_DEBUG_NAME(name, DOT_DEBUG_NAME_LEN);
}RN_VK_Buffer;

enum { DOT_MAX_DESCRIPTOR_SET_LAYOUTS = 16 };

typedef struct RN_VK_LayoutBinding{
    VkDescriptorType type;
    u16              start;
    u16              count;
    String8          name;
}RN_VK_LayoutBinding;

typedef struct RN_VK_ShaderResourceLayout{
    VkDescriptorSetLayoutBinding    *vk_binding;
    VkDescriptorSetLayout           *vk_descriptor_set_layout;

    // VkDescriptorSetLayoutBinding*   vk_binding      = nullptr;
    // DescriptorBinding*              bindings        = nullptr;
    // u16                             num_bindings    = 0;
    // u16                             set_index       = 0;
    //
    // DescriptorSetLayoutHandle       handle;

}RN_VK_ShaderResourceLayout;
   //
typedef struct RN_VK_Pipeline{
    VkPipeline                      vk_pipeline;
    VkPipelineLayout                vk_pipeline_layout;

    VkPipelineBindPoint             vk_bind_point;

    RN_ShaderModuleHandle          shader_state;

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
}RN_VK_Pipeline;

typedef struct RN_VK_Sampler{
    VkSampler            vk_sampler;

    VkFilter             vk_min_filter; // = VK_FILTER_NEAREST
    VkFilter             vk_mag_filter; // = VK_FILTER_NEAREST
    VkSamplerMipmapMode  vk_mipmap_filter; // = VK_SAMPLER_MIPMAP_MODE_NEAREST

    VkSamplerAddressMode vk_address_mode_u; // = VK_SAMPLER_ADDRESS_MODE_REPEAT
    VkSamplerAddressMode vk_address_mode_v; // = VK_SAMPLER_ADDRESS_MODE_REPEAT
    VkSamplerAddressMode vk_address_mode_w; // = VK_SAMPLER_ADDRESS_MODE_REPEAT

    DOT_DEBUG_NAME(name, DOT_DEBUG_NAME_LEN);
}RN_VK_Sampler;

typedef struct RN_VK_Texture{
    VkImage     vk_image;
    VkImageView vk_image_view;
    VkExtent3D  vk_extent3d;

    VkFormat    vk_format; // DOT_TEXTURE_FORMAT to vk
    VkImageLayout vk_image_layout;

    // Store layout here for transitioning from one the other and only provide target layout?
    RN_VK_MemoryAlloc alloc;
    u8 mip_levels;
    // u8 flags; // Needed?

    RN_VK_SamplerHandle sampler;
    DOT_DEBUG_NAME(name, DOT_DEBUG_NAME_LEN);
}RN_VK_Texture;

typedef struct RN_VK_SwapchainImage{
    VkImage     vk_image;
    VkImageView vk_image_view;
    VkSemaphore semaphore_render_complete;
}RN_VK_SwapchainImage;

typedef struct RN_VK_Swapchain{
    VkSwapchainKHR swapchain;
    VkExtent2D     extent;
    VkFormat       image_format;

    SLICE(RN_VK_SwapchainImage) swapchain_images;
    // ARRAY(VkSemaphore,          RENDER_SWAPCHAIN_TEXTURES_MAX) render_complete_semaphores;
    // SLICE(RN_VK_SwapchainTexture) image_datas;
}RN_VK_Swapchain;

enum{
    RENDER_BACKEND_MAX_COMMAND_POOLS = 4,
    RENDER_BACKEND_MAX_COMMAND_BUFFERS_PER_POOL = 4,
};
typedef struct RN_VK_FrameData{
    Arena          *frame_arena;

    // VkCommandBuffer frame_command_buffer; // (jd) NOTE: * parallel recordings in flight
    // VkCommandBuffer immediate_command_buffer;

    RN_VK_TextureHandle     draw_image;

    // Sync
    u32             swapchain_image_idx; // Selected swpachain img for a given frame
    VkSemaphore     semaphore_image_acquired;
    VkFence         render_complete_fence;
    ARRAY(VkCommandPool, RENDER_BACKEND_MAX_COMMAND_POOLS) vk_command_pools;
    ARRAY(VkCommandBuffer, RENDER_BACKEND_MAX_COMMAND_BUFFERS_PER_POOL) vk_command_buffers;
    u32 vk_command_buffers_in_use;
}RN_VK_FrameData;

// (jd) TODO: Move this to renderer and use DOT_TextureHandles and so on

typedef struct RN_VK_ResourceCleanupCtx{
    TreeHeader node;
    ARRAY(RN_VK_TextureHandle, RENDER_RESOURCE_CLEANUP_CTX_TEXTURES)   texture_ids;
    ARRAY(RN_VK_BufferHandle,  RENDER_RESOURCE_CLEANUP_CTX_BUFFERS)    buffer_ids;
    ARRAY(RN_VK_SamplerHandle, RENDER_RESOURCE_CLEANUP_CTX_SAMPLERS)   sampler_ids;
    ARRAY(RN_VK_ShaderResourceLayoutHandle, RENDER_RESOURCE_CLEANUP_CTX_DESCRIPTOR_SET_LAYOUTS)   descriptor_set_layotu_ids;
    SLICE(Arena *) temp_arenas;
}RN_VK_ResourceCleanupCtx;
typedef TREE_POOL(RN_VK_ResourceCleanupCtx) ResourceCleanupListTree;

typedef struct RN_VK_AttachmentOps {
    VkImageLayout initial_layout;
    VkImageLayout final_layout;
    VkAttachmentLoadOp load_op;
    VkAttachmentStoreOp store_op;
    VkClearValue clear_value;
}RN_VK_AttachmentOps;


typedef struct RN_VK_RenderingAttachments {
    RN_VK_TextureHandle color[RN_IMAGE_OUTPUTS_MAX];
    RN_VK_TextureHandle depth_stencil;

    u32 num_color;
    u16 width, height;

    f32 scale_x, scale_y;
    u8 resize;

    DOT_DEBUG_NAME(name, DOT_DEBUG_NAME_LEN);
}RN_VK_RenderingAttachments;

typedef struct RN_VK_RenderPassOutput{
    VkFormat depth_stencil_format;
    VkFormat color_formats[RN_IMAGE_OUTPUTS_MAX];
    u32      color_formats_count;
}RN_VK_RenderPassOutput;

typedef struct RendererBackendVk{
    RendererBackend base;

    RN_VK_Device     device;
    RN_VK_Swapchain  swapchain;
    RN_VK_RenderPassOutput swapchain_output;
    VkInstance      instance;
    VkSurfaceKHR    surface;


    u32 current_frame;
    u32 previous_frame;
    u64 absolute_frame;
    SLICE(RN_VK_FrameData) frame_datas;

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

    RN_VK_MemoryPools memory_pools;
    POOL(RN_VK_Texture)  texture_pool;
    POOL(RN_VK_Buffer)   buffer_pool;
    POOL(RN_VK_Sampler)  sampler_pool;
    // ResourceCleanupListTree resource_cleanup_list_tree;

    // NOTE: vk expects a malloc like allocator, which I don't intend on make or using for now
    // so our push arenas do not work for this :(
    VkAllocationCallbacks    vk_allocator;
    VkDebugUtilsMessengerEXT debug_messenger;
}RendererBackendVk;

#define FN(ret, name, params) internal ret rn_vk_##name params;
RN_BACKEND_FN_LIST
#undef FN

internal RendererBackendVk *rn_vk_create(Arena *arena);
internal RendererBackendVk *rn_as_vk(RendererBackend *base);


// (jd) NOTE: should we allow this and mem copy the resource list?
// If we already know the layout/size and we use a specific arena chunk we can maybe
// work like a pool? tho maybe there is too much memory wasted
internal void renderer_backend_resource_cleanup_list_reparent_at(u32 idx); // reparents children

// Internal API
internal void rn_vk_frame_counters_advance();

internal RN_VK_TextureHandle                rn_vk_texture_create_(const RN_TextureDesc *desc, void *data, String8 debug_name);
internal RN_VK_SamplerHandle                rn_vk_sampler_create_(const RN_SamplerDesc *desc, String8 debug_name);
internal RN_VK_BufferHandle                 rn_vk_buffer_create_(const RN_BufferDesc *create_info, u8 *data, String8 debug_name);
internal RN_VK_ShaderResourceLayoutHandle   rn_vk_shader_resource_layout_create_(RN_ShaderResourceLayout *resource_layout);

internal void                               rn_vk_texture_destroy_h(RN_VK_TextureHandle tex_h);
internal void                               rn_vk_texture_destroy_(RN_VK_Texture *image);
internal void                               rn_vk_buffer_destroy_(RN_VK_Buffer *image);
internal void                               rn_vk_sampler_destroy_(RN_VK_Sampler *buff);

internal RN_VK_FrameData    *rn_vk_frame_data_get_current();

// Should these be also coming from a pool like RN_VK_Resources?
internal RN_ShaderModuleHandle rn_vk_dot_shader_module_from_vk_shader_module(VkShaderModule vk_sm);
internal VkShaderModule         rn_vk_vk_shader_module_from_rn_shader_module(RN_ShaderModuleHandle dot_smh);

#endif // !rn_vk_H
