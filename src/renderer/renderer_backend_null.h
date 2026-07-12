#ifndef rn_null_H
#define rn_null_H

typedef struct RN_Null_BackendCtx{
    RN_BackendCtx base;
}RN_Null_BackendCtx;

global RN_Null_BackendCtx *g_null_ctx;

DOT_DIAGNOSTIC_PUSH
#if DOT_COMPILER_CLANG || DOT_COMPILER_GCC
    DOT_DIAGNOSTIC_IGNORE("-Wunused-parameter")
#elif DOT_COMPILER_MSVC
    DOT_DIAGNOSTIC_IGNORE(4100)
#endif

internal void rn_null_init(DOT_Window* window){}
internal void rn_null_shutdown(){}
internal void rn_null_frame_begin(){}
internal void rn_null_frame_end(){}
internal void rn_null_clear_bg(){}
internal void rn_null_shader_unload(RN_ShaderStageHandle sm){}
internal void rn_null_resource_cleanup_list_push(void){}
internal void rn_null_resource_cleanup_list_pop_at(PoolHandle idx){}
internal void rn_null_resource_cleanup_list_pop_last(void){}

// (jd) Empty handles
internal RN_BufferHandle                rn_null_buffer_create(RN_BufferDesc *desc, u8 *data){ return((RN_BufferHandle){0}); }
internal RN_SamplerHandle               rn_null_sampler_create(RN_SamplerDesc *desc){ return((RN_SamplerHandle){0}); }
internal RN_TextureHandle               rn_null_texture_create(RN_TextureDesc *desc, void *data) { return((RN_TextureHandle){0}); }
internal RN_PipelineHandle              rn_null_pipeline_create(RN_PipelineDesc *desc) { return((RN_PipelineHandle){0}); }
internal RN_ShaderResourceLayoutHandle  rn_null_shader_resource_layout_create(RN_ShaderResourceLayoutDesc *desc) { return((RN_ShaderResourceLayoutHandle){0}); }
internal RN_ShaderResourceHandle        rn_null_shader_resource_create(RN_ShaderResourceDesc *desc) { return((RN_ShaderResourceHandle){0}); }
internal RN_ShaderStageHandle           rn_null_shader_create(String8 shader_data) { return((RN_ShaderStageHandle){0}); }

internal void rn_null_texture_destroy(RN_TextureHandle texture_handle){}
internal void rn_null_buffer_destroy(RN_BufferHandle buffer_handle){}
internal void rn_null_sampler_destroy(RN_SamplerHandle sampler_handle){}

// Delete pls
internal void rn_null_overlay_init(const void *font_pixels, int font_w, int font_h){}
internal void rn_null_overlay_shutdown(void){}
internal void rn_null_overlay_render(u8 frame_idx, OverlayDrawList *draw_list){}

internal RN_Null_BackendCtx* rn_null_create(Arena *arena){ g_null_ctx = PUSH_STRUCT(arena, RN_Null_BackendCtx);

#define FN(ret, name, params) g_null_ctx->base.name = rn_null_##name;
    RN_BACKEND_FN_LIST
#undef FN
    return(g_null_ctx);
}

DOT_DIAGNOSTIC_POP
#endif // !rn_null_H
