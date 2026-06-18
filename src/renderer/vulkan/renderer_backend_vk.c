global RendererBackendVk *g_vk_ctx;

internal RendererBackendVk*
renderer_backend_as_vk(RendererBackend *base)
{
    DOT_ASSERT(base->backend_kind == RendererBackendKind_Vk);
    return cast(RendererBackendVk*) base;
}

internal RendererBackendVk*
renderer_backend_vk_create(Arena *arena, RendererBackendConfig *backend_config)
{
    Arena *backend_arena = ARENA_CREATE(
        .parent = arena,
        .reserve_size = backend_config->backend_memory_size,);

    g_vk_ctx = PUSH_STRUCT(backend_arena, RendererBackendVk);
    g_vk_ctx->base.backend_kind = RendererBackendKind_Vk;
    g_vk_ctx->base.permanent_arena = backend_arena;
    g_vk_ctx->base.transient_arena = ARENA_CREATE(
        .parent = backend_arena,
        .reserve_size = backend_config->backend_transient_memory_size,);

#define FN(ret, name, params) g_vk_ctx->base.name = renderer_backend_vk_##name;
    RENDERER_BACKEND_FN_LIST
#undef FN
    renderer_backend_vk_merge_render_settings(backend_config);
    return g_vk_ctx;
}

internal void
renderer_backend_vk_merge_render_settings(RendererBackendConfig *backend_config)
{
    g_rbvk_render_settings.frame.frame_overlap = backend_config->frame_overlap;
    g_rbvk_render_settings.swapchain.preferred_present_mode = vk_helper_present_mode_kind_to_vk_present_mode_khr(backend_config->present_mode);
    g_vk_ctx->base.frame_overlap = backend_config->frame_overlap;
}

internal inline VKAPI_ATTR u32 VKAPI_CALL
renderer_backend_vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data)
{
    DOT_UNUSED(message_type); DOT_UNUSED(user_data);
    if(message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT){
        DOT_WARNING("MessageID: %s %i\nMessage: %s\n\n", callback_data->pMessageIdName, callback_data->messageIdNumber, callback_data->pMessage);
        // os_print_stacktrace();
    }
    return VK_FALSE;
}

internal inline VKAPI_ATTR void VKAPI_CALL
rbvk_vk_resource_set_name(VkObjectType type, u64 handle, const char *name)
{
    if(!VK_EXT_DEBUG_UTILS_ENABLE){
        return;
    }
#ifndef DOT_USE_VOLK
    error (jd) I havent't bothered to support some of this EXT ptrs without volk
#endif
    vkSetDebugUtilsObjectNameEXT(g_vk_ctx->device.vk_device,
        &(VkDebugUtilsObjectNameInfoEXT){
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .objectType = type,
            .objectHandle = handle,
            .pObjectName = name,
        });
}

internal DOT_ShaderModuleHandle
rbvk_dot_shader_module_from_vk_shader_module(VkShaderModule vk_sm)
{
    DOT_ShaderModuleHandle dot_smh = {
        .handle[0] = cast(u64) vk_sm,
    };
    return dot_smh;
}

internal VkShaderModule
rbvk_vk_shader_module_render_types_shader_module(DOT_ShaderModuleHandle dot_smh)
{
    VkShaderModule vk_sm = cast(VkShaderModule)dot_smh.handle[0];
    return vk_sm;
}

internal DOT_ShaderModuleHandle
renderer_backend_vk_shader_load_from_data(String8 data)
{
    VkShaderModule vk_shader_module = VK_NULL_HANDLE;
    VkShaderModuleCreateInfo create_info = {
        .sType      = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext      = NULL,
        .codeSize   = data.size,
        .pCode      = cast(u32*)data.str,
    };
    VK_CHECK(vkCreateShaderModule(g_vk_ctx->device.vk_device, &create_info, NULL, &vk_shader_module));
    DOT_ShaderModuleHandle dot_shader_module_handle = rbvk_dot_shader_module_from_vk_shader_module(vk_shader_module);
    return dot_shader_module_handle;
}

internal void
renderer_backend_vk_shader_unload(DOT_ShaderModuleHandle shader_module_handle)
{
    VkShaderModule vk_sm = rbvk_vk_shader_module_render_types_shader_module(shader_module_handle);
    vkDestroyShaderModule(g_vk_ctx->device.vk_device, vk_sm, NULL);
}

internal DOT_SamplerHandle
renderer_backend_vk_sampler_create(const RenderTypes_SamplerDesc *desc, String8 debug_name)
{
    RBVK_SamplerHandle h = rbvk_sampler_create(desc, debug_name);
    DOT_SamplerHandle dot_sampler_handle = {.handle[0] = pool_handle_pack(h),};
    return dot_sampler_handle;
}

internal RBVK_SamplerHandle
rbvk_sampler_create(const RenderTypes_SamplerDesc *desc, String8 debug_name)
{
    if(desc == NULL){
       DOT_WARNING("Missing Sampler desc");
       RBVK_SamplerHandle sampler_h = POOL_NULL_HANDLE_GET(&g_vk_ctx->sampler_pool);
       return sampler_h;
    }
    const RBVK_SamplerHandle sampler_h = POOL_ALLOC(&g_vk_ctx->sampler_pool);
    if(pool_handle_is_null(sampler_h)){
        return sampler_h;
    }
    RBVK_Sampler *sampler = POOL_GET(&g_vk_ctx->sampler_pool, sampler_h);
    sampler->vk_min_filter      = vk_helper_vk_filter_render_types_sampler_filter(desc->min_filter),
    sampler->vk_mag_filter      = vk_helper_vk_filter_render_types_sampler_filter(desc->mag_filter),
    sampler->vk_mipmap_filter   = vk_helper_vk_sampler_mipmap_mode_render_types_sampler_mipmap_mode(desc->mipmap_filter),
    sampler->vk_address_mode_u  = vk_helper_vk_sampler_address_mode_render_types_sampler_address_mode(desc->address_mode_u),
    sampler->vk_address_mode_v  = vk_helper_vk_sampler_address_mode_render_types_sampler_address_mode(desc->address_mode_v),
    sampler->vk_address_mode_w  = vk_helper_vk_sampler_address_mode_render_types_sampler_address_mode(desc->address_mode_w),
    DOT_DEBUG_NAME_SET(sampler->name, debug_name);

    vkCreateSampler(g_vk_ctx->device.vk_device,
        &(VkSamplerCreateInfo){
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .minFilter      = sampler->vk_min_filter,
            .magFilter      = sampler->vk_mag_filter,
            .mipmapMode     = sampler->vk_mipmap_filter,
            .addressModeU   = sampler->vk_address_mode_u,
            .addressModeV   = sampler->vk_address_mode_v,
            .addressModeW   = sampler->vk_address_mode_w,
            .anisotropyEnable = 0,
            .compareEnable = 0,
            .unnormalizedCoordinates = 0,
            .borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE,
            .minLod = 0,
            .maxLod = 16,
            // TODO:
            /*float                   mipLodBias;
            float                   maxAnisotropy;
            VkCompareOp             compareOp;
            VkBorderColor           borderColor;
            VkBool32                unnormalizedCoordinates;*/
        }, NULL, &sampler->vk_sampler);
    rbvk_vk_resource_set_name(VK_OBJECT_TYPE_SAMPLER, cast(u64)sampler->vk_sampler, sampler->name);
    renderer_backend_vk_resource_cleanup_list_push_rbvk_sampler(sampler_h);
    return sampler_h;
}

internal DOT_TextureHandle
renderer_backend_vk_texture_create(const RenderTypes_TextureDesc *desc, void *data, String8 debug_name)
{
    RBVK_TextureHandle h = rbvk_texture_create(desc, data, debug_name);
    DOT_TextureHandle dot_texture_handle = {.handle[0] = pool_handle_pack(h),};
    return dot_texture_handle;
}

