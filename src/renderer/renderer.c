DOT_SETTING_U32("Render", g_rn_transient_memory_size_b, DOT_MB(80));
DOT_SETTING_U32("Render", g_rn_permanent_memory_size_b, DOT_MB(80));
DOT_SETTING_U32("Render", g_frame_arena_size_b, DOT_KB(32));
DOT_SETTING_U32("Render", g_cleanup_resource_pool_max, 128);

DOT_SETTING_U32("Render", g_frame_overlap, 3);
DOT_SETTING_U32("Render", g_thread_count, 1);

DOT_SETTING_U32("Render", g_draw_texture_format, RN_TextureFormatKind_RGBA16F);
DOT_SETTING_U32("Render", g_swapchaing_texture_format, RN_TextureFormatKind_RGBA8_SRGB);
DOT_SETTING_U32("Render", g_depth_stencil_format, RN_TextureFormatKind_D32F);
DOT_SETTING_U32("Render", g_present_mode, RN_PresentModeKind_Mailbox);

DOT_SETTING_U32("RenderBackend", g_render_backend_transient_memory_size_b, DOT_KB(512));
DOT_SETTING_U32("RenderBackend", g_render_backend_permanent_memory_size_b, DOT_MB(4));
DOT_SETTING_U32("RenderBackend", g_texture_max, 512);
DOT_SETTING_U32("RenderBackend", g_buffer_max, 4096);
DOT_SETTING_U32("RenderBackend", g_sampler_max, 128);
DOT_SETTING_U32("RenderBackend", rn_vk_g_shader_resource_max, 128);
DOT_SETTING_U32("RenderBackend", g_command_buffers_per_thread, 4);

DOT_SETTING_U32("ShaderCache", g_shader_cache_bucket_count, 64);

#if defined(DOT_RENDER_BACKEND_ONLY_DX12)
#   if DOT_OS_POSIX
#       error Unavaliable backend on linux!
#   endif
    DOT_SETTING_U32("RenderBackend", g_backend_kind, RN_BackendKind_Dx12);
#elif defined(DOT_RENDER_BACKEND_ONLY_VK)
    DOT_SETTING_U32("RenderBackend", g_backend_kind, RN_BackendKind_Vk);
#else
    DOT_SETTING_U32("RenderBackend", g_backend_kind, RN_BackendKind_Auto);
#endif

internal RN_BackendCtx *
rn_backend_create(Arena *arena)
{
    switch(g_backend_kind){
    case RN_BackendKind_Null: return cast(RN_BackendCtx*) rn_null_create(arena);
    case RN_BackendKind_Vk:   return cast(RN_BackendCtx*) rn_vk_create(arena);
    case RN_BackendKind_Dx12: //base = cast(RN_BackendCtx*) rn_renderer_dx12_create(arena); break;
    default: DOT_ERROR("Unsupported render backend");
    }
}

internal void
rn_init(Arena *arena, RN_RenderCtx *renderer, DOT_Window *window)
{
    renderer->permanent_arena = ARENA_CREATE(
        .parent       = arena,
        .reserve_size = g_rn_permanent_memory_size_b,
        .name         = "Application render permanent");

    renderer->transient_arena = ARENA_CREATE(
        .parent       = arena,
        .reserve_size = g_rn_transient_memory_size_b,
        .name         = "Application renderer transient");

    TREE_INIT(renderer->permanent_arena, RN_ResourceCleanupCtx, &renderer->cleanup_tree, g_cleanup_resource_pool_max);
    hash_map_RN_ShaderModule_init(renderer->permanent_arena, &renderer->shader_cache, g_shader_cache_bucket_count);

    renderer->backend = rn_backend_create(renderer->permanent_arena);
    renderer->frame_data_count = g_frame_overlap;
    renderer->frame_datas    = PUSH_ARRAY(renderer->permanent_arena, FrameData, renderer->frame_data_count);
    for(u8 i = 0; i < renderer->frame_data_count; ++i){
        renderer->frame_datas[i].temp_arena = ARENA_CREATE(
            .parent       = renderer->permanent_arena,
            .reserve_size = g_frame_arena_size_b);
    }
    RENDER_BACKEND_CALL(init(window));
    null_texture = rn_texture_create_h(
        renderer,
        &(RN_TextureDesc) {
            .width = 1,
            .height = 1,
            .depth = 1,
            .mip_levels = 1,
            .format_kind = RN_TextureFormatKind_RGBA8_UNORM,
            .dimension_kind = RN_TextureDimensionKind_2D,
        },
        (u8[]){0,0,0,0},
        string8_lit("null_texture")
    );
    (void)null_texture;
}

