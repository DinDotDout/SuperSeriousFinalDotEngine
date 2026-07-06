#ifndef RENDERER_H
#define RENDERER_H

////////////////////////////////////////////////////////////////
// UI Overlay

// (jd) TODO: Kill this AI Overlay stuff and redo
typedef struct OverlayVertex{
    f32 position[2];
    f32 uv[2];
    u8  col[4];
} OverlayVertex;

typedef struct OverlayDrawCmd{
    u32 elem_count;
    f32 clip_x, clip_y, clip_w, clip_h;
} OverlayDrawCmd;

typedef struct OverlayDrawList {
    const void     *vertices;
    usize           vertex_size;
    const void     *indices;
    usize           index_size;
    OverlayDrawCmd *cmds;
    u32             cmd_count;
    u32             width;
    u32             height;
} OverlayDrawList;

typedef enum RN_BackendKind{
    RN_BackendKind_Null, // Headless
    RN_BackendKind_Vk,
    RN_BackendKind_Dx12,
#if DOT_OS_WINDOWS
    RN_BackendKind_Auto = RN_BackendKind_Dx12,
#elif DOT_OS_POSIX
    RN_BackendKind_Auto = RN_BackendKind_Vk,
#else
    RN_BackendKind_Auto = RN_BackendKind_None,
#endif
    RN_BackendKind_Count,
}RN_BackendKind;

enum{
    RENDER_RESOURCE_CLEANUP_CTX_TEXTURES  = 64,
    RENDER_RESOURCE_CLEANUP_CTX_BUFFERS   = 64,
    RENDER_RESOURCE_CLEANUP_CTX_SAMPLERS  = 64,
    RENDER_RESOURCE_CLEANUP_CTX_SHADER_RESOURCE_LAYOUTS = 64,
};

typedef struct RN_ResourceCleanupCtx{
    TreeHeader node;
    ARRAY(RN_TextureHandle, RENDER_RESOURCE_CLEANUP_CTX_TEXTURES)   texture_ids;
    ARRAY(RN_BufferHandle,  RENDER_RESOURCE_CLEANUP_CTX_BUFFERS)    buffer_ids;
    ARRAY(RN_SamplerHandle, RENDER_RESOURCE_CLEANUP_CTX_SAMPLERS)   sampler_ids;
    ARRAY(RN_ShaderResourceLayoutHandle, RENDER_RESOURCE_CLEANUP_CTX_SHADER_RESOURCE_LAYOUTS) shader_resource_layout_ids;
    struct{ // (jd) use this for a resource free list / pool source
        u32 count;
        Arena **data;
    }arenas;

}RN_ResourceCleanupCtx;

typedef TREE_POOL(RN_ResourceCleanupCtx) RN_ResourceCleanupTree;

// (jd) Backend functions called through this to allow swapping func signature
#if defined(DOT_RENDER_BACKEND_ONLY_VK)
#   define RENDER_BACKEND_CALL(fn) rn_backend_vk_##fn
#elif defined(DOT_RENDER_BACKEND_ONLY_DX12)
#   define RENDER_BACKEND_CALL(fn) rn_backend_dx12_##fn
#else
#   define RENDER_BACKEND_CALL(fn) renderer->backend->fn
#endif

#define RN_BACKEND_FN_LIST \
FN(void, init, (DOT_Window *window)) \
FN(void, shutdown, (void)) \
FN(void, frame_begin, (void)) \
FN(void, frame_end, (void)) \
FN(void, clear_bg, (vec3 color)) \
FN(RN_ShaderStageHandle,           shader_create,                  (String8 fb)) \
FN(RN_TextureHandle,                texture_create,                 (RN_TextureDesc *desc, void *data)) \
FN(RN_SamplerHandle,                sampler_create,                 (RN_SamplerDesc *desc)) \
FN(RN_BufferHandle,                 buffer_create,                  (RN_BufferDesc *desc, u8 *data)) \
FN(RN_ShaderResourceLayoutHandle,   shader_resource_layout_create,  (RN_ShaderResourceLayoutDesc *desc)) \
FN(RN_PipelineHandle,               pipeline_create,                (RN_PipelineDesc *desc)) \
FN(void, shader_unload,     (RN_ShaderStageHandle shader_module)) \
FN(void, texture_destroy,   (RN_TextureHandle texture_handle)) \
FN(void, buffer_destroy,    (RN_BufferHandle buffer_handle)) \
FN(void, sampler_destroy,   (RN_SamplerHandle sampler_handle)) \

