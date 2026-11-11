#ifndef DOT_MODEL_H
#define DOT_MODEL_H

////////////////////////////////////////////////////////////////
/// DOT_MODEL_FILE_CONSTANTS

#define DOT_MODEL_FILE_MAGIC        0xD07D07D07ull
#define DOT_MODEL_FILE_NONE         U32_MAX
#define DOT_MODEL_CACHE_PATH        "build/cache/"

enum
{
    DOT_MODEL_FILE_VERSION     = 2,
    DOT_MODEL_FILE_ALIGN       = 256,
    DOT_MODEL_FILE_TABLE_ALIGN = 16,
    DOT_MODEL_FILE_MAX_MIPS    = 16,
};

////////////////////////////////////////////////////////////////
/// DOT_MODEL

typedef struct DOT_Submesh
{
    f32x4x4 transform;

    RB_GpuAddress index_addr;
    RB_GpuAddress position_addr;
    RB_GpuAddress normal_addr;
    RB_GpuAddress tangent_addr;    // 0 if the source mesh had no tangents
    RB_GpuAddress texcoord_addr;
    RB_GpuAddress material_addr;

    u32 index_count;
}DOT_Submesh;

typedef struct DOT_Model
{
    u32 sub_mesh_draw_count;
    DOT_Submesh *sub_mesh_draw;

    u32 texture_count;
    RB_Texture *textures;
}DOT_Model;

////////////////////////////////////////////////////////////////
/// DOT_MODEL_FILE_FORMAT
// .dotm layout: header, then one table per kind (each DOT_MODEL_FILE_TABLE_ALIGN aligned), then
// the deduplicated mesh and texture data (each DOT_MODEL_FILE_ALIGN aligned) the tables point into

typedef struct DOT_ModelFileHeader
{
    u64 magic;
    u32 version;

    u32 mesh_count;
    u32 submesh_count;
    u32 material_count;
    u32 texture_count;
    u32 sampler_count;

    u64 meshes_offset;
    u64 submeshes_offset;
    u64 materials_offset;
    u64 textures_offset;
    u64 samplers_offset;

    u64 file_size;
}DOT_ModelFileHeader;

// The mesh's index and vertex data sit at offset in the file and the *_offset fields are relative
// to it. Indices come first, so tangent_offset is 0 when the mesh has no tangents
typedef struct DOT_ModelFileMesh
{
    Hash128 hash;
    u64 offset;
    u64 size;
    u64 index_offset;
    u64 position_offset;
    u64 normal_offset;
    u64 texcoord_offset;
    u64 tangent_offset;
    u32 index_count;
    u32 vertex_count;
}DOT_ModelFileMesh;

typedef struct DOT_ModelFileSubmesh
{
    f32x4x4 transform;
    u32 mesh_index;
    u32 material_index;
}DOT_ModelFileSubmesh;

typedef struct DOT_ModelFileMaterial
{
    DOT_MaterialGpu gpu;
    u32 texture_indices[DOT_PBRTextureKind_Count];
    u32 sampler_indices[DOT_PBRTextureKind_Count];
}DOT_ModelFileMaterial;

// Every mip of the texture sits at offset in the file and mip_offsets are relative to it
typedef struct DOT_ModelFileTexture
{
    Hash128 hash;
    u64 offset;
    u64 size;
    u32 width;
    u32 height;
    u32 format_kind;
    u32 mip_count;
    u64 mip_offsets[DOT_MODEL_FILE_MAX_MIPS];
    u64 mip_sizes[DOT_MODEL_FILE_MAX_MIPS];
}DOT_ModelFileTexture;

typedef struct DOT_ModelFileSampler
{
    u32 min_filter;
    u32 mag_filter;
    u32 mipmap_filter;
    u32 address_mode_u;
    u32 address_mode_v;
}DOT_ModelFileSampler;

////////////////////////////////////////////////////////////////
/// DOT_ASSET_CACHE
// GPU resources shared by every loaded model, keyed by the content hash of their mesh or texture data

typedef struct DOT_AssetCache
{
    bool          initialized;
    Arena        *arena;
    RN_RenderCtx *renderer;
    HashMap(Hash128, RN_BindlessTexture) textures;
    HashMap(Hash128, RB_GpuAddress)      meshes;
}DOT_AssetCache;

////////////////////////////////////////////////////////////////
/// DOT_GLTF_FUNCTIONS

internal inline void*
dot_cgltf_alloc(void *user, cgltf_size size)
{
    Arena *arena = (Arena*)user;
    void *result = PUSH_SIZE(arena, size);
    DOT_PRINT("allocating %u onto %p", size, result);
    return result;
}

internal inline void
dot_cgltf_free(void *user, void* ptr)
{
    DOT_PRINT("freeing memory %p", ptr);
    DOT_UNUSED(user, ptr);
}

internal cgltf_data    *dot_gltf_load_from_path(Arena *arena, String8 path);
internal DOT_Model      dot_model_from_gltf(DOT_AssetCache *cache, const cgltf_data *gltf_data, String8 gltf_path);
internal DOT_Model      dot_model_load_from_path(DOT_AssetCache *cache, String8 path);

////////////////////////////////////////////////////////////////
/// DOT_MODEL_FUNCTIONS

internal DOT_Model      dot_model_load(DOT_AssetCache *cache, String8 gltf_path);
internal void           dot_model_destroy(DOT_AssetCache *cache, DOT_Model *model);
internal String8        dot_model_baked_path(Arena *arena, String8 source_path);
internal DOT_Model      dot_model_bake(DOT_AssetCache *cache, String8 gltf_path, String8 baked_path, bool compress);
internal DOT_Model      dot_model_load_baked(DOT_AssetCache *cache, String8 baked_path);

////////////////////////////////////////////////////////////////
/// DOT_ASSET_CACHE_FUNCTIONS

internal void           dot_asset_cache_init(Arena *arena, RN_RenderCtx *renderer, DOT_AssetCache *cache);
internal void           dot_asset_cache_end(DOT_AssetCache *cache);
internal void           dot_asset_cache_add_texture(DOT_AssetCache *cache, DOT_ModelFileTexture *entry, RB_CpuGpuSpan staging);
internal void           dot_asset_cache_add_mesh(DOT_AssetCache *cache, DOT_ModelFileMesh *entry, RB_CpuGpuSpan staging);

#endif // DOT_MODEL_H!
