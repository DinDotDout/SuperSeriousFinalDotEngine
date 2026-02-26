#ifndef RENDERER_BACKEND_NULL_H
#define RENDERER_BACKEND_NULL_H

#include "base/dot.h"
typedef struct RendererBackendNull{
    RendererBackend base;
}RendererBackendNull;

internal void
renderer_backend_null_init(DOT_Window* window)
{
	UNUSED(window);
}

internal void
renderer_backend_null_shutdown()
{
}

internal void
renderer_backend_null_begin_frame(u8 current_frame)
{
	 UNUSED(current_frame);
}

internal void
renderer_backend_null_end_frame(u8 current_frame)
{
	 UNUSED(current_frame);
}

internal void
renderer_backend_null_clear_bg(u8 current_frame, vec3 color)
{
	 UNUSED(current_frame); UNUSED(color);
}

// internal void
// renderer_backend_null_draw(RendererBackend* base_ctx, u8 current_frame, u64 frame){
// 	 UNUSED(base_ctx); UNUSED(current_frame); UNUSED(frame);
// }

internal RendererBackendNull*
renderer_backend_null_create(Arena *arena, RendererBackendConfig *backend_config){
    UNUSED(backend_config);
	RendererBackendNull *backend = PUSH_STRUCT(arena, RendererBackendNull);
    RendererBackend *base = &backend->base;
    base->backend_kind = RendererBackendKind_Null;
    base->init         = renderer_backend_null_init;
    base->shutdown     = renderer_backend_null_shutdown;
    base->begin_frame  = renderer_backend_null_begin_frame;
    base->end_frame    = renderer_backend_null_end_frame;
    base->clear_bg     = renderer_backend_null_clear_bg;
    return backend;
}

#endif // !RENDERER_BACKEND_NULL_H