typedef struct RN_BackendCtx{
    RN_BackendKind backend_kind;
    Arena *permanent_arena;
    Arena *transient_arena;

#define FN(ret, name, params) ret (*name) params;
RN_BACKEND_FN_LIST
#undef FN

}RN_BackendCtx;

typedef struct FrameData{
    Arena *temp_arena;
    // u8 frame_idx;
}FrameData;

// (jd) NOTE:Use permanent arena for all init stuff
// use transient arena as a series of pop markers based on context?
typedef struct RN_RenderCtx{
    Arena                   *permanent_arena;
    Arena                   *transient_arena;
    HashMap_RN_ShaderCachedData shader_cache; // This should be a general cache, and check there before loading assets
    RN_ResourceCleanupTree  cleanup_tree;

    u32         frame_data_count;
    FrameData  *frame_datas;

    // ShaderCache shader_cache;
    RN_BackendCtx *backend;
}RN_RenderCtx;

DOT_ENGINE_API void      rn_clear_background(RN_RenderCtx *renderer, vec3 color);

internal RN_ShaderStageHandle rn_shader_stage_handle_get_or_create(RN_RenderCtx *renderer, RN_ShaderStage *stage);
// This should be an external loader and renderer should fill in the rest
DOT_ENGINE_API RN_Texture     rn_texture_load_from_path(RN_RenderCtx *renderer, String8 name, String8 path, u8 mip_levels);

internal void               rn_init(Arena *arena, RN_RenderCtx *renderer, DOT_Window *window);
internal void               rn_shutdown(RN_RenderCtx *renderer);
internal RN_BackendCtx     *rn_backend_create(Arena *arena);

internal RN_TextureHandle               rn_texture_create_h(RN_RenderCtx *renderer, RN_TextureDesc *desc, void *data);
internal RN_SamplerHandle               rn_sampler_create_h(RN_RenderCtx *renderer, RN_SamplerDesc *desc);
internal RN_BufferHandle                rn_buffer_create_h(RN_RenderCtx *renderer, RN_BufferDesc *desc, u8 *data);
internal RN_PipelineHandle              rn_pipeline_create_h(RN_RenderCtx *renderer, RN_PipelineDesc *desc);
internal RN_ShaderResourceLayoutHandle  rn_shader_resource_layout_create_h(RN_RenderCtx *renderer, RN_ShaderResourceLayoutDesc *desc);

internal void rn_texture_destroy(RN_RenderCtx *renderer, RN_TextureHandle handle);
internal void rn_buffer_destroy(RN_RenderCtx *renderer, RN_BufferHandle handle);
internal void rn_sampler_destroy(RN_RenderCtx *renderer, RN_SamplerHandle handle);

internal void               rn_frame_begin(RN_RenderCtx *renderer);
internal void               rn_frame_end(RN_RenderCtx *renderer);

internal void rn_resource_cleanup_list_pop_all(RN_RenderCtx *renderer);
// internal void rn_resource_cleanup_list_pop_at(PoolHandle pop_start);
// internal void rn_resource_cleanup_list_pop_last();
internal void rn_resource_cleanup_list_push_buffer(RN_RenderCtx *renderer, RN_BufferHandle buffer_id);
internal void rn_resource_cleanup_list_push_sampler(RN_RenderCtx *renderer, RN_SamplerHandle sampler_id);
internal void rn_resource_cleanup_list_push_texture(RN_RenderCtx *renderer, RN_TextureHandle texture_id);
// internal void rn_vk_resource_cleanup_list_push_scope();

internal RN_RenderPassOutput rn_swapchain_output();

// internal void rn_draw(RN_RenderCtx *renderer);
#endif // !RENDERER_H
