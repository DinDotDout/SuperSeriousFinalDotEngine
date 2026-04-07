#ifndef RENDERER_H
#define RENDERER_H

#include "dot_engine/dot_engine.h"
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

typedef enum RendererPresentModeKind{
    RENDERER_PRESENT_MODE_KINDS(DOT_X_ENUM_ARG)
}RendererPresentModeKind;

global const char *renderer_present_mode_kind_str[] = {
    RENDERER_PRESENT_MODE_KINDS(DOT_X_ENUM_STR)
};

typedef struct RendererBackendConfig{
    RendererBackendKind backend_kind;
    u64 backend_memory_size;

    RendererPresentModeKind present_mode;
    u8 frame_overlap;
}RendererBackendConfig;

typedef struct RendererConfig{
    u64 renderer_memory_size;
    u64 frame_arena_size;

    RendererBackendConfig *backend_config;
    ShaderCacheConfig     *shader_cache_config;
}RendererConfig;

// (JD) NOTE: Consider moving all the type definitions to its own header
#define DOT_TEXTURE_KIND(X) \
    X(DOT_TextureDimension, 1D) \
    X(DOT_TextureDimension, 2D) \
    X(DOT_TextureDimension, 3D) \
    X(DOT_TextureDimension, Array1D) \
    X(DOT_TextureDimension, Array2D) \
    X(DOT_TextureDimension, Array3D) \

#define DOT_TEXTURE_FORMAT(X) \
    X(DOT_TextureFormat, Invalid) \
    /* 8 BIT*/\
    X(DOT_TextureFormat, R8_UNORM) \
    X(DOT_TextureFormat, R8_UINT) \
    X(DOT_TextureFormat, RG8_UNORM) \
    X(DOT_TextureFormat, RGBA8_UNORM) \
    X(DOT_TextureFormat, RGBA8_SRGB) \
    X(DOT_TextureFormat, BGRA8_UNORM) \
    X(DOT_TextureFormat, BGRA8_SRGB) \
    /* HDR */\
    X(DOT_TextureFormat, R16F) \
    X(DOT_TextureFormat, RG16F) \
    X(DOT_TextureFormat, RGBA16F) \
    X(DOT_TextureFormat, R32F) \
    X(DOT_TextureFormat, RG32F) \
    X(DOT_TextureFormat, RGBA32F) \
    /* Depth stencil */\
    X(DOT_TextureFormat, D16) \
    X(DOT_TextureFormat, D24S8) \
    X(DOT_TextureFormat, D32F) \
    X(DOT_TextureFormat, D32FS8) \
    /* Block compressed */ \
    X(DOT_TextureFormat, BC1) \
    X(DOT_TextureFormat, BC1_SRGB) \
    X(DOT_TextureFormat, BC3) \
    X(DOT_TextureFormat, BC3_SRGB) \
    X(DOT_TextureFormat, BC7) \
    X(DOT_TextureFormat, BC7_SRGB) \
    X(DOT_TextureFormat, ETC2_RGB8) \
    X(DOT_TextureFormat, ETC2_RGBA8)

typedef enum DOT_TextureFormatKind{
    DOT_TEXTURE_FORMAT(DOT_X_ENUM_ARG)
    // DOT_TextureFormat_Auto = DOT_TextureFormat_RGBA8_UNORM,
}DOT_TextureFormatKind;

typedef DOT_ENUM(u8, DOT_TextureFormatFlags){
    DOT_TextureFormat_Compressed    = DOT_BIT(0),
    DOT_TextureFormat_SRGB          = DOT_BIT(1),
    DOT_TextureFormat_Depth         = DOT_BIT(2),
    DOT_TextureFormat_Stencil       = DOT_BIT(3),
};

typedef struct DOT_TextureFormatInfo{
    u8 channels;
    u8 block_size; // How big channel combination is ie rgba8 = 32b

    u8 block_width;
    u8 block_height;
    DOT_TextureFormatFlags flags;
}DOT_TextureFormatInfo;



global const char *dot_texture_format_kind_str[] = {
    DOT_TEXTURE_FORMAT(DOT_X_ENUM_STR)
};

typedef enum DOT_TextureDimensionKind{
    DOT_TEXTURE_KIND(DOT_X_ENUM_ARG)
}DOT_TextureDimensionKind;

global const char *dot_texture_kind_str[] = {
    DOT_TEXTURE_KIND(DOT_X_ENUM_STR)
};

typedef struct DOT_TextureHandle{
    DOT_AssetHandle h;
}DOT_TextureHandle;

