#ifndef RENDERER_H
#define RENDERER_H

enum
{
    RN_RESOURCE_CLEANUP_CTX_TEXTURES  = 64,
};

typedef struct RN_BindlessTexture
{
    RB_Texture handle;
    u32        bindless_slot;
}RN_BindlessTexture;

typedef struct RN_PSOGraphics{
    String8         fs_path;
    String8         vs_path;
    RB_RasterDesc   raster_desc;
    DOT_DebugName16(debug_name);
}RN_PSOGraphics;

typedef struct RN_ResourceCleanupCtx{
    TreeHeader node;
    ARRAY(RB_Texture, RN_RESOURCE_CLEANUP_CTX_TEXTURES)   texture_ids;
    struct{ // (jd) use this for a resource free list / pool source
        u32 count;
        Arena **data;
    }arenas;
}RN_ResourceCleanupCtx;

typedef TREE_POOL(RN_ResourceCleanupCtx) RN_ResourceCleanupTree;

// Reset by rn_frame_begin once this slot comes back around, after its last_submit completes.
typedef struct RN_FrameData{
    Arena           *temp_arena;
    RB_CpuGpuArena   frame_heap;  // per-frame data (draw roots etc.); rn_frame_push from any thread
    RB_CpuGpuArena   upload_heap; // staging for rn_upload_*
    RB_CommandPool   frame_pool;
    RB_CommandPool   upload_pool;
    RB_TimelinePoint last_submit;
}RN_FrameData;

// (jd) NOTE:Use permanent arena for all init stuff
// use transient arena as a series of pop markers based on context?
typedef struct RN_RenderCtx{
    Arena *permanent_arena;
    Arena *transient_arena;

    HashMap(String8, RN_ShaderCachedData) shader_cache; // This should be a general cache, and check there before loading assets
    RN_ResourceCleanupTree cleanup_tree;
    RB_DeleteQueue         delete_queue;

    RB_TextureHeapArena texture_heap;
    RB_Texture          null_texture; // failed texture creates fall back to it

    // Persistent geometry/material data, filled through rn_upload_memory.
    RB_GpuArena data_heap;

    RB_GpuHeap      texture_descriptor_heap;
    u32             next_texture_slot;

    RB_GpuHeap      sampler_descriptor_heap;
    u32             next_sampler_slot;
    RB_SamplerDesc *sampler_slot_descs;

    // One upload batch open at a time, guarded by upload_mutex (rn_upload_begin/rn_upload_end).
    Mutex                upload_mutex;
    RB_CommandBuffer     upload_cmd;
    RB_TimelineSemaphore upload_timeline;
    u64                  upload_value;

    u32           frame_data_count;
    RN_FrameData *frame_datas;

    RB_TimelineSemaphore frame_timeline;
    u64                  frame_value;
    b32                  current_frame_valid;
    RB_RenderView        current_view;
    i32x2                current_extent;
    RB_CommandBuffer     current_cmd;

    RB_FormatKind swapchain_format;

    // Depth target matching the swapchain, recreated in rn_frame_begin when the extent changes.
    RB_FormatKind depth_format;
    RB_Texture    depth_texture;
    RB_RenderView depth_view;
    i32x2         depth_extent;
}RN_RenderCtx;

internal void rn_init(Arena *arena, RN_RenderCtx *renderer, PlatformNativeWindow native_window);
internal void rn_shutdown(RN_RenderCtx *renderer);

internal void        rn_frame_begin(RN_RenderCtx *renderer, i32x2 framebuffer_size);
internal void        rn_frame_end(RN_RenderCtx *renderer);
internal RB_GpuRange rn_frame_push_(RN_RenderCtx *renderer, void *data, i64 size);
DOT_ENGINE_API void  rn_clear_background(RN_RenderCtx *renderer, f32x3 color);

internal RB_GpuAddress    rn_data_alloc(RN_RenderCtx *renderer, i64 size);
internal void             rn_upload_begin(RN_RenderCtx *renderer);
internal void             rn_upload_memory(RN_RenderCtx *renderer, RB_GpuAddress dst, void *src, i64 size);
internal RB_TimelinePoint rn_upload_end(RN_RenderCtx *renderer);

internal RB_Texture         rn_texture_create(RN_RenderCtx *renderer, RB_TextureDesc *desc, void *data, RB_Texture *fallback);
internal RN_BindlessTexture rn_texture_create_bindless(RN_RenderCtx *renderer, RB_TextureDesc *desc, void *data);
// This should be an external loader and renderer should fill in the rest
DOT_ENGINE_API RN_BindlessTexture rn_texture_load_from_path(RN_RenderCtx *renderer, String8 name, String8 path, u8 mip_levels);
internal void               rn_texture_destroy(RN_RenderCtx *renderer, RB_Texture handle);

internal u32 rn_sampler_bindless_slot_create(RN_RenderCtx *renderer, RB_SamplerDesc *desc);

DOT_ENGINE_API RB_Pipeline rn_pipeline_create(RN_RenderCtx *renderer, String8 vs_path, String8 fs_path, RB_RasterDesc *raster);
DOT_ENGINE_API RB_Pipeline rn_graphics_pipeline_create(RN_RenderCtx *renderer, RN_PSOGraphics *graphics_pso);
DOT_ENGINE_API void        rn_pipeline_destroy(RN_RenderCtx *renderer, RB_Pipeline handle);

#define rn_frame_push(renderer, value) rn_frame_push_((renderer), &(value), sizeof(value))

#endif // !RENDERER_H
