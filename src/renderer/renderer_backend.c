internal DOT_RendererBackendBase* DOT_RendererBackend_Create(Arena* arena, DOT_RendererBackendKind backend_kind){
    switch (backend_kind) {
    case DOT_RENDERER_BACKEND_VK: return cast(DOT_RendererBackendBase*) DOT_RendererBackendVk_Create(arena);
    // case DOT_RENDERER_BACKEND_GL: return (DOT_RendererBackendBase*) DOT_RendererBackendGL_Create(arena);
    // case DOT_RENDERER_BACKEND_DX: return (DOT_RendererBackendBase*) DOT_RendererBackendDx_Create(arena);
    default:
        DOT_ERROR("Unsupported renderer backend");
        return NULL;
    }
}

internal void DOT_RendererBackend_Sutdown(DOT_RendererBackendBase* backend){
    DOT_ASSERT(backend);
    backend->Shutdown(backend);
}
