#ifndef VK_HELPER_H
#define VK_HELPER_H
#include "renderer/renderer.h"
#include <vulkan/vk_enum_string_helper.h>

typedef struct VkHelper_SwapchainDetails{
    // IN
    DOT_Extent2D frame_buffer_size;
    VkFormat preferred_format;
    VkPresentModeKHR preferred_present_mode;

    // OUT
    VkSurfaceFormatKHR            best_surface_format;
    VkPresentModeKHR              best_present_mode;
    VkExtent2D                    surface_extent;
    VkSurfaceTransformFlagBitsKHR current_transform;
    u32                           image_count;
}VkHelper_SwapchainDetails;

typedef struct VkHelper_CandidateDeviceInfo{
    VkPhysicalDevice gpu;
    u16 graphics_family;
    u16 present_family;
    i32 score;
    b32 shared_present_graphics_queues;
    b32 is_integrated_gpu;
}VkHelper_CandidateDeviceInfo;

#ifdef NDEBUG
#define RN_VK_CHECK(x) x
#else
#define RN_VK_CHECK(x) \
    do { \
        VkResult err = x; \
        if(err < 0){ \
            printf("Detected Vulkan error: %s\n", string_VkResult(err)); \
            abort(); \
        } \
    } while (0)
#endif

#define VkSurfaceFormat2KHRParams(...) \
    (VkSurfaceFormat2KHR){ \
        .sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR, \
        __VA_ARGS__ \
    }

#define VkSurfaceCapabilities2KHRParams(...) \
    (VkSurfaceCapabilities2KHR){ \
        .sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR, \
        __VA_ARGS__ \
    }

#define VkPhysicalDeviceSurfaceInfo2KHRParams(...) \
    (VkPhysicalDeviceSurfaceInfo2KHR){ \
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR, \
        __VA_ARGS__ \
    }

///////////////////////////////////////////
/// Vk Support helpers
///

typedef struct RN_VK_VulkanConfig RN_VK_VulkanConfig;
internal b32 vk_helper_all_layers(const RN_VK_VulkanConfig *vk_config);
internal VkHelper_CandidateDeviceInfo vk_helper_pick_best_device(
    const RN_VK_VulkanConfig *vk_config,
    VkInstance instance,
    VkSurfaceKHR surface);

internal b32 vk_helper_physical_device_swapchain_support(
    VkPhysicalDevice gpu,
    VkSurfaceKHR surface,
    VkHelper_SwapchainDetails *details);

internal b32              vk_helper_physical_device_all_required_extensions(const RN_VK_VulkanConfig *vk_config, VkPhysicalDevice device);
internal b32              vk_helper_instance_all_required_extensions(const RN_VK_VulkanConfig *vk_config);

///////////////////////////////////////////
///DOT to VK

internal VkFilter               rn_vk_filter_from_rn_sampler_filter(RN_SamplerFilterKind sampler_filter);
internal VkBlendOp              rn_vk_blendop_from_rn_blend_op_kind(RN_BlendOpKind op);
internal VkSamplerMipmapMode    rn_vk_sampler_mipmap_mode_from_rn_sampler_mipmap_mode(RN_SamplerMipmapFilterKind sample_mipmap_mode);
internal VkSamplerAddressMode   rn_vk_sampler_address_mode_from_rn_sampler_address_mode(RN_SamplerAddressModeKind address_mode);
internal VkBufferUsageFlags     rn_vk_buffer_usage_flags_from_dt_buffer_usage_flags(RN_BufferUsageFlags usage_flags);

internal VkImageType            rn_vk_image_type_from_texture_dimension(RN_TextureDimensionKind texture_dimension);
internal VkImageViewType        rn_vk_image_view_type_from_texture_dimension(RN_TextureDimensionKind texture_dimension);
internal VkFormat               rn_vk_format_from_texture_format(RN_TextureFormatKind present_mode);
internal VkPresentModeKHR       rn_vk_present_mode_from_present_mode(RN_PresentModeKind present_mode);

internal RN_TextureFormatKind   rn_texture_format_from_vk_format(VkFormat texture_format);

///////////////////////////////////////////
/// Vk misc helpers
///

internal VkExtent3D vk_helper_extent3d_from_extent2d(VkExtent2D extent2d);
internal VkExtent2D vk_helper_extent2d_from_extent3d(VkExtent3D extent3d);
internal void vk_helper_transition_image(
    VkCommandBuffer cmd,
    VkImage image,
    VkImageLayout current_layout,
    VkImageLayout new_layout);

///////////////////////////////////////////
/// Vk CreateInfo helpers
///

// NOTE: Maybe we could use a bunch of default arg init macros for this
internal VkImageCreateInfo          vk_image_create_info(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);
internal VkSubmitInfo2              vk_submit_info(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signal_semaphore_info, VkSemaphoreSubmitInfo* wait_semaphore_info);
internal VkImageViewCreateInfo      vk_imageview_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);
internal VkCommandBufferSubmitInfo  vk_command_buffer_submit_info(VkCommandBuffer cmd);
internal VkSemaphoreSubmitInfo      vk_semaphore_submit_info(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);
internal VkImageSubresourceRange    vk_image_subresource_range(VkImageAspectFlags aspect_mask);
internal VkMemoryRequirements2      vk_buffer_memory_requirements(VkDevice vk_device, VkBuffer vk_buffer);

