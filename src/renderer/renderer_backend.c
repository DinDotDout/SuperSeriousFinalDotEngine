internal RendererBackend*
renderer_backend_create(Arena* arena, RendererBackendKind backend_kind){
    switch (backend_kind) {
    case RENDERER_BACKEND_VK: return cast(RendererBackend*) renderer_backend_vk_create(arena);
    // case RENDERER_BACKEND_GL: return (RendererBackend*) RendererBackendGL_Create(arena);
    // case RENDERER_BACKEND_DX: return (RendererBackend*) RendererBackendDx_Create(arena);
    default:
        DOT_ERROR("Unsupported renderer backend");
        return NULL;
    }
}

internal void
renderer_backend_shutdown(RendererBackend* backend){
    DOT_ASSERT(backend);
    backend->Shutdown(backend);
}