internal RBVK_TextureHandle
rbvk_texture_create(const RenderTypes_TextureDesc *desc, void *data, String8 debug_name)
{
    if(desc == NULL){
       DOT_WARNING("Missing Texture desc");
       RBVK_TextureHandle texture_h = POOL_NULL_HANDLE_GET(&g_vk_ctx->texture_pool);
       return texture_h;
    }
    const RBVK_TextureHandle texture_h = POOL_ALLOC(&g_vk_ctx->texture_pool);
    if(pool_handle_is_null(texture_h)){
        return texture_h;
    }
    RBVK_Texture *texture = POOL_GET(&g_vk_ctx->texture_pool, texture_h);
    VkExtent3D image_extent_3d  = {desc->width, desc->height, desc->depth};
    texture->vk_extent3d        = image_extent_3d;
    texture->mip_levels         = desc->mip_levels;
    texture->vk_format          = vk_helper_texture_format_to_vk_texture_format(desc->format_kind);
    texture->vk_image_layout    = VK_IMAGE_LAYOUT_UNDEFINED;
    DOT_DEBUG_NAME_SET(texture->name, debug_name);

    const RenderTypes_TextureFormatInfo format_info = renderer_texture_format_info_from_format(desc->format_kind);
    const b8 format_depth_bit           = DOT_BITS_MATCH(format_info.format_flags, RenderTypes_TextureFormatBit_Depth);
    const b8 format_stencil_bit         = DOT_BITS_MATCH(format_info.format_flags, RenderTypes_TextureFormatBit_Stencil);
    const b8 usage_compute_bit          = DOT_BITS_MATCH(desc->texture_usage_flags, RenderTypes_TextureUsageBit_Compute);
    const b8 usage_render_target_bit    = DOT_BITS_MATCH(desc->texture_usage_flags, RenderTypes_TextureUsageBit_RenderTarget);

    VkDevice vk_device = g_vk_ctx->device.vk_device;
    {
        VkImageUsageFlags image_usage_flags = VK_IMAGE_USAGE_SAMPLED_BIT | (usage_compute_bit ? VK_IMAGE_USAGE_STORAGE_BIT : 0);
        if(format_depth_bit || format_stencil_bit){
            image_usage_flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }else{
            image_usage_flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            image_usage_flags |= usage_render_target_bit ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0;
        }
        VK_CHECK(vkCreateImage(
            vk_device,
            &(VkImageCreateInfo){
                .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = vk_helper_texture_dimension_to_vk_image_type(desc->dimension_kind),
                .format = vk_helper_texture_format_to_vk_texture_format(desc->format_kind),
                .extent = image_extent_3d,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,

                .mipLevels = desc->mip_levels,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = image_usage_flags,
            },
            NULL,
            &texture->vk_image));
        rbvk_vk_resource_set_name(VK_OBJECT_TYPE_IMAGE, cast(u64)texture->vk_image, texture->name);
        texture->alloc = rbvk_memory_gpu_image_alloc(&g_vk_ctx->memory_pools, texture->vk_image);
    }
    {
        VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_NONE;
        aspect_mask |= format_depth_bit ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
        aspect_mask |= format_stencil_bit ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
        if(aspect_mask == 0){aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;}

        VK_CHECK(vkCreateImageView(
            vk_device,
            &(VkImageViewCreateInfo){
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = texture->vk_image,
                .viewType = vk_helper_texture_dimension_to_vk_image_view_type(desc->dimension_kind),
                .format = texture->vk_format,
                .subresourceRange = {
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                    .aspectMask = aspect_mask,
                }
            },
            NULL,
            &texture->vk_image_view));
        rbvk_vk_resource_set_name(VK_OBJECT_TYPE_IMAGE_VIEW, cast(u64)texture->vk_image_view, texture->name);
    }

    if(data){
        u64 texture_size = format_info.block_size * image_extent_3d.height * image_extent_3d.width  * image_extent_3d.depth;
        VkMemory_Alloc mem_alloc = rbvk_memory_pools_staging_ring_buffer_push(&g_vk_ctx->memory_pools, texture_size, data);

        RBVK_FrameData *frame_data  = rbvk_frame_data_get_current();
        DOT_ASSERT(frame_data->vk_command_buffers_in_use == 0, "Missing generic upload buffers");
	    VkCommandBuffer vk_command_buffer = frame_data->vk_command_buffers.data[1];

        vkBeginCommandBuffer(vk_command_buffer,
            &(VkCommandBufferBeginInfo){
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            });
        vk_helper_rbvk_texture_transition(vk_command_buffer, texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        vkCmdCopyBufferToImage(vk_command_buffer, mem_alloc.vk_buffer, texture->vk_image, texture->vk_image_layout, 1, 
            &(VkBufferImageCopy){
                .bufferOffset = mem_alloc.offset,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,

                .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .imageSubresource.mipLevel = 0,
                .imageSubresource.baseArrayLayer = 0,
                .imageSubresource.layerCount = 1,

                .imageOffset = { 0, 0, 0 },
                .imageExtent = image_extent_3d});
        vk_helper_rbvk_texture_transition(vk_command_buffer, texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        vkEndCommandBuffer(vk_command_buffer);
        vkQueueSubmit(g_vk_ctx->device.graphics_queue, 1,
            &(VkSubmitInfo){
                VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &vk_command_buffer,
            },
            VK_NULL_HANDLE);
 
        // (jd) NOTE: We are synchronizing here explicitly, we will want to deferr checking all uploads till later on
        vkQueueWaitIdle(g_vk_ctx->device.graphics_queue);
        rbvk_memory_pools_staging_ring_buffer_pop(&g_vk_ctx->memory_pools, &mem_alloc);
        vkResetCommandBuffer(vk_command_buffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    }
    renderer_backend_vk_resource_cleanup_list_push_rbvk_texture(texture_h);
    return texture_h;
}

internal RBVK_BufferHandle
rbvk_buffer_create(const RenderTypes_BufferDesc *desc, u8 *data, String8 debug_name)
{
    if(desc == NULL){
       DOT_WARNING("Missing buffer desc");
       RBVK_BufferHandle buffer_h = POOL_NULL_HANDLE_GET(&g_vk_ctx->buffer_pool);
       return buffer_h;
    }

    const RBVK_BufferHandle buffer_h = POOL_ALLOC(&g_vk_ctx->buffer_pool);
    if(pool_handle_is_null(buffer_h)){
        return buffer_h;
    }

    RBVK_Buffer *buffer = POOL_GET(&g_vk_ctx->buffer_pool, buffer_h);
    buffer->vk_size = desc->size;
    buffer->resource_usage = desc->resource_usage;
    buffer->vk_buffer_usage_flags = vk_helper_vk_buffer_usage_flags_from_dt_buffer_usage_flags(desc->buffer_usage_flags);
    // buffer->handle = handle;
    buffer->global_offset = 0;
    // buffer->parent_buffer = POOL_NULL_HANDLE;
    DOT_DEBUG_NAME_SET(buffer->name, debug_name);

    // static const VkBufferUsageFlags k_dynamic_buffer_mask = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    // const b8 use_global_buffer = DOT_BITS_ANY(k_dynamic_buffer_mask, buffer->vk_buffer_usage_flags);
    // if(desc->resource_usage == DOT_ResourceUsageKind_Dynamic && use_global_buffer){
    //     buffer->alloc = rbvk_memory_gpu_buffer_alloc(&g_vk_ctx->memory_pools, buffer->vk_buffer);
    //     rbvk_vk_resource_set_name(VK_OBJECT_TYPE_BUFFER, cast(u64)buffer->vk_buffer, buffer->name);
    //     VkMemory_Alloc mem_alloc = rbvk_memory_pools_staging_ring_buffer_push(&g_vk_ctx->memory_pools, desc->size, data);
    //     buffer->vk_buffer = buffer->alloc.vk_buffer;
    // }
    // else if(desc->resource_usage == DOT_ResourceUsageKind_Readback){
    //     DOT_TODO("Not implemented yet");
    //     // rbvk_memory_pools_readback_ring_buffer_push
    //     // buffer->vk_buffer = buffer->alloc.vk_buffer;
    // }else if(desc->resource_usage == DOT_ResourceUsageKind_GPUOnly){
    //     VK_CHECK(vkCreateBuffer(
    //         g_vk_ctx->device.vk_device,
    //         &(VkBufferCreateInfo){
    //             .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    //             .usage = buffer->vk_buffer_usage_flags,
    //             .size = desc->size > 0 ? desc->size : 1,
    //         },
    //         NULL,
    //         &buffer->vk_buffer));
    //     buffer->alloc = rbvk_memory_gpu_buffer_alloc(&g_vk_ctx->memory_pools, buffer->vk_buffer);
    //     rbvk_vk_resource_set_name(VK_OBJECT_TYPE_BUFFER, cast(u64)buffer->vk_buffer, buffer->name);
    // }
    VK_CHECK(vkCreateBuffer(g_vk_ctx->device.vk_device,
        &(VkBufferCreateInfo){
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .usage = buffer->vk_buffer_usage_flags,
            .size = desc->size > 0 ? desc->size : 1,
        },
        NULL,
        &buffer->vk_buffer));
    rbvk_vk_resource_set_name(VK_OBJECT_TYPE_BUFFER, cast(u64)buffer->vk_buffer, buffer->name);
    buffer->alloc = rbvk_memory_gpu_buffer_alloc(&g_vk_ctx->memory_pools, buffer->vk_buffer);

    if(data){
        VkMemory_Alloc mem_alloc = rbvk_memory_pools_staging_ring_buffer_push(&g_vk_ctx->memory_pools, desc->size, data);
        RBVK_FrameData *frame_data  = rbvk_frame_data_get_current();
        DOT_ASSERT(frame_data->vk_command_buffers_in_use == 0, "Missing generic upload buffers");
	    VkCommandBuffer vk_command_buffer = frame_data->vk_command_buffers.data[1];
        vkBeginCommandBuffer(vk_command_buffer,
            &(VkCommandBufferBeginInfo){
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            });
        vkCmdCopyBuffer(vk_command_buffer, mem_alloc.vk_buffer, buffer->vk_buffer, 1,
            &(VkBufferCopy){
                .srcOffset = mem_alloc.offset,
                .dstOffset = 0,
                .size = buffer->vk_size
            }
        );
        vkEndCommandBuffer(vk_command_buffer);
        vkQueueSubmit(g_vk_ctx->device.graphics_queue, 1,
            &(VkSubmitInfo){
                VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &vk_command_buffer,
            },
            VK_NULL_HANDLE);
        // (jd) NOTE: We are synchronizing here explicitly, we will want to deferr checking all uploads till later on
        vkQueueWaitIdle(g_vk_ctx->device.graphics_queue);
        rbvk_memory_pools_staging_ring_buffer_pop(&g_vk_ctx->memory_pools, &mem_alloc);
        vkResetCommandBuffer(vk_command_buffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    }

    renderer_backend_vk_resource_cleanup_list_push_rbvk_buffer(buffer_h);
    return buffer_h;
}

internal DOT_BufferHandle
renderer_backend_vk_buffer_create(const RenderTypes_BufferDesc *desc, u8 *data, String8 debug_name)
{
    RBVK_BufferHandle h = rbvk_buffer_create(desc, data, debug_name);
    DOT_BufferHandle dot_buffer_handle = {.handle[0] = pool_handle_pack(h),};
    return dot_buffer_handle;
    return (DOT_BufferHandle){0};

}

internal void
renderer_backend_vk_resource_cleanup_list_push_scope()
{
}

// NOTE(JD): Will start to just make enclosing scopes
// Should extend to be a graph
internal void
renderer_backend_vk_resource_cleanup_list_push_rbvk_texture(RBVK_TextureHandle texture_id)
{
    RBVK_ResourceCleanupCtx *root = TREE_GET_ROOT(&g_vk_ctx->resource_cleanup_list_tree);
    ARRAY_PUSH(root->texture_ids, texture_id);
}

internal void
renderer_backend_vk_resource_cleanup_list_push_rbvk_sampler(RBVK_SamplerHandle sampler_id)
{
    RBVK_ResourceCleanupCtx *root = TREE_GET_ROOT(&g_vk_ctx->resource_cleanup_list_tree);
    ARRAY_PUSH(root->sampler_ids, sampler_id);
}

internal void
renderer_backend_vk_resource_cleanup_list_push_rbvk_buffer(RBVK_BufferHandle buffer_id)
{
    RBVK_ResourceCleanupCtx *root = TREE_GET_ROOT(&g_vk_ctx->resource_cleanup_list_tree);
    ARRAY_PUSH(root->buffer_ids, buffer_id);
}

// internal void
// renderer_backend_resource_cleanup_list_push_child(RBVK_ResourceCleanupCtx *parent, u32 idx){
//     DOT_ASSERT(idx < g_vk_ctx->cleanup_list_idx, "Idx must be an active resource list!");
//     // cleanup_list
//     RBVK_ResourceCleanupCtx *elems = &g_vk_ctx->cleanup_list[g_vk_ctx->cleanup_list_idx];
//     elems.
//     RBVK_TEXURE_MAX
//     g_vk_ctx->cleanup_list_idx += 1;
// }

internal void
renderer_backend_vk_resource_cleanup_list_pop_last()
{
}

internal void
renderer_backend_vk_resource_cleanup_list_pop_at(PoolHandle pop_start)
{
    (void)pop_start;
}

internal void
renderer_backend_vk_resource_cleanup_list_pop_all(){
    TempArena t = threadctx_get_temp(0);
    ResourceCleanupListTree *tree = &g_vk_ctx->resource_cleanup_list_tree;
    TreeIterator it = TREE_ITER_BEGIN(t.arena, tree, tree->tree_pool.tree_root);
    for EACH_TREE_NODE(node_h, &it){
        RBVK_ResourceCleanupCtx *cleanup_list = TREE_GET(tree, node_h);
        for EACH_INDEX(i, cleanup_list->texture_ids.count){
            RBVK_TextureHandle tex_h = ARRAY_GET(cleanup_list->texture_ids, i);
            RBVK_Texture *tex = POOL_GET(&g_vk_ctx->texture_pool, tex_h);
            rbvk_texture_destroy(tex);
            POOL_FREE(&g_vk_ctx->texture_pool, tex_h);
        }
        for EACH_INDEX(i, cleanup_list->buffer_ids.count){
            RBVK_BufferHandle tex_h = ARRAY_GET(cleanup_list->buffer_ids, i);
            POOL_FREE(&g_vk_ctx->buffer_pool, tex_h);
        }
        for EACH_INDEX(i, cleanup_list->sampler_ids.count){
            RBVK_SamplerHandle tex_h = ARRAY_GET(cleanup_list->sampler_ids, i);
            POOL_FREE(&g_vk_ctx->sampler_pool, tex_h);
        }
    }
    temp_arena_restore(t);
}


// internal RBVK_Buffer
// rbvk_buffer_create2(
//     VkDeviceSize size,
//     VkBufferUsageFlags usage,
//     // VkMemory_PoolsKind pool_kind,
//     String8 name)
// {
//     RBVK_Buffer buf = {
//         .vk_size = size,
//     };
//     DOT_DEBUG_NAME_SET(buf.name, name);
//
//     VkDevice device = g_vk_ctx->device.vk_device;
//     VkBufferCreateInfo info = {
//         .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//         .size  = size,
//         .usage = usage,
//         .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//     };
//
//     VK_CHECK(vkCreateBuffer(device, &info, NULL, &buf.vk_buffer));
//     VkMemory_Alloc alloc = rbvk_memory_gpu_buffer_alloc(&g_vk_ctx->memory_pools, buf.vk_buffer);
//     buf.alloc.vk_memory = alloc.vk_memory;
//     buf.alloc.offset = alloc.offset;
//
//     VK_CHECK(vkBindBufferMemory(device, buf.vk_buffer, buf.alloc.vk_memory, buf.alloc.offset));
//     return buf;
// }

internal void
renderer_backend_vk_texture_destroy(DOT_TextureHandle handle)
{
    const PoolHandle texture_h = pool_handle_unpack(handle.handle[0]);
    RBVK_Texture *tex = POOL_GET(&g_vk_ctx->texture_pool, texture_h);
    rbvk_texture_destroy(tex);
    POOL_FREE(&g_vk_ctx->texture_pool, texture_h);
}

internal void
rbvk_texture_destroy(RBVK_Texture *image){
    VkDevice device = g_vk_ctx->device.vk_device;
    DOT_ASSERT(image->vk_image_view);
    DOT_ASSERT(image->vk_image);
    vkDestroyImageView(device, image->vk_image_view, NULL);
    // vkUnmapMemory(device, image->alloc.vk_memory);
    vkDestroyImage(device, image->vk_image, NULL);
}

internal void
renderer_backend_vk_init(DOT_Window *window)
{
    // (jd) This is all super messy and error prone. Cleanup
    Arena *ctx_arena = g_vk_ctx->base.permanent_arena;
    POOL_INIT(ctx_arena, &g_vk_ctx->texture_pool,   RENDER_TEXURE_MAX);
    POOL_INIT(ctx_arena, &g_vk_ctx->buffer_pool,    RENDER_BUFFER_MAX);
    POOL_INIT(ctx_arena, &g_vk_ctx->sampler_pool,   RENDER_SAMPLER_MAX);
    TREE_INIT(ctx_arena, RBVK_ResourceCleanupCtx, &g_vk_ctx->resource_cleanup_list_tree, RENDER_CTX_RESCOURCE_POOL);
#ifdef DOT_USE_VOLK
    volkInitialize();
#endif
    // g_vk_ctx->vk_allocator = VkAllocatorParams(ctx_arena);
    TempArena temp = threadctx_get_temp(0);
    if(!vk_helper_all_layers(&g_rbvk_vk_config)){
        DOT_ERROR("Could not find all requested layers");
    }

    if(!vk_helper_instance_all_required_extensions(&g_rbvk_vk_config)){
        DOT_ERROR("Could not find all requested instance extensions");
    }

    // --- Create Instance ---
    {
        VkDebugUtilsMessengerCreateInfoEXT *debug_utils_info_ptr = NULL;
        if(VK_EXT_DEBUG_UTILS_ENABLE){
            VkDebugUtilsMessengerCreateInfoEXT debug_utils_info = {
                .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .messageSeverity =
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                .messageType     =
                    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                .pfnUserCallback = renderer_backend_vk_debug_callback,
            };
            debug_utils_info_ptr = &debug_utils_info;
        }
        VK_CHECK(vkCreateInstance(
            &(VkInstanceCreateInfo){
                .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                .ppEnabledLayerNames     = cstr_array_from_string8_array(
                    temp.arena,
                    g_rbvk_vk_config.validation_layers.count,
                    g_rbvk_vk_config.validation_layers.data),
                .enabledLayerCount       = g_rbvk_vk_config.validation_layers.count,
                .ppEnabledExtensionNames = cstr_array_from_string8_array(
                    temp.arena,
                    g_rbvk_vk_config.instance.extensions.count,
                    g_rbvk_vk_config.instance.extensions.data),
                .enabledExtensionCount = g_rbvk_vk_config.instance.extensions.count,
                .pApplicationInfo = &(VkApplicationInfo){
                    .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                    .pApplicationName   = g_rbvk_vk_config.instance.application_name.cstr,
                    .applicationVersion = g_rbvk_vk_config.instance.application_version,
                    .pEngineName        = g_rbvk_vk_config.instance.engine_name.cstr,
                    .engineVersion      = g_rbvk_vk_config.instance.engine_version,
                    .apiVersion         = g_rbvk_vk_config.instance.api_version,
                },
                .pNext = debug_utils_info_ptr,
            }, NULL, &g_vk_ctx->instance));
#ifdef DOT_USE_VOLK
        volkLoadInstance(g_vk_ctx->instance);
#endif

        if(debug_utils_info_ptr){
#ifndef DOT_USE_VOLK
            PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT =
                (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(g_vk_ctx->instance, "vkCreateDebugUtilsMessengerEXT");
#endif
            if(vkCreateDebugUtilsMessengerEXT){
                VK_CHECK(vkCreateDebugUtilsMessengerEXT(g_vk_ctx->instance, debug_utils_info_ptr, NULL, &g_vk_ctx->debug_messenger));
            }
        }
    }

    // --- Create Surface --- 
    dot_window_create_surface(window, &g_vk_ctx->base);
    // --- Create Device --- 
    {
        VkHelper_CandidateDeviceInfo candidate_device_info = vk_helper_pick_best_device(&g_rbvk_vk_config, &g_rbvk_render_settings, g_vk_ctx->instance, g_vk_ctx->surface);
        if(candidate_device_info.score == -1){
            DOT_ERROR("Could not find a suitable device");
        }

        RBVK_Device* device = &g_vk_ctx->device;
        device->vk_gpu                = candidate_device_info.gpu;
        device->graphics_queue_idx = candidate_device_info.graphics_family;
        device->present_queue_idx  = candidate_device_info.present_family;
        device->is_integrated_gpu  = candidate_device_info.is_integrated_gpu;

        b8 shared_present_graphics_queues = candidate_device_info.graphics_family == candidate_device_info.present_family;

        VkDeviceQueueCreateInfo queue_infos[2];
        u32 queue_count = 0;
        const float priority = 1.0f;
        queue_infos[queue_count++] = (VkDeviceQueueCreateInfo) {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = candidate_device_info.graphics_family,
            .queueCount       = 1,
            .pQueuePriorities = &priority,
        };

        if(!shared_present_graphics_queues){
            queue_infos[queue_count++] = (VkDeviceQueueCreateInfo) {
                .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = candidate_device_info.present_family,
                .queueCount       = 1,
                .pQueuePriorities = &priority,
            };
        }

        VkDeviceCreateInfo device_create_info = {
            .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pQueueCreateInfos       = queue_infos,
            .queueCreateInfoCount    = queue_count,
            .pEnabledFeatures        = &(VkPhysicalDeviceFeatures){},
            .ppEnabledExtensionNames = cstr_array_from_string8_array(
                temp.arena,
                g_rbvk_vk_config.device.extensions.count,
                g_rbvk_vk_config.device.extensions.data),
            .enabledExtensionCount   = g_rbvk_vk_config.device.extensions.count,
            .pNext = g_rbvk_vk_config.device.features,
        };

        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(candidate_device_info.gpu, &properties);
        DOT_PRINT("Vulkan version: %u.%u.%u", VK_VERSION_MAJOR(properties.apiVersion), VK_VERSION_MINOR(properties.apiVersion), VK_VERSION_PATCH(properties.apiVersion));
        VK_CHECK(vkCreateDevice(candidate_device_info.gpu, &device_create_info, NULL, &device->vk_device));

#ifdef DOT_USE_VOLK
        volkLoadDevice(device->vk_device);
#endif
        vkGetDeviceQueue(device->vk_device, candidate_device_info.graphics_family, 0, &device->graphics_queue);
        if(shared_present_graphics_queues){
            device->present_queue = device->graphics_queue;
        } else {
            DOT_ERROR("Different present and graphics queues unsupported");
            vkGetDeviceQueue(device->vk_device, candidate_device_info.present_family, 0, &device->present_queue);
        }
    }
    g_vk_ctx->memory_pools = vk_memory_pools_create(&g_vk_ctx->device);

    // TODO: We will probably need to move this to its own function to allow recreation
    // --- Create Swapchain ---
    {
        g_vk_ctx->draw_extent = (VkExtent2D){cast(u32) window->window->w, cast(u32) window->window->h};
        VkHelper_SwapchainDetails details = {0};
        vk_helper_physical_device_swapchain_support(&g_rbvk_render_settings, g_vk_ctx->device.vk_gpu, g_vk_ctx->surface, window, &details);

        RBVK_Swapchain* swapchain = &g_vk_ctx->swapchain;
        swapchain->extent = details.surface_extent;
        swapchain->image_format = details.best_surface_format.format;
        VkSwapchainCreateInfoKHR swapchain_create_info = {
            .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface          = g_vk_ctx->surface,
            .minImageCount    = details.image_count,
            .imageFormat      = details.best_surface_format.format,
            .imageColorSpace  = details.best_surface_format.colorSpace,
            .imageExtent      = details.surface_extent,
            .preTransform     = details.current_transform,
            .presentMode      = details.best_present_mode,
            .clipped          = VK_TRUE,
            .imageArrayLayers = 1,
            .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .oldSwapchain     = VK_NULL_HANDLE, // Will need to update this for recreation
        };
        if(g_vk_ctx->device.graphics_queue_idx != g_vk_ctx->device.present_queue_idx){
            DOT_ERROR("Different present and graphics queues unsupported");
            u32 queues[] = {g_vk_ctx->device.graphics_queue_idx, g_vk_ctx->device.present_queue_idx};
            swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            swapchain_create_info.pQueueFamilyIndices = queues;
            swapchain_create_info.queueFamilyIndexCount = DOT_ARRAY_COUNT(queues);
        }
        VK_CHECK(vkCreateSwapchainKHR(g_vk_ctx->device.vk_device, &swapchain_create_info, NULL, &swapchain->swapchain));
        g_vk_ctx->draw_image = rbvk_texture_create(
            RENDER_TYPES_TEXTURE_DESC(
                .dimension_kind = RenderTypes_TextureDimensionKind_2D,
                .format_kind = RenderTypes_TextureFormatKind_RGBA16F,
                .texture_usage_flags = RenderTypes_TextureUsageBit_RenderTarget | RenderTypes_TextureUsageBit_Compute,
                .width = cast(u16)g_vk_ctx->draw_extent.width,
                .height = cast(u16)g_vk_ctx->draw_extent.height,
                .depth = 1,
                .mip_levels = 1
            ), NULL, String8Lit("Draw Image"));

        {
            //  --- Create Image Datas ---
            VkDevice device = g_vk_ctx->device.vk_device;

            // (jd) NOTE: Since vk expects a VkImage array we need to temp alloc this and copy it over
            u32 swapchain_image_count = 0;
            vkGetSwapchainImagesKHR(device, swapchain->swapchain, &swapchain_image_count, NULL);
            array(VkImage) swapchain_images = PUSH_ARRAY(temp.arena, VkImage, swapchain_image_count);
            vkGetSwapchainImagesKHR(device, swapchain->swapchain, &swapchain_image_count, swapchain_images);

            SLICE_INIT(ctx_arena, &swapchain->swapchain_images, swapchain_image_count);

            for(u32 i = 0; i < swapchain->swapchain_images.count; ++i){
                RBVK_SwapchainImage *swapchain_image = &swapchain->swapchain_images.data[i];
                swapchain_image->vk_image = swapchain_images[i];
                VK_CHECK(vkCreateImageView(device,
                    &(VkImageViewCreateInfo) {
                        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                        .image = swapchain_image->vk_image,
                        .viewType = VK_IMAGE_VIEW_TYPE_2D,
                        .format   = swapchain->image_format,
                        .components = {
                            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                        },
                        .subresourceRange = {
                            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                            .baseMipLevel   = 0,
                            .levelCount     = 1,
                            .baseArrayLayer = 0,
                            .layerCount     = 1,
                        },
                    }, NULL, &swapchain_image->vk_image_view));

                VK_CHECK(vkCreateSemaphore(device,
                    &(VkSemaphoreCreateInfo) {
                        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                        .flags = 0,
                        .pNext = NULL,
                    }, NULL, &swapchain_image->semaphore_render_complete));
            }
        }
    }
    // --- Create Frame Structures ---
    {
        u8 frame_overlap = g_vk_ctx->base.frame_overlap;
        VkDevice device = g_vk_ctx->device.vk_device;
        // WARN: Any allocation that can be recreated may be need to be pushed onto its own arena alloc ctx to avoid leaking
        SLICE_INIT(ctx_arena, &g_vk_ctx->frame_datas, frame_overlap);
        {
            for(u32 i = 0; i < frame_overlap; ++i){
                RBVK_FrameData *frame_data = &SLICE_GET(g_vk_ctx->frame_datas, i);
                for(u32 j = 0; j <  RENDER_THREAD_COUNT_MAX; ++j){

                    VkCommandPoolCreateInfo cmd_create_info = {
                        .sType               = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                        .flags               = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                        .queueFamilyIndex    = g_vk_ctx->device.graphics_queue_idx,
                    };

                    VkCommandPool cmd_pool;
                    VK_CHECK(vkCreateCommandPool(device, &cmd_create_info, NULL, &cmd_pool));
                    ARRAY_PUSH(frame_data->vk_command_pools, cmd_pool);

                    for(u32 k = 0; k < RENDER_COMMAND_BUFFERS_PER_POOL; ++k){
                        VkCommandBufferAllocateInfo cmd_alloc_info = {
                            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                            .commandBufferCount = 1,
                            .commandPool = cmd_pool,
                        };
                        VK_CHECK(vkAllocateCommandBuffers(device, &cmd_alloc_info, &ARRAY_GET(frame_data->vk_command_buffers, k)));
                    }

                    // VK_CHECK(vkAllocateCommandBuffers(device, &cmd_alloc_info, &frame_data->frame_command_buffer));
                    // VK_CHECK(vkAllocateCommandBuffers(device, &cmd_alloc_info, &frame_data->immediate_command_buffer));
                    frame_data->frame_arena = ARENA_CREATE(.parent = ctx_arena, .reserve_size = DOT_KB(32));
                }

            }
        }
        // --- Init Sync Structures ---
        {
            for(u8 i = 0; i < frame_overlap; ++i){
                RBVK_FrameData *frame_data = &SLICE_GET(g_vk_ctx->frame_datas, i);
                VK_CHECK(vkCreateFence(device,
                    &(VkFenceCreateInfo){
                        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
                    },
                    NULL, &frame_data->render_complete_fence));

                VK_CHECK(vkCreateSemaphore(device,
                    &(VkSemaphoreCreateInfo){
                        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                        .flags = 0,
                    },
                    NULL, &frame_data->semaphore_image_acquired));
            }
        }
        // --- Create descriptors
        {
            VkDescriptorPoolSize pool_sizes[] = {
                {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 4096},
                {.type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = 4096},
                {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 512},
                {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1},
                // Keeping this for the first draw compute pass on the tutorial
                {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1},
            };

            VkDescriptorPoolCreateInfo pool_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .flags = 0,
                .poolSizeCount = DOT_ARRAY_COUNT(pool_sizes),
                .pPoolSizes = pool_sizes,
                .maxSets = 2,
            };

            vkCreateDescriptorPool(device, &pool_info, NULL, &g_vk_ctx->descriptor_pool);
            VkDescriptorSetLayoutBinding bindings[] = {
                { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                    .descriptorCount = 4096, .stageFlags = VK_SHADER_STAGE_ALL },
                { .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                    .descriptorCount = 4096, .stageFlags = VK_SHADER_STAGE_ALL },
                { .binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .descriptorCount = 512, .stageFlags = VK_SHADER_STAGE_ALL },
                { .binding = 3, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_ALL },
            };

            VkDescriptorSetLayoutBinding bindings_compute[] = {
                { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_ALL },
            };
            // Bindless layout
            VkDescriptorSetLayoutCreateInfo bindless_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = DOT_ARRAY_COUNT(bindings),
                .pBindings = bindings,
            };
            VK_CHECK(vkCreateDescriptorSetLayout(device, &bindless_info, NULL, &g_vk_ctx->bindless_layout));

            // Compute layout
            VkDescriptorSetLayoutCreateInfo compute_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = DOT_ARRAY_COUNT(bindings_compute),
                .pBindings = bindings_compute,
            };
            VK_CHECK(vkCreateDescriptorSetLayout(device, &compute_info, NULL, &g_vk_ctx->compute_layout));

            VkDescriptorSetLayout layouts[] = {
                g_vk_ctx->compute_layout,
                // g_vk_ctx->bindless_layout,
            };

            VkDescriptorSetAllocateInfo alloc_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = g_vk_ctx->descriptor_pool,
                .descriptorSetCount = DOT_ARRAY_COUNT(layouts),
                .pSetLayouts = layouts,
            };

            g_vk_ctx->descriptor_set_count = DOT_ARRAY_COUNT(layouts);
            VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, g_vk_ctx->descriptor_sets));
        }
    }

    // {
    //     VkDevice device = g_vk_ctx->device.device;
    //     VkPipelineLayoutCreateInfo pipeline_compute_layout = {
    //      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    //      .pNext = NULL,
    //      .pSetLayouts = &g_vk_ctx->compute_layout,
    //      .setLayoutCount = 1,
    //     };
    //
    //  VK_CHECK(vkCreatePipelineLayout(device, &pipeline_compute_layout, NULL, &g_vk_ctx->gradient_pipeline_layout));
    //     VkShaderModule compute_draw_shader = rbvk_vk_shader_module_render_types_shader_module(test_shader_module->shader_module_handle);
    //  VkPipelineShaderStageCreateInfo stageinfo = {
    //      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    //      .pNext = NULL,
    //      .stage = VK_SHADER_STAGE_COMPUTE_BIT,
    //      .module = compute_draw_shader,
    //      .pName = "main",
    //  };
    //
    //  VkComputePipelineCreateInfo compute_pipeline_create_info = {
    //      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
    //      .pNext = NULL,
    //      .layout = g_vk_ctx->gradient_pipeline_layout,
    //      .stage = stageinfo,
    //  };
    //
    //  VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &compute_pipeline_create_info, NULL, &g_vk_ctx->gradient_pipeline));
    // }
    temp_arena_restore(temp);
}