#endif // !VK_HELPER_H

#ifdef VK_HELPER_IMPLEMENTATION
///////////////////////////////////////////
/// Vk Support helpers
///

internal b32
vk_helper_all_layers(const RN_VK_VulkanConfig *vk_config)
{
    TempArena temp = threadctx_temp_begin(0);
    u32 available_layer_count = 0;
    vkEnumerateInstanceLayerProperties(&available_layer_count, NULL);
    array(VkLayerProperties) available_layers = PUSH_ARRAY(temp.arena, VkLayerProperties, available_layer_count);
    vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers);
    b32 all_found = true;
    for(u64 i = 0; i < vk_config->validation_layers.count; ++i){
        b32 found = false;
        const String8 layer_name = vk_config->validation_layers.data[i];
        for(u64 j = 0; j < available_layer_count; ++j){
            if(string8_equal(layer_name, string8_from_cstring(available_layers[j].layerName))){
                DOT_PRINT("Found Layer %s", layer_name);
                found = true;
                break;
            }
        }
        if(!found){
            all_found = false;
            DOT_WARNING("Requested layer %s not found", layer_name);
        }
    }
    threadctx_temp_end(temp);
    return all_found;
}

internal b32
vk_helper_instance_all_required_extensions(const RN_VK_VulkanConfig* vk_config)
{
    TempArena temp = threadctx_temp_begin(0);
    u32 extension_count;
    vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);
    array(VkExtensionProperties) available_extensions = PUSH_ARRAY(temp.arena, VkExtensionProperties, extension_count);
    vkEnumerateInstanceExtensionProperties(NULL, &extension_count, available_extensions);

    b32 all_found = true;
    for(u64 i = 0; i < vk_config->instance.extensions.count; ++i){
        b32 found = false;
        const String8 instance_extension_name = vk_config->instance.extensions.data[i];
        for(u64 j = 0; j < extension_count; ++j){
            char* name = available_extensions[j].extensionName;
            if(string8_equal(instance_extension_name, string8_from_cstring(name))){
                DOT_PRINT("Found instance extension \"%s\"", instance_extension_name);
                found = true;
                break;
            }
        }
        if(!found){ // Might aswell list all missing dev extensions
            all_found = false;
            DOT_WARNING("Requested instance extension \"%s\" not found", instance_extension_name);
        }
    }
    threadctx_temp_end(temp);
    return all_found;
}

internal VkBlendOp
rn_vk_blendop_from_rn_blend_op_kind(RN_BlendOpKind blend_op)
{
    switch(blend_op){
        default: DOT_ERROR("undefined op %u", blend_op);
        case RN_BlendOpKind_Add:             return VK_BLEND_OP_ADD;
        case RN_BlendOpKind_Subtract:        return VK_BLEND_OP_SUBTRACT;
        case RN_BlendOpKind_ReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
        case RN_BlendOpKind_Min:             return VK_BLEND_OP_MIN;
        case RN_BlendOpKind_Max:             return VK_BLEND_OP_MAX;
    }
}

internal VkBlendFactor
rn_vk_blend_factor_from_rn_blend_factor_kind(RN_BlendFactorKind blend_factor)
{
    switch(blend_factor){
    default: DOT_ERROR("undefined blend factor %u", blend_factor);
    case RN_BlendFactorKind_Zero:                  return VK_BLEND_FACTOR_ZERO;
    case RN_BlendFactorKind_One:                   return VK_BLEND_FACTOR_ONE;
    case RN_BlendFactorKind_SrcColor:              return VK_BLEND_FACTOR_SRC_COLOR;
    case RN_BlendFactorKind_OneMinusSrcColor:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case RN_BlendFactorKind_DstColor:              return VK_BLEND_FACTOR_DST_COLOR;
    case RN_BlendFactorKind_OneMinusDstColor:      return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case RN_BlendFactorKind_SrcAlpha:              return VK_BLEND_FACTOR_SRC_ALPHA;
    case RN_BlendFactorKind_OneMinusSrcAlpha:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case RN_BlendFactorKind_DstAlpha:              return VK_BLEND_FACTOR_DST_ALPHA;
    case RN_BlendFactorKind_OneMinusDstAlpha:      return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case RN_BlendFactorKind_ConstantColor:         return VK_BLEND_FACTOR_CONSTANT_COLOR;
    case RN_BlendFactorKind_OneMinusConstantColor: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    case RN_BlendFactorKind_ConstantAlpha:         return VK_BLEND_FACTOR_CONSTANT_ALPHA;
    case RN_BlendFactorKind_OneMinusConstantAlpha: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
    case RN_BlendFactorKind_SrcAlphaSaturate:      return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    case RN_BlendFactorKind_Src1Color:             return VK_BLEND_FACTOR_SRC1_COLOR;
    case RN_BlendFactorKind_OneMinusSrc1Color:     return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
    case RN_BlendFactorKind_Src1Alpha:             return VK_BLEND_FACTOR_SRC1_ALPHA;
    case RN_BlendFactorKind_OneMinusSrc1Alpha:     return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;

    }
}

