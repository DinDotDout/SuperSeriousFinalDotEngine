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


typedef struct RendererBackendConfig{
    RendererBackendKind backend_kind;
    u32 backend_memory_size;
    u32 backend_transient_memory_size;

    RN_PresentModeKind present_mode;
    u8 frame_overlap;
}RendererBackendConfig;

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
    FN(DOT_ShaderModuleHandle, shader_load_from_data, (String8 fb)) \
    FN(void, shader_unload, (DOT_ShaderModuleHandle shader_module)) \
    FN(DOT_TextureHandle, texture_create, (const RN_TextureDesc *desc, void *data, String8 debug_name)) \
    FN(DOT_SamplerHandle, sampler_create, (const RN_SamplerDesc *desc, String8 debug_name)) \
    FN(DOT_BufferHandle, buffer_create, (const RN_BufferDesc *desc, u8 *data, String8 debug_name)) \
    FN(void, texture_destroy, (DOT_TextureHandle texture_handle)) \
    FN(void, overlay_init, (const void *font_pixels, int font_w, int font_h)) \
    FN(void, overlay_shutdown, (void)) \
    FN(void, overlay_render, (u8 frame_idx, OverlayDrawList *draw_list)) \
    // FN(void, resource_cleanup_list_push, (void)) \
    // FN(void, resource_cleanup_list_pop_at, (PoolHandle)) \
    // FN(void, resource_cleanup_list_pop_last, (void))

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
typedef struct DOT_Renderer{
    Arena *permanent_arena;
    Arena *transient_arena;
    u32         frame_data_count;
    FrameData  *frame_datas;

    HashMap_DOT_ShaderModule shader_cache;
    // ShaderCache shader_cache;
    RendererBackend *backend;
}DOT_Renderer;

DOT_ENGINE_API void                 rn_clear_background(DOT_Renderer *renderer, vec3 color);
DOT_ENGINE_API DOT_ShaderModule*    rn_shader_module_load_from_path(DOT_Renderer *renderer, String8 path);
DOT_ENGINE_API DOT_TextureAsset     rn_texture_asset_create(DOT_Renderer *renderer, const DOT_AssetCreateInfo *asset_info, u8 mip_levels);

internal void               rn_init(Arena *arena, DOT_Renderer *renderer, DOT_Window *window);
internal void               rn_shutdown(DOT_Renderer *renderer);
internal RendererBackend*   rn_backend_create(Arena *arena);

internal DOT_Asset          dot_asset_from_create_info(DOT_Renderer *renderer, const DOT_AssetCreateInfo *asset_info, DOT_AssetKind kind);
internal DOT_TextureHandle  rn_texture_create(DOT_Renderer *renderer, const RN_TextureDesc *desc, void *data, String8 debug_name);
internal void               rn_texture_destroy(DOT_Renderer *renderer, DOT_TextureHandle handle);

internal DOT_SamplerHandle  rn_sampler_create(DOT_Renderer *renderer, const RN_SamplerDesc *desc, String8 debug_name);

internal DOT_BufferHandle  rn_buffer_create(DOT_Renderer *renderer, const RN_BufferDesc *desc, u8 *data, String8 debug_name);

internal void               rn_frame_begin(DOT_Renderer *renderer);
internal void               rn_frame_end(DOT_Renderer *renderer);


internal void               rn_overlay_init(DOT_Renderer *renderer, const void *font_pixels, int font_w, int font_h);
internal void               rn_overlay_render(DOT_Renderer *renderer, u8 frame_idx, OverlayDrawList *draw_list);
internal void               rn_overlay_shutdown(DOT_Renderer *renderer);

internal RN_RenderPassOutput rn_swapchain_output();

// internal void rn_draw(DOT_Renderer *renderer);
#endif // !RENDERER_H
