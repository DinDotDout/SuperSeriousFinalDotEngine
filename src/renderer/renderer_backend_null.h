#ifndef RENDERER_BACKEND_NULL_H
#define RENDERER_BACKEND_NULL_H

typedef struct RendererBackendNull{
    RendererBackend base;
}RendererBackendNull;

global RendererBackendNull *g_null_ctx;

DIAGNOSTIC_PUSH
#if DOT_COMPILER_CLANG || DOT_COMPILER_GCC
    DIAGNOSTIC_IGNORE("-Wunused-parameter")
#elif DOT_COMPILER_MSVC
    DIAGNOSTIC_IGNORE(4100)
#endif

internal void renderer_backend_null_load(DOT_Window* window) { }
internal void renderer_backend_null_init(DOT_Window* window) { }
internal void renderer_backend_null_shutdown() { }
internal void renderer_backend_null_begin_frame(u8 current_frame) { }
internal void renderer_backend_null_end_frame(u8 current_frame) { }
internal void renderer_backend_null_clear_bg(u8 current_frame, vec3 color) { }
internal void renderer_backend_null_overlay_init(const void *font_pixels, int font_w, int font_h) { }
internal void renderer_backend_null_overlay_shutdown(void) { }
internal void renderer_backend_null_overlay_render(u8 frame_idx, OverlayDrawList *draw_list) { }
internal void renderer_backend_null_unload_shader_module(DOT_ShaderModuleHandle sm) { }

internal DOT_TextureHandle renderer_backend_null_create_texture(DOT_TextureCreateInfo *create_info)
{ 
    return (DOT_TextureHandle){0};
}

internal DOT_ShaderModuleHandle
renderer_backend_null_load_shader_from_file_buffer(String8 fb)
{
    return (DOT_ShaderModuleHandle){0};
}

internal RendererBackendNull*
renderer_backend_null_create(Arena *arena, RendererBackendConfig *backend_config){
	g_null_ctx = PUSH_STRUCT(arena, RendererBackendNull);
#define FN(ret, name, ...) g_null_ctx->base.name = renderer_backend_null_##name;
    RENDERER_BACKEND_FN_LIST
#undef FN
    return g_null_ctx;
}

DIAGNOSTIC_POP
#endif // !RENDERER_BACKEND_NULL_H