internal VkFilter
rn_vk_filter_from_rn_sampler_filter(RN_SamplerFilterKind sampler_filter)
{
    switch(sampler_filter){
    default: DOT_ERROR("undefined sampler filter %u", sampler_filter);
    case RN_SamplerFilterKind_Nearest: return VK_FILTER_NEAREST;
    case RN_SamplerFilterKind_Linear:  return VK_FILTER_LINEAR;
    case RN_SamplerFilterKind_Cubic:   return VK_FILTER_CUBIC_IMG;
    }
}

internal VkSamplerMipmapMode
rn_vk_sampler_mipmap_mode_from_rn_sampler_mipmap_mode(RN_SamplerMipmapFilterKind sample_mipmap_mode)
{
    switch(sample_mipmap_mode){
    default: DOT_ERROR("undefined mip map mode %u", sample_mipmap_mode);
    case RN_SamplerFilterKind_Nearest: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    case RN_SamplerFilterKind_Linear:  return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

internal VkSamplerAddressMode
rn_vk_sampler_address_mode_from_rn_sampler_address_mode(RN_SamplerAddressModeKind address_mode)
{
    switch(address_mode){
    default: DOT_ERROR("undefined address mode %u", address_mode);
    case RN_SamplerAddressModeKind_Repeat:             return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case RN_SamplerAddressModeKind_Mirrored_repeat:    return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case RN_SamplerAddressModeKind_ClampToEdge:        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case RN_SamplerAddressModeKind_ClampToBorder:      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    case RN_SamplerAddressModeKind_MirrorClampToEdge:  return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    }
}

internal VkPresentModeKHR
rn_vk_present_mode_from_present_mode(RN_PresentModeKind present_mode)
{
    switch(present_mode){
    case RN_PresentModeKind_Immediate:    return VK_PRESENT_MODE_IMMEDIATE_KHR;
    case RN_PresentModeKind_Mailbox:      return VK_PRESENT_MODE_MAILBOX_KHR;
    case RN_PresentModeKind_Fifo:         return VK_PRESENT_MODE_FIFO_KHR;
    case RN_PresentModeKind_FifoRelaxed: return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    // case RN_PresentModeKind_FIFO_Count:
    default: DOT_WARNING("Unsuported requested present mode %s, defaulting to "
                  "VK_PRESENT_MODE_IMMEDIATE_KHR", string8_from_RN_PresentModeKind[present_mode]);
        return VK_PRESENT_MODE_IMMEDIATE_KHR;
    }
}

internal VkBufferUsageFlags
rn_vk_buffer_usage_flags_from_dt_buffer_usage_flags(RN_BufferUsageFlags flags)
{
    VkBufferUsageFlags vk_usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vk_usage |= DOT_BITS_ANY(flags, RN_BufferUsageBit_Vertex)   ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT : 0;
    vk_usage |= DOT_BITS_ANY(flags, RN_BufferUsageBit_Index)    ? VK_BUFFER_USAGE_INDEX_BUFFER_BIT : 0;
    vk_usage |= DOT_BITS_ANY(flags, RN_BufferUsageBit_Uniform)  ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : 0;
    vk_usage |= DOT_BITS_ANY(flags, RN_BufferUsageBit_Storage)  ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : 0;
    vk_usage |= DOT_BITS_ANY(flags, RN_BufferUsageBit_Indirect) ? VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT: 0;
    vk_usage |= DOT_BITS_ANY(flags, RN_BufferUsageBit_DeviceAddress) ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0;
    return vk_usage;
}

internal VkImageType
rn_vk_image_type_from_texture_dimension(RN_TextureDimensionKind texture_dimension)
{
    switch(texture_dimension){
    case RN_TextureDimensionKind_1D: return VK_IMAGE_TYPE_1D;
    case RN_TextureDimensionKind_2D: return VK_IMAGE_TYPE_2D;
    case RN_TextureDimensionKind_3D: return VK_IMAGE_TYPE_3D;
    case RN_TextureDimensionKind_Array1D: return VK_IMAGE_TYPE_1D;
    case RN_TextureDimensionKind_Array2D: return VK_IMAGE_TYPE_2D;
    case RN_TextureDimensionKind_Array3D: return VK_IMAGE_TYPE_3D;
    }
}

internal VkImageViewType
rn_vk_image_view_type_from_texture_dimension(RN_TextureDimensionKind texture_dimension)
{
    switch(texture_dimension){
    case RN_TextureDimensionKind_1D: return VK_IMAGE_VIEW_TYPE_1D;
    case RN_TextureDimensionKind_2D: return VK_IMAGE_VIEW_TYPE_2D;
    case RN_TextureDimensionKind_3D: return VK_IMAGE_VIEW_TYPE_3D;
    case RN_TextureDimensionKind_Array1D: return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
    case RN_TextureDimensionKind_Array2D: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    case RN_TextureDimensionKind_Array3D: return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    }
}

internal VkFormat
rn_vk_format_from_texture_format(RN_TextureFormatKind texture_format)
{
    switch(texture_format){
    case RN_TextureFormatKind_Invalid: return VK_FORMAT_UNDEFINED;
    // 8‑bit formats
    case RN_TextureFormatKind_R8_UNORM: return VK_FORMAT_R8_UNORM;
    case RN_TextureFormatKind_R8_UINT: return VK_FORMAT_R8_UINT;
    case RN_TextureFormatKind_RG8_UNORM: return VK_FORMAT_R8G8_UNORM;
    case RN_TextureFormatKind_RGB8_UNORM: return VK_FORMAT_R8G8B8_UNORM;
    case RN_TextureFormatKind_RGB8_SRGB: return VK_FORMAT_R8G8B8_SRGB;
    case RN_TextureFormatKind_RGBA8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
    case RN_TextureFormatKind_RGBA8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
    case RN_TextureFormatKind_BGRA8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;
    case RN_TextureFormatKind_BGRA8_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;
    // HDR / Float formats
    case RN_TextureFormatKind_R16F: return VK_FORMAT_R16_SFLOAT;
    case RN_TextureFormatKind_RG16F: return VK_FORMAT_R16G16_SFLOAT;
    case RN_TextureFormatKind_RGBA16F: return VK_FORMAT_R16G16B16A16_SFLOAT;
    case RN_TextureFormatKind_R32F: return VK_FORMAT_R32_SFLOAT;
    case RN_TextureFormatKind_RG32F: return VK_FORMAT_R32G32_SFLOAT;
    case RN_TextureFormatKind_RGBA32F: return VK_FORMAT_R32G32B32A32_SFLOAT;
    // Depth / Stencil formats
    case RN_TextureFormatKind_D16: return VK_FORMAT_D16_UNORM;
    case RN_TextureFormatKind_D24S8: return VK_FORMAT_D24_UNORM_S8_UINT;
    case RN_TextureFormatKind_D32F: return VK_FORMAT_D32_SFLOAT;
    case RN_TextureFormatKind_D32FS8: return VK_FORMAT_D32_SFLOAT_S8_UINT;
    // Block‑compressed formats
    case RN_TextureFormatKind_BC1: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
    case RN_TextureFormatKind_BC1_SRGB: return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
    case RN_TextureFormatKind_BC3: return VK_FORMAT_BC3_UNORM_BLOCK;
    case RN_TextureFormatKind_BC3_SRGB: return VK_FORMAT_BC3_SRGB_BLOCK;
    case RN_TextureFormatKind_BC7: return VK_FORMAT_BC7_UNORM_BLOCK;
    case RN_TextureFormatKind_BC7_SRGB: return VK_FORMAT_BC7_SRGB_BLOCK;
    // ETC2 formats
    case RN_TextureFormatKind_ETC2_RGB8: return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
    case RN_TextureFormatKind_ETC2_RGBA8: return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
    }
}

internal RN_TextureFormatKind
rn_texture_format_from_vk_format(VkFormat texture_format)
{
    DOT_ALLOW_PARTIAL_SWITCH
    switch(texture_format){
    default: DOT_ERROR("Format not implemented");
    case VK_FORMAT_UNDEFINED: return RN_TextureFormatKind_Invalid;
    // 8‑bit formats
    case VK_FORMAT_R8_UNORM: return RN_TextureFormatKind_R8_UNORM;
    case VK_FORMAT_R8_UINT: return RN_TextureFormatKind_R8_UINT;
    case VK_FORMAT_R8G8_UNORM: return RN_TextureFormatKind_RG8_UNORM;
    case VK_FORMAT_R8G8B8_UNORM: return RN_TextureFormatKind_RGB8_UNORM;
    case VK_FORMAT_R8G8B8_SRGB: return RN_TextureFormatKind_RGB8_SRGB;
    case VK_FORMAT_R8G8B8A8_UNORM: return RN_TextureFormatKind_RGBA8_UNORM;
    case VK_FORMAT_R8G8B8A8_SRGB: return RN_TextureFormatKind_RGBA8_SRGB;
    case VK_FORMAT_B8G8R8A8_UNORM: return RN_TextureFormatKind_BGRA8_UNORM;
    case VK_FORMAT_B8G8R8A8_SRGB: return RN_TextureFormatKind_BGRA8_SRGB;
    // HDR / Float formats
    case VK_FORMAT_R16_SFLOAT: return RN_TextureFormatKind_R16F;
    case VK_FORMAT_R16G16_SFLOAT: return RN_TextureFormatKind_RG16F;
    case VK_FORMAT_R16G16B16A16_SFLOAT: return RN_TextureFormatKind_RGBA16F;
    case VK_FORMAT_R32_SFLOAT: return RN_TextureFormatKind_R32F;
    case VK_FORMAT_R32G32_SFLOAT: return RN_TextureFormatKind_RG32F;
    case VK_FORMAT_R32G32B32A32_SFLOAT: return RN_TextureFormatKind_RGBA32F;
    // Depth / Stencil formats
    case VK_FORMAT_D16_UNORM: return RN_TextureFormatKind_D16;
    case VK_FORMAT_D24_UNORM_S8_UINT: return RN_TextureFormatKind_D24S8;
    case VK_FORMAT_D32_SFLOAT: return RN_TextureFormatKind_D32F;
    case VK_FORMAT_D32_SFLOAT_S8_UINT: return RN_TextureFormatKind_D32FS8;
    // Block‑compressed formats
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK: return RN_TextureFormatKind_BC1;
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: return RN_TextureFormatKind_BC1_SRGB;
    case VK_FORMAT_BC3_UNORM_BLOCK: return RN_TextureFormatKind_BC3;
    case VK_FORMAT_BC3_SRGB_BLOCK: return RN_TextureFormatKind_BC3_SRGB;
    case VK_FORMAT_BC7_UNORM_BLOCK: return RN_TextureFormatKind_BC7;
    case VK_FORMAT_BC7_SRGB_BLOCK: return RN_TextureFormatKind_BC7_SRGB;
    // ETC2 formats
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK: return RN_TextureFormatKind_ETC2_RGB8;
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK: return RN_TextureFormatKind_ETC2_RGBA8;
    }
    DOT_RESTORE_PARTIAL_SWITCH
}

internal b32
vk_helper_physical_device_all_required_extensions(
    const RN_VK_VulkanConfig *vk_config,
    VkPhysicalDevice device)
{
    TempArena temp = threadctx_temp_begin(0);
    u32 extension_count;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);
    array(VkExtensionProperties) available_extensions = PUSH_ARRAY(temp.arena, VkExtensionProperties, extension_count);
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, available_extensions);

    b32 all_found = true;
    for(u64 i = 0; i < vk_config->device.extensions.count; ++i){
        b32 found = false;
        String8 device_extension_name = vk_config->device.extensions.data[i];
        for(u64 j = 0; j < extension_count; ++j){
            if(string8_equal(device_extension_name, string8_from_cstring(available_extensions[j].extensionName))){
                DOT_PRINT("Found extension \"%s\"", device_extension_name);
                found = true;
                break;
            }
        }
        if(!found){  // Might aswell list all missing physical dev extensions
            all_found = false;
            DOT_WARNING("Requested extension %s not found", device_extension_name);
        }
    }
    threadctx_temp_end(temp);
    return all_found;
}

