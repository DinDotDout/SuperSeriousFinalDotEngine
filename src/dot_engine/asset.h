typedef DOT_ENUM(u8, DOT_AssetKind){
    DOT_Asset_Unknown,
    DOT_Asset_Texture,
    DOT_Asset_Count,
};

const String8 dot_asset_str[] = {
    String8Lit("Unknown"),
    String8Lit("Texture"),
};

DOT_STATIC_ASSERT(DOT_Asset_Count == ARRAY_COUNT(dot_asset_str));

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