void 
renderer_create_postprocess_module(DOT_ShaderModuleHandle shader_module_h)
{
        VkDescriptorSetLayout layouts[] = {
            g_vk_ctx->compute_layout,
            // g_vk_ctx->bindless_layout,
        };
        VkDevice device = g_vk_ctx->device.vk_device;
        VkPipelineLayoutCreateInfo pipeline_compute_layout = {
	        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
	        .setLayoutCount = DOT_ARRAY_COUNT(layouts),
	        .pSetLayouts = layouts,
        };

	    VK_CHECK(vkCreatePipelineLayout(device, &pipeline_compute_layout, NULL, &g_vk_ctx->gradient_pipeline_layout));
        VkShaderModule compute_draw_shader = rbvk_vk_shader_module_render_types_shader_module(shader_module_h);
	    VkPipelineShaderStageCreateInfo stageinfo = {
	        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
	        .pNext = NULL,
	        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
	        .module = compute_draw_shader,
	        .pName = "main",
	    };

	    VkComputePipelineCreateInfo compute_pipeline_create_info = {
	        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
	        .pNext = NULL,
	        .layout = g_vk_ctx->gradient_pipeline_layout,
	        .stage = stageinfo,
	    };

        RBVK_Texture *draw_tex = POOL_GET(&g_vk_ctx->texture_pool, g_vk_ctx->draw_image);
	    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &compute_pipeline_create_info, NULL, &g_vk_ctx->gradient_pipeline));
        VkDescriptorImageInfo imgInfo = {
	        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
	        .imageView = draw_tex->vk_image_view,
	    };

	    VkWriteDescriptorSet drawImageWrite = {
	        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	        .pNext = NULL,
	        .dstBinding = 0,
	        .dstSet = g_vk_ctx->descriptor_sets[0],
	        .descriptorCount = 1,
	        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
	        .pImageInfo = &imgInfo,
	    };
	    vkUpdateDescriptorSets(g_vk_ctx->device.vk_device, 1, &drawImageWrite, 0, NULL);

}

