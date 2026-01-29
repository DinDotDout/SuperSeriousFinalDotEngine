internal void
renderer_init(Arena* arena, DOT_Renderer* renderer, DOT_Window* window, RendererConfig* config){
    renderer->permanent_arena = ARENA_ALLOC(
        .parent = arena,
        .reserve_size = config->mem_size,
        .name = "Application_Renderer");

    renderer->backend = renderer_backend_create(renderer->permanent_arena, &config->backend_config);
    if(renderer->backend){
        renderer->backend->init(renderer->backend, window);
    }
}

internal void
renderer_shutdown(DOT_Renderer* renderer){
    renderer_backend_shutdown(renderer->backend);
}
