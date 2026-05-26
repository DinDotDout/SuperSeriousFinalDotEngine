#ifndef DOT_ASSET_H
#define DOT_ASSET_H
// (jd) TODO: Move shader cache to be asset cache
//
#define DOT_ASSET_KINDS(X) \
    X(DOT_Asset, Unknown) \
    X(DOT_Asset, Texture) \
    X(DOT_Asset, Sampler) \
    X(DOT_Asset, Buffer) \
    X(DOT_Asset, ShaderModule)

DOT_ENUM_REFLECT(DOT_AssetKind, DOT_ASSET_KINDS);

typedef struct DOT_AssetCreateInfo{
    String8 path;
    String8 name;
    String8 desc;
}DOT_AssetCreateInfo;

typedef u64 DOT_AssetHandle[1];
typedef struct DOT_Asset{
    DOT_AssetKind kind;
    String8 name;
    String8 path;
    String8 desc;
    u64     last_modified;
}DOT_Asset;

typedef struct AssetCacheNode AssetCacheNode;
struct AssetCacheNode{
    DOT_Asset *asset; // Upcast to any other asset type
    AssetCacheNode  *next;
};

typedef u64 HashIdx;
typedef struct DOT_AssetCache{
    hashmap(AssetCacheNode*) shader_modules[DOT_AssetKind_Count];
    u32 shader_modules_count;
    u32 load_shader_modules;
}DOT_AssetCache;

#endif // !DOT_ASSET_H
