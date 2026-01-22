#ifndef RENDERER_H
#define RENDERER_H
typedef struct RendererConfig{
        u64 mem_size;
        RendererBackendKind backend_kind;
}RendererConfig;

typedef struct DOT_Renderer{
        RendererBackendBase* backend;
        Arena* arena; // NOTE: Will the renderer need this or only the backend
}DOT_Renderer;

internal void Renderer_Init(Arena* arena, DOT_Renderer* renderer, DOT_Window* window, RendererConfig config);
internal void Renderer_Shutdown(DOT_Renderer* renderer);
#endif