internal RBVK_FrameData *
rbvk_frame_data_get_current()
{
    RBVK_FrameData *frame_data = &SLICE_GET(g_vk_ctx->frame_datas, g_vk_ctx->current_frame);
    return frame_data;
}

internal void rbvk_frame_data_reset()
{
    RBVK_FrameData *frame_data = rbvk_frame_data_get_current();
    frame_data->vk_command_buffers_in_use = 0;
    ARENA_RESET(frame_data->frame_arena);
    for( u32 i = 0; i < frame_data->vk_command_pools.count; i++ ){
        vkResetCommandPool(g_vk_ctx->device.vk_device, ARRAY_GET(frame_data->vk_command_pools, i), 0);
    }

}

internal VkCommandPool
rbvk_command_pool_get()
{
    RBVK_FrameData *frame_data = rbvk_frame_data_get_current();
    VkCommandPool pool = ARRAY_GET(frame_data->vk_command_pools, threadctx_id());
    return pool;
}

internal VkCommandBuffer
rbvk_command_buffer_get()
{
    RBVK_FrameData *frame_data = rbvk_frame_data_get_current();
    u32 cmd_idx = frame_data->vk_command_buffers_in_use * threadctx_id();
    VkCommandBuffer vk_cmd_buffer = ARRAY_GET(frame_data->vk_command_buffers, cmd_idx);
    frame_data->vk_command_buffers_in_use += 1;
    return vk_cmd_buffer;
}