internal void
rn_shutdown(RN_RenderCtx *renderer)
{
    HashMap_RN_ShaderModule *shader_cache = &renderer->shader_cache;
    HashMap_Iter it = hash_map_iter_init();
    RN_ShaderModule *shader_module = NULL;
    while((shader_module = hash_map_RN_ShaderModule_iter_next(shader_cache, &it))){
            RN_ShaderModuleHandle h = shader_module->shader_module_handle;
            RENDER_BACKEND_CALL(shader_unload(h));
    }
    hash_map_RN_ShaderModule_end(shader_cache);
    rn_resource_cleanup_list_pop_all(renderer);
    RENDER_BACKEND_CALL(shutdown());
}

void
rn_texture_destroy(RN_RenderCtx *renderer, RN_TextureHandle handle)
{
    RENDER_BACKEND_CALL(texture_destroy(handle));
}

void
rn_buffer_destroy(RN_RenderCtx *renderer, RN_BufferHandle handle)
{
    RENDER_BACKEND_CALL(buffer_destroy(handle));
}

void
rn_sampler_destroy(RN_RenderCtx *renderer, RN_SamplerHandle handle)
{
    RENDER_BACKEND_CALL(sampler_destroy(handle));
}

internal RN_TextureHandle
rn_texture_create_h(RN_RenderCtx *renderer, RN_TextureDesc *texture_desc, void *data, String8 debug_name)
{
    RN_TextureHandle h = RENDER_BACKEND_CALL(texture_create(texture_desc, data, debug_name));
    rn_resource_cleanup_list_push_texture(renderer, h);
    return(h);
}

// (jd) NOTE: Renderer shouldn't be doing the loading
RN_Texture
rn_texture_load_from_path(RN_RenderCtx *renderer, String8 name, String8 path, u8 mip_levels)
{
    DOT_WARNING("Creating texture with name %S, from path: ", name, path);
    TempArena temp = temp_arena_start(renderer->transient_arena);
    String8 file_data = platform_read_entire_file(temp.arena, path);

    void *data = NULL;
    u8 size_bytes = 0;
    b32 is_hdr = cast(b32)stbi_is_hdr_from_memory(file_data.str, cast(int)file_data.size);
    b32 is_16_bit = is_hdr || stbi_is_16_bit_from_memory(file_data.str, cast(int)file_data.size);
    int width = 0;
    int height = 0;
    int comp = 0;
    stbi_allocator alloc = {.stbi_alloc = arena_push, .stbi_free = arena_free, .arena = temp.arena};
    stbi_allocator_set(&alloc);
    stbi_info_from_memory(file_data.str, cast(int)file_data.size, &width, &height, &comp);
    comp = comp == 3 ? STBI_rgb_alpha : comp;
    if(is_hdr){
        data = stbi_loadf_from_memory(file_data.str, cast(int)file_data.size, &width, (int*)&height, NULL, comp);
        size_bytes = 4;
    }else if(is_16_bit){
        data = stbi_load_16_from_memory(file_data.str, cast(int)file_data.size, &width, &height, NULL, comp);
        size_bytes = 2;
    }else{
        data = stbi_load_from_memory(file_data.str, cast(int)file_data.size, &width, &height, NULL, comp);
        size_bytes = 1;
    }

    if(mip_levels == 0){
        int mip_w = width;
        int mip_h = height;
        mip_levels = 1;
        while(mip_w > 1 || mip_h > 1){
            mip_w = mip_w > 1 ? mip_w / 2 : 1;
            mip_h = mip_h > 1 ? mip_h / 2 : 1;
            mip_levels++;
        }
    }
    RN_Texture res = {
        .resource.kind = RN_ResourceKind_Texture,
        .desc = {
            .dimension_kind = RN_TextureDimensionKind_2D,
            .format_kind = rn_texture_format_from_info(comp, size_bytes, true),
            .mip_levels = mip_levels,
            .width = cast(u16)width,
            .height = cast(u16)height,
            .depth = 1,
        },
    };
    res.handle = rn_texture_create_h(renderer, &res.desc, data, name),
    stbi_allocator_unset();
    temp_arena_restore(temp);
    return res;
}

