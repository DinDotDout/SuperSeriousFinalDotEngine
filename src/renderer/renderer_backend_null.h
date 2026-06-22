#ifndef RN_BACKEND_NULL_H
#define RN_BACKEND_NULL_H

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

internal void rn_backend_null_load(DOT_Window* window) { }
internal void rn_backend_null_init(DOT_Window* window) { }
internal void rn_backend_null_shutdown() { }
internal void rn_backend_null_frame_begin() { }
internal void rn_backend_null_frame_end() { }
internal void rn_backend_null_clear_bg() { }
internal void rn_backend_null_shader_unload(DOT_ShaderModuleHandle sm) { }
internal void rn_backend_null_resource_cleanup_list_push(void){}
internal void rn_backend_null_resource_cleanup_list_pop_at(PoolHandle idx){}
internal void rn_backend_null_resource_cleanup_list_pop_last(void){}

internal DOT_BufferHandle
rn_backend_null_buffer_create(const RN_BufferDesc *desc, u8 *data, String8 debug_name)
{
    DOT_UNUSED(desc); DOT_UNUSED(debug_name); 
    return((DOT_BufferHandle){0});
}

internal DOT_SamplerHandle
rn_backend_null_sampler_create(const RN_SamplerDesc *desc, String8 debug_name)
{
    DOT_UNUSED(desc); DOT_UNUSED(debug_name); 
    return((DOT_SamplerHandle){0});
}

internal DOT_TextureHandle
rn_backend_null_texture_create(const RN_TextureDesc *desc, void *data, String8 debug_name)
{
    DOT_UNUSED(desc); DOT_UNUSED(data); DOT_UNUSED(debug_name); 
    return((DOT_TextureHandle){0});
}

internal void
rn_backend_null_texture_destroy(DOT_TextureHandle texture_handle)
{
   DOT_UNUSED(texture_handle);
}

internal DOT_ShaderModuleHandle
rn_backend_null_shader_load_from_data(String8 fb)
{
    return((DOT_ShaderModuleHandle){0});
}

internal void rn_backend_null_overlay_init(const void *font_pixels, int font_w, int font_h) { }
internal void rn_backend_null_overlay_shutdown(void) { }
internal void rn_backend_null_overlay_render(u8 frame_idx, OverlayDrawList *draw_list) { }


internal RendererBackendNull*
rn_null_create(Arena *arena){ g_null_ctx = PUSH_STRUCT(arena, RendererBackendNull);
#define FN(ret, name, params) g_null_ctx->base.name = rn_backend_null_##name;
    RN_BACKEND_FN_LIST
#undef FN
    return(g_null_ctx);
}

DOT_DIAGNOSTIC_POP
#endif // !RN_BACKEND_NULL_H