internal void
renderer_backend_vk_clear_bg(vec3 color)
{
    RBVK_FrameData *frame_data = rbvk_frame_data_get_current();

	DOT_ASSERT(frame_data->vk_command_buffers_in_use == 0, "Multi command buffers per frame not implemented yet");
	VkCommandBuffer cmd = frame_data->vk_command_buffers.data[0];

	(void)color;
	RBVK_Texture *draw_image = POOL_GET(&g_vk_ctx->texture_pool, g_vk_ctx->draw_image);
	(void) draw_image;

	VkClearColorValue clear_value = {
	    .float32 = { color.r, color.g,color.b, 0 },
	};
	(void) clear_value;
	VkImageSubresourceRange clear_range = vk_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
	(void) clear_range;
	// vkCmdClearColorImage(cmd, draw_image->vk_image, VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1, &clear_range);


	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, g_vk_ctx->gradient_pipeline);
	// bind the descriptor set containing the draw image for the compute pipeline
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, g_vk_ctx->gradient_pipeline_layout, 0, 1, g_vk_ctx->descriptor_sets, 0, NULL);
	// execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
	vkCmdDispatch(cmd, cast(u32)ceil(g_vk_ctx->draw_extent.width / 16.0), cast(u32)ceil(g_vk_ctx->draw_extent.height / 16.0), 1);
}

internal void
renderer_backend_vk_frame_begin()
{
    RBVK_FrameData *frame_data = rbvk_frame_data_get_current();
    VkDevice device = g_vk_ctx->device.vk_device;
    VK_CHECK(vkWaitForFences(device, 1, &frame_data->render_complete_fence, true, TO_NSEC(1)));
    VK_CHECK(vkResetFences(device, 1, &frame_data->render_complete_fence));
    rbvk_frame_data_reset();

    VkResult res = vkAcquireNextImageKHR(device, g_vk_ctx->swapchain.swapchain, TO_NSEC(1), frame_data->semaphore_image_acquired, NULL, &frame_data->swapchain_image_idx);
    if(res== VK_ERROR_OUT_OF_DATE_KHR ){
        // TODO: Resize swapchain
    }


	DOT_ASSERT(frame_data->vk_command_buffers_in_use == 0, "Multi command buffers per frame not implemented yet");
	VkCommandBuffer cmd = frame_data->vk_command_buffers.data[0];

    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo cmd_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));
    RBVK_SwapchainImage *swapchain_image = &SLICE_GET(g_vk_ctx->swapchain.swapchain_images, frame_data->swapchain_image_idx);
    vk_helper_transition_image(cmd, swapchain_image->vk_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	RBVK_Texture *draw_image = POOL_GET(&g_vk_ctx->texture_pool, g_vk_ctx->draw_image);
    vk_helper_transition_image(cmd, draw_image->vk_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL); // (jd) TODO: use rbvk_texture helper
}

internal void
renderer_backend_vk_frame_end()
{
    RBVK_FrameData *frame_data = rbvk_frame_data_get_current();

	DOT_ASSERT(frame_data->vk_command_buffers_in_use == 0, "Parallel end frame not implemented yet");
	VkCommandBuffer cmd = frame_data->vk_command_buffers.data[0];

    RBVK_SwapchainImage *swapchain_image = &SLICE_GET(g_vk_ctx->swapchain.swapchain_images, frame_data->swapchain_image_idx);
    // Copy draw image into swapchain
    {
	    RBVK_Texture *draw_image = POOL_GET(&g_vk_ctx->texture_pool, g_vk_ctx->draw_image);

        // vk_helper_rbvk_texture_transition(cmd, draw_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        vk_helper_transition_image(cmd, draw_image->vk_image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        vk_helper_transition_image(cmd, swapchain_image->vk_image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        // vk_helper_rbvk_texture_transition(cmd, draw_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        vk_helper_copy_image_to_image(cmd, draw_image->vk_image, swapchain_image->vk_image, g_vk_ctx->draw_extent, g_vk_ctx->swapchain.extent);
        vk_helper_transition_image(cmd, swapchain_image->vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }

	VK_CHECK(vkEndCommandBuffer(cmd));
    VkCommandBufferSubmitInfo cmd_info = vk_command_buffer_submit_info(cmd);

    // Present image
    {
        VkSemaphoreSubmitInfo wait_info   = vk_semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, frame_data->semaphore_image_acquired);
        VkSemaphoreSubmitInfo signal_info = vk_semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, swapchain_image->semaphore_render_complete);

        VkSubmitInfo2 submit = vk_submit_info(&cmd_info, &signal_info, &wait_info);
        VK_CHECK(vkQueueSubmit2(g_vk_ctx->device.graphics_queue, 1, &submit, frame_data->render_complete_fence));

        VkPresentInfoKHR presentInfo = {
	        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
	        .pNext = NULL,
	        .pSwapchains = &g_vk_ctx->swapchain.swapchain,
	        .swapchainCount = 1,
	        .pWaitSemaphores = &signal_info.semaphore,
	        .waitSemaphoreCount = 1,
	        .pImageIndices = &frame_data->swapchain_image_idx,
	    };
	    VK_CHECK(vkQueuePresentKHR(g_vk_ctx->device.graphics_queue, &presentInfo));
	}
	rbvk_frame_counters_advance();
}