internal RN_SamplerHandle
rn_sampler_create_h(RN_RenderCtx *renderer, RN_SamplerDesc *sampler_desc, String8 debug_name)
{
    RN_SamplerHandle h = RENDER_BACKEND_CALL(sampler_create(sampler_desc, debug_name));
    rn_resource_cleanup_list_push_sampler(renderer, h);
    return(h);
}

internal RN_Sampler
rn_sampler_create(RN_RenderCtx *renderer, RN_SamplerDesc *sampler_desc, String8 debug_name)
{
    RN_Sampler sampler = {
        .resource.kind = RN_ResourceKind_Sampler,
        // .asset = dot_asset_from_create_info(renderer, asset_info, RN_ResourceKind_Sampler), .desc = *sampler_desc,
        .handle = rn_sampler_create_h(renderer, sampler_desc, debug_name),
    };
    return sampler;
}

internal RN_BufferHandle
rn_buffer_create_h(RN_RenderCtx *renderer, RN_BufferDesc *buffer_desc, u8 *data, String8 debug_name)
{
    RN_BufferHandle h = RENDER_BACKEND_CALL(buffer_create(buffer_desc, data, debug_name));
    rn_resource_cleanup_list_push_buffer(renderer, h);
    return(h);
}

internal RN_Buffer
rn_buffer_create(RN_RenderCtx *renderer, RN_BufferDesc *buffer_desc, u8 *data, String8 debug_name)
{
    RN_Buffer buffer_asset = {
        .resource.kind = RN_ResourceKind_Buffer,
        .desc = *buffer_desc,
    };
    buffer_asset.handle = rn_buffer_create_h(renderer, buffer_desc, data, debug_name);
    return buffer_asset;
}

internal RN_PipelineHandle
rn_pipeline_create(RN_RenderCtx *renderer, RN_PipelineDesc *pipeline_desc)
{
    RN_PipelineHandle h = RENDER_BACKEND_CALL(pipeline_create(pipeline_desc));
    return(h);
}

internal RN_ShaderResourceLayoutHandle
rn_shader_resource_layout_create(RN_RenderCtx *renderer, RN_ShaderResourceLayoutDesc *desc)
{
    RN_ShaderResourceLayoutHandle h = RENDER_BACKEND_CALL(shader_resource_layout_create(desc));
    // rn_resource_cleanup_list_push_buffer(renderer, h);
    return(h);
}

internal RN_RenderPassOutput
rn_swapchain_output()
{
    RN_RenderPassOutput rpo = {0};
    rpo.depth_stencil_format = g_depth_stencil_format;
    RN_TextureFormatKind texture_fmt = g_swapchaing_texture_format;
    ARRAY_PUSH(rpo.color_formats, texture_fmt);
    return(rpo);
}


void
rn_clear_background(RN_RenderCtx *renderer, vec3 color)
{
    RENDER_BACKEND_CALL(clear_bg(color));
}

internal void
rn_frame_begin(RN_RenderCtx *renderer)
{
    RENDER_BACKEND_CALL(frame_begin());
}

internal void
rn_frame_end(RN_RenderCtx *renderer)
{
    RENDER_BACKEND_CALL(frame_end());
}

internal void
rn_vk_resource_cleanup_list_push_scope()
{
}

// NOTE(JD): Will start to just make enclosing scopes
// Should extend to be a graph
internal void
rn_resource_cleanup_list_push_texture(RN_RenderCtx *renderer, RN_TextureHandle texture_id)
{
    RN_ResourceCleanupCtx *root = TREE_GET_ROOT(&renderer->cleanup_tree);
    ARRAY_PUSH(root->texture_ids, texture_id);
}