typedef struct DOT_TextureDesc{
    DOT_TextureDimensionKind dimension_kind; // DOT_TextureKind_2D
    DOT_TextureFormatKind format_kind; // DOT_TextureKind_2D
    u16 width; // = 1;
    u16 height; // = 1;
    u16 depth; // = 1;
    u8 mip_levels; // = 1;
}DOT_TextureDesc;

typedef struct DOT_TextureAsset{
    DOT_Asset asset;
    DOT_TextureHandle handle;
    DOT_TextureDesc desc;
}DOT_TextureAsset;

typedef struct DOT_TextureCreateInfo{
    DOT_AssetCreateInfo asset_info;
    void *data;
    b32 create_mips; // Enabling this will auto fill mip_levels if no mip_levels

    DOT_TextureDesc texture_desc; // This is now filled in by the texture loaded
    // u8 flags; // = 0; // TextureFlags bitmasks
    // TextureHandle alias = k_invalid_texture;
}DOT_TextureCreateInfo;

/* ------------------------------------------------------------------ */
/*  Overlay integration types (backend-agnostic)                      */
/* ------------------------------------------------------------------ */

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

// renderer_backend_funcs.x.h
#define RENDERER_BACKEND_FN_LIST \
    FN(void, init, DOT_Window *window) \
    FN(void, shutdown) \
    FN(void, begin_frame, u8 current_frame) \
    FN(void, end_frame, u8 current_frame) \
    FN(void, clear_bg, u8 current_frame, vec3 color) \
    FN(DOT_ShaderModuleHandle, load_shader_from_file_buffer, String8 fb) \
    FN(DOT_TextureHandle, create_texture, DOT_TextureCreateInfo *create_info) \
    FN(void, unload_shader_module, DOT_ShaderModuleHandle shader_module) \
    FN(void, overlay_init, const void *font_pixels, int font_w, int font_h) \
    FN(void, overlay_shutdown) \
    FN(void, overlay_render, u8 frame_idx, OverlayDrawList *draw_list)

typedef struct RendererBackend{
    RendererBackendKind backend_kind;
    Arena *permanent_arena;
    u8 frame_overlap;

#define FN(ret, name, ...) ret (*name) (__VA_ARGS__);
    RENDERER_BACKEND_FN_LIST
#undef FN
}RendererBackend;

typedef struct FrameData{
    Arena *temp_arena;
    // u8 frame_idx;
}FrameData;

typedef struct DOT_Renderer{
    Arena *permanent_arena;

    // u8         frame_overlap;
    u64        current_frame;
    FrameData *frame_data;

    ShaderCache shader_cache;
    RendererBackend *backend;
}DOT_Renderer;

#if defined(DOT_RENDER_BACKEND_ONLY_VK)
#   define RENDER_BACKEND_CALL(fn) renderer_backend_vk_##fn
#elif defined(DOT_RENDER_BACKEND_ONLY_DX12)
#   define RENDER_BACKEND_CALL(fn) renderer_backend_dx12_##fn
#else
#   define RENDER_BACKEND_CALL(fn) renderer->backend->fn
#endif

DOT_ENGINE_API void renderer_clear_background(DOT_Renderer *renderer, vec3 color);
DOT_ENGINE_API DOT_ShaderModule* renderer_load_shader_module_from_path(DOT_Renderer *renderer, String8 path);

// Use renderer arena or own arena?
DOT_ENGINE_API DOT_TextureAsset renderer_create_texture_asset(DOT_Renderer *renderer, DOT_TextureCreateInfo *create_info);
DOT_TextureHandle renderer_create_texture(DOT_Renderer *renderer, DOT_TextureCreateInfo *create_info);

internal DOT_TextureFormatInfo renderer_get_texture_format_info(DOT_TextureFormatKind fmt);

internal void renderer_begin_frame(DOT_Renderer *renderer);
internal void renderer_end_frame(DOT_Renderer *renderer);

internal RendererBackend* renderer_backend_create(Arena *arena, RendererBackendConfig *backend_config);
internal void renderer_init(Arena *arena, DOT_Renderer *renderer, DOT_Window *window, RendererConfig *config);
internal void renderer_shutdown(DOT_Renderer *renderer);

internal void renderer_overlay_init(DOT_Renderer *renderer, const void *font_pixels, int font_w, int font_h);
internal void renderer_overlay_render(DOT_Renderer *renderer, u8 frame_idx, OverlayDrawList *draw_list);
internal void renderer_overlay_shutdown(DOT_Renderer *renderer);


// internal void renderer_draw(DOT_Renderer *renderer);
#endif // !RENDERER_H
