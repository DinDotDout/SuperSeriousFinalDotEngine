#ifndef RENDERER_H
#define RENDERER_H

typedef DOT_ENUM(u8, RendererBackendKind){
    RendererBackendKind_Null,
    RendererBackendKind_Vk,
    RendererBackendKind_Gl,
    RendererBackendKind_Dx12,
    RendererBackendKind_Count,
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
typedef void (*RendererBackend_BeginFrameFn)(RendererBackend *ctx, u8 current_frame);
typedef void (*RendererBackend_EndFrameFn)(RendererBackend *ctx, u8 current_frame);
// typedef void (*RendererBackend_Draw)(RendererBackend *ctx, u8 current_frame, u64 frame);

struct RendererBackend{
    RendererBackendKind backend_kind;
    Arena *permanent_arena;
    RendererBackend_InitFn       init;
    RendererBackend_ShutdownFn   shutdown;
    RendererBackend_BeginFrameFn begin_frame;
    RendererBackend_EndFrameFn   end_frame;
    // RendererBackend_Draw         draw;
};

typedef struct FrameData{
    Arena *temp_arena;
    // u8 frame_idx;
}FrameData;

typedef struct DOT_Renderer{
    RendererBackend *backend;
    Arena           *permanent_arena;

    u8         frame_overlap;
    u64        current_frame;
    FrameData *frame_data;
}DOT_Renderer;


typedef struct DOT_ShaderModule{
    String8 path;
    u8*     bytes;
}DOT_ShaderModule;

internal void renderer_load_shader_module_from_path(Arena *arena, String8 path);

internal void renderer_begin_frame(DOT_Renderer *renderer);
internal void renderer_end_frame(DOT_Renderer *renderer);
internal void renderer_clear_background(DOT_Renderer *renderer);

internal RendererBackend* renderer_backend_create(Arena *arena, RendererBackendConfig *config);
internal void renderer_init(Arena *arena, DOT_Renderer *renderer, DOT_Window *window, RendererConfig *config);
internal void renderer_shutdown(DOT_Renderer *renderer);
// internal void renderer_draw(DOT_Renderer *renderer);
#endif // !RENDERER_H
