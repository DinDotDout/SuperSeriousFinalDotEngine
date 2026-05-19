internal DOT_TextureFormatInfo
renderer_texture_format_info_from_format(DOT_TextureFormatKind fmt)
{
    DOT_TextureFormatInfo info = {0};
    switch(fmt){
    case DOT_TextureFormat_Invalid:
        return info;
    // 8‑bit formats
    case DOT_TextureFormat_R8_UNORM:
    case DOT_TextureFormat_R8_UINT:
        info.channels     = 1;
        info.block_size   = 1;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case DOT_TextureFormat_RG8_UNORM:
        info.channels     = 2;
        info.block_size   = 2;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case DOT_TextureFormat_RGB8_SRGB:
        info.format_flags |= DOT_TextureFormatFlags_SRGB;
        DOT_FALLTHROUGH;
    case DOT_TextureFormat_RGB8_UNORM:
        info.channels     = 3;
        info.block_size   = 3;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case DOT_TextureFormat_RGBA8_SRGB:
        info.format_flags |= DOT_TextureFormatFlags_SRGB;
        DOT_FALLTHROUGH;
    case DOT_TextureFormat_RGBA8_UNORM:
        info.channels     = 4;
        info.block_size   = 4;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case DOT_TextureFormat_BGRA8_SRGB:
        info.format_flags |= DOT_TextureFormatFlags_SRGB;
        DOT_FALLTHROUGH;
    case DOT_TextureFormat_BGRA8_UNORM:
        info.channels     = 4;
        info.block_size   = 4;
        info.block_width  = 1;
        info.block_height = 1;
    break;

    // HDR capable formats
    case DOT_TextureFormat_R16F:
        info.channels     = 1;
        info.block_size   = 2;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case DOT_TextureFormat_RG16F:
        info.channels     = 2;
        info.block_size   = 4;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case DOT_TextureFormat_RGBA16F:
        info.channels     = 4;
        info.block_size   = 8;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case DOT_TextureFormat_R32F:
        info.channels     = 1;
        info.block_size   = 4;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case DOT_TextureFormat_RG32F:
        info.channels     = 2;
        info.block_size   = 8;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case DOT_TextureFormat_RGBA32F:
        info.channels     = 4;
        info.block_size   = 16;
        info.block_width  = 1;
        info.block_height = 1;
    break;

    // Depth / Stencil formats
    case DOT_TextureFormat_D16:
        info.channels     = 1;
        info.block_size   = 2;
        info.block_width  = 1;
        info.block_height = 1;
        info.format_flags |= DOT_TextureFormatFlags_Depth;
    break;
    case DOT_TextureFormat_D24S8:
        info.channels     = 2;
        info.block_size   = 4;
        info.block_width  = 1;
        info.block_height = 1;
        info.format_flags |= DOT_TextureFormatFlags_Depth | DOT_TextureFormatFlags_Stencil;
    break;
    case DOT_TextureFormat_D32F:
        info.channels     = 1;
        info.block_size   = 4;
        info.block_width  = 1;
        info.block_height = 1;
        info.format_flags |= DOT_TextureFormatFlags_Depth;
    break;
    case DOT_TextureFormat_D32FS8:
        info.channels     = 2;
        info.block_size   = 5; /* 32F + 8 stencil */
        info.block_width  = 1;
        info.block_height = 1;
        info.format_flags |= DOT_TextureFormatFlags_Depth | DOT_TextureFormatFlags_Stencil;
    break;
    // BC block‑compressed formats
    case DOT_TextureFormat_BC1_SRGB:
        info.format_flags |= DOT_TextureFormatFlags_SRGB;
        DOT_FALLTHROUGH;
    case DOT_TextureFormat_BC1:
        info.channels     = 4;
        info.block_size   = 8;
        info.block_width  = 4;
        info.block_height = 4;
        info.format_flags |= DOT_TextureFormatFlags_Compressed;
    break;
    case DOT_TextureFormat_BC3_SRGB:
        info.format_flags |= DOT_TextureFormatFlags_SRGB;
        DOT_FALLTHROUGH;
    case DOT_TextureFormat_BC3:
        info.channels     = 4;
        info.block_size   = 16;
        info.block_width  = 4;
        info.block_height = 4;
        info.format_flags |= DOT_TextureFormatFlags_Compressed;
    break;
    case DOT_TextureFormat_BC7_SRGB:
        info.format_flags |= DOT_TextureFormatFlags_SRGB;
        DOT_FALLTHROUGH;
    case DOT_TextureFormat_BC7:
        info.channels     = 4;
        info.block_size   = 16;
        info.block_width  = 4;
        info.block_height = 4;
        info.format_flags |= DOT_TextureFormatFlags_Compressed;
    break;

    // ETC2 formats
    case DOT_TextureFormat_ETC2_RGB8:
        info.channels     = 3;
        info.block_size   = 8;
        info.block_width  = 4;
        info.block_height = 4;
        info.format_flags |= DOT_TextureFormatFlags_Compressed;
    break;
    case DOT_TextureFormat_ETC2_RGBA8:
        info.channels     = 4;
        info.block_size   = 16;
        info.block_width  = 4;
        info.block_height = 4;
        info.format_flags |= DOT_TextureFormatFlags_Compressed;
    break;
    }
    return info;
}

// (JD) NOTE: ideally we would pass in something like DOT_TextureFormatInfo in the future
internal DOT_TextureFormatKind
renderer_texture_format_from_info(int comp, u8 size_bytes, b32 srgb)
{
    if(size_bytes == 4){ // 32-bit float per channel (.hdr) 
        switch(comp){
        case 1: return DOT_TextureFormat_R32F;
        case 2: return DOT_TextureFormat_RG32F;
        case 3: return DOT_TextureFormat_RGBA32F;
        case 4: return DOT_TextureFormat_RGBA32F;
        }
    }else if(size_bytes == 2){ // 16-bit integer formats (PNG, TGA, etc.)
        switch(comp){
        case 1: return DOT_TextureFormat_R16F;
        case 2: return DOT_TextureFormat_RG16F;
        case 3: return DOT_TextureFormat_RGBA16F;
        case 4: return DOT_TextureFormat_RGBA16F;
        }
    }else if(size_bytes == 1){
        switch(comp){ // 8-bit formats
        case 1: return DOT_TextureFormat_R8_UNORM;
        case 2: return DOT_TextureFormat_RG8_UNORM;
        case 3: return srgb ? DOT_TextureFormat_RGB8_SRGB : DOT_TextureFormat_RGB8_UNORM;
        case 4: return srgb ? DOT_TextureFormat_RGBA8_SRGB : DOT_TextureFormat_RGBA8_UNORM;
        }
    }
    return DOT_TextureFormat_Invalid;
}