internal void
rbvk_frame_counters_advance() {
    // (jd) decide where tf  frame_overlap is going
    g_vk_ctx->previous_frame = g_vk_ctx->current_frame;
    g_vk_ctx->current_frame = (g_vk_ctx->current_frame + 1) % g_vk_ctx->base.frame_overlap;
    ++g_vk_ctx->absolute_frame;
}

/* ================================================================== */
/*  Overlay – Vulkan backend implementation                           */
/*                                                                    */
/*  Renders a 2-D overlay onto draw_image (before the blit to the     */
/*  swapchain).  All GPU memory comes from the existing RBVKMemory_Pools */
/*  (gpu_only for the font image, staging for vertex/index data).     */
/* ================================================================== */


#define RBVK_OVERLAY_MAX_VERTEX_BUFFER  (512 * 1024)
#define RBVK_OVERLAY_MAX_ELEMENT_BUFFER (128 * 1024)

typedef struct RBVK_OverlayPushConstants {
    f32 projection[4][4];
} RBVK_OverlayPushConstants;

typedef struct RBVK_OverlayState {
    VkPipeline           pipeline;
    VkPipelineLayout     pipeline_layout;
    VkDescriptorPool     descriptor_pool;
    VkDescriptorSetLayout global_set_layout;
    VkDescriptorSetLayout material_set_layout;
    VkDescriptorSet      descriptor_sets[2];
    u32                   descriptor_set_count;
    VkSampler            font_sampler;
    VkImage              font_image;
    VkImageView          font_image_view;
    VkShaderModule       vert_shader;
    VkShaderModule       frag_shader;
    VkRenderPass         render_pass;
    VkFramebuffer        framebuffer; /* single fb for draw_image */

    /* Offsets into the staging pool buffer for vertex/index data */
    u64                  vertex_offset;
    u64                  index_offset;
} RBVK_OverlayState;

global RBVK_OverlayState g_rbvk_overlay;

/* ------------------------------------------------------------------ */
/*  Render pass (overlay on draw_image, GENERAL → GENERAL)            */
/* ------------------------------------------------------------------ */

internal void
rbvk_overlay_create_render_pass(RBVK_OverlayState *overlay_state)
{
    VkDevice device = g_vk_ctx->device.vk_device;

    RBVK_Texture *draw_tex = POOL_GET(&g_vk_ctx->texture_pool, g_vk_ctx->draw_image);
    VkFormat color_format = draw_tex->vk_format;

    VkAttachmentDescription color_attachment = {
        .format         = color_format,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_GENERAL,
        .finalLayout    = VK_IMAGE_LAYOUT_GENERAL,
    };

    VkAttachmentReference color_ref = {
        .attachment = 0,
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &color_ref,
    };

    VkSubpassDependency dependency = {
        .srcSubpass    = VK_SUBPASS_EXTERNAL,
        .dstSubpass    = 0,
        .srcStageMask  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo rp_info = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &color_attachment,
        .subpassCount    = 1,
        .pSubpasses      = &subpass,
        .dependencyCount = 1,
        .pDependencies   = &dependency,
    };
    VK_CHECK(vkCreateRenderPass(device, &rp_info, NULL, &overlay_state->render_pass));
}

/* ------------------------------------------------------------------ */
/*  Single framebuffer (wrapping draw_image)                          */
/* ------------------------------------------------------------------ */

internal void
rbvk_overlay_create_framebuffer(RBVK_OverlayState *overlay_state)
{
    VkDevice device = g_vk_ctx->device.vk_device;
	RBVK_Texture *draw_image = POOL_GET(&g_vk_ctx->texture_pool, g_vk_ctx->draw_image);

    VkFramebufferCreateInfo fb_info = {
        .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass      = overlay_state->render_pass,
        .attachmentCount = 1,
        .pAttachments    = &draw_image->vk_image_view,
        .width           = g_vk_ctx->draw_extent.width,
        .height          = g_vk_ctx->draw_extent.height,
        .layers          = 1,
    };
    VK_CHECK(vkCreateFramebuffer(device, &fb_info, NULL, &overlay_state->framebuffer));
}

/* ------------------------------------------------------------------ */
/*  Font atlas → VkImage from gpu_only pool + staging upload          */
/* ------------------------------------------------------------------ */

// internal void
// rbvk_overlay_upload_font(RBVK_OverlayState *overlay_state, const void *pixels, int atlas_w, int atlas_h)
// {
//     VkDevice device = g_vk_ctx->device.vk_device;
//     RBVKMemory_Pools *pools = &g_vk_ctx->memory_pools;
//     VkDeviceSize image_size = (VkDeviceSize)atlas_w * atlas_h * 4;
//
//     // TODO: USE rbvk_texture_create
//     /* --- Create font image and bind to gpu_only pool --- */
//     VkImageCreateInfo img_info = {
//         .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
//         .imageType   = VK_IMAGE_TYPE_2D,
//         .format      = VK_FORMAT_R8G8B8A8_UNORM,
//         .extent      = { (u32)atlas_w, (u32)atlas_h, 1 },
//         .mipLevels   = 1,
//         .arrayLayers = 1,
//         .samples     = VK_SAMPLE_COUNT_1_BIT,
//         .tiling      = VK_IMAGE_TILING_OPTIMAL,
//         .usage       = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
//         .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//         .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
//     };
//     VK_CHECK(vkCreateImage(device, &img_info, NULL, &overlay_state->font_image));
//     VkMemory_Alloc font_alloc = rbvk_memory_gpu_image_alloc(pools, overlay_state->font_image);
//     VK_CHECK(vkBindImageMemory(device, overlay_state->font_image, font_alloc.vk_memory, font_alloc.offset));
//
//     /* --- Copy pixels through the staging buffer --- */
//     {
//         /* Map a region of the staging buffer and write pixels */
//         void *mapped;
//         VK_CHECK(vkMapMemory(device, pools->staging_buffer.vk_memory, 0, image_size, 0, &mapped));
//         MEMORY_COPY(mapped, pixels, (usize)image_size);
//         vkUnmapMemory(device, pools->staging_buffer.vk_memory);
//
//         /* One-shot command buffer for the upload */
//         VkCommandPool tmp_pool;
//         VkCommandPoolCreateInfo pool_ci = {
//             .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
//             .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
//             .queueFamilyIndex = g_vk_ctx->device.graphics_queue_idx,
//         };
//         VK_CHECK(vkCreateCommandPool(device, &pool_ci, NULL, &tmp_pool));
//
//         VkCommandBuffer cmd;
//         VkCommandBufferAllocateInfo cmd_alloc = {
//             .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
//             .commandPool        = tmp_pool,
//             .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
//             .commandBufferCount = 1,
//         };
//         VK_CHECK(vkAllocateCommandBuffers(device, &cmd_alloc, &cmd));
//
//         VkCommandBufferBeginInfo begin_info = {
//             .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
//             .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
//         };
//         VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));
//
//         VkImageMemoryBarrier barrier_to_dst = {
//             .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
//             .srcAccessMask       = 0,
//             .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
//             .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
//             .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
//             .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
//             .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
//             .image               = overlay_state->font_image,
//             .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
//         };
//         vkCmdPipelineBarrier(cmd,
//             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
//             0, 0, NULL, 0, NULL, 1, &barrier_to_dst);
//
//         VkBufferImageCopy region = {
//             .bufferOffset      = 0, /* start of the staging buffer */
//             .bufferRowLength   = 0,
//             .bufferImageHeight = 0,
//             .imageSubresource  = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
//             .imageOffset       = {0, 0, 0},
//             .imageExtent       = { (u32)atlas_w, (u32)atlas_h, 1 },
//         };
//         vkCmdCopyBufferToImage(cmd, pools->staging_buffer.vk_buffer, overlay_state->font_image,
//                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
//
//         VkImageMemoryBarrier barrier_to_read = {
//             .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
//             .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
//             .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
//             .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
//             .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//             .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
//             .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
//             .image               = overlay_state->font_image,
//             .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
//         };
//         vkCmdPipelineBarrier(cmd,
//             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
//             0, 0, NULL, 0, NULL, 1, &barrier_to_read);
//
//         VK_CHECK(vkEndCommandBuffer(cmd));
//
//         VkSubmitInfo submit = {
//             .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
//             .commandBufferCount = 1,
//             .pCommandBuffers    = &cmd,
//         };
//         VK_CHECK(vkQueueSubmit(g_vk_ctx->device.graphics_queue, 1, &submit, VK_NULL_HANDLE));
//         VK_CHECK(vkQueueWaitIdle(g_vk_ctx->device.graphics_queue));
//
//         vkDestroyCommandPool(device, tmp_pool, NULL);
//     }
//
//     /* --- Image view --- */
//     VkImageViewCreateInfo view_info = {
//         .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
//         .image    = overlay_state->font_image,
//         .viewType = VK_IMAGE_VIEW_TYPE_2D,
//         .format   = VK_FORMAT_R8G8B8A8_UNORM,
//         .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
//     };
//     (void)(view_info);
//     // VK_CHECK(vkCreateImageView(device, &view_info, NULL, &overlay_state->font_image_view));
//
//     /* --- Sampler --- */
//     VkSamplerCreateInfo sampler_info = {
//         .sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
//         .magFilter    = VK_FILTER_LINEAR,
//         .minFilter    = VK_FILTER_LINEAR,
//         .mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR,
//         .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
//         .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
//         .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
//     };
//     VK_CHECK(vkCreateSampler(device, &sampler_info, NULL, &overlay_state->font_sampler));
// }

