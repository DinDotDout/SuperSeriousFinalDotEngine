DOT_SETTING_U32("Render", g_rn_transient_memory_size_b, DOT_MB(80));
DOT_SETTING_U32("Render", g_rn_permanent_memory_size_b, DOT_MB(80));
DOT_SETTING_U32("Render", g_frame_arena_size_b, DOT_KB(32));

DOT_SETTING_U32("Render", g_frame_overlap, 3);
DOT_SETTING_U32("Render", g_thread_count, 1);

DOT_SETTING_U32("Render", g_draw_texture_format, RN_TextureFormatKind_RGBA16F);
DOT_SETTING_U32("Render", g_swapchaing_texture_format, RN_TextureFormatKind_RGBA8_SRGB);
DOT_SETTING_U32("Render", g_depth_stencil_format, RN_TextureFormatKind_D32F);
DOT_SETTING_U32("Render", g_present_mode, RN_PresentModeKind_Mailbox);

DOT_SETTING_U32("RenderBackend", g_render_backend_transient_memory_size_b, DOT_KB(512));
DOT_SETTING_U32("RenderBackend", g_render_backend_permanent_memory_size_b, DOT_MB(4));
DOT_SETTING_U32("RenderBackend", g_texture_count_max, 512);
DOT_SETTING_U32("RenderBackend", g_buffer_count_max, 4096);
DOT_SETTING_U32("RenderBackend", g_sampler_count_max, 128);
DOT_SETTING_U32("RenderBackend", g_cleanup_resource_pool_max, 128);
DOT_SETTING_U32("RenderBackend", g_command_buffers_per_thread, 4);

DOT_SETTING_U32("ShaderCache", g_shader_cache_bucket_count, 64);

#if defined(DOT_RENDER_BACKEND_ONLY_DX12)
#   if DOT_OS_POSIX
#       error Unavaliable backend on linux!
#   endif
    DOT_SETTING_U32("RenderBackend", g_backend_kind, RendererBackendKind_Dx12);
#elif defined(DOT_RENDER_BACKEND_ONLY_VK)
    DOT_SETTING_U32("RenderBackend", g_backend_kind, RendererBackendKind_Vk);
#else
    DOT_SETTING_U32("RenderBackend", g_backend_kind, RendererBackendKind_Auto);
#endif

internal RendererBackend *
rn_backend_create(Arena *arena)
{
    switch(g_backend_kind){
    case RendererBackendKind_Null: return cast(RendererBackend*) rn_null_create(arena);
    case RendererBackendKind_Vk:   return cast(RendererBackend*) rn_vk_create(arena);
    case RendererBackendKind_Dx12: //base = cast(RendererBackend*) rn_renderer_dx12_create(arena); break;
    default: DOT_ERROR("Unsupported render backend");
    }
}

internal void
rn_init(Arena *arena, DOT_Renderer *renderer, DOT_Window *window)
{
    renderer->permanent_arena = ARENA_CREATE(
        .parent       = arena,
        .reserve_size = g_rn_permanent_memory_size_b,
        .name         = "Application render permanent");

    renderer->transient_arena = ARENA_CREATE(
        .parent       = arena,
        .reserve_size = g_rn_transient_memory_size_b,
        .name         = "Application renderer transient");

    hash_map_DOT_ShaderModule_init(renderer->permanent_arena, &renderer->shader_cache, g_shader_cache_bucket_count);

    renderer->backend = rn_backend_create(renderer->permanent_arena);
    renderer->frame_data_count = g_frame_overlap;
    renderer->frame_datas    = PUSH_ARRAY(renderer->permanent_arena, FrameData, renderer->frame_data_count);
    for(u8 i = 0; i < renderer->frame_data_count; ++i){
        renderer->frame_datas[i].temp_arena = ARENA_CREATE(
            .parent       = renderer->permanent_arena,
            .reserve_size = g_frame_arena_size_b);
    }
    RENDER_BACKEND_CALL(init(window));
    null_texture = rn_texture_create(
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
        String8Lit("null_texture")
    );
    (void)null_texture;
}

internal void
rn_shutdown(DOT_Renderer *renderer)
{
    HashMap_DOT_ShaderModule *shader_cache = &renderer->shader_cache;
    HashMap_Iter it = hash_map_iter_init();
    DOT_ShaderModule *shader_module = NULL;
    while((shader_module = hash_map_DOT_ShaderModule_iter_next(shader_cache, &it))){
            DOT_ShaderModuleHandle h = shader_module->shader_module_handle;
            RENDER_BACKEND_CALL(shader_unload(h));
    }
    hash_map_DOT_ShaderModule_end(shader_cache);
    RENDER_BACKEND_CALL(shutdown());
}

void
rn_texture_destroy(DOT_Renderer *renderer, DOT_TextureHandle handle)
{
    RENDER_BACKEND_CALL(texture_destroy(handle));
}

DOT_TextureAsset
rn_texture_asset_create(DOT_Renderer *renderer, const DOT_AssetCreateInfo *asset_info, u8 mip_levels)
{
    DOT_WARNING("Creating texture with name %S, from path: ", asset_info->name, asset_info->path);
    TempArena temp = temp_arena_get(renderer->transient_arena);
    String8 file_data = platform_read_entire_file(temp.arena, asset_info->path);

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
    stbi_allocator_unset();

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
    DOT_TextureAsset asset = {
        .asset = dot_asset_from_create_info(renderer, asset_info, DOT_AssetKind_Texture),
        .desc = {
            .dimension_kind = RN_TextureDimensionKind_2D,
            .format_kind = rn_texture_format_from_info(comp, size_bytes, true),
            .mip_levels = mip_levels,
            .width = cast(u16)width,
            .height = cast(u16)height,
            .depth = 1,
        },
    };
    asset.handle = rn_texture_create(renderer, &asset.desc, data, asset_info->name),
    temp_arena_restore(temp);
    return asset;
}

