internal RN_TextureFormatInfo
rn_texture_format_info_from_format(RN_TextureFormatKind fmt)
{
    RN_TextureFormatInfo info = {0};
    switch(fmt){
    case RN_TextureFormatKind_Invalid:
        return info;
    // 8‑bit formats
    case RN_TextureFormatKind_R8_UNORM:
    case RN_TextureFormatKind_R8_UINT:
        info.channels     = 1;
        info.block_size   = 1;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case RN_TextureFormatKind_RG8_UNORM:
        info.channels     = 2;
        info.block_size   = 2;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case RN_TextureFormatKind_RGB8_SRGB:
        info.format_flags |= RN_TextureFormatBit_SRGB;
        DOT_FALLTHROUGH;
    case RN_TextureFormatKind_RGB8_UNORM:
        info.channels     = 3;
        info.block_size   = 3;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case RN_TextureFormatKind_RGBA8_SRGB:
        info.format_flags |= RN_TextureFormatBit_SRGB;
        DOT_FALLTHROUGH;
    case RN_TextureFormatKind_RGBA8_UNORM:
        info.channels     = 4;
        info.block_size   = 4;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case RN_TextureFormatKind_BGRA8_SRGB:
        info.format_flags |= RN_TextureFormatBit_SRGB;
        DOT_FALLTHROUGH;
    case RN_TextureFormatKind_BGRA8_UNORM:
        info.channels     = 4;
        info.block_size   = 4;
        info.block_width  = 1;
        info.block_height = 1;
    break;

    // HDR capable formats
    case RN_TextureFormatKind_R16F:
        info.channels     = 1;
        info.block_size   = 2;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case RN_TextureFormatKind_RG16F:
        info.channels     = 2;
        info.block_size   = 4;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case RN_TextureFormatKind_RGBA16F:
        info.channels     = 4;
        info.block_size   = 8;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case RN_TextureFormatKind_R32F:
        info.channels     = 1;
        info.block_size   = 4;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case RN_TextureFormatKind_RG32F:
        info.channels     = 2;
        info.block_size   = 8;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case RN_TextureFormatKind_RGBA32F:
        info.channels     = 4;
        info.block_size   = 16;
        info.block_width  = 1;
        info.block_height = 1;
    break;

    // Depth / Stencil formats
    case RN_TextureFormatKind_D16:
        info.channels     = 1;
        info.block_size   = 2;
        info.block_width  = 1;
        info.block_height = 1;
        info.format_flags |= RN_TextureFormatBit_Depth;
    break;
    case RN_TextureFormatKind_D24S8:
        info.channels     = 2;
        info.block_size   = 4;
        info.block_width  = 1;
        info.block_height = 1;
        info.format_flags |= RN_TextureFormatBit_Depth | RN_TextureFormatBit_Stencil;
    break;
    case RN_TextureFormatKind_D32F:
        info.channels     = 1;
        info.block_size   = 4;
        info.block_width  = 1;
        info.block_height = 1;
        info.format_flags |= RN_TextureFormatBit_Depth;
    break;
    case RN_TextureFormatKind_D32FS8:
        info.channels     = 2;
        info.block_size   = 5; /* 32F + 8 stencil */
        info.block_width  = 1;
        info.block_height = 1;
        info.format_flags |= RN_TextureFormatBit_Depth | RN_TextureFormatBit_Stencil;
    break;
    // BC block‑compressed formats
    case RN_TextureFormatKind_BC1_SRGB:
        info.format_flags |= RN_TextureFormatBit_SRGB;
        DOT_FALLTHROUGH;
    case RN_TextureFormatKind_BC1:
        info.channels     = 4;
        info.block_size   = 8;
        info.block_width  = 4;
        info.block_height = 4;
        info.format_flags |= RN_TextureFormatBit_Compressed;
    break;
    case RN_TextureFormatKind_BC3_SRGB:
        info.format_flags |= RN_TextureFormatBit_SRGB;
        DOT_FALLTHROUGH;
    case RN_TextureFormatKind_BC3:
        info.channels     = 4;
        info.block_size   = 16;
        info.block_width  = 4;
        info.block_height = 4;
        info.format_flags |= RN_TextureFormatBit_Compressed;
    break;
    case RN_TextureFormatKind_BC7_SRGB:
        info.format_flags |= RN_TextureFormatBit_SRGB;
        DOT_FALLTHROUGH;
    case RN_TextureFormatKind_BC7:
        info.channels     = 4;
        info.block_size   = 16;
        info.block_width  = 4;
        info.block_height = 4;
        info.format_flags |= RN_TextureFormatBit_Compressed;
    break;

    // ETC2 formats
    case RN_TextureFormatKind_ETC2_RGB8:
        info.channels     = 3;
        info.block_size   = 8;
        info.block_width  = 4;
        info.block_height = 4;
        info.format_flags |= RN_TextureFormatBit_Compressed;
    break;
    case RN_TextureFormatKind_ETC2_RGBA8:
        info.channels     = 4;
        info.block_size   = 16;
        info.block_width  = 4;
        info.block_height = 4;
        info.format_flags |= RN_TextureFormatBit_Compressed;
    break;
    }
    return info;
}

// (JD) NOTE: ideally we would pass in something like RN_TextureFormatInfo in the future
internal RN_TextureFormatKind
rn_texture_format_from_info(int comp, u8 size_bytes, b32 srgb)
{
    if(size_bytes == 4){ // 32-bit float per channel (.hdr) 
        switch(comp){
        case 1: return RN_TextureFormatKind_R32F;
        case 2: return RN_TextureFormatKind_RG32F;
        case 3: return RN_TextureFormatKind_RGBA32F;
        case 4: return RN_TextureFormatKind_RGBA32F;
        }
    }else if(size_bytes == 2){ // 16-bit integer formats (PNG, TGA, etc.)
        switch(comp){
        case 1: return RN_TextureFormatKind_R16F;
        case 2: return RN_TextureFormatKind_RG16F;
        case 3: return RN_TextureFormatKind_RGBA16F;
        case 4: return RN_TextureFormatKind_RGBA16F;
        }
    }else if(size_bytes == 1){
        switch(comp){ // 8-bit formats
        case 1: return RN_TextureFormatKind_R8_UNORM;
        case 2: return RN_TextureFormatKind_RG8_UNORM;
        case 3: return srgb ? RN_TextureFormatKind_RGB8_SRGB : RN_TextureFormatKind_RGB8_UNORM;
        case 4: return srgb ? RN_TextureFormatKind_RGBA8_SRGB : RN_TextureFormatKind_RGBA8_UNORM;
        }
    }
    return RN_TextureFormatKind_Invalid;
}


internal DOT_Asset
dot_asset_from_create_info(DOT_Renderer *renderer, const DOT_AssetCreateInfo *asset_info, DOT_AssetKind kind)
{
    DOT_Asset asset = {
        .kind = kind,
        .name = string8_copy(renderer->transient_arena, asset_info->name),
        .desc = string8_copy(renderer->transient_arena, asset_info->desc),
        .path = string8_copy(renderer->transient_arena, asset_info->path),
    };
    return asset;
}
