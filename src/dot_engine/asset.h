#ifndef DOT_ASSET_H
#define DOT_ASSET_H
// (jd) TODO: Move shader cache to be asset cache

#define DOT_ASSET_KINDS(X) \
    X(DOT_AssetKind, Unknown) \
    X(DOT_AssetKind, Texture) \
    X(DOT_AssetKind, ShaderModule) \
    X(DOT_AssetKind, Count)

DOT_ENUM_REFLECT(DOT_AssetKind, DOT_ASSET_KINDS);

typedef struct DOT_AssetCreateInfo{
    String8 path;
    String8 name;
    String8 desc;
}DOT_AssetCreateInfo;
#define DOT_ASSET_CREATE_INFO(...) \
    &(DOT_AssetCreateInfo){ \
        .path = String8Lit(""), \
        .name = String8Lit(""), \
        .desc = String8Lit(""), \
        __VA_ARGS__ \
    }

typedef u64 DOT_AssetHandle[1];
typedef struct DOT_TextureHandle{
    DOT_AssetHandle h;
}DOT_TextureHandle;

typedef struct DOT_ShaderModuleHandle{
    DOT_AssetHandle h;
}DOT_ShaderModuleHandle;

typedef struct DOT_Asset{
    DOT_AssetKind kind;
    String8 name;
    String8 path;
    String8 desc;
    u64     last_modified;
}DOT_Asset;

#define HM_T DOT_AssetHandle
// #define HM_PREFIX DOT_AssetPtr
#include "base/templates/hashmap.h"

typedef struct DOT_AssetCache{
    HashMap_DOT_AssetHandle asset_cache; // This should be a general cache, and check there before loading assets
}DOT_AssetCache;

internal DOT_TextureHandle      dot_asset_cache_shader_module_load(String8 path);
internal DOT_ShaderModuleHandle dot_asset_cache_texture_load(String8 path);

internal DOT_Asset dot_asset_from_create_info(Arena *arena, MultiMemoryPool *pool, const DOT_AssetCreateInfo *asset_info, DOT_AssetKind kind);
internal DOT_Asset
dot_asset_from_create_info(Arena *arena, MultiMemoryPool *pool, const DOT_AssetCreateInfo *asset_info, DOT_AssetKind kind)
{
    // (jd) NOTE:: We'll probably want an allocation among this lines maybe?
    String8 name = { .str = multi_memory_pool_alloc(arena, pool, asset_info->name.size), .size = asset_info->name.size};
    String8 desc = { .str = multi_memory_pool_alloc(arena, pool, asset_info->desc.size), .size = asset_info->desc.size};
    String8 path = { .str = multi_memory_pool_alloc(arena, pool, asset_info->path.size), .size = asset_info->path.size};
    (void)name;
    (void)desc;
    (void)path;
    DOT_Asset asset = {
        .kind = kind,
        .name = {0},
        .desc = {0},
        .path = {0},
    };
    return asset;
}

#endif // !DOT_ASSET_H
