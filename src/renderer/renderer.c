internal void
renderer_init(Arena* arena, DOT_Renderer* renderer, DOT_Window* window, RendererConfig config){
    u8* mem = PUSH_SIZE_NO_ZERO(arena, config.mem_size);

    renderer->arena = ARENA_ALLOC_FROM_MEMORY(
        mem,
        .reserve_size = config.mem_size,
        .name = "Application_Renderer");

    renderer->backend = renderer_backend_create(renderer->arena, config.backend_kind);
    if(renderer->backend){
        renderer->backend->Init(renderer->backend, window);
    }
}

internal void
renderer_shutdown(DOT_Renderer* renderer){
    renderer_backend_shutdown(renderer->backend);
}
