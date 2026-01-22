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

typedef struct RendererBackendBase RendererBackendBase;
typedef void (*RendererBackend_InitFn)(RendererBackendBase* ctx, DOT_Window* window);
typedef void (*RendererBackend_ShutdownFn)(RendererBackendBase* ctx);

struct RendererBackendBase{
        RendererBackendKind backend_kind;
        Arena* arena;
        RendererBackend_InitFn Init;
        RendererBackend_ShutdownFn Shutdown;
};

internal RendererBackendBase* RendererBackend_Create(Arena* arena, RendererBackendKind backend_kind);
internal void RendererBackend_Shutdown(RendererBackendBase* backend);
#endif
