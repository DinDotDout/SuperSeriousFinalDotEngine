#define RB_HANDLE_PACK(T, handle) (T){ .h = cast(u64) handle }
#define RB_HANDLE_UNPACK(T, handle) ((T) handle.h)

#if DOT_ENABLED(RB_BACKEND_SINGLE)
#   if DOT_ENABLED(RB_BACKEND_VK)
#       define RB_SINGLE_DEVICE_KIND RB_DeviceKind_Vk
#       define X(name, ret, params) global ret (* const rb_##name) params = rb_vk_##name;
#   elif DOT_ENABLED(RB_BACKEND_SINGLE) && DOT_ENABLED(RB_BACKEND_DX12)
#       define RB_SINGLE_DEVICE_KIND RB_DeviceKind_Dx12
#       define X(name, ret, params) global ret (* const rb_##name) params = rb_dx12_##name;
#   else
#       define X(name, ret, params) global ret (*rb_##name) params;
#   endif

    RB_ProcedureXList
#   undef X
#endif

global RB_DebugMessageFn *rb_g_debug_message_fn = rb_debug_message_default;
global void              *rb_g_debug_message_user_data;

internal void
rb_debug_message_default(RB_DebugMessage *msg, void *user_data)
{
    DOT_UNUSED(user_data);
    if(msg->severity < RB_DebugSeverity_Warning){
        return;
    }

    Scratch s = scratch_begin(0,0);
    String8 mssg = string8_format(s.arena, "MessageID: %S %lld\nMessage: %S\n\n", msg->id_name, msg->id, msg->text);

    partial_switch_begin(msg->severity){
    case RB_DebugSeverity_Error:{
        // Validation errors are our bug, so panic. Loader/general ones are environmental: log and go on.
        if(msg->categories & RB_DebugCategoryBit_Validation){
            DOT_ERROR("%S", mssg);
        }else{
			DOT_WARNING("%S", mssg);
			os_print_stacktrace();
		}
    }break;
    case RB_DebugSeverity_Warning:{
        DOT_WARNING("%S", mssg);
    }break;
    default: break;
    }partial_switch_end;

    scratch_end(s);
}

internal void
rb_debug_message_set(RB_DebugMessageFn *fn, void *user_data)
{
    rb_g_debug_message_fn = fn ? fn : rb_debug_message_default;
    rb_g_debug_message_user_data = fn ? user_data : 0;
}

internal void
rb_debug_message_emit(RB_DebugMessage *msg)
{
    rb_g_debug_message_fn(msg, rb_g_debug_message_user_data);
}

internal void
rb_begin(RB_DeviceKind kind, Arena *arena, PlatformNativeWindow window, RB_DeviceDesc const *desc)
{
    DOT_ASSERT(window.kind < PlatformWindowSystemKind_Count);
    DOT_ASSERT(kind == RB_DeviceKind_Null || platform_window_system_is_native(window.kind), "Window system %s is not native to this platform", platfrom_window_system_str[window.kind]);
#if DOT_ENABLED(RB_BACKEND_SINGLE)
    DOT_ASSERT(kind == RB_SINGLE_DEVICE_KIND, "RB_BACKEND_SINGLE build can't switch to device kind %d", kind);
#else
    partial_switch_begin(kind){
    case RB_DeviceKind_Null:
    {
        #define X(name, ret, params) rb_##name = rb_null_##name;
        RB_ProcedureXList
        #undef X
    }break;
#if DOT_ENABLED(RB_BACKEND_VK)
    case RB_DeviceKind_Vk:
    {
        #define X(name, ret, params) rb_##name = rb_vk_##name;
        RB_ProcedureXList
        #undef X
    }break;
#endif
#if DOT_ENABLED(RB_BACKEND_DX12)
    case RB_DeviceKind_Dx12:
    {
        #define X(name, ret, params) rb_##name = rb_dx12_##name;
        RB_ProcedureXList
        #undef X
    }break;
#endif
    default: DOT_ERROR("Device kind %d is not compiled in (see RB_BACKEND_*)", kind); return;
    }partial_switch_end;
#endif
    rb_init(arena, window, desc);
}

