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


internal void renderer_load_shader_module_from_path(Arena *arena, String8 path)
{
    File file = platform_read_entire_file(arena, path);

}

internal void
renderer_init(Arena *arena, DOT_Renderer *renderer, DOT_Window *window, RendererConfig *renderer_config)
{
    renderer->permanent_arena = ARENA_ALLOC(
        .parent       = arena,
        .reserve_size = renderer_config->renderer_memory_size,
        .name         = "Application_Renderer");

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

internal void
renderer_clear_background(DOT_Renderer *renderer){
    u8 frame_idx = renderer->current_frame % renderer->frame_overlap;
    f64 flash = fabs(sin(renderer->current_frame  / 2000.f));
    // Harcoded for now
    renderer_backend_vk_clear_bg(renderer->backend, frame_idx, v3(0,0,flash));
}
