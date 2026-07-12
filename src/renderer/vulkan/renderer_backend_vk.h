#ifndef RN_VK_H
#define RN_VK_H

typedef struct RN_VK_Device{
    VkDevice         vk_device;
    VkPhysicalDevice vk_gpu;
    b32              is_integrated_gpu;

    u32     graphics_queue_idx;
    VkQueue graphics_queue;

    u32     present_queue_idx;
    VkQueue present_queue;
}RN_VK_Device;

typedef PoolHandle RN_VK_TextureHandle;
typedef PoolHandle RN_VK_BufferHandle;
typedef PoolHandle RN_VK_SamplerHandle;
typedef PoolHandle RN_VK_ShaderResourceLayoutHandle;
typedef PoolHandle RN_VK_ShaderResourceHandle;
typedef PoolHandle RN_VK_PipelineHandle;
typedef PoolHandle RN_VK_ShaderStateHandle;

RN_VK_TextureHandle *rn_vk_g_default_texture = {0};
RN_VK_SamplerHandle *rn_vk_g_default_sampler = {0};
RN_VK_BufferHandle  *rn_vk_g_default_buffer  = {0};
RN_VK_ShaderResourceLayoutHandle *rn_vk_g_default_shader_resource_layout = {0};

typedef struct RN_VK_Sampler{
    VkSampler            vk_sampler;

    VkFilter             vk_min_filter;
    VkFilter             vk_mag_filter;
    VkSamplerMipmapMode  vk_mipmap_filter;

    VkSamplerAddressMode vk_address_mode_u;
    VkSamplerAddressMode vk_address_mode_v;
    VkSamplerAddressMode vk_address_mode_w;
    DOT_DebugName16(debug_name);
}RN_VK_Sampler;

typedef struct RN_VK_Texture{
    VkImage     vk_image;
    VkImageView vk_image_view;
    VkExtent3D  vk_extent3d;

    VkFormat    vk_format;
    VkImageLayout vk_image_layout;

    RN_VK_GpuAllocHandle alloc;
    u8 mip_levels;

    RN_VK_SamplerHandle sampler;
    DOT_DebugName16(debug_name);
}RN_VK_Texture;

typedef struct RN_VK_Buffer{
    VkBuffer            vk_buffer;
    u64                 vk_size;
    VkBufferUsageFlags  vk_buffer_usage_flags;

    RN_VK_GpuAllocHandle alloc;
    u64                  offset;

    RN_ResourceUsageKind resource_usage; // here for now;
    DOT_DebugName16(debug_name);
}RN_VK_Buffer;

typedef struct RN_VK_ShaderResourceLayout{
    VkDescriptorSetLayout           vk_descriptor_set_layout;

    u16 binding_count;
    VkDescriptorSetLayoutBinding    *vk_bindings;
    RN_ShaderResourceBinding        *bindings; // Not even sure we store this here

    u16 resource_layout_idx;
    RN_VK_ShaderResourceLayoutHandle handle;
}RN_VK_ShaderResourceLayout;

typedef struct RN_VK_ShaderResource{
    VkDescriptorSet vk_descriptor_set;
}RN_VK_ShaderResource;

typedef struct RN_VK_ShaderState{
    u32                             shader_stage_info_count;
    VkPipelineShaderStageCreateInfo shader_stage_info[RN_SHADER_STAGES_MAX];
    bool is_graphics_pipeline;

    DOT_DebugName16(debug_name);
}RN_VK_ShaderState;

typedef struct RN_VK_RenderPassOutput{
    VkFormat depth_stencil_format;
    VkFormat color_formats[RN_IMAGE_OUTPUTS_MAX];
    u32      color_formats_count;
}RN_VK_RenderPassOutput;

typedef struct RN_VK_Pipeline{
    VkPipeline           vk_pipeline;
    VkPipelineLayout     vk_pipeline_layout;

    VkPipelineBindPoint  vk_bind_point;
    RN_VK_ShaderStateHandle shader_state_handle;

    u32                  shader_resource_layout_count;
    RN_VK_ShaderResourceLayoutHandle shader_resource_layouts[RN_SHADER_RESOURCE_LAYOUT_MAX];
    RN_VK_RenderPassOutput renderpass_output;
} RN_VK_Pipeline;

typedef struct RN_VK_SwapchainImage{
    VkImage     vk_image;
    VkImageView vk_image_view;
    VkSemaphore semaphore_render_complete;
}RN_VK_SwapchainImage;

typedef struct RN_VK_Swapchain{
    VkSwapchainKHR swapchain;
    VkExtent2D     extent;
    VkFormat       image_format;

    u32 swapchain_image_count;
    RN_VK_SwapchainImage *swapchain_images;
}RN_VK_Swapchain;

