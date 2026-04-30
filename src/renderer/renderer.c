internal RendererBackend*
renderer_backend_create(Arena *arena, RendererBackendConfig *backend_config)
{
    switch(backend_config->backend_kind){
    case RendererBackendKind_Null: return cast(RendererBackend*) renderer_backend_null_create(arena, backend_config);
    case RendererBackendKind_Vk:   return cast(RendererBackend*) renderer_backend_vk_create(arena, backend_config);
    case RendererBackendKind_Dx12: //base = cast(RendererBackend*) renderer_backend_dx12_create(arena); break;
    default: DOT_ERROR("Unsupported render backend");
    }
}

internal void
renderer_init(Arena *arena, DOT_Renderer *renderer, DOT_Window *window, RendererConfig *renderer_config)
{
    renderer->permanent_arena = ARENA_ALLOC(
        .parent       = arena,
        .reserve_size = renderer_config->renderer_memory_size,
        .name         = "Application_Renderer");

    shader_cache_init(renderer->permanent_arena, &renderer->shader_cache, renderer_config->shader_cache_config);
    renderer->backend = renderer_backend_create(renderer->permanent_arena, renderer_config->backend_config);


    // renderer->frame_data    = PUSH_ARRAY(renderer->permanent_arena, FrameData, renderer->frame_overlap);
    // for(u8 i = 0; i < backend->frame_data_count; ++i){
    //     renderer->frame_data[i].temp_arena = ARENA_ALLOC(
    //         .parent       = renderer->permanent_arena,
    //         .reserve_size = renderer_config->frame_arena_size);
    // }
    RENDER_BACKEND_CALL(init(window));
    null_texture = renderer_create_texture(
        renderer,
        &(DOT_TextureCreateInfo) {
            .texture_desc = {
                .width = 1,
                .height = 1,
                .depth = 1,
                .mip_levels = 1,
                .format_kind = DOT_TextureFormat_RGBA8_UNORM,
            },
            .data = (u8[]){0,0,0,0},
        }
    );
    (void)null_texture;
}

internal void
renderer_shutdown(DOT_Renderer *renderer)
{
    ShaderCache *shader_cache = &renderer->shader_cache;
    for(u32 i = 0; i < shader_cache->shader_modules_count; ++i){
        ShaderCacheNode *node = shader_cache->shader_modules[i];
        for EACH_NODE(it, ShaderCacheNode, node){
            DOT_ShaderModuleHandle h = it->shader_module->shader_module_handle;
            RENDER_BACKEND_CALL(shader_unload(h));
        }
    }
    shader_cache_end(shader_cache);
    // renderer_destroy_texture(&null_texture);
    RENDER_BACKEND_CALL(shutdown());
}

