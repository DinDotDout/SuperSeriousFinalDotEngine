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

typedef enum RendererBackendKind{
    RendererBackendKind_Null, // Headless
    RendererBackendKind_Vk,
    RendererBackendKind_Dx12,
#if DOT_OS_WINDOWS
    RendererBackendKind_Auto = RendererBackendKind_Dx12,
#elif DOT_OS_POSIX
    RendererBackendKind_Auto = RendererBackendKind_Vk,
#else
    RendererBackendKind_Auto = RendererBackendKind_None,
#endif
    RendererBackendKind_Count,
}RendererBackendKind;

enum{
    RENDER_RESOURCE_CLEANUP_CTX_TEXTURES  = 64,
    RENDER_RESOURCE_CLEANUP_CTX_BUFFERS   = 64,
    RENDER_RESOURCE_CLEANUP_CTX_SAMPLERS  = 64,
    RENDER_RESOURCE_CLEANUP_CTX_DESCRIPTOR_SET_LAYOUTS= 64,
};

typedef struct RN_ResourceCleanupCtx{
    TreeHeader node;
    ARRAY(RN_TextureHandle, RENDER_RESOURCE_CLEANUP_CTX_TEXTURES)   texture_ids;
    ARRAY(RN_BufferHandle,  RENDER_RESOURCE_CLEANUP_CTX_BUFFERS)    buffer_ids;
    ARRAY(RN_SamplerHandle, RENDER_RESOURCE_CLEANUP_CTX_SAMPLERS)   sampler_ids;
    ARRAY(RN_ShaderResourceLayoutHandle, RENDER_RESOURCE_CLEANUP_CTX_DESCRIPTOR_SET_LAYOUTS)   descriptor_set_layout_ids;
    SLICE(Arena *) temp_arenas;
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
    FN(RN_ShaderModuleHandle,           shader_create,                  (String8 fb)) \
    FN(RN_TextureHandle,                texture_create,                 (const RN_TextureDesc *desc, void *data, String8 debug_name)) \
    FN(RN_SamplerHandle,                sampler_create,                 (const RN_SamplerDesc *desc, String8 debug_name)) \
    FN(RN_BufferHandle,                 buffer_create,                  (const RN_BufferDesc *desc, u8 *data, String8 debug_name)) \
    FN(RN_ShaderResourceLayoutHandle,   shader_resource_layout_create,  (RN_ShaderResourceLayout *resource_layout)) \
    FN(void, shader_unload,     (RN_ShaderModuleHandle shader_module)) \
    FN(void, texture_destroy,   (RN_TextureHandle texture_handle)) \
    FN(void, buffer_destroy,    (RN_BufferHandle buffer_handle)) \
    FN(void, sampler_destroy,   (RN_SamplerHandle sampler_handle)) \
    FN(void, overlay_init, (const void *font_pixels, int font_w, int font_h)) \
    FN(void, overlay_shutdown, (void)) \
    FN(void, overlay_render, (u8 frame_idx, OverlayDrawList *draw_list)) \

typedef struct RendererBackend{
    RendererBackendKind backend_kind;
    Arena *permanent_arena;
    Arena *transient_arena;
#define FN(ret, name, params) ret (*name) params;
    RN_BACKEND_FN_LIST
#undef FN
}RendererBackend;

typedef struct FrameData{
    Arena *temp_arena;
    // u8 frame_idx;
}FrameData;

// (jd) NOTE:Use permanent arena for all init stuff
// use transient arena as a series of pop markers based on context?
typedef struct RN_Renderer{
    Arena                   *permanent_arena;
    Arena                   *transient_arena;
    HashMap_RN_ShaderModule shader_cache; // This should be a general cache, and check there before loading assets
    RN_ResourceCleanupTree  cleanup_tree;

    u32         frame_data_count;
    FrameData  *frame_datas;

    // ShaderCache shader_cache;
    RendererBackend *backend;
}RN_Renderer;

DOT_ENGINE_API void                 rn_clear_background(RN_Renderer *renderer, vec3 color);
DOT_ENGINE_API RN_ShaderModule      *rn_shader_module_load_from_path(RN_Renderer *renderer, String8 path);

// This should be an external loader and renderer should fill in the rest
DOT_ENGINE_API RN_Texture           rn_texture_load_from_path(RN_Renderer *renderer, String8 name, String8 path, u8 mip_levels);

internal void               rn_init(Arena *arena, RN_Renderer *renderer, DOT_Window *window);
internal void               rn_shutdown(RN_Renderer *renderer);
internal RendererBackend*   rn_backend_create(Arena *arena);

internal RN_TextureHandle   rn_texture_create(RN_Renderer *renderer, const RN_TextureDesc *desc, void *data, String8 debug_name);
internal void               rn_texture_destroy(RN_Renderer *renderer, RN_TextureHandle handle);

internal RN_SamplerHandle   rn_sampler_create(RN_Renderer *renderer, const RN_SamplerDesc *desc, String8 debug_name);

internal RN_BufferHandle    rn_buffer_create(RN_Renderer *renderer, const RN_BufferDesc *desc, u8 *data, String8 debug_name);

internal void               rn_frame_begin(RN_Renderer *renderer);
internal void               rn_frame_end(RN_Renderer *renderer);


internal void               rn_overlay_init(RN_Renderer *renderer, const void *font_pixels, int font_w, int font_h);
internal void               rn_overlay_render(RN_Renderer *renderer, u8 frame_idx, OverlayDrawList *draw_list);
internal void               rn_overlay_shutdown(RN_Renderer *renderer);

internal void rn_resource_cleanup_list_pop_all(RN_Renderer *renderer);
// internal void rn_resource_cleanup_list_pop_at(PoolHandle pop_start);
// internal void rn_resource_cleanup_list_pop_last();
internal void rn_resource_cleanup_list_push_buffer(RN_Renderer *renderer, RN_BufferHandle buffer_id);
internal void rn_resource_cleanup_list_push_sampler(RN_Renderer *renderer, RN_SamplerHandle sampler_id);
internal void rn_resource_cleanup_list_push_texture(RN_Renderer *renderer, RN_TextureHandle texture_id);
// internal void rn_vk_resource_cleanup_list_push_scope();

internal RN_RenderPassOutput rn_swapchain_output();

// internal void rn_draw(RN_Renderer *renderer);
#endif // !RENDERER_H