enum{
    RENDER_BACKEND_MAX_COMMAND_POOLS = 4,
    RENDER_BACKEND_MAX_COMMAND_BUFFERS_PER_POOL = 4,
};
typedef struct RN_VK_FrameData{
    Arena          *frame_arena;

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

// vkCmdBeginRendering
typedef struct RN_VK_AttachmentOps {
    // VkImageLayout initial_layout;
    // VkImageLayout final_layout;
    VkAttachmentLoadOp load_op;
    VkAttachmentStoreOp store_op;
    VkClearValue clear_value;
}RN_VK_AttachmentOps;

// vkCmdBeginRendering
typedef struct RN_VK_RenderingAttachments{
    u32 color_texture_count;
    RN_VK_TextureHandle color_textures[RN_IMAGE_OUTPUTS_MAX];
    RN_VK_TextureHandle depth_stencil_texture;

    u16 width, height;

    f32 scale_x, scale_y;
    u8 resize;

    DOT_DebugName16(debug_name);
}RN_VK_RenderingAttachments;


typedef struct RN_VK_BackendCtx{
    RN_BackendCtx base;

    RN_VK_Device     device;
    RN_VK_Swapchain  swapchain;
    RN_VK_RenderPassOutput swapchain_output;
    VkInstance      instance;
    VkSurfaceKHR    surface;

    u32 current_frame;
    u32 previous_frame;
    u64 absolute_frame;

    u32 frame_data_count;
    RN_VK_FrameData *frame_datas;

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

    RN_VK_Memory memory_pools;

    // (jd) NOTE: Maybe I should just use a free list. Tho clearing a scope might be slow
    POOL(RN_VK_Texture)  texture_pool;
    POOL(RN_VK_Buffer)   buffer_pool;
    POOL(RN_VK_Sampler)  sampler_pool;
    POOL(RN_VK_ShaderResourceLayout) shader_resource_layout_pool;
    POOL(RN_VK_ShaderResource)  shader_resource_pool;
    POOL(RN_VK_ShaderState) shader_state_pool;
    POOL(RN_VK_Pipeline) pipeline_pool;

    // NOTE: vk expects a malloc like allocator, which I don't intend on make or using for now
    // so our push arenas do not work for this :(
    VkAllocationCallbacks    vk_allocator;
    VkDebugUtilsMessengerEXT debug_messenger;
}RN_VK_BackendCtx;

#define FN(ret, name, params) internal ret rn_vk_##name params;
RN_BACKEND_FN_LIST
#undef FN

internal RN_VK_BackendCtx *rn_vk_create(Arena *arena);
internal RN_VK_BackendCtx *rn_as_vk(RN_BackendCtx *base);


// (jd) NOTE: should we allow this and mem copy the resource list?
// If we already know the layout/size and we use a specific arena chunk we can maybe
// work like a pool? tho maybe there is too much memory wasted
internal void renderer_backend_resource_cleanup_list_reparent_at(u32 idx); // reparents children

// Internal API
internal void rn_vk_frame_counters_advance();

// TODO: Not sure if missing desc should just mean returning a defaulted g_ thing
// initialized the first time falling through that path or we should treat it as
// an error
internal RN_VK_TextureHandle                rn_vk_texture_create_(RN_TextureDesc *desc, void *data);
internal RN_VK_SamplerHandle                rn_vk_sampler_create_(RN_SamplerDesc *desc);
internal RN_VK_BufferHandle                 rn_vk_buffer_create_(RN_BufferDesc *desc, u8 *data);
internal RN_VK_ShaderResourceLayoutHandle   rn_vk_shader_resource_layout_create_(RN_ShaderResourceLayoutDesc *desc);
internal RN_VK_ShaderResourceHandle                rn_vk_shader_resource_create_(RN_ShaderResourceDesc *desc);
internal RN_VK_PipelineHandle               rn_vk_pipeline_create_(RN_PipelineDesc *desc);

internal void                               rn_vk_texture_destroy_(RN_VK_TextureHandle h);
internal void                               rn_vk_buffer_destroy_(RN_VK_BufferHandle h);
internal void                               rn_vk_sampler_destroy_(RN_VK_SamplerHandle h);
internal void                               rn_vk_shader_resource_layout_destroy_(RN_VK_SamplerHandle h);
internal void                               rn_vk_pipeline_destroy_(RN_VK_PipelineHandle h);

internal RN_VK_FrameData    *rn_vk_frame_data_get_current();
#endif // !RN_VK_H
