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
internal void rn_backend_null_shader_unload(RN_ShaderModuleHandle sm) { }
internal void rn_backend_null_resource_cleanup_list_push(void){}
internal void rn_backend_null_resource_cleanup_list_pop_at(PoolHandle idx){}
internal void rn_backend_null_resource_cleanup_list_pop_last(void){}

internal RN_BufferHandle
rn_backend_null_buffer_create(const RN_BufferDesc *desc, u8 *data, String8 debug_name)
{
    DOT_UNUSED(desc); DOT_UNUSED(debug_name);
    return((RN_BufferHandle){0});
}

internal RN_SamplerHandle
rn_backend_null_sampler_create(const RN_SamplerDesc *desc, String8 debug_name)
{
    DOT_UNUSED(desc); DOT_UNUSED(debug_name);
    return((RN_SamplerHandle){0});
}

internal RN_TextureHandle
rn_backend_null_texture_create(const RN_TextureDesc *desc, void *data, String8 debug_name)
{
    DOT_UNUSED(desc); DOT_UNUSED(data); DOT_UNUSED(debug_name);
    return((RN_TextureHandle){0});
}

internal RN_ShaderResourceLayoutHandle
rn_backend_null_shader_resource_layout_create(RN_ShaderResourceLayout *resource_layout)
{
    DOT_UNUSED(resource_layout);
    return((RN_ShaderResourceLayoutHandle){0});
}

internal void
rn_backend_null_texture_destroy(RN_TextureHandle texture_handle)
{
   DOT_UNUSED(texture_handle);
}

internal void
rn_backend_null_buffer_destroy(RN_BufferHandle buffer_handle)
{
   DOT_UNUSED(buffer_handle);
}

internal void
rn_backend_null_sampler_destroy(RN_SamplerHandle sampler_handle)
{
   DOT_UNUSED(sampler_handle);
}

internal RN_ShaderModuleHandle
rn_backend_null_shader_create(String8 fb)
{
    return((RN_ShaderModuleHandle){0});
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
