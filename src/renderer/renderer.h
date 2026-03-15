#ifndef RENDERER_H
#define RENDERER_H

#define DOT_RENDER_BACKEND_VK 1
// #define DOT_RENDER_BACKEND_HOTSWAP 1

#if defined(DOT_RENDER_BACKEND_HOTSWAP)
// #define RENDER_BACKEND_CALL(fn, ...) renderer->backend->fn(__VA_ARGS__)
#define RENDER_BACKEND_CALL(fn) renderer->backend->fn
#elif defined(DOT_RENDER_BACKEND_VK)
// #define RENDER_BACKEND_CALL(fn, ...) renderer_backend_vk_##fn(__VA_ARGS__)
#define RENDER_BACKEND_CALL(fn) renderer_backend_vk_##fn
#elif defined(DOT_RENDER_BACKEND_DX12)
// #define RENDER_BACKEND_CALL(fn, ...) renderer_backend_dx12_##fn(__VA_ARGS__)
#define RENDER_BACKEND_CALL(fn) renderer_backend_dx12_##fn
#endif

typedef DOT_ENUM(u8, RendererBackendKind){
    RendererBackendKind_None, // Stub backend?
    RendererBackendKind_Null, // Offscreen render
    RendererBackendKind_Vk,
    RendererBackendKind_Dx12,
    RendererBackendKind_Count,
};

typedef DOT_ENUM(u8, RendererPresentModeKind){
    RendererPresentModeKind_Immediate,
    RendererPresentModeKind_Mailbox,
    RendererPresentModeKind_FIFO,
    RendererPresentModeKind_FIFO_Relaxed,
    RendererPresentModeKind_FIFO_Count,
};
global const char *renderer_present_mode_kind_str[] = {
    [RendererPresentModeKind_Immediate]   = "RendererPresentModeKind_Immediate",
    [RendererPresentModeKind_Mailbox] = "RendererPresentModeKind_Mailbox",
    [RendererPresentModeKind_FIFO]  = "RendererPresentModeKind_FIFO",
    [RendererPresentModeKind_FIFO_Relaxed] = "RendererPresentModeKind_FIFO_Relaxed",
};
DOT_STATIC_ASSERT(RendererPresentModeKind_FIFO_Count == ARRAY_COUNT(renderer_present_mode_kind_str));

typedef struct RendererBackendConfig{
    RendererBackendKind backend_kind;
    u64 backend_memory_size;

    RendererPresentModeKind present_mode;
    u8 frame_overlap;
}RendererBackendConfig;

typedef struct RendererConfig{
    u64 renderer_memory_size;
    u64 frame_arena_size;

    RendererBackendConfig backend_config;
    ShaderCacheConfig     shader_cache_config;
}RendererConfig;

/* ------------------------------------------------------------------ */
/*  Overlay integration types (backend-agnostic)                      */
/* ------------------------------------------------------------------ */

typedef struct OverlayVertex {
    f32 position[2];
    f32 uv[2];
    u8  col[4];
} OverlayVertex;

typedef struct OverlayDrawCmd {
    u32 elem_count;
    f32 clip_x, clip_y, clip_w, clip_h;
} OverlayDrawCmd;

typedef struct OverlayDrawList {
    const void     *vertices;
    usize           vertex_size;
    const void     *indices;
    usize           index_size;
    OverlayDrawCmd *cmds;
    u32             cmd_count;
    u32             width;
    u32             height;
} OverlayDrawList;

// renderer_backend_funcs.x.h
#define RENDERER_BACKEND_FN_LIST \
    FN(void, init, (DOT_Window *window)) \
    FN(void, shutdown, (void)) \
    FN(void, begin_frame, (u8 current_frame)) \
    FN(void, end_frame, (u8 current_frame)) \
    FN(void, clear_bg, (u8 current_frame, vec3 color)) \
    FN(DOT_ShaderModuleHandle, load_shader_from_file_buffer, (DOT_FileBuffer fb)) \
    FN(void, unload_shader_module, (DOT_ShaderModuleHandle)) \
    FN(void, overlay_init, (const void *font_pixels, int font_w, int font_h)) \
    FN(void, overlay_shutdown, (void)) \
    FN(void, overlay_render, (u8 frame_idx, OverlayDrawList *draw_list))

typedef struct RendererBackend{
    RendererBackendKind backend_kind;
    Arena *permanent_arena;
    u8 frame_overlap;

#define FN(ret, name, args) ret (*name) args;
    RENDERER_BACKEND_FN_LIST
#undef FN
}RendererBackend;

typedef struct FrameData{
    Arena *temp_arena;
    // u8 frame_idx;
}FrameData;

typedef struct DOT_Renderer{
    Arena *permanent_arena;

    // u8         frame_overlap;
    u64        current_frame;
    FrameData *frame_data;

    ShaderCache shader_cache;
    RendererBackend *backend;
}DOT_Renderer;

#if DOT_OS_WINDOWS
#  if DOT_RENDERER_BUILD
#    define DOT_RENDERER_API __declspec(dllexport)
#  else
#    define DOT_RENDERER_API __declspec(dllimport)
#  endif
#else
#  define DOT_RENDERER_API
#endif


DOT_RENDERER_API void renderer_clear_background(DOT_Renderer *renderer, vec3 color);
DOT_RENDERER_API DOT_ShaderModule* renderer_load_shader_module_from_path(Arena *arena, DOT_Renderer *renderer, String8 path);

internal void renderer_begin_frame(DOT_Renderer *renderer);
internal void renderer_end_frame(DOT_Renderer *renderer);

internal RendererBackend* renderer_backend_create(Arena *arena, RendererBackendConfig *config);
internal void renderer_init(Arena *arena, DOT_Renderer *renderer, DOT_Window *window, RendererConfig *config);
internal void renderer_shutdown(DOT_Renderer *renderer);

internal void renderer_overlay_init(DOT_Renderer *renderer, const void *font_pixels, int font_w, int font_h);
internal void renderer_overlay_render(DOT_Renderer *renderer, u8 frame_idx, OverlayDrawList *draw_list);
internal void renderer_overlay_shutdown(DOT_Renderer *renderer);

// internal void renderer_draw(DOT_Renderer *renderer);
#endif // !RENDERER_H
