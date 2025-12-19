#ifndef DOT_RENDERER_H
#define DOT_RENDERER_H
typedef struct RendererConfig{
        u64 mem_size;
        DOT_RendererBackendKind backend_kind;
}RendererConfig;

typedef struct DOT_Renderer{
        DOT_RendererBackendBase* backend;
        Arena* arena; // NOTE: Will the renderer need this or only the backend
}DOT_Renderer;

internal void DOT_Renderer_Init(Arena* arena, DOT_Renderer* renderer, DOT_Window* window, RendererConfig config);
internal void DOT_Renderer_Shutdown(DOT_Renderer* renderer);
#endif
