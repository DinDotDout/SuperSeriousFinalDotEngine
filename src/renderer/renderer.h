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

#define RENDERER_PRESENT_MODE_KINDS(FN) \
    FN(RendererPresentModeKind, Immediate) \
    FN(RendererPresentModeKind, Mailbox) \
    FN(RendererPresentModeKind, FIFO) \
    FN(RendererPresentModeKind, FIFO_Relaxed) \

DOT_ENUM_REFLECT(RendererPresentModeKind, RENDERER_PRESENT_MODE_KINDS)

typedef struct RendererBackendConfig{
    RendererBackendKind backend_kind;
    u32 backend_memory_size;
    u32 backend_transient_memory_size;

    RendererPresentModeKind present_mode;
    u8 frame_overlap;
}RendererBackendConfig;

typedef struct RendererConfig{
    u64 renderer_permanent_memory_size;
    u64 renderer_transient_memory_size;
    u64 frame_arena_size;

    RendererBackendConfig *backend_config;
    ShaderCacheConfig     *shader_cache_config;
}RendererConfig;


// (jd) Backend functions called through this to allow swapping func signature
#if defined(DOT_RENDER_BACKEND_ONLY_VK)
#   define RENDER_BACKEND_CALL(fn) renderer_backend_vk_##fn
#elif defined(DOT_RENDER_BACKEND_ONLY_DX12)
#   define RENDER_BACKEND_CALL(fn) renderer_backend_dx12_##fn
#else
#   define RENDER_BACKEND_CALL(fn) renderer->backend->fn
#endif
#define RENDERER_BACKEND_FN_LIST \
    FN(void, init, (DOT_Window *window)) \
    FN(void, shutdown, (void)) \
    FN(void, frame_begin, (void)) \
    FN(void, frame_end, (void)) \
    FN(void, clear_bg, (vec3 color)) \
    FN(DOT_ShaderModuleHandle, shader_load_from_data, (String8 fb)) \
    FN(void, shader_unload, (DOT_ShaderModuleHandle shader_module)) \
    FN(DOT_TextureHandle, texture_create, (const RenderTypes_TextureDesc *desc, void *data, String8 debug_name)) \
    FN(DOT_SamplerHandle, sampler_create, (const RenderTypes_SamplerDesc *desc, String8 debug_name)) \
    FN(DOT_BufferHandle, buffer_create, (const RenderTypes_BufferDesc *desc, u8 *data, String8 debug_name)) \
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
    u8 frame_overlap;
#define FN(ret, name, params) ret (*name) params;
    RENDERER_BACKEND_FN_LIST
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

    ShaderCache shader_cache;
    RendererBackend *backend;
}DOT_Renderer;

DOT_ENGINE_API void                 renderer_clear_background(DOT_Renderer *renderer, vec3 color);
DOT_ENGINE_API DOT_ShaderModule*    renderer_shader_module_load_from_path(DOT_Renderer *renderer, String8 path);
DOT_ENGINE_API DOT_TextureAsset     renderer_texture_asset_create(DOT_Renderer *renderer, const DOT_AssetCreateInfo *asset_info, u8 mip_levels);

internal void               renderer_init(Arena *arena, DOT_Renderer *renderer, DOT_Window *window, const RendererConfig *config);
internal void               renderer_shutdown(DOT_Renderer *renderer);
internal RendererBackend*   renderer_backend_create(Arena *arena, RendererBackendConfig *backend_config);

internal DOT_Asset          dot_asset_from_create_info(DOT_Renderer *renderer, const DOT_AssetCreateInfo *asset_info, DOT_AssetKind kind);
internal DOT_TextureHandle  renderer_texture_create(DOT_Renderer *renderer, const RenderTypes_TextureDesc *desc, void *data, String8 debug_name);
internal void               renderer_texture_destroy(DOT_Renderer *renderer, DOT_TextureHandle handle);

internal DOT_SamplerHandle  renderer_sampler_create(DOT_Renderer *renderer, const RenderTypes_SamplerDesc *desc, String8 debug_name);

internal DOT_BufferHandle  renderer_buffer_create(DOT_Renderer *renderer, const RenderTypes_BufferDesc *desc, u8 *data, String8 debug_name);

internal void               renderer_frame_begin(DOT_Renderer *renderer);
internal void               renderer_frame_end(DOT_Renderer *renderer);


internal void               renderer_overlay_init(DOT_Renderer *renderer, const void *font_pixels, int font_w, int font_h);
internal void               renderer_overlay_render(DOT_Renderer *renderer, u8 frame_idx, OverlayDrawList *draw_list);
internal void               renderer_overlay_shutdown(DOT_Renderer *renderer);

// internal void renderer_draw(DOT_Renderer *renderer);
#endif // !RENDERER_H
