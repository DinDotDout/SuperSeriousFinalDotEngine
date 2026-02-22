internal RendererBackend*
renderer_backend_create(Arena *arena, RendererBackendConfig *backend_config)
{
    Arena *backend_arena = ARENA_ALLOC(
        .parent = arena,
        .reserve_size = backend_config->backend_memory_size,
    );

    RendererBackend *base;
    switch (backend_config->backend_kind){
    case RendererBackendKind_Vk:   base = cast(RendererBackend*) renderer_backend_vk_create(backend_arena); break;
    case RendererBackendKind_Null: base = cast(RendererBackend*) renderer_backend_null_create(backend_arena); break;
    case RendererBackendKind_Gl: //base = cast(RendererBackend*) renderer_backend_gl_create(arena); break;
    case RendererBackendKind_Dx12: //base = cast(RendererBackend*) renderer_backend_dx_create(arena); break;
    default:
        DOT_ERROR("Unsupported renderer backend");
    }
    base->permanent_arena = arena;
    return base;
}

void
renderer_clear_background(DOT_Renderer *renderer, vec3 color)
{
    RendererBackend *backend = renderer->backend;
    u8 frame_idx = renderer->current_frame % renderer->frame_overlap;
    backend->clear_bg(renderer->backend, frame_idx, color);
}

internal void
renderer_init(Arena *arena, DOT_Renderer *renderer, DOT_Window *window, RendererConfig *renderer_config)
{
    renderer->permanent_arena = ARENA_ALLOC(
        .parent       = arena,
        .reserve_size = renderer_config->renderer_memory_size,
        .name         = "Application_Renderer");

    shader_cache_init(renderer->permanent_arena, &renderer->shader_cache, &renderer_config->shader_cache_config);
    RendererBackend *backend = renderer_backend_create(renderer->permanent_arena, &renderer_config->backend_config);
    renderer->backend       = backend;
    renderer->frame_overlap = renderer_config->frame_overlap;
    renderer->frame_data    = PUSH_ARRAY(renderer->permanent_arena, FrameData, renderer->frame_overlap);
    for(u8 i = 0; i < renderer->frame_overlap; ++i){
        renderer->frame_data[i].temp_arena = ARENA_ALLOC(
            .parent       = renderer->permanent_arena,
            .reserve_size = renderer_config->frame_arena_size);
    }
    backend->init(backend, window);
}

internal void
renderer_shutdown(DOT_Renderer *renderer)
{
    RendererBackend *backend = renderer->backend;
    ShaderCache *shader_cache = &renderer->shader_cache;

    for(u32 i = 0; i < shader_cache->shader_modules_count; ++i){
        ShaderCacheNode *node = shader_cache->shader_modules[i];
        for EACH_NODE(it, ShaderCacheNode, node){
            DOT_ShaderModuleHandle h = it->shader_module->shader_module_handle;
            renderer->backend->unload_shader_module(backend, h);
        }
    }
    shader_cache_end(shader_cache);
    backend->shutdown(backend);
}

internal void
renderer_begin_frame(DOT_Renderer *renderer)
{
    RendererBackend *backend = renderer->backend;
    u8 frame_idx = renderer->current_frame % renderer->frame_overlap;
    backend->begin_frame(backend, frame_idx);
}

internal void
renderer_end_frame(DOT_Renderer *renderer)
{
    RendererBackend *backend = renderer->backend;
    u8 frame_idx = renderer->current_frame % renderer->frame_overlap;
    backend->end_frame(backend, frame_idx);
    ++renderer->current_frame;
}


DOT_ShaderModule*
renderer_load_shader_module_from_path(Arena *arena, DOT_Renderer *renderer, String8 path)
{
    TempArena temp = threadctx_get_temp(&arena,0);
    String8 compiled_path = shader_cache_get_compiled_path(arena, path);
    b8 source_updated = platform_file_is_newer(cast(char*)path.str, cast(char*)compiled_path.str);
    b8 compilation_success = false;
    if(source_updated){
        DOT_PRINT("Recompiling %S", path);
        compilation_success = shader_compile_from_path(path, compiled_path);
        if(!compilation_success){
            DOT_WARNING("Failed to compile shader %S", path);
        }
    }

    DOT_ShaderModule *shader_module = shader_cache_get_or_create(arena, &renderer->shader_cache, path, compiled_path);
    b8 should_update_shader = (source_updated && compilation_success) || !shader_module_initialized(shader_module);
    if(should_update_shader){
        DOT_FileBuffer compiled_shader_content = platform_read_entire_file(temp.arena, compiled_path);
        if(compiled_shader_content.size > 0){
            shader_module->shader_module_handle = renderer->backend->load_shader_module(renderer->backend, compiled_shader_content);
        }else{
            DOT_WARNING("Failed to read shader %S compilation %S", path, compiled_path);
        }
    }
    temp_arena_restore(temp);
    return shader_module;
}