/* ------------------------------------------------------------------ */
/*  Descriptor set (combined image sampler for font texture)          */
/* ------------------------------------------------------------------ */

// internal void
// rbvk_overlay_create_descriptors(RBVK_OverlayState *overlay_state)
// {
//     VkDevice device = g_vk_ctx->device.vk_device;
//
//     VkDescriptorSetLayoutCreateInfo layout_info = {
//         .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
//         .bindingCount = 1,
//         .pBindings    = &(VkDescriptorSetLayoutBinding) {
//             .binding         = 0,
//             .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
//             .descriptorCount = 1,
//             .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
//         },
//
//     };
//     VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_info, NULL, &overlay_state->material_set_layout));
//
//     VkDescriptorSetLayoutCreateInfo ubo_layout_info = {
//         .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
//         .bindingCount = 1,
//         .pBindings = &(VkDescriptorSetLayoutBinding){
//             .binding = 0,
//             .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
//             .descriptorCount = 1,
//             .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
//
//         },
//     };
//     VK_CHECK(vkCreateDescriptorSetLayout(device, &ubo_layout_info, NULL, &overlay_state->global_set_layout));
//     VkDescriptorPoolSize pool_sizes[] = {
//         { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1 },          // for set 0
//         { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1 },  // for set 1
//     };
//     VkDescriptorPoolCreateInfo pool_info = {
//         .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
//         .poolSizeCount = DOT_ARRAY_COUNT(pool_sizes),
//         .pPoolSizes    = pool_sizes,
//         .maxSets       = 2,
//     };
//
//     VkDescriptorSetLayout layouts[] = {
//         overlay_state->global_set_layout,
//         overlay_state->material_set_layout,
//     };
//     VK_CHECK(vkCreateDescriptorPool(device, &pool_info, NULL, &overlay_state->descriptor_pool));
//     VkDescriptorSetAllocateInfo dset_alloc = {
//         .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
//         .descriptorPool     = overlay_state->descriptor_pool,
//         .descriptorSetCount = DOT_ARRAY_COUNT(layouts),
//         .pSetLayouts        = layouts,
//     };
//
//     overlay_state->descriptor_set_count = DOT_ARRAY_COUNT(layouts);
//     VK_CHECK(vkAllocateDescriptorSets(device, &dset_alloc, overlay_state->descriptor_sets));
//
//     VkWriteDescriptorSet write = {
//         .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
//         .dstSet          = overlay_state->descriptor_sets[1],
//         .dstBinding      = 0,
//         .descriptorCount = 1,
//         .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
//         .pImageInfo      = &(VkDescriptorImageInfo){
//             .sampler     = overlay_state->font_sampler,
//             .imageView   = overlay_state->font_image_view,
//             .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//         },
//     };
//     vkUpdateDescriptorSets(device, 1, &write, 0, NULL);
//
//     typedef struct {
//         f32 projection[4][4];
//         // mat4 projection;
//     } OverlayUBO;
//
//     // VkBufferCreateInfo buf_info = {
//     //     .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//     //     .size = sizeof(OverlayUBO),
//     //     .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
//     //     .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//     // };
//     //
//     //
//     // TODO: Update the 500 to match viewport
//     OverlayUBO ubo_data = {0};
//     ubo_data.projection[0][0] =  2.0f / 500;
//     ubo_data.projection[1][1] = -2.0f / 500;
//     ubo_data.projection[2][2] = -1.0f;
//     ubo_data.projection[3][0] = -1.0f;
//     ubo_data.projection[3][1] =  1.0f;
//     ubo_data.projection[3][3] =  1.0f;
//
//     RBVK_Buffer overlay_ubo_buffer = rbvk_buffer_create2(
//         sizeof(OverlayUBO),
//         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
//         // VkMemory_PoolsKind_Staging,
//         String8Lit("OverlayUBO"));
//     // VkDeviceMemory       ubo_memory;
//     // VkDeviceSize         ubo_offset;
//
//     void* mapped;
//     vkMapMemory(device,
//         overlay_ubo_buffer.alloc.vk_memory,
//         overlay_ubo_buffer.alloc.offset,
//         sizeof(ubo_data),
//         0,
//         &mapped);
//
//     memcpy(mapped, &ubo_data, sizeof(ubo_data));
//     vkUnmapMemory(device, overlay_ubo_buffer.alloc.vk_memory);
//
//     VkDescriptorBufferInfo buffer_info = {
//         .buffer = overlay_ubo_buffer.vk_buffer,
//         .offset = 0,
//         .range  = sizeof(OverlayUBO),
//     };
//
//     VkWriteDescriptorSet write_ubo = {
//         .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
//         .dstSet = g_rbvk_overlay.descriptor_sets[0],      // set = 0
//         .dstBinding = 0,                   // binding = 0
//         .descriptorCount = 1,
//         .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
//         .pBufferInfo = &buffer_info,
//     };
//
//     vkUpdateDescriptorSets(device, 1, &write_ubo, 0, NULL);
// }

/* ------------------------------------------------------------------ */
/*  Graphics pipeline                                                 */
/* ------------------------------------------------------------------ */

internal void
rbvk_overlay_create_pipeline(RBVK_OverlayState *overlay_state)
{
    VkDevice device = g_vk_ctx->device.vk_device;

    VkShaderModuleCreateInfo vert_create_info = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(nuklearshaders_nuklear_vert_spv),
        .pCode    = cast(u32*) nuklearshaders_nuklear_vert_spv,
    };
    VK_CHECK(vkCreateShaderModule(device, &vert_create_info, NULL, &overlay_state->vert_shader));

    VkShaderModuleCreateInfo frag_create_info = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(nuklearshaders_nuklear_frag_spv),
        .pCode    = cast(u32*) nuklearshaders_nuklear_frag_spv,
    };
    VK_CHECK(vkCreateShaderModule(device, &frag_create_info, NULL, &overlay_state->frag_shader));

    VkPipelineShaderStageCreateInfo stages[2] = {
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_VERTEX_BIT,
            .module = overlay_state->vert_shader,
            .pName  = "main",
        },
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = overlay_state->frag_shader,
            .pName  = "main",
        },
    };

    VkVertexInputBindingDescription vtx_binding = {
        .binding   = 0,
        .stride    = sizeof(OverlayVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    VkVertexInputAttributeDescription attrs[3] = {
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT,
          .offset = offsetof(OverlayVertex, position) },
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT,
          .offset = offsetof(OverlayVertex, uv) },
        { .location = 2, .binding = 0, .format = VK_FORMAT_R8G8B8A8_UINT,
          .offset = offsetof(OverlayVertex, col) },
    };

    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &vtx_binding,
        .vertexAttributeDescriptionCount = DOT_ARRAY_COUNT(attrs),
        .pVertexAttributeDescriptions    = attrs,
    };

    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset     = 0,
        .size       = sizeof(RBVK_OverlayPushConstants),
    };

    VkDescriptorSetLayout set_layouts[] = {
        overlay_state->global_set_layout,   // set 0
        overlay_state->material_set_layout,  // set 1
    };

    VkPipelineLayoutCreateInfo pipe_layout_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = DOT_ARRAY_COUNT(set_layouts),
        .pSetLayouts            = set_layouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push_range,
    };

    VK_CHECK(vkCreatePipelineLayout(device, &pipe_layout_info, NULL, &overlay_state->pipeline_layout));

    VkGraphicsPipelineCreateInfo pipeline_create_infos = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount          = DOT_ARRAY_COUNT(stages),
        .pStages             = stages,
        .pVertexInputState   = &vertex_input,
        .pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo) {
            .sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        },
        .pViewportState = &(VkPipelineViewportStateCreateInfo){
            .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount  = 1,
        } ,
        .pRasterizationState = &(VkPipelineRasterizationStateCreateInfo){
            .sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode    = VK_CULL_MODE_NONE,
            .frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .lineWidth   = 1.0f,
        },
        .pMultisampleState   = &(VkPipelineMultisampleStateCreateInfo){
            .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        },
        .pDepthStencilState  = &(VkPipelineDepthStencilStateCreateInfo){
            .sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable  = VK_FALSE,
            .depthWriteEnable = VK_FALSE,
        },
        .pColorBlendState    = &(VkPipelineColorBlendStateCreateInfo){
            .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments    = &(VkPipelineColorBlendAttachmentState){
                .blendEnable         = VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp        = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp        = VK_BLEND_OP_ADD,
                .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            },
        },
        .pDynamicState       = &(VkPipelineDynamicStateCreateInfo) {
            .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = DOT_ARRAY_COUNT(dynamic_states),
            .pDynamicStates    = dynamic_states,
        },
        .layout              = overlay_state->pipeline_layout,
        .renderPass          = overlay_state->render_pass,
        .subpass             = 0,
    };
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_create_infos, NULL, &overlay_state->pipeline));
}

/* ------------------------------------------------------------------ */
/*  Reserve vertex / index regions from the staging pool              */
/* ------------------------------------------------------------------ */

// internal void
// rbvk_overlay_reserve_buffers(RBVK_OverlayState *s)
// {
//     RBVKMemory_Pools *pools = &g_vk_ctx->memory_pools;
//
//     /* Vertex region */
//     VkMemoryRequirements vtx_reqs = {
//         .size           = RBVK_OVERLAY_MAX_VERTEX_BUFFER,
//         .alignment      = 16,
//         .memoryTypeBits = (1u << pools->staging_type),
//     };
//     VkMemory_Alloc vtx_alloc = vk_memory_pools_bump(pools, vtx_reqs, VkMemory_PoolsKind_Staging);
//     s->vertex_offset = vtx_alloc.offset;
//     /* Manually advance past the allocation (pool bump for staging doesn't add size) */
//     pools->staging_used = vtx_alloc.offset + RBVK_OVERLAY_MAX_VERTEX_BUFFER;
//
//     /* Index region */
//     VkMemoryRequirements idx_reqs = {
//         .size           = RBVK_OVERLAY_MAX_ELEMENT_BUFFER,
//         .alignment      = 16,
//         .memoryTypeBits = (1u << pools->staging_type),
//     };
//     VkMemory_Alloc idx_alloc = vk_memory_pools_bump(pools, idx_reqs, VkMemory_PoolsKind_Staging);
//     s->index_offset = idx_alloc.offset;
//     pools->staging_used = idx_alloc.offset + RBVK_OVERLAY_MAX_ELEMENT_BUFFER;
// }