internal RB_TextureFormatInfo
rb_texture_format_info_from_format(RB_FormatKind fmt)
{
    RB_TextureFormatInfo info = {0};
    switch(fmt){
    case RB_FormatKind_None:
        return info;

    // 1 byte/texel
    case RB_FormatKind_R8_UNORM:
    case RB_FormatKind_R8_UINT:
        info.block_size = 1;
    break;
    case RB_FormatKind_R8_SRGB:
        info.block_size = 1;
        info.format_flags |= RB_TextureFormatBit_SRGB;
    break;
    case RB_FormatKind_S8_UINT:
        info.block_size = 1;
        info.format_flags |= RB_TextureFormatBit_Stencil;
    break;

    // 2 bytes/texel
    case RB_FormatKind_RG8_UNORM:
    case RB_FormatKind_RG8_UINT:
    case RB_FormatKind_RGBA4_UNORM:
    case RB_FormatKind_R5G5B5A1_UNORM:
    case RB_FormatKind_R5G6B5_UNORM:
    case RB_FormatKind_R16_UNORM:
    case RB_FormatKind_R16_UINT:
    case RB_FormatKind_R16_FLOAT:
        info.block_size = 2;
    break;
    case RB_FormatKind_RG8_SRGB:
        info.block_size = 2;
        info.format_flags |= RB_TextureFormatBit_SRGB;
    break;
    case RB_FormatKind_D16_UNORM:
        info.block_size = 2;
        info.format_flags |= RB_TextureFormatBit_Depth;
    break;

    // 4 bytes/texel
    case RB_FormatKind_RGBA8_UNORM:
    case RB_FormatKind_RGBA8_UINT:
    case RB_FormatKind_BGRA8_UNORM:
    case RB_FormatKind_BGRA8_UINT:
    case RB_FormatKind_RG16_UNORM:
    case RB_FormatKind_RG16_UINT:
    case RB_FormatKind_RG16_FLOAT:
    case RB_FormatKind_R32_UINT:
    case RB_FormatKind_R32_FLOAT:
    case RB_FormatKind_RGB10A2_UNORM:
    case RB_FormatKind_RG11B10_FLOAT:
        info.block_size = 4;
    break;
    case RB_FormatKind_RGBA8_SRGB:
    case RB_FormatKind_BGRA8_SRGB:
        info.block_size = 4;
        info.format_flags |= RB_TextureFormatBit_SRGB;
    break;
    case RB_FormatKind_D32_FLOAT:
        info.block_size = 4;
        info.format_flags |= RB_TextureFormatBit_Depth;
    break;
    case RB_FormatKind_D24_UNORM_S8_UINT:
        info.block_size = 4;
        info.format_flags |= RB_TextureFormatBit_Depth | RB_TextureFormatBit_Stencil;
    break;

    // Depth + stencil, 32F + 8 stencil
    case RB_FormatKind_D32_FLOAT_S8_UINT:
        info.block_size = 5;
        info.format_flags |= RB_TextureFormatBit_Depth | RB_TextureFormatBit_Stencil;
    break;

    // 8 bytes/texel
    case RB_FormatKind_RGBA16_UNORM:
    case RB_FormatKind_RGBA16_UINT:
    case RB_FormatKind_RGBA16_FLOAT:
    case RB_FormatKind_RG32_UINT:
    case RB_FormatKind_RG32_FLOAT:
        info.block_size = 8;
    break;

    // 12 bytes/texel
    case RB_FormatKind_RGB32_UINT:
    case RB_FormatKind_RGB32_FLOAT:
        info.block_size = 12;
    break;

    // 16 bytes/texel
    case RB_FormatKind_RGBA32_UINT:
    case RB_FormatKind_RGBA32_FLOAT:
        info.block_size = 16;
    break;

    // Compressed, 16 bytes per 4x4 block
    case RB_FormatKind_EAC_RG:
    case RB_FormatKind_ASTC_4X4_UNORM:
    case RB_FormatKind_BC3_UNORM:
    case RB_FormatKind_BC5_RG:
    case RB_FormatKind_BC6H_UFLOAT:
    case RB_FormatKind_BC6H_SFLOAT:
    case RB_FormatKind_BC7_UNORM:
        info.block_size = 16;
        info.format_flags |= RB_TextureFormatBit_Compressed;
    break;
    case RB_FormatKind_ASTC_4X4_SRGB:
    case RB_FormatKind_BC3_SRGB:
    case RB_FormatKind_BC7_SRGB:
        info.block_size = 16;
        info.format_flags |= RB_TextureFormatBit_Compressed | RB_TextureFormatBit_SRGB;
    break;
    }
    return info;
}

// (JD) NOTE: ideally we would pass in something like RB_TextureFormatInfo in the future
internal RB_FormatKind
rb_texture_format_from_info(int comp, u8 size_bytes, b32 srgb)
{
    if(size_bytes == 4){ // 32-bit float per channel (.hdr)
        switch(comp){
        case 1: return RB_FormatKind_R32_FLOAT;
        case 2: return RB_FormatKind_RG32_FLOAT;
        case 3: return RB_FormatKind_RGBA32_FLOAT;
        case 4: return RB_FormatKind_RGBA32_FLOAT;
        }
    }else if(size_bytes == 2){ // 16-bit integer formats (PNG, TGA, etc.)
        switch(comp){
        case 1: return RB_FormatKind_R16_FLOAT;
        case 2: return RB_FormatKind_RG16_FLOAT;
        case 3: return RB_FormatKind_RGBA16_FLOAT;
        case 4: return RB_FormatKind_RGBA16_FLOAT;
        }
    }else if(size_bytes == 1){
        switch(comp){ // 8-bit formats
        case 1: return RB_FormatKind_R8_UNORM;
        case 2: return RB_FormatKind_RG8_UNORM;
        // case 3: return srgb ? RB_FormatKind_RGB8_SRGB : RB_FormatKind_RGB8_UNORM;
        case 4: return srgb ? RB_FormatKind_RGBA8_SRGB : RB_FormatKind_RGBA8_UNORM;
        }
    }
    return RB_FormatKind_None;
}