internal b32
vk_helper_physical_device_swapchain_support(
    VkPhysicalDevice gpu,
    VkSurfaceKHR surface,
    VkHelper_SwapchainDetails *details)
{
    TempArena temp = threadctx_temp_begin(0);
    typedef struct SwapchainSupportDetails{
        VkSurfaceCapabilities2KHR surface_capabilities;

        array(VkSurfaceFormat2KHR) surface_formats;
        u32 format_count;

        array(VkPresentModeKHR) present_modes;
        u32 present_modes_count;
    }SwapchainSupportDetails;

    SwapchainSupportDetails swapchain_support_details = {
        .surface_capabilities = VkSurfaceCapabilities2KHRParams(),
    };

    // 2KHR version expect this instead of just surface....
    VkPhysicalDeviceSurfaceInfo2KHR surface_info = VkPhysicalDeviceSurfaceInfo2KHRParams(.surface = surface);
    // --- Query Surface Capabilities ---
    vkGetPhysicalDeviceSurfaceCapabilities2KHR(gpu, &surface_info, &swapchain_support_details.surface_capabilities);
 
    // --- Query Surface Formats ---
    RN_VK_CHECK(vkGetPhysicalDeviceSurfaceFormats2KHR(gpu, &surface_info, &swapchain_support_details.format_count, NULL));
    swapchain_support_details.surface_formats = PUSH_ARRAY(temp.arena, VkSurfaceFormat2KHR, swapchain_support_details.format_count);
    for(u32 it = 0; it < swapchain_support_details.format_count; ++it){
        swapchain_support_details.surface_formats[it] = VkSurfaceFormat2KHRParams();
    }
    vkGetPhysicalDeviceSurfaceFormats2KHR(gpu, &surface_info, &swapchain_support_details.format_count, swapchain_support_details.surface_formats);
 
    // --- Query Present Modes ---
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &swapchain_support_details.present_modes_count, NULL);
    swapchain_support_details.present_modes = PUSH_ARRAY(temp.arena, VkPresentModeKHR, swapchain_support_details.present_modes_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &swapchain_support_details.present_modes_count, swapchain_support_details.present_modes);
    b32 has_support = swapchain_support_details.format_count > 0 && swapchain_support_details.present_modes_count > 0;
    if(has_support && details != NULL){
        VkSurfaceCapabilitiesKHR surface_capabilities = swapchain_support_details.surface_capabilities.surfaceCapabilities;
        VkExtent2D surface_extent = surface_capabilities.currentExtent;
        if(surface_extent.width == U32_MAX){ // Should we just do this outside this func?
            surface_extent.width = DOT_CLAMP(cast(u32)details->frame_buffer_size.x, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
            surface_extent.height = DOT_CLAMP(cast(u32)details->frame_buffer_size.y, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
        }

        VkColorSpaceKHR  preferred_colorspace   = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        VkSurfaceFormat2KHR desired_format = swapchain_support_details.surface_formats[0]; // Keep this one if none found
        VkSurfaceFormat2KHR *surface_formats = swapchain_support_details.surface_formats;
        for(u32 i = 0; i < swapchain_support_details.format_count; ++i){
            VkSurfaceFormat2KHR desiredVk_format = surface_formats[i];
            if (desiredVk_format.surfaceFormat.format == details->preferred_format &&
                desiredVk_format.surfaceFormat.colorSpace == preferred_colorspace){
                desired_format = desiredVk_format;
                break;
            }
        }

        VkPresentModeKHR best_present_mode = VK_PRESENT_MODE_FIFO_KHR; // Keep this one if none found for now
        array(VkPresentModeKHR) present_modes = swapchain_support_details.present_modes;
        for(u32 i = 0; i < swapchain_support_details.present_modes_count; ++i){
            if (present_modes[i] == details->preferred_present_mode){
                best_present_mode = present_modes[i];
                break;
            }
        }
        details->best_present_mode   = best_present_mode;
        details->best_surface_format = desired_format.surfaceFormat;
        details->surface_extent      = surface_extent;
        details->image_count         = surface_capabilities.minImageCount+1;
        if(surface_capabilities.maxImageCount > 0 && details->image_count > surface_capabilities.maxImageCount){
            details->image_count = surface_capabilities.maxImageCount;
        }
        details->current_transform   = surface_capabilities.currentTransform;
    }
    threadctx_temp_end(temp);
    return has_support;
}

internal VkHelper_CandidateDeviceInfo
vk_helper_pick_best_device(
    const RN_VK_VulkanConfig *vk_config,
    VkInstance instance,
    VkSurfaceKHR surface)
{
    TempArena temp = threadctx_temp_begin(0);
    u32 device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, NULL);
    if (device_count == 0) {
        DOT_ERROR("No Vulkan-capable GPUs found");
    }

    array(VkPhysicalDevice) devices = PUSH_ARRAY(temp.arena, VkPhysicalDevice, device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices);
    DOT_PRINT("device count %i", device_count);

    VkHelper_CandidateDeviceInfo best_device = {0};
    best_device.score = -1;

    for(u32 i = 0; i < device_count; i++){
        b32 is_integrated_gpu = true;
        VkPhysicalDevice dev = devices[i];
        // We to first ensure we have what we want before even rating the device
        if (!vk_helper_physical_device_all_required_extensions(vk_config, dev)
            || !vk_helper_physical_device_swapchain_support(dev, surface, NULL)){
            continue;
        }

        VkPhysicalDeviceProperties device_properties;
        VkPhysicalDeviceFeatures device_features;
        vkGetPhysicalDeviceProperties(dev, &device_properties);
        vkGetPhysicalDeviceFeatures(dev, &device_features);

        u32 queue_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queue_count, NULL);
        array(VkQueueFamilyProperties) queue_properties =
            PUSH_ARRAY(temp.arena, VkQueueFamilyProperties, queue_count);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queue_count,
                                                 queue_properties);

        u32 graphics_idx = U32_MAX;
        u32 present_idx = U32_MAX;
        b32 shared_present_graphics_queues = false;
        for(u32 q = 0; q < queue_count; q++){
            b32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, q, surface, &present_support);
            if ((queue_properties[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                present_support) {
                shared_present_graphics_queues = true;
                graphics_idx = q;
                present_idx = q;
                break;
            }
        }
        if(!shared_present_graphics_queues){
            for (u32 q = 0; q < queue_count; q++){
                if (queue_properties[q].queueFlags & VK_QUEUE_GRAPHICS_BIT){
                    graphics_idx = q;
                }
                u32 present_support = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(dev, q, surface, &present_support);
                if (present_support) {
                    present_idx = q;
                }
            }
        }
        if(graphics_idx == U32_MAX || present_idx == U32_MAX){
            continue;
        }

        int score = -1;
        if(device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU){
            score += 2000;
            is_integrated_gpu = false;
        }
        // if(device_properties.deviceType ==
        // VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 100;
        if(device_features.geometryShader)
            score += 10;
        DOT_PRINT("graphics family: %d; present family: %i; score: %i",
                  graphics_idx, present_idx, score);
        if(score > best_device.score){
            best_device.is_integrated_gpu = is_integrated_gpu;
            best_device.gpu = dev;
            best_device.graphics_family = cast(u16) graphics_idx;
            best_device.present_family = cast(u16) present_idx;
            best_device.score = score;
            best_device.shared_present_graphics_queues =
                shared_present_graphics_queues;
        }
    }

    if(best_device.score < 0){
        DOT_ERROR("No suitable GPU found");
    }

    DOT_PRINT("best_device  graphics family: %d; present family: %i; score: %i",
              best_device.graphics_family, best_device.present_family,
              best_device.score);
    threadctx_temp_end(temp);
    return best_device;
}