/* ------------------------------------------------------------------ */
/*  Public backend interface                                          */
/* ------------------------------------------------------------------ */

internal void
renderer_backend_vk_overlay_init(const void *font_pixels, int font_w, int font_h)
{
    DOT_UNUSED(font_pixels); DOT_UNUSED(font_w); DOT_UNUSED(font_h);
    MEMORY_ZERO_STRUCT(&g_rbvk_overlay);
    rbvk_overlay_create_render_pass(&g_rbvk_overlay);
    rbvk_overlay_create_framebuffer(&g_rbvk_overlay);
    // rbvk_overlay_upload_font(&g_rbvk_overlay, font_pixels, font_w, font_h);
    // rbvk_overlay_create_descriptors(&g_rbvk_overlay);
    rbvk_overlay_create_pipeline(&g_rbvk_overlay);
    // rbvk_overlay_reserve_buffers(&g_rbvk_overlay);
}

internal void
renderer_backend_vk_overlay_render(u8 frame_idx, OverlayDrawList *draw_list)
{
    (void)frame_idx; (void)draw_list;
    // RBVK_FrameData *fd = &g_vk_ctx->frame_datas[frame_idx];
    // VkCommandBuffer cmd = fd->frame_command_buffer;
    // VkDevice device = g_vk_ctx->device.vk_device;
    // RBVKMemory_Pools *pools = &g_vk_ctx->memory_pools;
    //
    // /* Upload vertex data into the staging buffer region */
    // if (draw_list->vertex_size > 0) {
    //     void *mapped;
    //     VK_CHECK(vkMapMemory(device, pools->staging_mem, g_rbvk_overlay.vertex_offset,
    //                          draw_list->vertex_size, 0, &mapped));
    //     MEMORY_COPY(mapped, draw_list->vertices, draw_list->vertex_size);
    //     vkUnmapMemory(device, pools->staging_mem);
    // }
    //
    // /* Upload index data into the staging buffer region */
    // if (draw_list->index_size > 0) {
    //     void *mapped;
    //     VK_CHECK(vkMapMemory(device, pools->staging_mem, g_rbvk_overlay.index_offset,
    //                          draw_list->index_size, 0, &mapped));
    //     MEMORY_COPY(mapped, draw_list->indices, draw_list->index_size);
    //     vkUnmapMemory(device, pools->staging_mem);
    // }
    //
    // /* Begin overlay render pass on draw_image */
    // VkRenderPassBeginInfo rp_begin = {
    //     .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    //     .renderPass  = g_rbvk_overlay.render_pass,
    //     .framebuffer = g_rbvk_overlay.framebuffer,
    //     .renderArea  = { {0, 0}, g_vk_ctx->draw_extent },
    // };
    // vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
    //
    // vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_rbvk_overlay.pipeline);
    // vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
    //     g_rbvk_overlay.pipeline_layout, 0, 2, g_rbvk_overlay.descriptor_sets, 0, NULL);
    //
    // /* Bind vertex/index buffers from the staging pool */
    // VkDeviceSize vb_offset = g_rbvk_overlay.vertex_offset;
    // vkCmdBindVertexBuffers(cmd, 0, 1, &pools->staging_buffer, &vb_offset);
    // vkCmdBindIndexBuffer(cmd, pools->staging_buffer, g_rbvk_overlay.index_offset, VK_INDEX_TYPE_UINT16);
    //
    // /* Push orthographic projection */
    // {
    //     RBVK_OverlayPushConstants pc;
    //     MEMORY_ZERO_STRUCT(&pc);
    //     pc.projection[0][0] =  2.0f / (f32)draw_list->width;
    //     pc.projection[1][1] = -2.0f / (f32)draw_list->height;
    //     pc.projection[2][2] = -1.0f;
    //     pc.projection[3][0] = -1.0f;
    //     pc.projection[3][1] =  1.0f;
    //     pc.projection[3][3] =  1.0f;
    //     vkCmdPushConstants(cmd, g_rbvk_overlay.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT,
    //                        0, sizeof(RBVK_OverlayPushConstants), &pc);
    // }
    //
    // VkViewport vp = { 0, 0, (f32)draw_list->width, (f32)draw_list->height, 0.0f, 1.0f };
    // vkCmdSetViewport(cmd, 0, 1, &vp);
    //
    // /* Draw commands */
    // u32 index_offset = 0;
    // for (u32 i = 0; i < draw_list->cmd_count; ++i) {
    //     OverlayDrawCmd *dc = &draw_list->cmds[i];
    //     if (!dc->elem_count) continue;
    //
    //     VkRect2D scissor = {
    //         .offset = {
    //             .x = MAX((i32)dc->clip_x, 0),
    //             .y = MAX((i32)dc->clip_y, 0),
    //         },
    //         .extent = {
    //             .width  = (u32)dc->clip_w,
    //             .height = (u32)dc->clip_h,
    //         },
    //     };
    //     vkCmdSetScissor(cmd, 0, 1, &scissor);
    //     vkCmdDrawIndexed(cmd, dc->elem_count, 1, index_offset, 0, 0);
    //     index_offset += dc->elem_count;
    // }
    //
    // vkCmdEndRenderPass(cmd);
}

internal void
renderer_backend_vk_overlay_shutdown(void)
{
    VkDevice device = g_vk_ctx->device.vk_device;
    vkDeviceWaitIdle(device);

    vkDestroyPipeline(device, g_rbvk_overlay.pipeline, NULL);
    vkDestroyPipelineLayout(device, g_rbvk_overlay.pipeline_layout, NULL);
    vkDestroyShaderModule(device, g_rbvk_overlay.vert_shader, NULL);
    vkDestroyShaderModule(device, g_rbvk_overlay.frag_shader, NULL);

    vkDestroyDescriptorPool(device, g_rbvk_overlay.descriptor_pool, NULL);
    vkDestroyDescriptorSetLayout(device, g_rbvk_overlay.material_set_layout, NULL);
    vkDestroyDescriptorSetLayout(device, g_rbvk_overlay.global_set_layout, NULL);

    vkDestroySampler(device, g_rbvk_overlay.font_sampler, NULL);
    vkDestroyImageView(device, g_rbvk_overlay.font_image_view, NULL);
    vkDestroyImage(device, g_rbvk_overlay.font_image, NULL);
    /* font image memory is owned by the gpu_only pool – not freed here */

    vkDestroyFramebuffer(device, g_rbvk_overlay.framebuffer, NULL);
    vkDestroyRenderPass(device, g_rbvk_overlay.render_pass, NULL);
    /* vertex/index memory is owned by the staging pool – not freed here */

    MEMORY_ZERO_STRUCT(&g_rbvk_overlay);
}

internal void
renderer_backend_vk_shutdown()
{
    VkDevice device = g_vk_ctx->device.vk_device;

    vkDeviceWaitIdle(device);
    {
        vkDestroyDescriptorSetLayout(device, g_vk_ctx->compute_layout, NULL);
        vkDestroyDescriptorSetLayout(device, g_vk_ctx->bindless_layout, NULL);
        vkDestroyDescriptorPool(device, g_vk_ctx->descriptor_pool, NULL);
    }

    for(u32 i = 0; i < g_vk_ctx->base.frame_overlap; ++i){
        RBVK_FrameData *frame_data = &SLICE_GET(g_vk_ctx->frame_datas, i);
        for(u32 j = 0; j <  frame_data->vk_command_pools.count; ++j){
            VkCommandPool cmd_pool = ARRAY_GET(frame_data->vk_command_pools, j);
            vkDestroyCommandPool(device, cmd_pool, NULL);
        }
        vkDestroyFence(device, frame_data->render_complete_fence, NULL);
        vkDestroySemaphore(device, frame_data->semaphore_image_acquired, NULL);
    }

    for(u32 i = 0; i < g_vk_ctx->swapchain.swapchain_images.count; ++i){
        RBVK_SwapchainImage *swapchain_image = &SLICE_GET(g_vk_ctx->swapchain.swapchain_images, i);
        vkDestroyImageView(device, swapchain_image->vk_image_view, NULL);
        vkDestroySemaphore(device, swapchain_image->semaphore_render_complete, NULL);
    }

    vkDestroyPipelineLayout(device, g_vk_ctx->gradient_pipeline_layout, NULL);
    vkDestroyPipeline(device, g_vk_ctx->gradient_pipeline, NULL);

    renderer_backend_vk_resource_cleanup_list_pop_all();

    vkDestroySwapchainKHR(device, g_vk_ctx->swapchain.swapchain, NULL);
    vk_memory_pools_destroy(&g_vk_ctx->device, &g_vk_ctx->memory_pools);
    vkDestroySurfaceKHR(g_vk_ctx->instance, g_vk_ctx->surface, NULL);
    vkDestroyDevice(g_vk_ctx->device.vk_device, NULL);
    if(VK_EXT_DEBUG_UTILS_ENABLE){
#ifndef DOT_USE_VOLK
        PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT =
            (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(g_vk_ctx->instance, "vkDestroyDebugUtilsMessengerEXT");
#endif
        if(vkDestroyDebugUtilsMessengerEXT){
            vkDestroyDebugUtilsMessengerEXT(g_vk_ctx->instance, g_vk_ctx->debug_messenger, NULL);
        }
    }
    vkDestroyInstance(g_vk_ctx->instance, NULL);
}