internal DOT_TextureHandle
rn_texture_create(DOT_Renderer *renderer, const RN_TextureDesc *texture_desc, void *data, String8 debug_name)
{
    return RENDER_BACKEND_CALL(texture_create(texture_desc, data, debug_name));
}

internal DOT_SamplerAsset
rn_sampler_asset_create(DOT_Renderer *renderer, const DOT_AssetCreateInfo *asset_info, const RN_SamplerDesc *sampler_desc)
{
    DOT_SamplerAsset sampler_asset = {
        .asset = dot_asset_from_create_info(renderer, asset_info, DOT_AssetKind_Sampler),
        .desc = *sampler_desc,
        .handle = rn_sampler_create(renderer, sampler_desc, asset_info->name),
    };
    return sampler_asset;
}
internal DOT_BufferAsset
rn_buffer_asset_create(DOT_Renderer *renderer, const DOT_AssetCreateInfo *asset_info, const RN_BufferDesc *buffer_desc, u8 *data)
{
    DOT_BufferAsset buffer_asset = {
        .asset = dot_asset_from_create_info(renderer, asset_info, DOT_AssetKind_Buffer),
        .desc = *buffer_desc,
    };
    buffer_asset.handle = rn_buffer_create(renderer, buffer_desc, data, asset_info->name);
    return buffer_asset;
}

internal RN_RenderPassOutput
render_swapchain_output()
{
    RN_RenderPassOutput rpo = {0};
    rpo.depth_stencil_format = g_depth_stencil_format;
    RN_TextureFormatKind texture_fmt = g_swapchaing_texture_format;
    ARRAY_PUSH(rpo.color_formats, texture_fmt);
    return(rpo);
}

internal DOT_BufferHandle
rn_buffer_create(DOT_Renderer *renderer, const RN_BufferDesc *buffer_desc, u8 *data, String8 debug_name)
{
    return RENDER_BACKEND_CALL(buffer_create(buffer_desc, data, debug_name));
}

internal DOT_SamplerHandle
rn_sampler_create(DOT_Renderer *renderer, const RN_SamplerDesc *sampler_desc, String8 debug_name)
{
    return RENDER_BACKEND_CALL(sampler_create(sampler_desc, debug_name));
}

void
rn_clear_background(DOT_Renderer *renderer, vec3 color)
{
    RENDER_BACKEND_CALL(clear_bg(color));
}

internal void
rn_frame_begin(DOT_Renderer *renderer)
{
    RENDER_BACKEND_CALL(frame_begin());
}

internal void
rn_frame_end(DOT_Renderer *renderer)
{
    RENDER_BACKEND_CALL(frame_end());
}

/* ------------------------------------------------------------------ */
/*  Overlay (backend-agnostic)                                        */
/* ------------------------------------------------------------------ */

internal void
rn_overlay_init(DOT_Renderer *renderer, const void *font_pixels, int font_w, int font_h)
{
    DOT_UNUSED(renderer);
    RENDER_BACKEND_CALL(overlay_init(font_pixels, font_w, font_h));
}

internal void
rn_overlay_render(DOT_Renderer *renderer, u8 frame_idx, OverlayDrawList *draw_list)
{
    DOT_UNUSED(renderer);
    rn_vk_overlay_render(frame_idx, draw_list);
}

internal void
rn_overlay_shutdown(DOT_Renderer *renderer)
{
    DOT_UNUSED(renderer);
    RENDER_BACKEND_CALL(overlay_shutdown());
}

DOT_ShaderModule*
rn_shader_module_load_from_path(DOT_Renderer *renderer, String8 path)
{
    TempArena temp = threadctx_get_temp(0);
    String8 compiled_path = shader_cache_get_compiled_path(renderer->permanent_arena, path);
    b32 source_updated = platform_file_is_newer(path, compiled_path);
    b32 compilation_success = false;
    if(source_updated){
        DOT_PRINT("Recompiling %S", path);
        compilation_success = shader_compile_from_path(path, compiled_path);
        if(!compilation_success){
            DOT_WARNING("Failed to compile shader %S", path);
        }
    }

    DOT_ShaderModule *shader_module = hash_map_DOT_ShaderModule_get_or_create(renderer->permanent_arena, &renderer->shader_cache, path);
    b32 should_update_shader = (source_updated && compilation_success) || !shader_module_initialized(shader_module);
    if(should_update_shader){
        shader_module->compiled_path = compiled_path;
        shader_module->asset.path = path;
        String8 compiled_shader_content = platform_read_entire_file(temp.arena, compiled_path);
        if(compiled_shader_content.size > 0){
            shader_module->shader_module_handle = RENDER_BACKEND_CALL(shader_load_from_data(compiled_shader_content));
        }else{
            DOT_WARNING("Failed to read shader %S compilation %S", path, compiled_path);
        }
    }
    temp_arena_restore(temp);
    return shader_module;
}