///////////////////////////////////////////
/// Vk misc helpers
///

internal VkExtent3D
vk_helper_extent3d_from_extent2d(VkExtent2D extent2d)
{
    return (VkExtent3D){extent2d.width, extent2d.height, 1};
}

internal VkExtent2D
vk_helper_extent2d_from_extent3d(VkExtent3D extent3d)
{
    return (VkExtent2D){extent3d.width, extent3d.height};
}

internal void
vk_helper_rn_vk_texture_transition(
    VkCommandBuffer cmd,
    RN_VK_Texture *tex,
    VkImageLayout new_layout)
{
    VkImageAspectFlags aspect_mask = (new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
    	? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    VkDependencyInfo dep_info = {
        .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &(VkImageMemoryBarrier2){
            .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .srcAccessMask    = VK_ACCESS_2_MEMORY_WRITE_BIT,
            .dstStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .dstAccessMask    = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
            .oldLayout        = tex->vk_image_layout,
            .newLayout        = new_layout,
            .image            = tex->vk_image,
            .subresourceRange = (VkImageSubresourceRange){
                .aspectMask = aspect_mask,
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS, // 1 ?
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS, // 1 ?
            }
        } ,
    };
    tex->vk_image_layout = new_layout;
    vkCmdPipelineBarrier2(cmd, &dep_info);
}

internal void
vk_helper_transition_image(
    VkCommandBuffer cmd,
    VkImage image,
    VkImageLayout current_layout,
    VkImageLayout new_layout)
{
    VkImageAspectFlags aspect_mask = (new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
    	? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    VkDependencyInfo dep_info = {
        .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &(VkImageMemoryBarrier2){
            .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .srcAccessMask    = VK_ACCESS_2_MEMORY_WRITE_BIT,
            .dstStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .dstAccessMask    = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
            .oldLayout        = current_layout,
            .newLayout        = new_layout,
            .image            = image,
            .subresourceRange = (VkImageSubresourceRange){
                .aspectMask = aspect_mask,
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS, // 1 ?
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS, // 1 ?
            }
        } ,
    };
    vkCmdPipelineBarrier2(cmd, &dep_info);
}

// NOTE: VkCmdCopyImage for faster copying when resolution matches
void vk_helper_copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize)
{
	VkBlitImageInfo2 blitInfo = {
	    .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
		.dstImage = destination,
		.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.srcImage = source,
		.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		.filter = VK_FILTER_LINEAR,
		.regionCount = 1,
		.pRegions = &(VkImageBlit2){
	        .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, 
	        .srcOffsets[1].x = cast(i32) srcSize.width,
	        .srcOffsets[1].y = cast(i32) srcSize.height,
	        .srcOffsets[1].z = 1,

	        .dstOffsets[1].x = cast(i32) dstSize.width,
	        .dstOffsets[1].y = cast(i32) dstSize.height,
	        .dstOffsets[1].z = 1,

	        .srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	        .srcSubresource.baseArrayLayer = 0,
	        .srcSubresource.layerCount = 1,
	        .srcSubresource.mipLevel = 0,

	        .dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	        .dstSubresource.baseArrayLayer = 0,
	        .dstSubresource.layerCount = 1,
	        .dstSubresource.mipLevel = 0,
        }
	};

	vkCmdBlitImage2(cmd, &blitInfo);
}
///
///////////////////////////////////////////
/// Vk CreateInfo helpers
///

internal VkImageSubresourceRange
vk_image_subresource_range(VkImageAspectFlags aspect_mask)
{
    VkImageSubresourceRange subImage = {
        .aspectMask = aspect_mask,
        .baseMipLevel = 0,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };
    return(subImage);
}

internal VkMemoryRequirements2
vk_image_memory_requirements(VkDevice vk_device, VkImage vk_image)
{
    VkMemoryRequirements2 reqs = { .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
    vkGetImageMemoryRequirements2(
        vk_device,
        &(VkImageMemoryRequirementsInfo2){
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
            .image = vk_image,
        },
        &reqs);
    return(reqs);
}

internal VkMemoryRequirements2
vk_buffer_memory_requirements(VkDevice vk_device, VkBuffer vk_buffer)
{
    VkMemoryRequirements2 reqs = { .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
    vkGetBufferMemoryRequirements2(
        vk_device,
        &(VkBufferMemoryRequirementsInfo2){
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,
            .buffer = vk_buffer,
        },
        &reqs);
    return(reqs);
}

internal VkSemaphoreSubmitInfo
vk_semaphore_submit_info(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore)
{
	VkSemaphoreSubmitInfo submitInfo = {
	    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
	    .semaphore = semaphore,
	    .stageMask = stageMask,
	    .deviceIndex = 0,
	    .value = 1
    };
	return(submitInfo);
}

internal VkCommandBufferSubmitInfo
vk_command_buffer_submit_info(VkCommandBuffer cmd)
{
	VkCommandBufferSubmitInfo info = {
	    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
	    .pNext = NULL,
	    .commandBuffer = cmd,
	    .deviceMask = 0,
    };
	return info;
}

internal VkSubmitInfo2
vk_submit_info(
    VkCommandBufferSubmitInfo* cmd,
    VkSemaphoreSubmitInfo* signal_semaphore_info,
    VkSemaphoreSubmitInfo* wait_semaphore_info)
{
    VkSubmitInfo2 info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = NULL,

        .waitSemaphoreInfoCount = wait_semaphore_info == NULL ? 0 : 1,
        .pWaitSemaphoreInfos = wait_semaphore_info,

        .signalSemaphoreInfoCount = signal_semaphore_info == NULL ? 0 : 1,
        .pSignalSemaphoreInfos = signal_semaphore_info,

        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = cmd,
    };

    return info;
}

internal VkImageCreateInfo
vk_image_create_info(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent)
{
    VkImageCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,

        .imageType = VK_IMAGE_TYPE_2D,

        .format = format,
        .extent = extent,

        .mipLevels = 1,
        .arrayLayers = 1,

        //for MSAA. we will not be using it by default, so default it to 1 sample per pixel.
        .samples = VK_SAMPLE_COUNT_1_BIT,

        //optimal tiling, which means the image is stored on the best gpu format
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usageFlags,
    };

    return info;
}

internal VkImageViewCreateInfo
vk_imageview_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .image = image,
        .format = format,
        .subresourceRange = {
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
            .aspectMask = aspectFlags,
        },
    };

    return info;
}

