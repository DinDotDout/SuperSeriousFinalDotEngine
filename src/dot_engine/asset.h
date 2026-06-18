#ifndef DOT_ASSET_H
#define DOT_ASSET_H
// (jd) TODO: Move shader cache to be asset cache
//
#define DOT_ASSET_KINDS(X) \
    X(DOT_AssetKind, Unknown) \
    X(DOT_AssetKind, Texture) \
    X(DOT_AssetKind, Sampler) \
    X(DOT_AssetKind, Buffer) \
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
typedef struct DOT_Asset{
    DOT_AssetKind kind;
    String8 name;
    String8 path;
    String8 desc;
    u64     last_modified;
}DOT_Asset;

typedef struct AssetCacheNode AssetCacheNode;
struct AssetCacheNode{
    DOT_AssetKind *asset; // Upcast to any other asset type
    AssetCacheNode  *next;
};

typedef u64 HashIdx;
typedef struct DOT_AssetCache{
    AssetCacheNode** shader_modules[DOT_AssetKind_Count];
    u32 shader_modules_count;
    u32 load_shader_modules;
}DOT_AssetCache;

#endif // !DOT_ASSET_H
