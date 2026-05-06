#ifndef RENDERER_BACKEND_NULL_H
#define RENDERER_BACKEND_NULL_H

typedef struct RendererBackendNull{
    RendererBackend base;
}RendererBackendNull;

global RendererBackendNull *g_null_ctx;

DOT_DIAGNOSTIC_PUSH
#if DOT_COMPILER_CLANG || DOT_COMPILER_GCC
    DOT_DIAGNOSTIC_IGNORE("-Wunused-parameter")
#elif DOT_COMPILER_MSVC
    DOT_DIAGNOSTIC_IGNORE(4100)
#endif

internal void renderer_backend_null_load(DOT_Window* window) { }
internal void renderer_backend_null_init(DOT_Window* window) { }
internal void renderer_backend_null_shutdown() { }
internal void renderer_backend_null_begin_frame() { }
internal void renderer_backend_null_end_frame() { }
internal void renderer_backend_null_clear_bg() { }
internal void renderer_backend_null_shader_unload(DOT_ShaderModuleHandle sm) { }

internal DOT_TextureHandle
renderer_backend_null_texture_create(const DOT_TextureDesc *desc, void *data, String8 debug_name)
{
    DOT_UNUSED(desc); DOT_UNUSED(data); DOT_UNUSED(debug_name); 
    return (DOT_TextureHandle){0};
}

internal void
renderer_backend_null_texture_destroy(DOT_TextureHandle texture_handle)
{
   DOT_UNUSED(texture_handle); 
}

internal DOT_ShaderModuleHandle
renderer_backend_null_shader_load_from_data(String8 fb)
{
    return (DOT_ShaderModuleHandle){0};
}

internal void renderer_backend_null_overlay_init(const void *font_pixels, int font_w, int font_h) { }
internal void renderer_backend_null_overlay_shutdown(void) { }
internal void renderer_backend_null_overlay_render(u8 frame_idx, OverlayDrawList *draw_list) { }

internal RendererBackendNull*
renderer_backend_null_create(Arena *arena, RendererBackendConfig *backend_config){
	g_null_ctx = PUSH_STRUCT(arena, RendererBackendNull);
#define FN(ret, name, ...) g_null_ctx->base.name = renderer_backend_null_##name;
    RENDERER_BACKEND_FN_LIST
#undef FN
    return g_null_ctx;
}

DOT_DIAGNOSTIC_POP
#endif // !RENDERER_BACKEND_NULL_H