internal void*
vk_alloc(void* data, usize size, usize alignment, VkSystemAllocationScope scope)
{
    DOT_UNUSED(data); DOT_UNUSED(size); DOT_UNUSED(alignment); DOT_UNUSED(scope);
    DOT_PRINT("VK Alloc: size=%M; scope=\n", size, string_VkSystemAllocationScope(scope));

    Arena* arena = cast(Arena*)data;
    void* mem = ARENA_PUSH(arena, size, alignment, true);
    arena_print_debug(arena);
    return mem;
}

internal void*
vk_realloc(void* data, void* old_mem, usize size, usize alignment, VkSystemAllocationScope scope)
{
    DOT_UNUSED(data); DOT_UNUSED(size); DOT_UNUSED(alignment); DOT_UNUSED(scope); DOT_UNUSED(old_mem);
    DOT_PRINT("VK Realloc: size=%M; scope=%s", size, string_VkSystemAllocationScope(scope) );
    Arena* arena = cast(Arena*)data;
    void* mem = ARENA_PUSH(arena, size, alignment, true);
    arena_print_debug(arena);
    return mem;
}

internal void
vk_free(void* data, void* mem)
{
    DOT_UNUSED(data); DOT_UNUSED(mem);
    Arena* arena = cast(Arena*)data;
    DOT_PRINT("VK free");
    arena_print_debug(arena);
}

