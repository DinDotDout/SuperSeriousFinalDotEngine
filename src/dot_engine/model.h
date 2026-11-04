#ifndef DOT_MODEL_H
#define DOT_MODEL_H

typedef struct DOT_Submesh{
    f32x4x4 transform;

    RB_GpuAddress index_addr;
    RB_GpuAddress position_addr;
    RB_GpuAddress normal_addr;
    RB_GpuAddress tangent_addr;    // 0 if the source mesh had no tangents
    RB_GpuAddress texcoord_addr;
    RB_GpuAddress material_addr;

    u32 index_count;
}DOT_Submesh;

typedef struct DOT_Model{
    u32 sub_mesh_draw_count;
    DOT_Submesh *sub_mesh_draw;

    u32 texture_count;
    RB_Texture *textures;
}DOT_Model;

////////////////////////////////////////////////////////////////
//
// Baked model file

#define DOT_MODEL_FILE_MAGIC        0x4D544F44u
#define DOT_MODEL_FILE_VERSION      6
#define DOT_MODEL_FILE_ALIGN        256
#define DOT_MODEL_FILE_TABLE_ALIGN  16
#define DOT_MODEL_FILE_MAX_MIPS     16
#define DOT_MODEL_FILE_NONE         U32_MAX
#define DOT_MODEL_CACHE_PATH        "build/cache/"

typedef struct DOT_Hash128
{
    u64 lo;
    u64 hi;
}DOT_Hash128;

typedef struct DOT_ModelFileHeader
{
    u32 magic;
    u32 version;
    u32 mesh_count;
    u32 submesh_count;
    u32 material_count;
    u32 texture_count;
    u32 sampler_count;
    u32 pad;
    u64 meshes_offset;
    u64 submeshes_offset;
    u64 materials_offset;
    u64 textures_offset;
    u64 samplers_offset;
    u64 file_size;
}DOT_ModelFileHeader;

typedef struct DOT_ModelFileMesh
{
    DOT_Hash128 hash;
    u64 offset;
    u64 size;
    u64 index_offset;
    u64 position_offset;
    u64 normal_offset;
    u64 texcoord_offset;
    u64 tangent_offset;
    u32 index_count;
    u32 vertex_count;
    u32 has_tangents;
    u32 pad;
}DOT_ModelFileMesh;

typedef struct DOT_ModelFileSubmesh
{
    f32x4x4 transform;
    u32 mesh_index;
    u32 material_index;
    u32 pad[2];
}DOT_ModelFileSubmesh;

typedef struct DOT_ModelFileMaterial
{
    DOT_MaterialGpu gpu;
    u32 texture_indices[DOT_PBRTextureKind_Count];
    u32 sampler_indices[DOT_PBRTextureKind_Count];
}DOT_ModelFileMaterial;

typedef struct DOT_ModelFileTexture
{
    DOT_Hash128 hash;
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

typedef struct DOT_ModelFileMeshKey
{
    u32 kind;
    u32 index_count;
    u32 vertex_count;
    u32 has_tangents;
}DOT_ModelFileMeshKey;

typedef struct DOT_ModelFileTextureKey
{
    u32 kind;
    u32 width;
    u32 height;
    u32 format_kind;
    u32 mip_count;
}DOT_ModelFileTextureKey;

typedef struct DOT_AssetCache
{
    b32 initialized;
    HashMap(DOT_Hash128, RN_BindlessTexture) textures;
    HashMap(DOT_Hash128, RB_GpuAddress)      meshes;
}DOT_AssetCache;

internal inline void*
dot_cgltf_alloc(void *user, cgltf_size size)
{
    Arena *arena = (Arena*)user;
    void *result = PUSH_SIZE(arena, size);
    // DOT_PRINT("allocating %u onto %p", size, result);
    return result;
}

internal inline void
dot_cgltf_free(void *user, void* ptr)
{
    // DOT_PRINT("freeing memory %p", ptr);
    DOT_UNUSED(user, ptr);
}

internal DOT_Model      dot_model_from_gltf(RN_RenderCtx *renderer, const cgltf_data *gltf_data, String8 gltf_path);
internal cgltf_data *   dot_gltf_load_from_path(Arena *arena, String8 path);
internal DOT_Model      dot_model_load_from_path(RN_RenderCtx *renderer, String8 path);
internal void           dot_model_destroy(RN_RenderCtx *renderer, DOT_Model *model);

internal DOT_Hash128    dot_hash128_from_data(void const *key, u64 key_size, void const *data, u64 size);
internal String8        dot_model_baked_path(Arena *arena, String8 source_path);
internal b32            dot_model_bake(RN_RenderCtx *renderer, String8 gltf_path, String8 baked_path, b32 compress);
internal DOT_Model      dot_model_load_baked(RN_RenderCtx *renderer, DOT_AssetCache *cache, String8 baked_path);
internal DOT_Model      dot_model_load(RN_RenderCtx *renderer, String8 gltf_path);
internal void           dot_asset_cache_destroy(RN_RenderCtx *renderer, DOT_AssetCache *cache);

#endif // DOT_MODEL_H!