internal void
rn_resource_cleanup_list_push_sampler(RN_RenderCtx *renderer, RN_SamplerHandle sampler_id)
{
    RN_ResourceCleanupCtx *root = TREE_GET_ROOT(&renderer->cleanup_tree);
    ARRAY_PUSH(root->sampler_ids, sampler_id);
}

internal void
rn_resource_cleanup_list_push_buffer(RN_RenderCtx *renderer, RN_BufferHandle buffer_id)
{
    RN_ResourceCleanupCtx *root = TREE_GET_ROOT(&renderer->cleanup_tree);
    ARRAY_PUSH(root->buffer_ids, buffer_id);
}

// internal void
// rn_resource_cleanup_list_push_child(RN_VK_ResourceCleanupCtx *parent, u32 idx){
//     ASSERT(idx < g_vk_ctx->cleanup_list_idx, "Idx must be an active resource list!");
//     // cleanup_list
//     RN_VK_ResourceCleanupCtx *elems = &g_vk_ctx->cleanup_list[g_vk_ctx->cleanup_list_idx];
//     elems.
//     RN_VK_TEXURE_MAX
//     g_vk_ctx->cleanup_list_idx += 1;
// }

internal void
rn_resource_cleanup_list_pop_last()
{
}

internal void
rn_resource_cleanup_list_pop_at(PoolHandle pop_start)
{
    (void)pop_start;
}

internal void
rn_resource_cleanup_list_pop_all(RN_RenderCtx *renderer)
{
    TempArena t = threadctx_temp_begin(0);
    RN_ResourceCleanupTree *tree = &renderer->cleanup_tree;
    TreeIterator it = TREE_ITER_BEGIN(t.arena, tree, tree->tree_pool.tree_root);
    for EACH_TREE_NODE(node_h, &it){
        RN_ResourceCleanupCtx *cleanup_list = TREE_GET(tree, node_h);
        for EACH_INDEX(i, cleanup_list->texture_ids.count){
            RN_TextureHandle tex_h = ARRAY_GET(cleanup_list->texture_ids, i);
            rn_texture_destroy(renderer, tex_h);
        }
        for EACH_INDEX(i, cleanup_list->buffer_ids.count){
            RN_BufferHandle buff_h = ARRAY_GET(cleanup_list->buffer_ids, i);
            rn_buffer_destroy(renderer, buff_h);
        }
        for EACH_INDEX(i, cleanup_list->sampler_ids.count){
            RN_SamplerHandle sampler_h = ARRAY_GET(cleanup_list->sampler_ids, i);
            rn_sampler_destroy(renderer, sampler_h);
        }
    }
    threadctx_temp_end(t);
}

RN_ShaderModule*
rn_shader_module_load_from_path(RN_RenderCtx *renderer, String8 path)
{
    TempArena temp = threadctx_temp_begin(0);
    String8 compiled_path = rn_shader_cache_get_compiled_path(renderer->permanent_arena, path);
    b32 source_updated = platform_file_is_newer(path, compiled_path);
    b32 compilation_success = false;
    if(source_updated){
        DOT_PRINT("Recompiling %S", path);
        compilation_success = rn_shader_compile_from_path(path, compiled_path);
        if(!compilation_success){
            DOT_WARNING("Failed to compile shader %S", path);
        }
    }

    RN_ShaderModule *shader_module = hash_map_RN_ShaderModule_get_or_create(renderer->permanent_arena, &renderer->shader_cache, path);
    b32 should_update_shader = (source_updated && compilation_success) || !rn_shader_module_is_initialized(shader_module);
    if(should_update_shader){
        shader_module->compiled_path = compiled_path;
        shader_module->path = path;
        String8 compiled_shader_content = platform_read_entire_file(temp.arena, compiled_path);
        if(compiled_shader_content.size > 0){
            shader_module->shader_module_handle = RENDER_BACKEND_CALL(shader_create(compiled_shader_content));
        }else{
            DOT_WARNING("Failed to read shader %S compilation %S", path, compiled_path);
        }
    }
    threadctx_temp_end(temp);
    return shader_module;
}