internal DOT_TextureFormatInfo
renderer_texture_format_info_get(DOT_TextureFormatKind fmt)
{
    DOT_TextureFormatInfo info = {0};
    switch(fmt){
    case DOT_TextureFormat_Invalid:
        return info;
    // 8‑bit formats
    case DOT_TextureFormat_R8_UNORM:
    case DOT_TextureFormat_R8_UINT:
        info.channels     = 1;
        info.block_size   = 1;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case DOT_TextureFormat_RG8_UNORM:
        info.channels     = 2;
        info.block_size   = 2;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case DOT_TextureFormat_RGBA8_SRGB:
        info.format_flags |= DOT_TextureFormatFlags_SRGB;
        DOT_FALLTHROUGH;
    case DOT_TextureFormat_RGBA8_UNORM:
        info.channels     = 4;
        info.block_size   = 4;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case DOT_TextureFormat_BGRA8_SRGB:
        info.format_flags |= DOT_TextureFormatFlags_SRGB;
        DOT_FALLTHROUGH;
    case DOT_TextureFormat_BGRA8_UNORM:
        info.channels     = 4;
        info.block_size   = 4;
        info.block_width  = 1;
        info.block_height = 1;
    break;

    // HDR capable formats
    case DOT_TextureFormat_R16F:
        info.channels     = 1;
        info.block_size   = 2;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case DOT_TextureFormat_RG16F:
        info.channels     = 2;
        info.block_size   = 4;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case DOT_TextureFormat_RGBA16F:
        info.channels     = 4;
        info.block_size   = 8;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case DOT_TextureFormat_R32F:
        info.channels     = 1;
        info.block_size   = 4;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case DOT_TextureFormat_RG32F:
        info.channels     = 2;
        info.block_size   = 8;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case DOT_TextureFormat_RGBA32F:
        info.channels     = 4;
        info.block_size   = 16;
        info.block_width  = 1;
        info.block_height = 1;
    break;

    // Depth / Stencil formats
    case DOT_TextureFormat_D16:
        info.channels     = 1;
        info.block_size   = 2;
        info.block_width  = 1;
        info.block_height = 1;
        info.format_flags |= DOT_TextureFormatFlags_Depth;
    break;
    case DOT_TextureFormat_D24S8:
        info.channels     = 2;
        info.block_size   = 4;
        info.block_width  = 1;
        info.block_height = 1;
        info.format_flags |= DOT_TextureFormatFlags_Depth | DOT_TextureFormatFlags_Stencil;
    break;
    case DOT_TextureFormat_D32F:
        info.channels     = 1;
        info.block_size   = 4;
        info.block_width  = 1;
        info.block_height = 1;
        info.format_flags |= DOT_TextureFormatFlags_Depth;
    break;
    case DOT_TextureFormat_D32FS8:
        info.channels     = 2;
        info.block_size   = 5; /* 32F + 8 stencil */
        info.block_width  = 1;
        info.block_height = 1;
        info.format_flags |= DOT_TextureFormatFlags_Depth | DOT_TextureFormatFlags_Stencil;
    break;
    // BC block‑compressed formats
    case DOT_TextureFormat_BC1_SRGB:
        info.format_flags |= DOT_TextureFormatFlags_SRGB;
        DOT_FALLTHROUGH;
    case DOT_TextureFormat_BC1:
        info.channels     = 4;
        info.block_size   = 8;
        info.block_width  = 4;
        info.block_height = 4;
        info.format_flags |= DOT_TextureFormatFlags_Compressed;
    break;
    case DOT_TextureFormat_BC3_SRGB:
        info.format_flags |= DOT_TextureFormatFlags_SRGB;
        DOT_FALLTHROUGH;
    case DOT_TextureFormat_BC3:
        info.channels     = 4;
        info.block_size   = 16;
        info.block_width  = 4;
        info.block_height = 4;
        info.format_flags |= DOT_TextureFormatFlags_Compressed;
    break;
    case DOT_TextureFormat_BC7_SRGB:
        info.format_flags |= DOT_TextureFormatFlags_SRGB;
        DOT_FALLTHROUGH;
    case DOT_TextureFormat_BC7:
        info.channels     = 4;
        info.block_size   = 16;
        info.block_width  = 4;
        info.block_height = 4;
        info.format_flags |= DOT_TextureFormatFlags_Compressed;
    break;

    // ETC2 formats
    case DOT_TextureFormat_ETC2_RGB8:
        info.channels     = 3;
        info.block_size   = 8;
        info.block_width  = 4;
        info.block_height = 4;
        info.format_flags |= DOT_TextureFormatFlags_Compressed;
    break;
    case DOT_TextureFormat_ETC2_RGBA8:
        info.channels     = 4;
        info.block_size   = 16;
        info.block_width  = 4;
        info.block_height = 4;
        info.format_flags |= DOT_TextureFormatFlags_Compressed;
    break;
    }
    return info;
}

// (JD) NOTE: ideally we would pass in something like DOT_TextureFormatInfo in the future
internal DOT_TextureFormatKind
renderer_pick_texture_format(int comp, u8 size_bytes, b32 srgb)
{
    if(size_bytes == 4){ // 32-bit float per channel (.hdr) 
        switch(comp){
        case 1: return DOT_TextureFormat_R32F;
        case 2: return DOT_TextureFormat_RG32F;
        case 3: return DOT_TextureFormat_RGBA32F;
        case 4: return DOT_TextureFormat_RGBA32F;
        }
    }else if(size_bytes == 2){ // 16-bit integer formats (PNG, TGA, etc.)

        switch(comp){
        case 1: return DOT_TextureFormat_R16F;
        case 2: return DOT_TextureFormat_RG16F;
        case 3: return DOT_TextureFormat_RGBA16F;
        case 4: return DOT_TextureFormat_RGBA16F;
        }
    }else if(size_bytes == 1){
        switch(comp){ // 8-bit formats
        case 1: return DOT_TextureFormat_R8_UNORM;
        case 2: return DOT_TextureFormat_RG8_UNORM;
        case 3: return srgb ? DOT_TextureFormat_RGBA8_SRGB : DOT_TextureFormat_RGBA8_UNORM;
        case 4: return srgb ? DOT_TextureFormat_RGBA8_SRGB : DOT_TextureFormat_RGBA8_UNORM;
        }
    }
    return DOT_TextureFormat_Invalid;
}

