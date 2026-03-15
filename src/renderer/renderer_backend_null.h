#ifndef RENDERER_BACKEND_NULL_H
#define RENDERER_BACKEND_NULL_H

#define FN(ret, name, args) internal ret renderer_backend_null_##name args;
RENDERER_BACKEND_FN_LIST
#undef FN

typedef struct RendererBackendNull{
    RendererBackend base;
}RendererBackendNull;

internal DOT_ShaderModuleHandle
renderer_backend_null_load_shader_from_file_buffer(DOT_FileBuffer fb)
{
    UNUSED(fb);
    return (DOT_ShaderModuleHandle){0};
}

internal void
renderer_backend_null_load(DOT_Window* window)
{
	UNUSED(window);
}

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

#define FN(ret, name, args) base->name = renderer_backend_null_##name;
    RENDERER_BACKEND_FN_LIST
#undef FN
    return backend;
}

#endif // !RENDERER_BACKEND_NULL_H
