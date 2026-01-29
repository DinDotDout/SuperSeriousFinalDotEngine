#ifndef RENDERER_BACKEND
#define RENDERER_BACKEND
// NOTE: Do we even need to split backend and frontend in different files

typedef u8 RendererBackendKind;
enum{
        RENDERER_BACKEND_INVALID,
        RENDERER_BACKEND_VK,
        RENDERER_BACKEND_GL,
        RENDERER_BACKEND_DX12,
        RENDERER_BACKEND_COUNT,
};

typedef struct RendererBackendConfig{
    u64 memory_size;
    RendererBackendKind backend_kind;
}RendererBackendConfig;

typedef struct RendererBackend RendererBackend;
typedef void (*RendererBackend_InitFn)(RendererBackend* ctx, DOT_Window* window);
typedef void (*RendererBackend_ShutdownFn)(RendererBackend* ctx);

struct RendererBackend{
        RendererBackendKind backend_kind;
        Arena* permanent_arena;
        RendererBackend_InitFn init;
        RendererBackend_ShutdownFn shutdown;
};

internal RendererBackend* renderer_backend_create(Arena* arena, RendererBackendConfig *config);
internal void renderer_backend_shutdown(RendererBackend* backend);
#endif