// (JD) NOTE: When having an asset loading system, we may not need to load raw textures directly
//      and can always pass it in the raw texture data. We probably also won't need to use stb to
//      parse arbitrary image formats
DOT_TextureAsset
renderer_create_texture_asset(DOT_Renderer *renderer, DOT_TextureCreateInfo *create_info)
{ 
    DOT_TextureAsset texture_asset = {
        .asset = {
            .kind = DOT_Asset_Texture,
            .name = create_info->asset_info.name,
            .path = create_info->asset_info.path,
            .desc = create_info->asset_info.desc,
        },
        .desc = create_info->texture_desc,
        .handle = renderer_create_texture(renderer, create_info),
    };
    return texture_asset;
}

DOT_TextureHandle
renderer_create_texture(DOT_Renderer *renderer, DOT_TextureCreateInfo *create_info)
{
    TempArena temp = threadctx_get_temp(0,0);
    DOT_TextureDesc *texture_desc = &create_info->texture_desc;
    if(create_info->data == NULL){
        String8 asset_path = create_info->asset_info.path;
        if(asset_path.size > 0){
            String8 file_data = platform_read_entire_file(temp.arena, asset_path);
            u8 size_bytes = 0;
            b32 is_hdr = stbi_is_16_bit_from_memory(file_data.str, file_data.size);
            b32 is_16_bit = is_hdr || stbi_is_16_bit_from_memory(file_data.str, file_data.size);
            int comp;
            if(is_hdr){
                create_info->data = stbi_loadf_from_memory(file_data.str, file_data.size, cast(int*)&texture_desc->width, cast(int*)&texture_desc->height, &comp, STBI_default);
                size_bytes = 4;
            }else if(is_16_bit){
                create_info->data = stbi_load_16_from_memory(file_data.str, file_data.size, cast(int*)&texture_desc->width, cast(int*)&texture_desc->height, &comp, STBI_default);
                size_bytes = 2;
            }else{
                create_info->data = stbi_load_from_memory(file_data.str, file_data.size, cast(int*)&texture_desc->width, cast(int*)&texture_desc->height, &comp, STBI_default);
                size_bytes = 1;
            }
            // (jd) NOTE: We assume we read srgb always for now
            create_info->texture_desc.format_kind = renderer_pick_texture_format(comp, size_bytes, true);
        }else{
            DOT_WARNING("No texture data nor asset path passed set");
        }
    }
    if(texture_desc->mip_levels == 0){
        texture_desc->mip_levels = 1;
        u32 mip_w = texture_desc->width;
        u32 mip_h = texture_desc->height;
        for(;mip_w > 1 && mip_h > 1; ++texture_desc->mip_levels){
            mip_w /= 2;
            mip_h /= 2;
        }
    }

    DOT_TextureHandle handle = RENDER_BACKEND_CALL(texture_create(create_info));
    temp_arena_restore(temp);
    return handle;
}

void
renderer_clear_background(DOT_Renderer *renderer, vec3 color)
{
    RENDER_BACKEND_CALL(clear_bg(color));
}

internal void
renderer_begin_frame(DOT_Renderer *renderer)
{
    RENDER_BACKEND_CALL(begin_frame());
}

internal void
renderer_end_frame(DOT_Renderer *renderer)
{
    RENDER_BACKEND_CALL(end_frame());
}

/* ------------------------------------------------------------------ */
/*  Overlay (backend-agnostic)                                        */
/* ------------------------------------------------------------------ */

internal void
renderer_overlay_init(DOT_Renderer *renderer, const void *font_pixels, int font_w, int font_h)
{
    UNUSED(renderer);
    RENDER_BACKEND_CALL(overlay_init(font_pixels, font_w, font_h));
}

internal void
renderer_overlay_render(DOT_Renderer *renderer, u8 frame_idx, OverlayDrawList *draw_list)
{
    UNUSED(renderer);
    renderer_backend_vk_overlay_render(frame_idx, draw_list);
}

internal void
renderer_overlay_shutdown(DOT_Renderer *renderer)
{
    UNUSED(renderer);
    RENDER_BACKEND_CALL(overlay_shutdown());
}

DOT_ShaderModule*
renderer_shader_module_load_from_path(DOT_Renderer *renderer, String8 path)
{
    TempArena temp = threadctx_get_temp(0,0);
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

    DOT_ShaderModule *shader_module = shader_cache_get_or_create(renderer->permanent_arena, &renderer->shader_cache, path, compiled_path);
    b32 should_update_shader = (source_updated && compilation_success) || !shader_module_initialized(shader_module);
    if(should_update_shader){
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
