#ifndef DOT_RENDERER_BACKEND
#define DOT_RENDERER_BACKEND
// NOTE: Do we even need to split backend and frontend in different files
typedef u8 DOT_RendererBackendKind;
enum{
        DOT_RENDERER_BACKEND_INVALID,
        DOT_RENDERER_BACKEND_VK,
        DOT_RENDERER_BACKEND_GL,
        DOT_RENDERER_BACKEND_DX12,
        DOT_RENDERER_BACKEND_COUNT,
};

typedef struct DOT_RendererBackendBase DOT_RendererBackendBase;
typedef struct DOT_Window DOT_Window;
typedef void (*DOT_RendererBackend_InitFn)(DOT_RendererBackendBase* ctx, DOT_Window* window);
typedef void (*DOT_RendererBackend_ShutdownFn)(DOT_RendererBackendBase* ctx);

struct DOT_RendererBackendBase {
        DOT_RendererBackendKind backend_kind;
        Arena* arena;
        DOT_RendererBackend_InitFn Init;
        DOT_RendererBackend_ShutdownFn Shutdown;
};

internal DOT_RendererBackendBase* DOT_RendererBackend_Create(Arena* arena, DOT_RendererBackendKind backend_kind);
internal void DOT_RendererBackend_Shutdown(DOT_RendererBackendBase* backend);
#endif
