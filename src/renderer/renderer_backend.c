internal RendererBackendBase* RendererBackend_Create(Arena* arena, RendererBackendKind backend_kind){
    switch (backend_kind) {
    case RENDERER_BACKEND_VK: return cast(RendererBackendBase*) RendererBackendVk_Create(arena);
    // case RENDERER_BACKEND_GL: return (RendererBackendBase*) RendererBackendGL_Create(arena);
    // case RENDERER_BACKEND_DX: return (RendererBackendBase*) RendererBackendDx_Create(arena);
    default:
        DOT_ERROR("Unsupported renderer backend");
        return NULL;
    }
}

internal void RendererBackend_Shutdown(RendererBackendBase* backend){
    DOT_ASSERT(backend);
    backend->Shutdown(backend);
}
