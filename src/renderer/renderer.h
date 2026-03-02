#ifndef RENDERER_H
#define RENDERER_H

#define DOT_RENDER_BACKEND_VK 1
// #define DOT_RENDER_BACKEND_HOTSWAP 1

#if defined(DOT_RENDER_BACKEND_HOTSWAP)
#define RENDER_BACKEND_CALL(fn, ...) renderer->backend->fn(__VA_ARGS__)
#elif defined(DOT_RENDER_BACKEND_VK)
#define RENDER_BACKEND_CALL(fn, ...) renderer_backend_vk_##fn(__VA_ARGS__)
#elif defined(DOT_RENDER_BACKEND_DX12)
#define RENDER_BACKEND_CALL(fn, ...) renderer_backend_dx12_##fn(__VA_ARGS__)
#endif

typedef DOT_ENUM(u8, RendererBackendKind){
    RendererBackendKind_Null,
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

typedef struct RendererBackend RendererBackend;
typedef void (*RendererBackend_InitFn)(DOT_Window *window);
typedef void (*RendererBackend_ShutdownFn)();
typedef void (*RendererBackend_BeginFrameFn)(u8 current_frame);
typedef void (*RendererBackend_EndFrameFn)(u8 current_frame);
typedef void (*RendererBackend_ClearBgFn)(u8 current_frame, vec3 color);

typedef DOT_ShaderModuleHandle (*RendererBackend_LoadShaderFromFileBufferFn)(DOT_FileBuffer fb);
typedef void (*RendererBackend_UnLoadShaderModuleFn)(DOT_ShaderModuleHandle);

struct RendererBackend{
    RendererBackendKind backend_kind;
    Arena *permanent_arena;
    u8 frame_overlap;

    RendererBackend_InitFn       init;
    RendererBackend_ShutdownFn   shutdown;
    RendererBackend_BeginFrameFn begin_frame;
    RendererBackend_EndFrameFn   end_frame;
    RendererBackend_ClearBgFn    clear_bg;
    RendererBackend_LoadShaderFromFileBufferFn load_shader_from_file_buffer;
    RendererBackend_UnLoadShaderModuleFn unload_shader_module;
};

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

// internal void renderer_draw(DOT_Renderer *renderer);
#endif // !RENDERER_H
