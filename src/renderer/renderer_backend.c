internal RendererBackend*
renderer_backend_create(Arena* arena, RendererBackendConfig *backend_config){
    Arena* backend_arena = ARENA_ALLOC(
        .parent = arena,
        .reserve_size = backend_config->memory_size,
    );
    RendererBackend* base;
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
renderer_backend_shutdown(RendererBackend* backend){
    DOT_ASSERT(backend);
    backend->shutdown(backend);
}
