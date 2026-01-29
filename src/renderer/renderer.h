#ifndef RENDERER_H
#define RENDERER_H
typedef struct RendererConfig{
    u64 mem_size;
    RendererBackendConfig backend_config;
}RendererConfig;

typedef struct DOT_Renderer{
    RendererBackend* backend;
    Arena* permanent_arena; // NOTE: Will the renderer need this or only the backend
}DOT_Renderer;

internal void renderer_init(Arena* arena, DOT_Renderer* renderer, DOT_Window* window, RendererConfig* config);
internal void renderer_shutdown(DOT_Renderer* renderer);
#endif
