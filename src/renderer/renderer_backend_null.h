#ifndef RENDERER_BACKEND_NULL_H
#define RENDERER_BACKEND_NULL_H

typedef struct RendererBackendNull{
    RendererBackend base;
}RendererBackendNull;

internal void
renderer_backend_null_init(RendererBackend* base_ctx, DOT_Window* window){
	UNUSED(window); UNUSED(base_ctx);
}
internal void
renderer_backend_null_shutdown(RendererBackend* base_ctx){
	UNUSED(base_ctx);

}
internal void
renderer_backend_null_draw(RendererBackend* base_ctx, u8 current_frame, u64 frame){
	 UNUSED(base_ctx); UNUSED(current_frame); UNUSED(frame);
}

internal RendererBackendNull*
renderer_backend_null_create(Arena* arena){
	RendererBackendNull *backend = PUSH_STRUCT(arena, RendererBackendNull);
    RendererBackend *base = &backend->base;
    base->backend_kind = RendererBackendKind_Null;
    base->init         = renderer_backend_null_init;
    base->shutdown     = renderer_backend_null_shutdown;
    base->draw         = renderer_backend_null_draw;
    return backend;
}
#endif // !RENDERER_BACKEND_NULL_H
