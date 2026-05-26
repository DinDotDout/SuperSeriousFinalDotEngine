#ifndef RENDER_TYPES_H
#define RENDER_TYPES_H

////////////////////////////////////////////////////////////////
/// DOT_TEXTURE

#define DOT_TEXTURE_DIMENSION_KINDS(X) \
    X(DOT_TextureDimension, 1D) \
    X(DOT_TextureDimension, 2D) \
    X(DOT_TextureDimension, 3D) \
    X(DOT_TextureDimension, Array1D) \
    X(DOT_TextureDimension, Array2D) \
    X(DOT_TextureDimension, Array3D) \

DOT_ENUM_REFLECT(DOT_TextureDimensionKind, DOT_TEXTURE_DIMENSION_KINDS)

#define DOT_TEXTURE_FORMAT_KINDS(X) \
    X(DOT_TextureFormat, Invalid) \
    /* 8 BIT*/\
    X(DOT_TextureFormat, R8_UNORM) \
    X(DOT_TextureFormat, R8_UINT) \
    X(DOT_TextureFormat, RG8_UNORM) \
    X(DOT_TextureFormat, RGB8_UNORM) \
    X(DOT_TextureFormat, RGB8_SRGB) \
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

DOT_ENUM_REFLECT(DOT_TextureFormatKind, DOT_TEXTURE_FORMAT_KINDS)

// ----- Texture format flags -----
typedef u8 DOT_TextureFormatFlags;
enum{
    DOT_TextureFormatFlags_None         = DOT_BIT(0),
    DOT_TextureFormatFlags_Compressed   = DOT_BIT(1),
    DOT_TextureFormatFlags_SRGB         = DOT_BIT(2),
    DOT_TextureFormatFlags_Depth        = DOT_BIT(3),
    DOT_TextureFormatFlags_Stencil      = DOT_BIT(4),
};

// ----- Texture usage flags -----
typedef u8 DOT_TextureUsageFlags;
enum{
    DOT_TextureUsageFlags_Default       = DOT_BIT(0),
    DOT_TextureUsageFlags_RenderTarget  = DOT_BIT(1),
    DOT_TextureUsageFlags_Compute       = DOT_BIT(2),
};

typedef struct DOT_TextureFormatInfo{
    u8 channels;
    u8 bit_depth;
    u8 block_size;

    u8 block_width;
    u8 block_height;
    DOT_TextureFormatFlags format_flags;
}DOT_TextureFormatInfo;

typedef struct DOT_TextureHandle{
    DOT_AssetHandle handle;
}DOT_TextureHandle;

global DOT_TextureHandle null_texture = {0};
typedef struct DOT_TextureDesc{
    DOT_TextureDimensionKind dimension_kind;
    DOT_TextureFormatKind format_kind;
    DOT_TextureUsageFlags texture_usage_flags;
    u16 width; // = 1;
    u16 height; // = 1;
    u16 depth; // = 1;
    u8 mip_levels; // = 1; // 0 will auto generate
}DOT_TextureDesc;

typedef struct DOT_TextureCreateInfo{
    void *data;
    String8 debug_name;
    DOT_TextureDesc texture_desc; // This filled in by the texture loaded
}DOT_TextureCreateInfo;

typedef struct DOT_TextureAsset{
    DOT_Asset asset;
    DOT_TextureHandle handle;
    DOT_TextureDesc desc;
}DOT_TextureAsset;

////////////////////////////////////////////////////////////////
/// DOT_BUFFER

// ----- Buffer usage flags -----
typedef u8 DOT_BufferUsageFlags;
enum{
    DOT_BufferUsageFlags_Default    = DOT_BIT(0),
    DOT_BufferUsageFlags_Staging    = DOT_BIT(1),
    DOT_BufferUsageFlags_GPU        = DOT_BIT(2),
};

typedef struct DOT_BufferHandle{
    DOT_AssetHandle handle;
}DOT_BufferHandle;

typedef struct DOT_BufferDesc{
    u64 size;
    DOT_BufferUsageFlags buffer_usage_flags;
}DOT_BufferDesc;

typedef struct DOT_BufferCreateInfo{
    DOT_AssetCreateInfo asset_info;
    void *data; // must fill in desc manually
                //
    // b32 create_mips; // Enabling this will auto fill mip_levels if no mip_levels

    DOT_BufferDesc buffer_desc; // This is now filled in by the texture loaded
    // u8 flags; // = 0; // TextureFlags bitmasks
    // TextureHandle alias = k_invalid_texture;
}DOT_BufferCreateInfo;

typedef struct DOT_BufferAsset{
    DOT_Asset asset;
    DOT_BufferHandle handle;
    DOT_BufferDesc desc;
}DOT_BufferAsset;

////////////////////////////////////////////////////////////////
/// DOT_SAMPLER

#define DOT_SAMPLER_FILTER_KINDS(X) \
    X(DOT_SamplerFilter, Nearest) \
    X(DOT_SamplerFilter, Linear) \
    X(DOT_SamplerFilter, Cubic)

DOT_ENUM_REFLECT(DOT_SamplerFilterKind, DOT_SAMPLER_FILTER_KINDS);

#define DOT_SAMPLER_MIP_MAP_MODE_KINDS(X) \
    X(DOT_MipMapMode, Nearest) \
    X(DOT_MipMapMode, Linear) \

DOT_ENUM_REFLECT(DOT_SamplerMipmapFilterKind, DOT_SAMPLER_MIP_MAP_MODE_KINDS);

#define DOT_SAMPLER_ADDRESS_MODE_KINDS(X) \
    X(DOT_SamplerAdressMode, Repeat) \
    X(DOT_SamplerAdressMode, Mirrored_repeat) \
    X(DOT_SamplerAdressMode, ClampToEdge) \
    X(DOT_SamplerAdressMode, ClampToBorder) \
    X(DOT_SamplerAdressMode, MirrorClampToEdge)
DOT_ENUM_REFLECT(DOT_SamplerAddressModeKind, DOT_SAMPLER_ADDRESS_MODE_KINDS);

typedef struct DOT_SamplerHandle{
    DOT_AssetHandle handle;
}DOT_SamplerHandle;

typedef struct DOT_SamplerDesc{
    DOT_SamplerFilterKind       min_filter;
    DOT_SamplerFilterKind       mag_filter;
    DOT_SamplerMipmapFilterKind mipmap_filter;

    DOT_SamplerAddressModeKind  address_mode_u;
    DOT_SamplerAddressModeKind  address_mode_v;
    DOT_SamplerAddressModeKind  address_mode_w;
}DOT_SamplerDesc;

typedef struct DOT_SamplerAsset{
    DOT_Asset asset;
    DOT_SamplerHandle handle;
    DOT_SamplerDesc desc;
}DOT_SamplerAsset;

internal DOT_TextureFormatInfo renderer_texture_format_info_from_format(DOT_TextureFormatKind fmt);
internal DOT_TextureFormatKind renderer_texture_format_from_info(int comp, u8 size_bytes, b32 srgb);
#endif // !RENDER_TYPES_H
