internal void DOT_Renderer_Init(Arena* arena, DOT_Renderer* renderer, DOT_Window* window, RendererConfig config){
    renderer->arena = Arena_AllocFromMemory(PushSize(arena, config.mem_size), .capacity = config.mem_size, .name = "Application_Renderer");
    renderer->backend = DOT_RendererBackend_Create(arena, config.backend_kind);
    if(renderer->backend){
        renderer->backend->Init(renderer->backend, window);
    }
}

internal void DOT_Renderer_Shutdown(DOT_Renderer* renderer){
    DOT_RendererBackend_Sutdown(renderer->backend);
}
