#ifndef RENDERER_H
#define RENDERER_H

typedef u8 RendererBackendKind;
enum{
    RENDERER_BACKEND_INVALID,
    RENDERER_BACKEND_VK,
    RENDERER_BACKEND_GL,
    RENDERER_BACKEND_DX12,
    RENDERER_BACKEND_COUNT,
};

typedef struct RendererBackendConfig{
    RendererBackendKind backend_kind;
    u64                 backend_memory_size;
}RendererBackendConfig;

typedef struct RendererConfig{
    u64                   renderer_memory_size;
    u64                   frame_arena_size;
    u8                    frame_overlap;
    RendererBackendConfig backend_config;
}RendererConfig;

typedef struct RendererBackend RendererBackend;
typedef void (*RendererBackend_InitFn)(RendererBackend *ctx, DOT_Window *window);
typedef void (*RendererBackend_ShutdownFn)(RendererBackend *ctx);
typedef void (*RendererBackend_Draw)(RendererBackend *ctx, u8 current_frame);

struct RendererBackend{
    RendererBackendKind        backend_kind;
    Arena                     *permanent_arena;
    RendererBackend_InitFn     init;
    RendererBackend_ShutdownFn shutdown;
    RendererBackend_Draw       draw;
};

typedef struct FrameData{
    Arena *temp_arena;
}FrameData;

typedef struct DOT_Renderer{
    RendererBackend *backend;
    Arena           *permanent_arena;

    u64              current_frame;
    u8               frame_overlap;
    FrameData       *frame_data;
}DOT_Renderer;

internal RendererBackend* renderer_backend_create(Arena *arena, RendererBackendConfig *config);
internal void renderer_init(Arena *arena, DOT_Renderer *renderer, DOT_Window *window, RendererConfig *config);
internal void renderer_shutdown(DOT_Renderer *renderer);
#endif
