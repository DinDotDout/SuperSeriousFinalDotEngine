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
rb_begin(RB_DeviceKind kind, Arena *arena, OS_NativeWindow window, RB_DeviceDesc const *desc)
{
    DOT_ASSERT(window.kind < OS_WindowSystemKind_Count);
    DOT_ASSERT(kind == RB_DeviceKind_Null || os_window_system_is_native(window.kind), "Window system %S is not native to this OS", os_window_system_name(window.kind));
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
    case RB_FormatKind_R8_Unorm:
    case RB_FormatKind_R8_Uint:
        info.block_size = 1;
    break;
    case RB_FormatKind_R8_Srgb:
        info.block_size = 1;
        info.format_flags |= RB_TextureFormatBit_Srgb;
    break;
    case RB_FormatKind_S8_Uint:
        info.block_size = 1;
        info.format_flags |= RB_TextureFormatBit_Stencil;
    break;

    // 2 bytes/texel
    case RB_FormatKind_Rg8_Unorm:
    case RB_FormatKind_Rg8_Uint:
    case RB_FormatKind_Rgba4_Unorm:
    case RB_FormatKind_R5g5b5a1_Unorm:
    case RB_FormatKind_R5g6b5_Unorm:
    case RB_FormatKind_R16_Unorm:
    case RB_FormatKind_R16_Uint:
    case RB_FormatKind_R16_Float:
        info.block_size = 2;
    break;
    case RB_FormatKind_Rg8_Srgb:
        info.block_size = 2;
        info.format_flags |= RB_TextureFormatBit_Srgb;
    break;
    case RB_FormatKind_D16_Unorm:
        info.block_size = 2;
        info.format_flags |= RB_TextureFormatBit_Depth;
    break;

    // 4 bytes/texel
    case RB_FormatKind_Rgba8_Unorm:
    case RB_FormatKind_Rgba8_Uint:
    case RB_FormatKind_Bgra8_Unorm:
    case RB_FormatKind_Bgra8_Uint:
    case RB_FormatKind_Rg16_Unorm:
    case RB_FormatKind_Rg16_Uint:
    case RB_FormatKind_Rg16_Float:
    case RB_FormatKind_R32_Uint:
    case RB_FormatKind_R32_Float:
    case RB_FormatKind_Rgb10a2_Unorm:
    case RB_FormatKind_Rg11b10_Float:
        info.block_size = 4;
    break;
    case RB_FormatKind_Rgba8_Srgb:
    case RB_FormatKind_Bgra8_Srgb:
        info.block_size = 4;
        info.format_flags |= RB_TextureFormatBit_Srgb;
    break;
    case RB_FormatKind_D32_Float:
        info.block_size = 4;
        info.format_flags |= RB_TextureFormatBit_Depth;
    break;
    case RB_FormatKind_D24_Unorm_S8_Uint:
        info.block_size = 4;
        info.format_flags |= RB_TextureFormatBit_Depth | RB_TextureFormatBit_Stencil;
    break;

    // Depth + stencil, 32F + 8 stencil
    case RB_FormatKind_D32_Float_S8_Uint:
        info.block_size = 5;
        info.format_flags |= RB_TextureFormatBit_Depth | RB_TextureFormatBit_Stencil;
    break;

    // 8 bytes/texel
    case RB_FormatKind_Rgba16_Unorm:
    case RB_FormatKind_Rgba16_Uint:
    case RB_FormatKind_Rgba16_Float:
    case RB_FormatKind_Rg32_Uint:
    case RB_FormatKind_Rg32_Float:
        info.block_size = 8;
    break;

    // 12 bytes/texel
    case RB_FormatKind_Rgb32_Uint:
    case RB_FormatKind_Rgb32_Float:
        info.block_size = 12;
    break;

    // 16 bytes/texel
    case RB_FormatKind_Rgba32_Uint:
    case RB_FormatKind_Rgba32_Float:
        info.block_size = 16;
    break;

    // Compressed, 16 bytes per 4x4 block
    case RB_FormatKind_Eac_Rg:
    case RB_FormatKind_Astc_4X4_Unorm:
    case RB_FormatKind_Bc3_Unorm:
    case RB_FormatKind_Bc5_Rg:
    case RB_FormatKind_Bc6h_UFloat:
    case RB_FormatKind_Bc6h_SFloat:
    case RB_FormatKind_Bc7_Unorm:
        info.block_size = 16;
        info.format_flags |= RB_TextureFormatBit_Compressed;
    break;
    case RB_FormatKind_Astc_4X4_Srgb:
    case RB_FormatKind_Bc3_Srgb:
    case RB_FormatKind_Bc7_Srgb:
        info.block_size = 16;
        info.format_flags |= RB_TextureFormatBit_Compressed | RB_TextureFormatBit_Srgb;
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
        case 1: return RB_FormatKind_R32_Float;
        case 2: return RB_FormatKind_Rg32_Float;
        case 3: return RB_FormatKind_Rgba32_Float;
        case 4: return RB_FormatKind_Rgba32_Float;
        }
    }else if(size_bytes == 2){ // 16-bit integer formats (PNG, TGA, etc.)
        switch(comp){
        case 1: return RB_FormatKind_R16_Float;
        case 2: return RB_FormatKind_Rg16_Float;
        case 3: return RB_FormatKind_Rgba16_Float;
        case 4: return RB_FormatKind_Rgba16_Float;
        }
    }else if(size_bytes == 1){
        switch(comp){ // 8-bit formats
        case 1: return RB_FormatKind_R8_Unorm;
        case 2: return RB_FormatKind_Rg8_Unorm;
        // case 3: return srgb ? RB_FormatKind_RGB8_Srgb : RB_FormatKind_RGB8_Unorm;
        case 4: return srgb ? RB_FormatKind_Rgba8_Srgb : RB_FormatKind_Rgba8_Unorm;
        }
    }
    return RB_FormatKind_None;
}

