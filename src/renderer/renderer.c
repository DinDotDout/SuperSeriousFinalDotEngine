internal RendererBackend*
renderer_backend_create(Arena *arena, RendererBackendConfig *backend_config){
    Arena* backend_arena = ARENA_ALLOC(
        .parent = arena,
        .reserve_size = backend_config->memory_size,
    );
    RendererBackend *base;
    switch (backend_config->backend_kind) {
    case RENDERER_BACKEND_VK: base = cast(RendererBackend*) renderer_backend_vk_create(backend_arena); break;
    // case RENDERER_BACKEND_GL: base = cast(RendererBackend*) renderer_backend_gl_create(arena); break;
    // case RENDERER_BACKEND_DX: base = cast(RendererBackend*) renderer_backend_dx_create(arena); break;
    default:
        DOT_ERROR("Unsupported renderer backend");
        return NULL;
    }
    base->permanent_arena = arena;
    return base;
}

internal void
renderer_init(Arena *arena, DOT_Renderer *renderer, DOT_Window *window, RendererConfig *renderer_config){
    renderer->permanent_arena = ARENA_ALLOC(
        .parent       = arena,
        .reserve_size = renderer_config->mem_size,
        .name         = "Application_Renderer");

    RendererBackend *backend = renderer_backend_create(renderer->permanent_arena, &renderer_config->backend_config);
    renderer->backend     = backend;
    renderer->frame_count = renderer_config->frame_overlap;
    renderer->frame_data  = PUSH_ARRAY(renderer->permanent_arena, FrameData, renderer->frame_count);
    for(u8 i = 0; i < renderer->frame_count; ++i){
        renderer->frame_data[i].temp_arena = ARENA_ALLOC(
            .parent       = renderer->permanent_arena,
            .reserve_size = renderer_config->frame_arena_size);
    }

    if(backend){
        backend->init(backend, window);
    }
}

internal void
renderer_shutdown(DOT_Renderer *renderer){
    RendererBackend *backend = renderer->backend;
    backend->shutdown(backend);
}

internal void
renderer_draw(DOT_Renderer *renderer){
    RendererBackend *backend = renderer->backend;
    backend->draw(backend, renderer->current_frame % renderer->frame_count);
    ++renderer->current_frame;
}