internal void
vk_internal_alloc(void* data, usize size, VkInternalAllocationType alloc_type, VkSystemAllocationScope scope)
{
    DOT_UNUSED(data); DOT_UNUSED(size); DOT_UNUSED(scope); DOT_UNUSED(alloc_type);
    DOT_PRINT("VK Internal Alloc: size=%M; scope=%s", size, string_VkSystemAllocationScope(scope));
    // Arena* arena = cast(Arena*)data;
    // arena_print_debug(arena);
}

internal void
vk_internal_free(void* data, usize size, VkInternalAllocationType alloc_type, VkSystemAllocationScope scope)
{
    DOT_UNUSED(data); DOT_UNUSED(size); DOT_UNUSED(scope); DOT_UNUSED(alloc_type);
    DOT_PRINT("VK Internal Free: size=%M; scope=%s", size, string_VkSystemAllocationScope(scope));
}

#ifdef VK_USE_CUSTOM_ALLOCATOR
#define VkAllocatorParams(arena) (VkAllocationCallbacks){ \
    .pUserData = arena, \
    .pfnAllocation = vk_alloc, \
    .pfnReallocation = vk_realloc, \
    .pfnFree = vk_free, \
    .pfnInternalAllocation = vk_internal_alloc, \
    .pfnInternalFree = vk_internal_free, \
}
#else
#define VkAllocatorParams(arena) NULL
#endif // !VK_USE_CUSTOM_ALLOCATOR

#endif // !VK_HELPER_IMPLEMENTATION
