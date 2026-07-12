#ifndef DOT_RENDER_CACHEH
#define DOT_RENDER_CACHEH
// (jd) TODO: Move shader cache to be asset cache

// #define DOT_RENDER_CACHEKINDS(X) \
//     X(RN_ResourceKind, Unknown) \
//     X(RN_ResourceKind, Texture) \
//     X(RN_ResourceKind, ShaderStage) \
//     X(RN_ResourceKind, Pipeline) \
//     X(RN_ResourceKind, Count)

// DOT_ENUM_REFLECT(RN_ResourceKind, DOT_RENDER_CACHEKINDS);

typedef struct DOT_AssetCreateInfo{
    String8 path;
    String8 name;
    String8 desc;
}DOT_AssetCreateInfo;

typedef u64 DOT_AssetHandle[1];
typedef struct DOT_TextureHandle{
    DOT_AssetHandle h;
}DOT_TextureHandle;

typedef struct DOT_ShaderModuleHandle{
    DOT_AssetHandle h;
}DOT_ShaderModuleHandle;

// TODO: Hash based on asset name and resource usage
typedef struct RN_Resource{
    RN_ResourceKind kind;
    String8 name;
    String8 path;
    String8 desc;
    u64     last_modified;
}RN_Resource;

#define HM_T RN_Handle
#include "base/templates/hashmap.h"

typedef struct DOT_AssetCache{
    HashMap_RN_Handle asset_cache[RN_ResourceKind_Count]; // This should be a general cache, and check there before loading assets
}DOT_AssetCache;

internal DOT_TextureHandle      dot_asset_cache_shader_module_load(String8 path);
internal DOT_ShaderModuleHandle dot_asset_cache_texture_load(String8 path);

internal RN_Resource dot_asset_from_create_info(Arena *arena, MultiMemoryPool *pool, const DOT_AssetCreateInfo *asset_info, RN_ResourceKind kind);

internal RN_Resource
dot_asset_from_create_info(Arena *arena, MultiMemoryPool *pool, const DOT_AssetCreateInfo *asset_info, RN_ResourceKind kind)
{
    (void)kind;
    // (jd) NOTE:: We'll probably want an allocation among this lines maybe?
    String8 name = { .str = multi_memory_pool_alloc(arena, pool, asset_info->name.size), .size = asset_info->name.size};
    String8 desc = { .str = multi_memory_pool_alloc(arena, pool, asset_info->desc.size), .size = asset_info->desc.size};
    String8 path = { .str = multi_memory_pool_alloc(arena, pool, asset_info->path.size), .size = asset_info->path.size};
    (void)name;
    (void)desc;
    (void)path;
    RN_Resource res = {
    //     .kind = kind,
    //     .name = {0},
    //     .desc = {0},
    //     .path = {0},
    };
    return res;
}

#endif // !DOT_RENDER_CACHEH
