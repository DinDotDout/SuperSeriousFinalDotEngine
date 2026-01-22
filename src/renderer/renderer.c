internal void Renderer_Init(Arena* arena, DOT_Renderer* renderer, DOT_Window* window, RendererConfig config){
    u8* mem = PushSizeNoZero(arena, config.mem_size);

    renderer->arena = Arena_AllocFromMemory(
        mem,
        .reserve_size = config.mem_size,
        .name = "Application_Renderer");

    renderer->backend = RendererBackend_Create(renderer->arena, config.backend_kind);
    if(renderer->backend){
        renderer->backend->Init(renderer->backend, window);
    }
}

internal void Renderer_Shutdown(DOT_Renderer* renderer){
    RendererBackend_Shutdown(renderer->backend);
}
