#define VOLK_IMPLEMENTATION
#include "third_party/volk/volk.h"

////////////////////////////////////////////////////////////////
/// RB_VK_CONFIG

#if DOT_ENABLED(DOT_DEBUG)
#   define RB_VK_VALIDATION_LAYERS_ENABLE 1
#   define RB_VK_DEBUG_UTILS_ENABLE 1
#else
#   define RB_VK_VALIDATION_LAYERS_ENABLE 0
#   define RB_VK_DEBUG_UTILS_ENABLE 0
#endif

typedef enum RB_VK_DeviceFeatureKind
{ 
    RB_VK_DeviceFeature_Vulkan12,
    RB_VK_DeviceFeature_Vulkan13,
    RB_VK_DeviceFeature_Vulkan14,
    RB_VK_DeviceFeature_Swapchain,
    RB_VK_DeviceFeature_SwapchainMaintenance1,
    RB_VK_DeviceFeature_DeviceAddressCommands,
    RB_VK_DeviceFeature_DescriptorHeap,
    RB_VK_DeviceFeature_ShaderUntypedPointers,
    RB_VK_DeviceFeature_ShaderDrawParameters,
    RB_VK_DeviceFeature_ExtendedDynamicState,
    RB_VK_DeviceFeature_MeshShader,
    RB_VK_DeviceFeature_UnifiedImageLayouts,
    RB_VK_DeviceFeature_MemoryBudget,
    RB_VK_DeviceFeature_COUNT,
}RB_VK_DeviceFeatureKind;

typedef struct RB_VK_DeviceFeature
{
    String8 extension;
    void *features;
    b8 optional;
}RB_VK_DeviceFeature;

typedef struct RB_VK_VulkanConfig
{
    struct
    {
        u32 extension_count;
        String8 *extensions;
        String8 application_name;
        u32 application_version;
        String8 engine_name;
        u32 engine_version;
        u32 api_version;
    }instance;

    struct
    {
        VkPhysicalDeviceFeatures core_features;
        RB_VK_DeviceFeature features[RB_VK_DeviceFeature_COUNT];
    }device;

    u32 validation_layer_count;
    String8 *validation_layers;
}RB_VK_VulkanConfig;

////////////////////////////////////////////////////////////////
/// RB_VK_MEMORY

enum
{
    RB_VK_MEMORY_TYPE_ALWAYS_FORBIDDEN = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT
        | VK_MEMORY_PROPERTY_PROTECTED_BIT
        | VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD
        | VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD,
};

typedef struct RB_VK_GpuMemoryProperties
{
    VkMemoryPropertyFlags required;
    VkMemoryPropertyFlags preferred;
    VkMemoryPropertyFlags forbidden;
    VkBufferUsageFlags usage;
}RB_VK_GpuMemoryProperties;

typedef struct RB_VK_DescriptorHeapLayout
{
    u64 descriptor_size;
    u64 reserved_alignment;
    u64 reserved_size;
    u64 heap_alignment;
}RB_VK_DescriptorHeapLayout;

////////////////////////////////////////////////////////////////
/// RB_VK_TEXTURE

internal VkDeviceMemory rb_vk_image_memory_alloc(VkImage vk_image);
internal b8             rb_vk_image_create_info_supported(VkImageCreateInfo *info);
internal void           rb_vk_texture_memory_select(void);

////////////////////////////////////////////////////////////////
/// RB_VK_GPU_ADDRESS

// (jd) Fallback path for GPUs without VK_KHR_device_address_commands.
internal VkBuffer rb_vk_buffer_from_gpu_address(RB_GpuAddress address, VkDeviceSize *out_offset);

////////////////////////////////////////////////////////////////
/// RB_VK_GLOBALS

global RB_VK_BackendCtx *rb_vk_g_ctx;

global read_only String8 rb_vk_g_instance_extensions[] = {
    str_lit(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME),
    str_lit(VK_KHR_SURFACE_EXTENSION_NAME),
    str_lit(VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME),
#if RB_VK_DEBUG_UTILS_ENABLE
    str_lit(VK_EXT_DEBUG_UTILS_EXTENSION_NAME),
#endif
};

global read_only String8 rb_vk_g_validation_layers[] = {
    str_lit("VK_LAYER_KHRONOS_validation"),
};

read_only global RB_VK_VulkanConfig rb_vk_g_config = {
    .instance = {
        .application_name    = str_lit("dot_engine"),
        .application_version = VK_MAKE_VERSION(1,0,0),
        .engine_name         = str_lit("dot_engine"),
        .engine_version      = VK_MAKE_VERSION(1,0,0),
        .api_version         = VK_API_VERSION_1_4,

        .extension_count     = DOT_ARRAY_COUNT(rb_vk_g_instance_extensions),
        .extensions          = rb_vk_g_instance_extensions,
    },
    .validation_layer_count = RB_VK_VALIDATION_LAYERS_ENABLE ? DOT_ARRAY_COUNT(rb_vk_g_validation_layers) : 0,
    .validation_layers      = rb_vk_g_validation_layers,
    .device = {
        .core_features = {
            .shaderInt64 = VK_TRUE,
        },
        .features = {
            [RB_VK_DeviceFeature_Vulkan12] = {
                .features = &(VkPhysicalDeviceVulkan12Features){
                    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
                    .bufferDeviceAddress = VK_TRUE,
                    .descriptorIndexing = VK_TRUE,
                    .descriptorBindingPartiallyBound = VK_TRUE,
                    .descriptorBindingVariableDescriptorCount = VK_TRUE,
                    .runtimeDescriptorArray = VK_TRUE,
                    .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
                    .shaderStorageBufferArrayNonUniformIndexing = VK_TRUE,
                    .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
                    .descriptorBindingStorageImageUpdateAfterBind = VK_TRUE,
                    .descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE,
                    .descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE,
                    .timelineSemaphore = VK_TRUE,
                    .hostQueryReset = VK_TRUE,
                    .scalarBlockLayout = VK_TRUE,
                },
            },
            [RB_VK_DeviceFeature_Vulkan13] = {
                .features = &(VkPhysicalDeviceVulkan13Features){
                    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
                    .synchronization2 = VK_TRUE,
                    .dynamicRendering = VK_TRUE,
                },
            },
            [RB_VK_DeviceFeature_Vulkan14] = {
                .features = &(VkPhysicalDeviceVulkan14Features){
                    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
                    .maintenance5 = VK_TRUE,
                },
            },
            [RB_VK_DeviceFeature_Swapchain] = {
                .extension = str_lit(VK_KHR_SWAPCHAIN_EXTENSION_NAME),
            },
            [RB_VK_DeviceFeature_SwapchainMaintenance1] = {
                .extension = str_lit(VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME),
                .features = &(VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR){
                    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR,
                    .swapchainMaintenance1 = VK_TRUE,
                },
            },
            [RB_VK_DeviceFeature_DeviceAddressCommands] = {
                .extension = str_lit(VK_KHR_DEVICE_ADDRESS_COMMANDS_EXTENSION_NAME),
                .optional = true,
                .features = &(VkPhysicalDeviceDeviceAddressCommandsFeaturesKHR){
                    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_ADDRESS_COMMANDS_FEATURES_KHR,
                    .deviceAddressCommands = VK_TRUE,
                },
            },
            [RB_VK_DeviceFeature_DescriptorHeap] = {
                .extension = str_lit(VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME),
                .features = &(VkPhysicalDeviceDescriptorHeapFeaturesEXT){
                    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_FEATURES_EXT,
                    .descriptorHeap = VK_TRUE,
                },
            },
            [RB_VK_DeviceFeature_ShaderUntypedPointers] = {
                .extension = str_lit(VK_KHR_SHADER_UNTYPED_POINTERS_EXTENSION_NAME),
                .features = &(VkPhysicalDeviceShaderUntypedPointersFeaturesKHR){
                    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_UNTYPED_POINTERS_FEATURES_KHR,
                    .shaderUntypedPointers = VK_TRUE,
                },
            },
            [RB_VK_DeviceFeature_ShaderDrawParameters] = {
                .extension = str_lit(VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME),
            },
            [RB_VK_DeviceFeature_ExtendedDynamicState] = {
                .extension = str_lit(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME),
                .features = &(VkPhysicalDeviceExtendedDynamicStateFeaturesEXT){
                    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
                    .extendedDynamicState = VK_TRUE,
                },
            },
            [RB_VK_DeviceFeature_MeshShader] = {
                .extension = str_lit(VK_EXT_MESH_SHADER_EXTENSION_NAME),
                .optional = true,
                .features = &(VkPhysicalDeviceMeshShaderFeaturesEXT){
                    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
                    .taskShader = VK_TRUE,
                    .meshShader = VK_TRUE,
                },
            },
            [RB_VK_DeviceFeature_UnifiedImageLayouts] = {
                .extension = str_lit(VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME),
                .optional = true,
                .features = &(VkPhysicalDeviceUnifiedImageLayoutsFeaturesKHR){
                    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFIED_IMAGE_LAYOUTS_FEATURES_KHR,
                    .unifiedImageLayouts = VK_TRUE,
                },
            },
            [RB_VK_DeviceFeature_MemoryBudget] = {
                .extension = str_lit(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME),
                .optional = true,
            },
        },
    },
};

////////////////////////////////////////////////////////////////
/// RB_VK_CHECK

#ifdef NDEBUG
#   define RB_VK_CHECK(x) x
#else
#   define RB_VK_CHECK(x) \
        do{ \
            VkResult err = x; \
            if(err < 0){ DOT_ERROR("Detected Vulkan error: %S\n", rb_vk_string8_from_vk_result(err)); } \
        }while (0)
#endif

internal inline String8
rb_vk_string8_from_vk_result(VkResult input_value)
{
    switch(input_value){
    case VK_SUCCESS                                             : return str_comp("VK_SUCCESS");
    case VK_NOT_READY                                           : return str_comp("VK_NOT_READY");
    case VK_TIMEOUT                                             : return str_comp("VK_TIMEOUT");
    case VK_EVENT_SET                                           : return str_comp("VK_EVENT_SET");
    case VK_EVENT_RESET                                         : return str_comp("VK_EVENT_RESET");
    case VK_INCOMPLETE                                          : return str_comp("VK_INCOMPLETE");
    case VK_ERROR_OUT_OF_HOST_MEMORY                            : return str_comp("VK_ERROR_OUT_OF_HOST_MEMORY");
    case VK_ERROR_OUT_OF_DEVICE_MEMORY                          : return str_comp("VK_ERROR_OUT_OF_DEVICE_MEMORY");
    case VK_ERROR_INITIALIZATION_FAILED                         : return str_comp("VK_ERROR_INITIALIZATION_FAILED");
    case VK_ERROR_DEVICE_LOST                                   : return str_comp("VK_ERROR_DEVICE_LOST");
    case VK_ERROR_MEMORY_MAP_FAILED                             : return str_comp("VK_ERROR_MEMORY_MAP_FAILED");
    case VK_ERROR_LAYER_NOT_PRESENT                             : return str_comp("VK_ERROR_LAYER_NOT_PRESENT");
    case VK_ERROR_EXTENSION_NOT_PRESENT                         : return str_comp("VK_ERROR_EXTENSION_NOT_PRESENT");
    case VK_ERROR_FEATURE_NOT_PRESENT                           : return str_comp("VK_ERROR_FEATURE_NOT_PRESENT");
    case VK_ERROR_INCOMPATIBLE_DRIVER                           : return str_comp("VK_ERROR_INCOMPATIBLE_DRIVER");
    case VK_ERROR_TOO_MANY_OBJECTS                              : return str_comp("VK_ERROR_TOO_MANY_OBJECTS");
    case VK_ERROR_FORMAT_NOT_SUPPORTED                          : return str_comp("VK_ERROR_FORMAT_NOT_SUPPORTED");
    case VK_ERROR_FRAGMENTED_POOL                               : return str_comp("VK_ERROR_FRAGMENTED_POOL");
    case VK_ERROR_UNKNOWN                                       : return str_comp("VK_ERROR_UNKNOWN");
    case VK_ERROR_VALIDATION_FAILED                             : return str_comp("VK_ERROR_VALIDATION_FAILED");
    case VK_ERROR_OUT_OF_POOL_MEMORY                            : return str_comp("VK_ERROR_OUT_OF_POOL_MEMORY");
    case VK_ERROR_INVALID_EXTERNAL_HANDLE                       : return str_comp("VK_ERROR_INVALID_EXTERNAL_HANDLE");
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS                : return str_comp("VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS");
    case VK_ERROR_FRAGMENTATION                                 : return str_comp("VK_ERROR_FRAGMENTATION");
    case VK_PIPELINE_COMPILE_REQUIRED                           : return str_comp("VK_PIPELINE_COMPILE_REQUIRED");
    case VK_ERROR_NOT_PERMITTED                                 : return str_comp("VK_ERROR_NOT_PERMITTED");
    case VK_ERROR_SURFACE_LOST_KHR                              : return str_comp("VK_ERROR_SURFACE_LOST_KHR");
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR                      : return str_comp("VK_ERROR_NATIVE_WINDOW_IN_USE_KHR");
    case VK_SUBOPTIMAL_KHR                                      : return str_comp("VK_SUBOPTIMAL_KHR");
    case VK_ERROR_OUT_OF_DATE_KHR                               : return str_comp("VK_ERROR_OUT_OF_DATE_KHR");
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR                      : return str_comp("VK_ERROR_INCOMPATIBLE_DISPLAY_KHR");
    case VK_ERROR_INVALID_SHADER_NV                             : return str_comp("VK_ERROR_INVALID_SHADER_NV");
    case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR                 : return str_comp("VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR");
    case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR        : return str_comp("VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR");
    case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR     : return str_comp("VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR");
    case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR        : return str_comp("VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR");
    case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR         : return str_comp("VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR");
    case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR           : return str_comp("VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR");
    case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT  : return str_comp("VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT");
    case VK_ERROR_PRESENT_TIMING_QUEUE_FULL_EXT                 : return str_comp("VK_ERROR_PRESENT_TIMING_QUEUE_FULL_EXT");
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT           : return str_comp("VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT");
    case VK_THREAD_IDLE_KHR                                     : return str_comp("VK_THREAD_IDLE_KHR");
    case VK_THREAD_DONE_KHR                                     : return str_comp("VK_THREAD_DONE_KHR");
    case VK_OPERATION_DEFERRED_KHR                              : return str_comp("VK_OPERATION_DEFERRED_KHR");
    case VK_OPERATION_NOT_DEFERRED_KHR                          : return str_comp("VK_OPERATION_NOT_DEFERRED_KHR");
    case VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR              : return str_comp("VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR");
    case VK_ERROR_COMPRESSION_EXHAUSTED_EXT                     : return str_comp("VK_ERROR_COMPRESSION_EXHAUSTED_EXT");
    case VK_INCOMPATIBLE_SHADER_BINARY_EXT                      : return str_comp("VK_INCOMPATIBLE_SHADER_BINARY_EXT");
    case VK_PIPELINE_BINARY_MISSING_KHR                         : return str_comp("VK_PIPELINE_BINARY_MISSING_KHR");
    case VK_ERROR_NOT_ENOUGH_SPACE_KHR                          : return str_comp("VK_ERROR_NOT_ENOUGH_SPACE_KHR");
    case VK_RESULT_MAX_ENUM                                     :
    default                                                     : return str_comp("Unhandled VkResult");
    }
}

////////////////////////////////////////////////////////////////
/// Instance / device selection

internal b32
rb_vk_all_layers(const RB_VK_VulkanConfig *vk_config)
{
    Scratch s = scratch_begin(0,0);
    u32 available_layer_count = 0;
    vkEnumerateInstanceLayerProperties(&available_layer_count, NULL);
    VkLayerProperties *available_layers = PUSH_ARRAY(s.arena, VkLayerProperties, available_layer_count);
    vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers);
    b32 all_found = true;
    for(u64 i = 0; i < vk_config->validation_layer_count; ++i){
        b32 found = false;
        const String8 layer_name = vk_config->validation_layers[i];
        for(u64 j = 0; j < available_layer_count; ++j){
            if(string8_equal(layer_name, string8_from_cstring(available_layers[j].layerName))){
                u32 layer_spec = available_layers[j].specVersion;
                b32 valid_spec = !(layer_spec < VK_HEADER_VERSION_COMPLETE);
                if(valid_spec){
                    found = true;
                    DOT_PRINT("Found Layer %S", layer_name);
                    break;
                }else{
                    DOT_WARNING("Layer %S targets Vulkan %u.%u.%u but our headers are %u.%u.%u; " "it cannot forward the extensions we use",
                        layer_name,
                        VK_API_VERSION_MAJOR(layer_spec),
                        VK_API_VERSION_MINOR(layer_spec),
                        VK_API_VERSION_PATCH(layer_spec),
                        VK_API_VERSION_MAJOR(VK_HEADER_VERSION_COMPLETE),
                        VK_API_VERSION_MINOR(VK_HEADER_VERSION_COMPLETE),
                        VK_API_VERSION_PATCH(VK_HEADER_VERSION_COMPLETE));
                }
            }
        }
        if(!found){
            all_found = false;
            DOT_WARNING("Requested layer %S not found", layer_name);
        }
    }
    scratch_end(s);
    return all_found;
}

internal b32
rb_vk_instance_all_required_extensions(const RB_VK_VulkanConfig* vk_config)
{
    Scratch s = scratch_begin(0,0);
    u32 extension_count;
    vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);
    VkExtensionProperties *available_extensions = PUSH_ARRAY(s.arena, VkExtensionProperties, extension_count);
    vkEnumerateInstanceExtensionProperties(NULL, &extension_count, available_extensions);

    b32 all_found = true;
    for(u64 i = 0; i < vk_config->instance.extension_count; ++i){
        b32 found = false;
        const String8 instance_extension_name = vk_config->instance.extensions[i];
        for(u64 j = 0; j < extension_count; ++j){
            char* name = available_extensions[j].extensionName;
            if(string8_equal(instance_extension_name, string8_from_cstring(name))){
                DOT_PRINT("Found instance extension \"%S\"", instance_extension_name);
                found = true;
                break;
            }
        }
        if(!found){ // Might aswell list all missing dev extensions
            all_found = false;
            DOT_WARNING("Requested instance extension \"%S\" not found", instance_extension_name);
        }
    }
    scratch_end(s);
    return all_found;
}

internal b32
rb_vk_physical_device_all_required_extensions(
    const RB_VK_VulkanConfig *vk_config,
    VkPhysicalDevice device,
    b8 *feature_enabled)
{
    Scratch s = scratch_begin(0,0);
    u32 extension_count;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);
    VkExtensionProperties *available_extensions = PUSH_ARRAY(s.arena, VkExtensionProperties, extension_count);
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, available_extensions);

    b32 all_found = true;
    for(u64 i = 0; i < RB_VK_DeviceFeature_COUNT; ++i){
        const RB_VK_DeviceFeature *feature = &vk_config->device.features[i];
        b32 found = feature->extension.size == 0;
        for(u64 j = 0; !found && j < extension_count; ++j){
            found = string8_equal(feature->extension, string8_from_cstring(available_extensions[j].extensionName));
        }
        feature_enabled[i] = cast(b8) found;
        if(!found && feature->optional){
            DOT_PRINT("Optional extension %S not found", feature->extension);
        }else if(!found){
            all_found = false;
            DOT_WARNING("Requested extension %S not found", feature->extension);
        }
    }
    scratch_end(s);
    return all_found;
}

internal b32
rb_vk_physical_device_swapchain_support(
    VkPhysicalDevice gpu,
    VkSurfaceKHR surface,
    RB_VK_SwapchainDetails *details)
{
    Scratch s = scratch_begin(0,0);

    typedef struct SwapchainSupportDetails
    {
        VkSurfaceCapabilities2KHR surface_capabilities;

        u32 format_count;
        VkSurfaceFormat2KHR* surface_formats;

        u32 present_modes_count;
        VkPresentModeKHR* present_modes;
    }SwapchainSupportDetails;

    SwapchainSupportDetails swapchain_support_details = {
        .surface_capabilities.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
    };

    VkPhysicalDeviceSurfaceInfo2KHR surface_info = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
        .surface = surface
    };

    vkGetPhysicalDeviceSurfaceCapabilities2KHR(gpu, &surface_info, &swapchain_support_details.surface_capabilities);

    RB_VK_CHECK(vkGetPhysicalDeviceSurfaceFormats2KHR(gpu, &surface_info, &swapchain_support_details.format_count, NULL));

    swapchain_support_details.surface_formats = PUSH_ARRAY(s.arena, VkSurfaceFormat2KHR, swapchain_support_details.format_count);
    for(u32 it = 0; it < swapchain_support_details.format_count; ++it){
        swapchain_support_details.surface_formats[it] = (VkSurfaceFormat2KHR){
            .sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR,
        };
    }
    vkGetPhysicalDeviceSurfaceFormats2KHR(gpu, &surface_info, &swapchain_support_details.format_count, swapchain_support_details.surface_formats);

    // --- Query Present Modes ---
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &swapchain_support_details.present_modes_count, NULL);
    swapchain_support_details.present_modes = PUSH_ARRAY(s.arena, VkPresentModeKHR, swapchain_support_details.present_modes_count);
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
        VkSurfaceFormat2KHR desired_format      = swapchain_support_details.surface_formats[0]; // Keep this one if none found
        VkSurfaceFormat2KHR *surface_formats    = swapchain_support_details.surface_formats;

        for(u32 i = 0; i < swapchain_support_details.format_count; ++i){
            VkSurfaceFormat2KHR desiredVk_format = surface_formats[i];
            if (desiredVk_format.surfaceFormat.format == details->preferred_format &&
                desiredVk_format.surfaceFormat.colorSpace == preferred_colorspace){
                desired_format = desiredVk_format;
                break;
            }
        }

        VkPresentModeKHR best_present_mode = VK_PRESENT_MODE_FIFO_KHR; // Keep this one if none found for now
        VkPresentModeKHR* present_modes = swapchain_support_details.present_modes;
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
    scratch_end(s);
    return has_support;
}

internal RB_VK_Device
rb_vk_pick_best_device(const RB_VK_VulkanConfig *vk_config, VkInstance instance, VkSurfaceKHR surface)
{
    Scratch s = scratch_begin(0,0);
    u32 device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, NULL);
    if (device_count == 0) {
        DOT_ERROR("No Vulkan-capable GPUs found");
    }

    VkPhysicalDevice* devices = PUSH_ARRAY(s.arena, VkPhysicalDevice, device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices);
    DOT_PRINT("device count %i", device_count);

    RB_VK_Device best_device = {0};
    VkPhysicalDeviceProperties best_properties = {0};
    i32 best_score = -1;

    for(u32 i = 0; i < device_count; i++){
        b32 is_integrated_gpu = true;
        VkPhysicalDevice dev = devices[i];
        VkPhysicalDeviceProperties device_properties;
        vkGetPhysicalDeviceProperties(dev, &device_properties);
        DOT_PRINT("Checking device \"%s\"", device_properties.deviceName);
        // We to first ensure we have what we want before even rating the device
        b8 feature_enabled[RB_VK_DeviceFeature_COUNT];
        if (!rb_vk_physical_device_all_required_extensions(vk_config, dev, feature_enabled)
            || !rb_vk_physical_device_swapchain_support(dev, surface, NULL)){
            DOT_PRINT("Skipping device \"%s\"", device_properties.deviceName);
            continue;
        }

        VkPhysicalDeviceFeatures device_features;
        vkGetPhysicalDeviceFeatures(dev, &device_features);

        u32 queue_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queue_count, NULL);
        VkQueueFamilyProperties *queue_properties = PUSH_ARRAY(s.arena, VkQueueFamilyProperties, queue_count);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queue_count, queue_properties);

        u32 graphics_idx = U32_MAX;
        u32 present_idx = U32_MAX;
        b32 shared_present_graphics_queues = false;
        for(u32 q = 0; q < queue_count; q++){
            b32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, q, surface, &present_support);
            if ((queue_properties[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present_support){
                shared_present_graphics_queues = true;
                graphics_idx = q;
                present_idx = q;
                break;
            }
        }
        if(!shared_present_graphics_queues){
            for (u32 q = 0; q < queue_count; q++){
                if(queue_properties[q].queueFlags & VK_QUEUE_GRAPHICS_BIT){
                    graphics_idx = q;
                }
                u32 present_support = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(dev, q, surface, &present_support);
                if(present_support){
                    present_idx = q;
                }
            }
        }
        if(graphics_idx == U32_MAX || present_idx == U32_MAX){
            continue;
        }

        i32 score = -1;
        if(device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU){
            score += 2000;
            is_integrated_gpu = false;
        }
        // if(device_properties.deviceType ==
        // VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 100;
        if(device_features.geometryShader){
            score += 10;
        }
        DOT_PRINT("\"%s\" graphics family: %d; present family: %i; score: %i", device_properties.deviceName, graphics_idx, present_idx, score);
        if(score > best_score){
            best_properties = device_properties;
            best_device.is_integrated_gpu = is_integrated_gpu;
            best_device.vk_gpu = dev;
            best_device.graphics_queue_idx = graphics_idx;
            best_device.present_queue_idx = present_idx;
            best_score = score;
        }
    }

    if(best_score < 0){
        DOT_ERROR("No suitable GPU found");
    }

    DOT_PRINT("best_device \"%s\" graphics family: %d; present family: %i; score: %i",
        best_properties.deviceName, best_device.graphics_queue_idx, best_device.present_queue_idx,
        best_score);

    scratch_end(s);
    return best_device;
}

////////////////////////////////////////////////////////////////
/// VK to RB

internal RB_FormatKind
rb_vk_format_kind_from_vk_format(VkFormat texture_format)
{
    partial_switch_begin(texture_format){
    default: DOT_ERROR("Format not implemented");
    case VK_FORMAT_UNDEFINED: return RB_FormatKind_None;
    // 8‑bit formats
    case VK_FORMAT_R8_UNORM             : return RB_FormatKind_R8_UNORM;
    case VK_FORMAT_R8_UINT              : return RB_FormatKind_R8_UINT;
    case VK_FORMAT_R8_SRGB              : return RB_FormatKind_R8_SRGB;

    case VK_FORMAT_R8G8_UNORM           : return RB_FormatKind_RG8_UNORM;
    case VK_FORMAT_R8G8_SRGB            : return RB_FormatKind_RG8_SRGB      ;
    // case VK_FORMAT_R8G8B8_UNORM         : return RB_FormatKind_RGB8_UNORM;
    // case VK_FORMAT_R8G8B8_SRGB          : return RB_FormatKind_RGB8_SRGB;
    case VK_FORMAT_R8G8B8A8_UNORM       : return RB_FormatKind_RGBA8_UNORM;
    case VK_FORMAT_R8G8B8A8_SRGB        : return RB_FormatKind_RGBA8_SRGB;
    case VK_FORMAT_B8G8R8A8_UNORM       : return RB_FormatKind_BGRA8_UNORM;
    case VK_FORMAT_B8G8R8A8_SRGB        : return RB_FormatKind_BGRA8_SRGB;
    case VK_FORMAT_R4G4B4A4_UNORM_PACK16: return RB_FormatKind_RGBA4_UNORM;

    case VK_FORMAT_R5G5B5A1_UNORM_PACK16: return RB_FormatKind_R5G5B5A1_UNORM;
    case VK_FORMAT_R5G6B5_UNORM_PACK16  : return RB_FormatKind_R5G6B5_UNORM ;
    // HDR / Float formats
    case VK_FORMAT_R16_SFLOAT           : return RB_FormatKind_R16_FLOAT;
    case VK_FORMAT_R16G16_SFLOAT        : return RB_FormatKind_RG16_FLOAT;
    case VK_FORMAT_R16G16B16A16_SFLOAT  : return RB_FormatKind_RGBA16_FLOAT;
    case VK_FORMAT_R32_SFLOAT           : return RB_FormatKind_R32_FLOAT;
    case VK_FORMAT_R32G32_SFLOAT        : return RB_FormatKind_RG32_FLOAT;
    case VK_FORMAT_R32G32B32A32_SFLOAT  : return RB_FormatKind_RGBA32_FLOAT;
    // Depth / Stencil formats
    case VK_FORMAT_D16_UNORM            : return RB_FormatKind_D16_UNORM;
    case VK_FORMAT_D24_UNORM_S8_UINT    : return RB_FormatKind_D24_UNORM_S8_UINT;
    case VK_FORMAT_D32_SFLOAT           : return RB_FormatKind_D32_FLOAT;
    case VK_FORMAT_D32_SFLOAT_S8_UINT   : return RB_FormatKind_D32_FLOAT_S8_UINT;
    // Block‑compressed formats
    // case VK_FORMAT_BC1_RGBA_UNORM_BLOCK : return RB_FormatKind_BC1;
    // case VK_FORMAT_BC1_RGBA_SRGB_BLOCK  : return RB_FormatKind_BC1_SRGB;
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK : return RB_FormatKind_EAC_RG;
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK  : return RB_FormatKind_ASTC_4X4_SRGB;
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK : return RB_FormatKind_ASTC_4X4_UNORM;

    case VK_FORMAT_BC3_UNORM_BLOCK      : return RB_FormatKind_BC3_UNORM;
    case VK_FORMAT_BC3_SRGB_BLOCK       : return RB_FormatKind_BC3_SRGB;
    case VK_FORMAT_BC5_UNORM_BLOCK      : return RB_FormatKind_BC5_RG;
    case VK_FORMAT_BC6H_UFLOAT_BLOCK    : return RB_FormatKind_BC6H_UFLOAT;
    case VK_FORMAT_BC6H_SFLOAT_BLOCK    : return RB_FormatKind_BC6H_SFLOAT;
    case VK_FORMAT_BC7_UNORM_BLOCK      : return RB_FormatKind_BC7_UNORM;
    case VK_FORMAT_BC7_SRGB_BLOCK       : return RB_FormatKind_BC7_SRGB;
    // ETC2 formats
    // case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK      : return RB_FormatKind_ETC2_RGB8;
    // case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK    : return RB_FormatKind_ETC2_RGBA8;
    }partial_switch_end;
}

////////////////////////////////////////////////////////////////
/// RB to VK

internal VkFilter
rb_vk_filter_from_sampler_filter_kind(RB_SamplerFilterKind sampler_filter)
{
    switch(sampler_filter){
    default: DOT_ERROR("undefined sampler filter %u", sampler_filter);
    case RB_SamplerFilterKind_Nearest: return VK_FILTER_NEAREST;
    case RB_SamplerFilterKind_Linear:  return VK_FILTER_LINEAR;
    case RB_SamplerFilterKind_Cubic:   return VK_FILTER_CUBIC_IMG;
    }
}

internal VkBlendOp
rb_vk_blend_op_from_blend_op_kind(RB_BlendOpKind blend_op)
{
    switch(blend_op){
        default: DOT_ERROR("undefined op %u", blend_op);
        case RB_BlendOpKind_Add:             return VK_BLEND_OP_ADD;
        case RB_BlendOpKind_Subtract:        return VK_BLEND_OP_SUBTRACT;
        case RB_BlendOpKind_ReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
        case RB_BlendOpKind_Min:             return VK_BLEND_OP_MIN;
        case RB_BlendOpKind_Max:             return VK_BLEND_OP_MAX;
    }
}

internal VkBlendFactor
rb_vk_blend_factor_from_blend_factor_kind(RB_BlendFactorKind blend_factor)
{
    switch(blend_factor){
    default: DOT_ERROR("undefined blend factor %u", blend_factor);
    case RB_BlendFactorKind_Zero:                  return VK_BLEND_FACTOR_ZERO;
    case RB_BlendFactorKind_One:                   return VK_BLEND_FACTOR_ONE;
    case RB_BlendFactorKind_SrcColor:              return VK_BLEND_FACTOR_SRC_COLOR;
    case RB_BlendFactorKind_OneMinusSrcColor:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case RB_BlendFactorKind_DstColor:              return VK_BLEND_FACTOR_DST_COLOR;
    case RB_BlendFactorKind_OneMinusDstColor:      return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case RB_BlendFactorKind_SrcAlpha:              return VK_BLEND_FACTOR_SRC_ALPHA;
    case RB_BlendFactorKind_OneMinusSrcAlpha:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case RB_BlendFactorKind_DstAlpha:              return VK_BLEND_FACTOR_DST_ALPHA;
    case RB_BlendFactorKind_OneMinusDstAlpha:      return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case RB_BlendFactorKind_ConstantColor:         return VK_BLEND_FACTOR_CONSTANT_COLOR;
    case RB_BlendFactorKind_OneMinusConstantColor: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    case RB_BlendFactorKind_ConstantAlpha:         return VK_BLEND_FACTOR_CONSTANT_ALPHA;
    case RB_BlendFactorKind_OneMinusConstantAlpha: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
    case RB_BlendFactorKind_SrcAlphaSaturate:      return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    case RB_BlendFactorKind_Src1Color:             return VK_BLEND_FACTOR_SRC1_COLOR;
    case RB_BlendFactorKind_OneMinusSrc1Color:     return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
    case RB_BlendFactorKind_Src1Alpha:             return VK_BLEND_FACTOR_SRC1_ALPHA;
    case RB_BlendFactorKind_OneMinusSrc1Alpha:     return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;

    }
}

internal VkSamplerMipmapMode
rb_vk_sampler_mipmap_mode_from_sampler_mipmap_filter_kind(RB_SamplerMipmapFilterKind sample_mipmap_mode)
{
    switch(sample_mipmap_mode){
    default: DOT_ERROR("undefined mip map mode %u", sample_mipmap_mode);
    case RB_SamplerMipmapFilterKind_Nearest: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    case RB_SamplerMipmapFilterKind_Linear:  return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

internal VkSamplerAddressMode
rb_vk_sampler_address_mode_from_sampler_address_mode_kind(RB_SamplerAddressModeKind address_mode)
{
    switch(address_mode){
    default: DOT_ERROR("undefined address mode %u", address_mode);
    case RB_SamplerAddressModeKind_Repeat:              return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case RB_SamplerAddressModeKind_MirroredRepeat:      return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case RB_SamplerAddressModeKind_ClampToEdge:         return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case RB_SamplerAddressModeKind_ClampToBorder:       return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    case RB_SamplerAddressModeKind_MirrorClampToEdge:   return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    }
}

internal VkPresentModeKHR
rb_vk_present_mode_from_present_mode_kind(RB_PresentModeKind present_mode)
{
    switch(present_mode){
    case RB_PresentModeKind_Immediate:      return VK_PRESENT_MODE_IMMEDIATE_KHR;
    case RB_PresentModeKind_Mailbox:        return VK_PRESENT_MODE_MAILBOX_KHR;
    case RB_PresentModeKind_Fifo:           return VK_PRESENT_MODE_FIFO_KHR;
    case RB_PresentModeKind_FifoRelaxed:    return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    // case RB_PresentModeKind_FIFO_Count:
    default: DOT_WARNING("Unsupported requested present mode %u, defaulting to "
                  "VK_PRESENT_MODE_FIFO_KHR", present_mode);
        return VK_PRESENT_MODE_FIFO_KHR;
    }
}

internal VkImageType
rb_vk_image_type_from_texture_dimension_kind(RB_TextureDimensionKind texture_dimension)
{
    switch(texture_dimension){
    case RB_TextureDimensionKind_1D         : return VK_IMAGE_TYPE_1D;
    case RB_TextureDimensionKind_2D         : return VK_IMAGE_TYPE_2D;
    case RB_TextureDimensionKind_3D         : return VK_IMAGE_TYPE_3D;
    case RB_TextureDimensionKind_Array2D    : return VK_IMAGE_TYPE_2D;
    case RB_TextureDimensionKind_Cube       : return VK_IMAGE_TYPE_2D;
    case RB_TextureDimensionKind_CubeArray  : return VK_IMAGE_TYPE_2D;
    }
    DOT_ERROR("Invalid texture dimension");
}

internal VkImageViewType
rb_vk_image_view_type_from_texture_dimension_kind(RB_TextureDimensionKind texture_dimension)
{
    switch(texture_dimension){
    case RB_TextureDimensionKind_1D         : return VK_IMAGE_VIEW_TYPE_1D;
    case RB_TextureDimensionKind_2D         : return VK_IMAGE_VIEW_TYPE_2D;
    case RB_TextureDimensionKind_3D         : return VK_IMAGE_VIEW_TYPE_3D;
    case RB_TextureDimensionKind_Array2D    : return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    case RB_TextureDimensionKind_Cube       : return VK_IMAGE_VIEW_TYPE_CUBE;
    case RB_TextureDimensionKind_CubeArray  : return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    }
    DOT_ERROR("Invalid texture dimension kind");
}

internal VkFormat
rb_vk_format_from_format_kind(RB_FormatKind texture_format)
{
    switch(texture_format){
    case RB_FormatKind_None          : return VK_FORMAT_UNDEFINED;
    // 8‑bit formats
    case RB_FormatKind_R8_UNORM      : return VK_FORMAT_R8_UNORM;
    case RB_FormatKind_R8_UINT       : return VK_FORMAT_R8_UINT;
    case RB_FormatKind_R8_SRGB       : return VK_FORMAT_R8_SRGB;
    case RB_FormatKind_RG8_UNORM     : return VK_FORMAT_R8G8_UNORM;
    case RB_FormatKind_RG8_SRGB      : return VK_FORMAT_R8G8_SRGB;
    case RB_FormatKind_RGBA4_UNORM   : return VK_FORMAT_R4G4B4A4_UNORM_PACK16;

    case RB_FormatKind_R5G5B5A1_UNORM: return VK_FORMAT_R5G5B5A1_UNORM_PACK16;
    case RB_FormatKind_R5G6B5_UNORM : return VK_FORMAT_R5G6B5_UNORM_PACK16;

    case RB_FormatKind_RGBA8_UNORM   : return VK_FORMAT_R8G8B8A8_UNORM;
    case RB_FormatKind_RGBA8_SRGB    : return VK_FORMAT_R8G8B8A8_SRGB;
    case RB_FormatKind_BGRA8_UNORM   : return VK_FORMAT_B8G8R8A8_UNORM;
    case RB_FormatKind_BGRA8_SRGB    : return VK_FORMAT_B8G8R8A8_SRGB;
    // UINT formats
    case RB_FormatKind_RG8_UINT      : return VK_FORMAT_R8G8_UINT;
    case RB_FormatKind_RGBA8_UINT    : return VK_FORMAT_R8G8B8A8_UINT;
    case RB_FormatKind_BGRA8_UINT    : return VK_FORMAT_B8G8R8A8_UINT;
    case RB_FormatKind_R16_UINT      : return VK_FORMAT_R16_UINT;
    case RB_FormatKind_RG16_UINT     : return VK_FORMAT_R16G16_UINT;
    case RB_FormatKind_RGBA16_UINT   : return VK_FORMAT_R16G16B16A16_UINT;
    case RB_FormatKind_R32_UINT      : return VK_FORMAT_R32_UINT;
    case RB_FormatKind_RG32_UINT     : return VK_FORMAT_R32G32_UINT;
    case RB_FormatKind_RGB32_UINT    : return VK_FORMAT_R32G32B32_UINT;
    case RB_FormatKind_RGBA32_UINT   : return VK_FORMAT_R32G32B32A32_UINT;
    // 16-bit UNORM formats
    case RB_FormatKind_R16_UNORM     : return VK_FORMAT_R16_UNORM;
    case RB_FormatKind_RG16_UNORM    : return VK_FORMAT_R16G16_UNORM;
    case RB_FormatKind_RGBA16_UNORM  : return VK_FORMAT_R16G16B16A16_UNORM;
    // HDR / Float formats
    case RB_FormatKind_R16_FLOAT     : return VK_FORMAT_R16_SFLOAT;
    case RB_FormatKind_RG16_FLOAT    : return VK_FORMAT_R16G16_SFLOAT;
    case RB_FormatKind_RGBA16_FLOAT  : return VK_FORMAT_R16G16B16A16_SFLOAT;
    case RB_FormatKind_R32_FLOAT     : return VK_FORMAT_R32_SFLOAT;
    case RB_FormatKind_RG32_FLOAT    : return VK_FORMAT_R32G32_SFLOAT;
    case RB_FormatKind_RGB32_FLOAT   : return VK_FORMAT_R32G32B32_SFLOAT;
    case RB_FormatKind_RGBA32_FLOAT  : return VK_FORMAT_R32G32B32A32_SFLOAT;
    // Special / packed formats
    case RB_FormatKind_RGB10A2_UNORM : return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case RB_FormatKind_RG11B10_FLOAT : return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    // Depth / Stencil formats
    case RB_FormatKind_D16_UNORM         : return VK_FORMAT_D16_UNORM;
    case RB_FormatKind_D24_UNORM_S8_UINT : return VK_FORMAT_D24_UNORM_S8_UINT;
    case RB_FormatKind_D32_FLOAT         : return VK_FORMAT_D32_SFLOAT;
    case RB_FormatKind_S8_UINT           : return VK_FORMAT_S8_UINT;
    case RB_FormatKind_D32_FLOAT_S8_UINT : return VK_FORMAT_D32_SFLOAT_S8_UINT;
    // Block‑compressed formats
    case RB_FormatKind_EAC_RG         : return VK_FORMAT_EAC_R11G11_UNORM_BLOCK;
    case RB_FormatKind_ASTC_4X4_SRGB  : return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
    case RB_FormatKind_ASTC_4X4_UNORM : return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
    case RB_FormatKind_BC3_UNORM      : return VK_FORMAT_BC3_UNORM_BLOCK;
    case RB_FormatKind_BC3_SRGB       : return VK_FORMAT_BC3_SRGB_BLOCK;
    case RB_FormatKind_BC5_RG         : return VK_FORMAT_BC5_UNORM_BLOCK;
    case RB_FormatKind_BC6H_UFLOAT    : return VK_FORMAT_BC6H_UFLOAT_BLOCK;
    case RB_FormatKind_BC6H_SFLOAT    : return VK_FORMAT_BC6H_SFLOAT_BLOCK;
    case RB_FormatKind_BC7_UNORM      : return VK_FORMAT_BC7_UNORM_BLOCK;
    case RB_FormatKind_BC7_SRGB       : return VK_FORMAT_BC7_SRGB_BLOCK;
    }
    DOT_ERROR("Invalid rn texture format kind");
}

internal VkCullModeFlags
rb_vk_cull_mode_flags_from_cull_mode_kind(RB_CullModeKind kind)
{
    switch(kind){
    case RB_CullModeKind_None:   return VK_CULL_MODE_NONE;
    case RB_CullModeKind_Front:  return VK_CULL_MODE_FRONT_BIT;
    case RB_CullModeKind_Back:   return VK_CULL_MODE_BACK_BIT;
    case RB_CullModeKind_Always: return VK_CULL_MODE_FRONT_AND_BACK;
    }
    DOT_WARNING("Invalid cull mode kind %u, returning VK_CULL_MODE_NONE", kind);
    return VK_CULL_MODE_NONE;
}

internal VkCompareOp
rb_vk_compare_op_from_compare_op_kind(RB_CompareOpKind kind)
{
    switch(kind){
    case RB_CompareOpKind_Always:         return VK_COMPARE_OP_ALWAYS;
    case RB_CompareOpKind_Never:          return VK_COMPARE_OP_NEVER;
    case RB_CompareOpKind_Less:           return VK_COMPARE_OP_LESS;
    case RB_CompareOpKind_Equal:          return VK_COMPARE_OP_EQUAL;
    case RB_CompareOpKind_LessOrEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
    case RB_CompareOpKind_Greater:        return VK_COMPARE_OP_GREATER;
    case RB_CompareOpKind_NotEqual:       return VK_COMPARE_OP_NOT_EQUAL;
    case RB_CompareOpKind_GreaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
    }
    DOT_WARNING("Invalid compare op %u , returning VK_COMPARE_OP_ALWAYS", kind);
    return VK_COMPARE_OP_ALWAYS;
}

internal VkStencilOp
rb_vk_stencil_op_from_stencil_op_kind(RB_StencilOpKind kind)
{
    switch(kind){
    case RB_StencilOpKind_Keep:              return VK_STENCIL_OP_KEEP;
    case RB_StencilOpKind_Zero:              return VK_STENCIL_OP_ZERO;
    case RB_StencilOpKind_Replace:           return VK_STENCIL_OP_REPLACE;
    case RB_StencilOpKind_IncrementAndClamp: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
    case RB_StencilOpKind_DecrementAndClamp: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
    case RB_StencilOpKind_Invert:            return VK_STENCIL_OP_INVERT;
    case RB_StencilOpKind_IncrementAndWrap:  return VK_STENCIL_OP_INCREMENT_AND_WRAP;
    case RB_StencilOpKind_DecrementAndWrap:  return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    }
    DOT_WARNING("Invalid stencil op %u, returning VK_STENCIL_OP_KEEP", kind);
    return VK_STENCIL_OP_KEEP;
}

////////////////////////////////////////////////////////////////
/// Vk misc helpers

internal void
rb_vk_image_transition(
    VkCommandBuffer cmd,
    VkImage image,
    VkImageAspectFlags aspect_mask,
    VkImageLayout current_layout,
    VkImageLayout new_layout)
{
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

internal VkMemoryRequirements2
rb_vk_image_memory_requirements(VkDevice vk_device, VkImage vk_image)
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

internal inline VKAPI_ATTR u32 VKAPI_CALL
rb_vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data)
{
    DOT_UNUSED(user_data);
    RB_DebugMessage msg = {
        .device_kind = RB_DeviceKind_Vk,
        .severity    = RB_DebugSeverity_Info,
        .id          = callback_data->messageIdNumber,
        .native      = cast(void*)callback_data,
    };
    if(message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT){           msg.severity = RB_DebugSeverity_Error;
    }else if(message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT){   msg.severity = RB_DebugSeverity_Warning; }

    if(message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT){     msg.categories |= RB_DebugCategoryBit_General; }
    if(message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT){  msg.categories |= RB_DebugCategoryBit_Validation; }
    if(message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT){ msg.categories |= RB_DebugCategoryBit_Performance; }

    if(callback_data->pMessageIdName){  msg.id_name = string8_from_cstring(cast(char*)callback_data->pMessageIdName); }
    if(callback_data->pMessage){        msg.text = string8_from_cstring(cast(char*)callback_data->pMessage); }
    rb_debug_message_emit(&msg);
    return VK_FALSE;
}

internal inline VKAPI_ATTR void VKAPI_CALL
rb_vk_resource_set_name(VkObjectType type, u64 handle, String8 name)
{
    if(!RB_VK_DEBUG_UTILS_ENABLE){ return; }

    Scratch s = scratch_begin(0,0);
    String8 tmp = string8_copy(s.arena, name);

    VkDebugUtilsObjectNameInfoEXT debug_utils_info = {0};
    debug_utils_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    debug_utils_info.objectType = type;
    debug_utils_info.objectHandle = handle;
    debug_utils_info.pObjectName = tmp.cstr;

    vkSetDebugUtilsObjectNameEXT(rb_vk_g_ctx->device.vk_device, &debug_utils_info);
    scratch_end(s);
}

internal void
rb_vk_assert_outside_suspended_pass(RB_VK_CommandBuffer *cmd)
{
    DOT_ASSERT(!cmd->in_render_pass, "Command recorded inside a render pass");
    DOT_ASSERT(!cmd->ends_suspending, "Command recorded between the segments of a suspended render pass");
    cmd->recorded_outside_pass = true;
}

internal VkImageAspectFlags
rb_vk_aspect_from_format(RB_FormatKind format)
{
    const RB_TextureFormatInfo format_info = rb_texture_format_info_from_format(format);
    VkImageAspectFlags aspect = 0;
    aspect |= DOT_BITS_MATCH(format_info.format_flags, RB_TextureFormatBit_Depth)   ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
    aspect |= DOT_BITS_MATCH(format_info.format_flags, RB_TextureFormatBit_Stencil) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
    return(aspect ? aspect : VK_IMAGE_ASPECT_COLOR_BIT);
}

internal VkImageUsageFlags
rb_vk_image_usage_from_texture_usage(RB_FormatKind format, RB_TextureUsageFlags usage)
{
    VkImageUsageFlags image_usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    image_usage |= DOT_BITS_MATCH(usage, RB_TextureUsageBit_Compute) ? VK_IMAGE_USAGE_STORAGE_BIT : 0;
    if(rb_vk_aspect_from_format(format) != VK_IMAGE_ASPECT_COLOR_BIT){
        image_usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }else{
        image_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        image_usage |= DOT_BITS_MATCH(usage, RB_TextureUsageBit_RenderTarget) ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0;
    }
    return(image_usage);
}

internal VkImageCreateInfo
rb_vk_image_create_info_from_texture_desc(RB_TextureDesc *desc)
{
    b8 is_cube = desc->dimension_kind == RB_TextureDimensionKind_Cube || desc->dimension_kind == RB_TextureDimensionKind_CubeArray;
    DOT_ASSERT(desc->array_layers >= 1, "Texture '%S' needs array_layers >= 1", desc->debug_name);
    DOT_ASSERT(desc->dimension_kind != RB_TextureDimensionKind_Cube || desc->array_layers == 6, "Cube texture '%S' needs array_layers == 6", desc->debug_name);
    DOT_ASSERT(!is_cube || desc->array_layers % 6 == 0, "Cube array texture '%S' needs array_layers to be a multiple of 6", desc->debug_name);
    DOT_ASSERT(!is_cube || desc->width == desc->height, "Cube texture '%S' needs square faces", desc->debug_name);
    DOT_ASSERT(desc->dimension_kind != RB_TextureDimensionKind_3D || desc->array_layers == 1, "3D texture '%S' can't have array layers", desc->debug_name);

    VkImageCreateInfo info = {0};
    info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.flags         = is_cube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
    info.imageType     = rb_vk_image_type_from_texture_dimension_kind(desc->dimension_kind);
    info.format        = rb_vk_format_from_format_kind(desc->format_kind);
    info.extent        = (VkExtent3D){desc->width, desc->height, desc->depth};
    info.mipLevels     = desc->mip_levels;
    info.arrayLayers   = desc->array_layers;
    info.samples       = VK_SAMPLE_COUNT_1_BIT;
    info.tiling        = VK_IMAGE_TILING_OPTIMAL;
    info.usage         = rb_vk_image_usage_from_texture_usage(desc->format_kind, desc->texture_usage_flags);
    info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    return(info);
}

internal RB_Texture
rb_vk_texture_create(RB_CommandBuffer cmd_h, RB_TextureDesc *desc, RB_TextureHeap heap, i64 offset)
{
    RB_VK_CommandBuffer *cmd = RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, cmd_h);
    DOT_ASSERT(desc, "Missing texture desc");

    RB_VK_Texture *texture = rb_vk_g_ctx->texture_free;
    if(texture){
        SLL_STACK_POP(rb_vk_g_ctx->texture_free);
        MEMORY_ZERO_STRUCT(texture);
    }else{
        texture = PUSH_STRUCT(rb_vk_g_ctx->permanent_arena, RB_VK_Texture);
    }
    VkImageCreateInfo image_create_info = rb_vk_image_create_info_from_texture_desc(desc);
    texture->vk_extent3d        = image_create_info.extent;
    texture->mip_levels         = desc->mip_levels;
    texture->array_layers       = desc->array_layers;
    texture->vk_format          = image_create_info.format;
    texture->vk_aspect          = rb_vk_aspect_from_format(desc->format_kind);
    texture->vk_usage           = image_create_info.usage;
    texture->vk_view_type       = rb_vk_image_view_type_from_texture_dimension_kind(desc->dimension_kind);
    DOT_DebugNameSet(texture->debug_name, desc->debug_name);

    VkDevice vk_device = rb_vk_g_ctx->device.vk_device;
    RB_VK_CHECK(vkCreateImage(vk_device, &image_create_info, NULL, &texture->vk_image));

    if(RB_HANDLE_VALID(heap)){
        RB_VK_CHECK(vkBindImageMemory(vk_device, texture->vk_image, RB_HANDLE_UNPACK(RB_VK_TextureHeap, heap), cast(u64)offset));
        texture->vk_memory = VK_NULL_HANDLE;
    }else{
        texture->vk_memory = rb_vk_image_memory_alloc(texture->vk_image);
    }
    rb_vk_resource_set_name(VK_OBJECT_TYPE_IMAGE, cast(u64)texture->vk_image, desc->debug_name);

    VkImageAspectFlags aspect_mask = texture->vk_aspect;
    if(aspect_mask & VK_IMAGE_ASPECT_DEPTH_BIT){ aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT; }

    RB_VK_CHECK(vkCreateImageView(
        vk_device,
        &(VkImageViewCreateInfo){
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = texture->vk_image,
            .viewType = rb_vk_image_view_type_from_texture_dimension_kind(desc->dimension_kind),
            .format = texture->vk_format,
            .subresourceRange = {
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = texture->array_layers,
                .aspectMask = aspect_mask,
            }
        },
        NULL,
        &texture->vk_image_view));
    rb_vk_resource_set_name(VK_OBJECT_TYPE_IMAGE_VIEW, cast(u64)texture->vk_image_view, desc->debug_name);

    rb_vk_assert_outside_suspended_pass(cmd);
    rb_vk_image_transition(cmd->vk_cmd, texture->vk_image, texture->vk_aspect, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    RB_Texture h = RB_HANDLE_PACK(RB_Texture, texture);
    return(h);
}

internal RB_VK_DescriptorHeapLayout
rb_vk_descriptor_heap_layout(RB_MemoryKind kind)
{
    VkPhysicalDeviceDescriptorHeapPropertiesEXT *p = &rb_vk_g_ctx->heap_properties;
    RB_VK_DescriptorHeapLayout layout = {0};
    if(kind == RB_MemoryKind_TextureDescriptorHeap){
        layout.descriptor_size    = p->imageDescriptorSize;
        layout.reserved_alignment = DOT_MAX(p->imageDescriptorAlignment, p->bufferDescriptorAlignment);
        layout.reserved_size      = p->minResourceHeapReservedRange;
        layout.heap_alignment     = p->resourceHeapAlignment;
    }else{
        DOT_ASSERT(kind == RB_MemoryKind_SamplerDescriptorHeap, "Not a descriptor heap kind (%d)", kind);
        layout.descriptor_size    = p->samplerDescriptorSize;
        layout.reserved_alignment = p->samplerDescriptorAlignment;
        layout.reserved_size      = p->minSamplerHeapReservedRange;
        layout.heap_alignment     = p->samplerHeapAlignment;
    }
    return(layout);
}

internal VkHostAddressRangeEXT
rb_vk_descriptor_heap_slot(RB_GpuHeap heap_h, RB_MemoryKind kind, u32 index)
{
    RB_VK_GpuHeap *heap = RB_HANDLE_UNPACK(RB_VK_GpuHeap*, heap_h);
    DOT_ASSERT(heap && heap->kind == kind, "Descriptor written into a heap of the wrong memory kind");
    u64 descriptor_size = rb_vk_descriptor_heap_layout(kind).descriptor_size;
    u64 offset = cast(u64) index * descriptor_size;
    DOT_ASSERT(offset + descriptor_size <= cast(u64) heap->size, "Descriptor index %u out of range", index);
    VkHostAddressRangeEXT range = {0};
    range.address = cast(u8*) heap->cpu_base + offset;
    range.size    = descriptor_size;
    return(range);
}

internal void
rb_vk_texture_descriptor_write(RB_GpuHeap heap, u32 index, RB_Texture texture_h, RB_TextureDescriptorKind kind)
{
    RB_VK_Texture *texture = RB_HANDLE_UNPACK(RB_VK_Texture*, texture_h);
    b32 storage = kind == RB_TextureDescriptorKind_Storage;
    DOT_ASSERT(!storage || (texture->vk_usage & VK_IMAGE_USAGE_STORAGE_BIT), "Texture '%s' needs RB_TextureUsageBit_Compute to be a storage texture", texture->debug_name.buff);

    VkImageAspectFlags aspect = texture->vk_aspect;
    if(aspect & VK_IMAGE_ASPECT_DEPTH_BIT){ aspect = VK_IMAGE_ASPECT_DEPTH_BIT; }

    VkImageViewUsageCreateInfo view_usage = {0};
    view_usage.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO;
    view_usage.usage = storage ? VK_IMAGE_USAGE_STORAGE_BIT : VK_IMAGE_USAGE_SAMPLED_BIT;

    VkImageViewCreateInfo view_info = {0};
    view_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.pNext                           = &view_usage;
    view_info.image                           = texture->vk_image;
    view_info.viewType                        = texture->vk_view_type;
    view_info.format                          = texture->vk_format;
    view_info.subresourceRange.aspectMask     = aspect;
    view_info.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
    view_info.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

    VkImageDescriptorInfoEXT image_info = {0};
    image_info.sType  = VK_STRUCTURE_TYPE_IMAGE_DESCRIPTOR_INFO_EXT;
    image_info.pView  = &view_info;
    image_info.layout = VK_IMAGE_LAYOUT_GENERAL;

    VkResourceDescriptorInfoEXT descriptor_info = {0};
    descriptor_info.sType       = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT;
    descriptor_info.type        = storage ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptor_info.data.pImage = &image_info;

    VkHostAddressRangeEXT destination = rb_vk_descriptor_heap_slot(heap, RB_MemoryKind_TextureDescriptorHeap, index);
    RB_VK_CHECK(vkWriteResourceDescriptorsEXT(rb_vk_g_ctx->device.vk_device, 1, &descriptor_info, &destination));
}

internal void
rb_vk_sampler_descriptor_write(RB_GpuHeap heap, u32 index, RB_SamplerDesc *desc)
{
    VkSamplerCreateInfo sampler_info = {0};
    sampler_info.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.minFilter    = rb_vk_filter_from_sampler_filter_kind(desc->min_filter);
    sampler_info.magFilter    = rb_vk_filter_from_sampler_filter_kind(desc->mag_filter);
    sampler_info.mipmapMode   = rb_vk_sampler_mipmap_mode_from_sampler_mipmap_filter_kind(desc->mipmap_filter);
    sampler_info.addressModeU = rb_vk_sampler_address_mode_from_sampler_address_mode_kind(desc->address_mode_u);
    sampler_info.addressModeV = rb_vk_sampler_address_mode_from_sampler_address_mode_kind(desc->address_mode_v);
    sampler_info.addressModeW = rb_vk_sampler_address_mode_from_sampler_address_mode_kind(desc->address_mode_w);
    sampler_info.borderColor  = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    sampler_info.maxLod       = VK_LOD_CLAMP_NONE;

    VkHostAddressRangeEXT destination = rb_vk_descriptor_heap_slot(heap, RB_MemoryKind_SamplerDescriptorHeap, index);
    RB_VK_CHECK(vkWriteSamplerDescriptorsEXT(rb_vk_g_ctx->device.vk_device, 1, &sampler_info, &destination));
}

internal VkBindHeapInfoEXT
rb_vk_descriptor_heap_bind_info(RB_GpuHeap heap_h, RB_MemoryKind kind)
{
    RB_VK_GpuHeap *heap = RB_HANDLE_UNPACK(RB_VK_GpuHeap*, heap_h);
    DOT_ASSERT(heap && heap->kind == kind, "Bound a heap of the wrong memory kind as a descriptor heap");
    RB_VK_DescriptorHeapLayout layout = rb_vk_descriptor_heap_layout(kind);
    u64 reserved_offset = ALIGN_POW2(cast(u64) heap->size, layout.reserved_alignment);

    VkBindHeapInfoEXT info = {0};
    info.sType               = VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT;
    info.heapRange.address   = heap->gpu_base;
    info.heapRange.size      = reserved_offset + layout.reserved_size;
    info.reservedRangeOffset = reserved_offset;
    info.reservedRangeSize   = layout.reserved_size;
    return(info);
}

internal void
rb_vk_texture_descriptor_heap_set(RB_CommandBuffer cmd_h, RB_GpuHeap heap)
{
    RB_VK_CommandBuffer *cmd = RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, cmd_h);
    VkBindHeapInfoEXT info = rb_vk_descriptor_heap_bind_info(heap, RB_MemoryKind_TextureDescriptorHeap);
    vkCmdBindResourceHeapEXT(cmd->vk_cmd, &info);
}

internal void
rb_vk_sampler_descriptor_heap_set(RB_CommandBuffer cmd_h, RB_GpuHeap heap)
{
    RB_VK_CommandBuffer *cmd = RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, cmd_h);
    VkBindHeapInfoEXT info = rb_vk_descriptor_heap_bind_info(heap, RB_MemoryKind_SamplerDescriptorHeap);
    vkCmdBindSamplerHeapEXT(cmd->vk_cmd, &info);
}

internal VkShaderModule
rb_vk_shader_module_create(String8 spirv)
{
    VkShaderModule module = VK_NULL_HANDLE;
    RB_VK_CHECK(vkCreateShaderModule(rb_vk_g_ctx->device.vk_device,
        &(VkShaderModuleCreateInfo){
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv.size,
            .pCode    = cast(u32*)spirv.str,
        }, NULL, &module));
    return(module);
}

internal RB_VK_Pipeline *
rb_vk_pipeline_alloc(void)
{
    RB_VK_Pipeline *pipeline = rb_vk_g_ctx->pipeline_free;
    if(pipeline){
        SLL_STACK_POP(rb_vk_g_ctx->pipeline_free);
        MEMORY_ZERO_STRUCT(pipeline);
    }else{
        pipeline = PUSH_STRUCT(rb_vk_g_ctx->permanent_arena, RB_VK_Pipeline);
    }
    return(pipeline);
}

internal VkPipeline
rb_vk_pipeline_raster_create(VkPipelineShaderStageCreateInfo *stages, u32 stage_count, b32 mesh, RB_RasterDesc *raster)
{
    VkFormat color_formats[RB_IMAGE_OUTPUTS_MAX];
    u32 color_target_count = 0;
    for(; color_target_count < RB_IMAGE_OUTPUTS_MAX && raster->color_targets[color_target_count].format != RB_FormatKind_None;  ++color_target_count){
        color_formats[color_target_count] = rb_vk_format_from_format_kind(raster->color_targets[color_target_count].format);
    }
    b32 has_depth_or_stencil = raster->depth_format != RB_FormatKind_None || raster->stencil_format != RB_FormatKind_None;
    DOT_ASSERT(color_target_count > 0 || has_depth_or_stencil, "Must provide a color target or a depth/stencil format");

    VkFormat depth_format = raster->depth_format == RB_FormatKind_None
        ? VK_FORMAT_UNDEFINED : rb_vk_format_from_format_kind(raster->depth_format);
    VkFormat stencil_format = raster->stencil_format == RB_FormatKind_None
        ? VK_FORMAT_UNDEFINED : rb_vk_format_from_format_kind(raster->stencil_format);

    VkPipelineRenderingCreateInfo rendering_info = {0};
    rendering_info.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering_info.colorAttachmentCount    = color_target_count;
    rendering_info.pColorAttachmentFormats = color_formats;
    rendering_info.depthAttachmentFormat   = depth_format;
    rendering_info.stencilAttachmentFormat = stencil_format;

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {0};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    switch(raster->raster_state.topology){
    case RB_PrimitiveTopologyKind_TriangleList:  topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
    case RB_PrimitiveTopologyKind_TriangleStrip: topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
    case RB_PrimitiveTopologyKind_LineList:      topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
    case RB_PrimitiveTopologyKind_PointList:     topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
    }
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {0};
    input_assembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = topology;

    VkPipelineRasterizationStateCreateInfo rasterizer = {0};
    rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = raster->raster_state.fill_mode == RB_FillModeKind_Wireframe ? VK_POLYGON_MODE_LINE
        : raster->raster_state.fill_mode == RB_FillModeKind_Point ? VK_POLYGON_MODE_POINT : VK_POLYGON_MODE_FILL;
    rasterizer.cullMode    = rb_vk_cull_mode_flags_from_cull_mode_kind(raster->raster_state.cull_mode);

    rasterizer.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample = {0};
    multisample.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depth_stencil = {0};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    VkPipelineColorBlendAttachmentState vk_blend_attachments[RB_IMAGE_OUTPUTS_MAX] = {0};
    for(u32 i = 0; i < color_target_count; ++i){
        RB_BlendState *bs = &raster->color_targets[i].blend;
        VkPipelineColorBlendAttachmentState *att = &vk_blend_attachments[i];
        att->colorWriteMask = VK_COLOR_COMPONENT_R_BIT
            | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT
            | VK_COLOR_COMPONENT_A_BIT;

        att->blendEnable = bs->blend_enabled ? VK_TRUE : VK_FALSE;
        att->srcColorBlendFactor = rb_vk_blend_factor_from_blend_factor_kind(bs->src_color);
        att->dstColorBlendFactor = rb_vk_blend_factor_from_blend_factor_kind(bs->dst_color);
        att->colorBlendOp        = rb_vk_blend_op_from_blend_op_kind(bs->blend_op);
        if(bs->separate_blend){
            att->srcAlphaBlendFactor = rb_vk_blend_factor_from_blend_factor_kind(bs->src_alpha);
            att->dstAlphaBlendFactor = rb_vk_blend_factor_from_blend_factor_kind(bs->dst_alpha);
            att->alphaBlendOp        = rb_vk_blend_op_from_blend_op_kind(bs->alpha_blend_op);
        }else{
            att->srcAlphaBlendFactor = att->srcColorBlendFactor;
            att->dstAlphaBlendFactor = att->dstColorBlendFactor;
            att->alphaBlendOp        = att->colorBlendOp;
        }
    }

    VkPipelineColorBlendStateCreateInfo color_blend = {0};
    color_blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend.attachmentCount = color_target_count;
    color_blend.pAttachments    = vk_blend_attachments;

    VkPipelineViewportStateCreateInfo viewport_state = {0};
    viewport_state.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount  = 1;

    VkDynamicState dyn_states[10];
    u32 dyn_state_count = 0;
    dyn_states[dyn_state_count++] = VK_DYNAMIC_STATE_VIEWPORT;
    dyn_states[dyn_state_count++] = VK_DYNAMIC_STATE_SCISSOR;
    if(has_depth_or_stencil){
        dyn_states[dyn_state_count++] = VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE;
        dyn_states[dyn_state_count++] = VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE;
        dyn_states[dyn_state_count++] = VK_DYNAMIC_STATE_DEPTH_COMPARE_OP;
        dyn_states[dyn_state_count++] = VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE;
        dyn_states[dyn_state_count++] = VK_DYNAMIC_STATE_STENCIL_OP;
        dyn_states[dyn_state_count++] = VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK;
        dyn_states[dyn_state_count++] = VK_DYNAMIC_STATE_STENCIL_WRITE_MASK;
        dyn_states[dyn_state_count++] = VK_DYNAMIC_STATE_STENCIL_REFERENCE;
    }
    VkPipelineDynamicStateCreateInfo dyn_state = {0};
    dyn_state.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn_state.dynamicStateCount = dyn_state_count;
    dyn_state.pDynamicStates    = dyn_states;

    VkPipelineCreateFlags2CreateInfo flags_info = {0};
    flags_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO;
    flags_info.pNext = &rendering_info;
    flags_info.flags = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    VkGraphicsPipelineCreateInfo pipeline_info = {0};
    pipeline_info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.pNext               = &flags_info;
    pipeline_info.stageCount          = stage_count;
    pipeline_info.pStages             = stages;
    pipeline_info.pVertexInputState   = mesh ? NULL : &vertex_input_info;
    pipeline_info.pInputAssemblyState = mesh ? NULL : &input_assembly;
    pipeline_info.pViewportState      = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState   = &multisample;
    pipeline_info.pDepthStencilState  = has_depth_or_stencil ? &depth_stencil : NULL;
    pipeline_info.pColorBlendState    = &color_blend;
    pipeline_info.pDynamicState       = &dyn_state;

    VkPipeline vk_pipeline = VK_NULL_HANDLE;
    RB_VK_CHECK(vkCreateGraphicsPipelines(rb_vk_g_ctx->device.vk_device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &vk_pipeline));
    rb_vk_resource_set_name(VK_OBJECT_TYPE_PIPELINE, cast(u64)vk_pipeline, raster->debug_name);
    return(vk_pipeline);
}

internal VkPipelineShaderStageCreateInfo
rb_vk_shader_stage_info(VkShaderStageFlagBits stage, VkShaderModule module)
{
    VkPipelineShaderStageCreateInfo info = {0};
    info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage  = stage;
    info.module = module;
    info.pName  = "main";
    return(info);
}

internal RB_Pipeline
rb_vk_pipeline_graphics_create(String8 vs_spirv, String8 fs_spirv, RB_RasterDesc *raster)
{
    RB_VK_Pipeline *pipeline = rb_vk_pipeline_alloc();
    pipeline->vk_bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;

    VkShaderModule modules[2] = {0};
    VkPipelineShaderStageCreateInfo stages[2];
    u32 stage_count = 0;
    modules[stage_count] = rb_vk_shader_module_create(vs_spirv);
    stages[stage_count]  = rb_vk_shader_stage_info(VK_SHADER_STAGE_VERTEX_BIT, modules[stage_count]); ++stage_count;
    if(fs_spirv.size){
        modules[stage_count] = rb_vk_shader_module_create(fs_spirv);
        stages[stage_count]  = rb_vk_shader_stage_info(VK_SHADER_STAGE_FRAGMENT_BIT, modules[stage_count]); ++stage_count;
    }

    pipeline->vk_pipeline = rb_vk_pipeline_raster_create(stages, stage_count, false, raster);

    for(u32 i = 0; i < stage_count; ++i){
        vkDestroyShaderModule(rb_vk_g_ctx->device.vk_device, modules[i], NULL);
    }
    RB_Pipeline h = RB_HANDLE_PACK(RB_Pipeline, pipeline);
    return(h);
}

internal RB_Pipeline
rb_vk_pipeline_mesh_create(String8 ts_spirv, String8 ms_spirv, String8 fs_spirv, RB_RasterDesc *raster)
{
    DOT_ASSERT(rb_vk_g_ctx->device.mesh_shaders, "Mesh pipelines need VK_EXT_mesh_shader, which this GPU doesn't expose");
    RB_VK_Pipeline *pipeline = rb_vk_pipeline_alloc();
    pipeline->vk_bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;

    VkShaderModule modules[3] = {0};
    VkPipelineShaderStageCreateInfo stages[3];
    u32 stage_count = 0;
    if(ts_spirv.size){
        modules[stage_count] = rb_vk_shader_module_create(ts_spirv);
        stages[stage_count]  = rb_vk_shader_stage_info(VK_SHADER_STAGE_TASK_BIT_EXT, modules[stage_count]); ++stage_count;
    }
    modules[stage_count] = rb_vk_shader_module_create(ms_spirv);
    stages[stage_count]  = rb_vk_shader_stage_info(VK_SHADER_STAGE_MESH_BIT_EXT, modules[stage_count]); ++stage_count;
    if(fs_spirv.size){
        modules[stage_count] = rb_vk_shader_module_create(fs_spirv);
        stages[stage_count]  = rb_vk_shader_stage_info(VK_SHADER_STAGE_FRAGMENT_BIT, modules[stage_count]); ++stage_count;
    }

    pipeline->vk_pipeline = rb_vk_pipeline_raster_create(stages, stage_count, true, raster);

    for(u32 i = 0; i < stage_count; ++i){
        vkDestroyShaderModule(rb_vk_g_ctx->device.vk_device, modules[i], NULL);
    }
    RB_Pipeline h = RB_HANDLE_PACK(RB_Pipeline, pipeline);
    return(h);
}

internal RB_Pipeline
rb_vk_pipeline_compute_create(String8 cs_spirv)
{
    RB_VK_Pipeline *pipeline = rb_vk_pipeline_alloc();
    pipeline->vk_bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;

    VkShaderModule module = rb_vk_shader_module_create(cs_spirv);
    VkPipelineCreateFlags2CreateInfo flags_info = {0};
    flags_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO;
    flags_info.flags = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    VkComputePipelineCreateInfo pipeline_info = {0};
    pipeline_info.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info.pNext  = &flags_info;
    pipeline_info.stage  = rb_vk_shader_stage_info(VK_SHADER_STAGE_COMPUTE_BIT, module);
    RB_VK_CHECK(vkCreateComputePipelines(rb_vk_g_ctx->device.vk_device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline->vk_pipeline));
    vkDestroyShaderModule(rb_vk_g_ctx->device.vk_device, module, NULL);
    RB_Pipeline h = RB_HANDLE_PACK(RB_Pipeline, pipeline);
    return(h);
}

internal void
rb_vk_pipeline_destroy(RB_Pipeline handle)
{
    RB_VK_Pipeline *pipeline = RB_HANDLE_UNPACK(RB_VK_Pipeline*, handle);
    if(!pipeline){ return; }
    vkDestroyPipeline(rb_vk_g_ctx->device.vk_device, pipeline->vk_pipeline, NULL);
    SLL_STACK_PUSH(rb_vk_g_ctx->pipeline_free, pipeline);
}

internal void
rb_vk_texture_destroy(RB_Texture handle)
{
    if(!RB_HANDLE_VALID(handle)){ return; }
    RB_VK_Texture *texture = RB_HANDLE_UNPACK(RB_VK_Texture*, handle);

    VkDevice device = rb_vk_g_ctx->device.vk_device;
    vkDestroyImageView(device, texture->vk_image_view, NULL);
    vkDestroyImage(device, texture->vk_image, NULL);

    if(texture->vk_memory != VK_NULL_HANDLE){
        vkFreeMemory(device, texture->vk_memory, NULL);
    }
    SLL_STACK_PUSH(rb_vk_g_ctx->texture_free, texture);
}

internal void
rb_vk_swapchain_images_destroy(void)
{
    VkDevice device = rb_vk_g_ctx->device.vk_device;
    RB_VK_Swapchain *swapchain = &rb_vk_g_ctx->swapchain;
    for(u32 i = 0; i < swapchain->swapchain_image_count; ++i){
        RB_VK_SwapchainImage *swapchain_image = &swapchain->swapchain_images[i];
        vkDestroyImageView(device, swapchain_image->vk_image_view, NULL);
    }
}

internal void
rb_vk_swapchain_create(void)
{
    Scratch s = scratch_begin(0,0);
    VkDevice device = rb_vk_g_ctx->device.vk_device;
    RB_VK_Swapchain *swapchain = &rb_vk_g_ctx->swapchain;
    VkSwapchainKHR old_swapchain = swapchain->swapchain;

    RB_VK_SwapchainDetails details = {
        .preferred_format = rb_vk_format_from_format_kind(rb_vk_g_ctx->desc.swapchain_format),
        .preferred_present_mode = rb_vk_present_mode_from_present_mode_kind(rb_vk_g_ctx->desc.present_mode),
        .frame_buffer_size = rb_vk_g_ctx->drawable_extent_requested,
    };
    rb_vk_physical_device_swapchain_support(rb_vk_g_ctx->device.vk_gpu, rb_vk_g_ctx->surface, &details);

    swapchain->extent = details.surface_extent;
    swapchain->image_format = details.best_surface_format.format;

    // (jd) NOTE: Window minimized/zero-sized; keep the old swapchain around, we'll retry next frame
    if(swapchain->extent.width > 0 || swapchain->extent.height > 0){
        VkSwapchainCreateInfoKHR swapchain_create_info = {0};
        swapchain_create_info.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_create_info.surface          = rb_vk_g_ctx->surface;
        swapchain_create_info.minImageCount    = details.image_count;
        swapchain_create_info.imageFormat      = details.best_surface_format.format;
        swapchain_create_info.imageColorSpace  = details.best_surface_format.colorSpace;
        swapchain_create_info.imageExtent      = details.surface_extent;
        swapchain_create_info.preTransform     = details.current_transform;
        swapchain_create_info.presentMode      = details.best_present_mode;
        swapchain_create_info.clipped          = VK_TRUE;
        swapchain_create_info.imageArrayLayers = 1;
        swapchain_create_info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        swapchain_create_info.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_create_info.oldSwapchain     = old_swapchain;

        if(rb_vk_g_ctx->device.graphics_queue_idx != rb_vk_g_ctx->device.present_queue_idx){
            DOT_ERROR("Different present and graphics queues unsupported");
            // u32 queues[] = {rb_vk_g_ctx->device.graphics_queue_idx, rb_vk_g_ctx->device.present_queue_idx};
            // swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            // swapchain_create_info.pQueueFamilyIndices = queues;
            // swapchain_create_info.queueFamilyIndexCount = DOT_ARRAY_COUNT(queues);
        }
        RB_VK_CHECK(vkCreateSwapchainKHR(device, &swapchain_create_info, NULL, &swapchain->swapchain));

        if(old_swapchain != VK_NULL_HANDLE){
            rb_vk_swapchain_images_destroy();
            vkDestroySwapchainKHR(device, old_swapchain, NULL);
            ARENA_RESET(rb_vk_g_ctx->swapchain_arena);
        }

        //  --- Create Image Datas ---
        // (jd) NOTE: Since vk expects a VkImage array we need to s alloc this and copy it over
        vkGetSwapchainImagesKHR(device, swapchain->swapchain, &swapchain->swapchain_image_count, NULL);
        VkImage *vk_swapchain_images    = PUSH_ARRAY(s.arena, VkImage, swapchain->swapchain_image_count);
        swapchain->swapchain_images     = PUSH_ARRAY(rb_vk_g_ctx->swapchain_arena, RB_VK_SwapchainImage, swapchain->swapchain_image_count);
        swapchain->render_views         = PUSH_ARRAY(rb_vk_g_ctx->swapchain_arena, RB_VK_RenderView, swapchain->swapchain_image_count);
        vkGetSwapchainImagesKHR(device, swapchain->swapchain, &swapchain->swapchain_image_count, vk_swapchain_images);

        RB_VK_SwapchainImage *swapchain_images = swapchain->swapchain_images;
        for(u32 i = 0; i < swapchain->swapchain_image_count; ++i){
            swapchain_images[i].vk_image = vk_swapchain_images[i];
            RB_VK_CHECK(vkCreateImageView(device,
                &(VkImageViewCreateInfo) {
                    .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .image      = swapchain_images[i].vk_image,
                    .viewType   = VK_IMAGE_VIEW_TYPE_2D,
                    .format     = swapchain->image_format,
                    .components = {
                        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                    },
                    .subresourceRange = {
                        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel   = 0,
                        .levelCount     = 1,
                        .baseArrayLayer = 0,
                        .layerCount     = 1,
                    },
                },
                NULL,
                &swapchain_images[i].vk_image_view
            ));

            RB_VK_RenderView *view = &swapchain->render_views[i];
            view->vk_image        = swapchain_images[i].vk_image;
            view->vk_image_view   = swapchain_images[i].vk_image_view;
            view->vk_format       = swapchain->image_format;
            view->vk_aspect       = VK_IMAGE_ASPECT_COLOR_BIT;
            view->extent          = swapchain->extent;
            view->swapchain_image = &swapchain_images[i];
        }
    }

    scratch_end(s);
}

internal void
rb_vk_present_contexts_wait(void)
{
    for(u32 i = 0; i < rb_vk_g_ctx->present_context_count; ++i){
        RB_VK_CHECK(vkWaitForFences(rb_vk_g_ctx->device.vk_device, 1, &rb_vk_g_ctx->present_contexts[i].presented, VK_TRUE, U64_MAX));
    }
}

internal void
rb_vk_swapchain_recreate(void)
{
    rb_vk_present_contexts_wait();
    rb_vk_swapchain_create();
    rb_vk_g_ctx->swapchain.recreate_required = false;
}

internal VkExtent2D
rb_vk_surface_extent(void)
{
    VkSurfaceCapabilitiesKHR caps = {0};
    RB_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(rb_vk_g_ctx->device.vk_gpu, rb_vk_g_ctx->surface, &caps));
    if(caps.currentExtent.width != U32_MAX){
        return(caps.currentExtent);
    }
    i32x2 requested = rb_vk_g_ctx->drawable_extent_requested;
    if(requested.x <= 0 || requested.y <= 0){
        return((VkExtent2D){0});
    }
    VkExtent2D extent = {
        DOT_CLAMP(cast(u32) requested.x, caps.minImageExtent.width,  caps.maxImageExtent.width),
        DOT_CLAMP(cast(u32) requested.y, caps.minImageExtent.height, caps.maxImageExtent.height),
    };
    return(extent);
}

internal const char *
rb_vk_surface_extension_name(PlatformWindowSystemKind kind)
{
    partial_switch_begin(kind){
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    case PlatformWindowSystemKind_Win32:   return(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif
#if defined(VK_USE_PLATFORM_XLIB_KHR)
    case PlatformWindowSystemKind_Xlib:    return(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
    case PlatformWindowSystemKind_Wayland: return(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#endif
    }partial_switch_end;
    DOT_ERROR("No Vulkan surface support for window system %u", kind);
}

internal VkResult
rb_vk_surface_create(PlatformNativeWindow window, VkSurfaceKHR *surface)
{
    VkInstance instance = rb_vk_g_ctx->instance;
    partial_switch_begin(window.kind){
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    case PlatformWindowSystemKind_Win32:{
        VkWin32SurfaceCreateInfoKHR info = {0};
        info.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        info.hinstance = GetModuleHandleW(NULL);
        info.hwnd      = cast(HWND) window.window;
        return(vkCreateWin32SurfaceKHR(instance, &info, NULL, surface));
    }
#endif
#if defined(VK_USE_PLATFORM_XLIB_KHR)
    case PlatformWindowSystemKind_Xlib:{
        VkXlibSurfaceCreateInfoKHR info = {0};
        info.sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        info.dpy    = cast(Display*) window.display;
        info.window = cast(Window) cast(uintptr_t) window.window;
        return(vkCreateXlibSurfaceKHR(instance, &info, NULL, surface));
    }
#endif
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
    case PlatformWindowSystemKind_Wayland:{
        VkWaylandSurfaceCreateInfoKHR info = {0};
        info.sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        info.display = cast(struct wl_display*) window.display;
        info.surface = cast(struct wl_surface*) window.window;
        return(vkCreateWaylandSurfaceKHR(instance, &info, NULL, surface));
    }
#endif
    }partial_switch_end;
    DOT_ERROR("No Vulkan surface support for window system %u", window.kind);
}

internal void
rb_vk_drawable_extent_set(i32x2 extent)
{
    rb_vk_g_ctx->drawable_extent_requested = extent;
}

internal void
rb_vk_init(Arena *arena, PlatformNativeWindow window, RB_DeviceDesc const *desc)
{
    Scratch s = scratch_begin(0,0);
    Arena *backend_arena = ARENA_CREATE(
        .parent = arena,
        .reserve_size = desc->permanent_memory_size);

    rb_vk_g_ctx = PUSH_STRUCT(backend_arena, RB_VK_BackendCtx);
    rb_vk_g_ctx->permanent_arena = backend_arena;
    rb_vk_g_ctx->native_window = window;
    rb_vk_g_ctx->desc = *desc;
    rb_vk_g_ctx->transient_arena = ARENA_CREATE(
        .parent = arena,
        .reserve_size = desc->transient_memory_size,);
    // (jd) This is all super messy and error prone. Cleanup
    Arena *ctx_arena = rb_vk_g_ctx->permanent_arena;
    rb_vk_g_ctx->swapchain_arena = ARENA_CREATE(.parent = ctx_arena, .reserve_size = DOT_KB(16), .name = "Vk Swapchain");

    volkInitialize();
    // rb_vk_g_ctx->vk_allocator = VkAllocatorParams(ctx_arena);
    u32 validation_layer_count = rb_vk_g_config.validation_layer_count;
    if(!rb_vk_all_layers(&rb_vk_g_config)){
        DOT_WARNING("Could not find all requested layers, continuing without validation layers");
        validation_layer_count = 0;
    }

    if(!rb_vk_instance_all_required_extensions(&rb_vk_g_config)){
        DOT_ERROR("Could not find all requested instance extensions");
    }

    VkDebugUtilsMessengerCreateInfoEXT debug_utils_info = {0};
    VkDebugUtilsMessengerCreateInfoEXT *debug_utils_info_ptr = NULL;
    if(RB_VK_DEBUG_UTILS_ENABLE){
        debug_utils_info = (VkDebugUtilsMessengerCreateInfoEXT){
            .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,

            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,

            .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = rb_vk_debug_callback,
        };
        debug_utils_info_ptr = &debug_utils_info;
    }
    u32 instance_extension_count = rb_vk_g_config.instance.extension_count;
    const char **instance_extensions = PUSH_ARRAY(s.arena, const char*, instance_extension_count + 1);
    for(u32 i = 0; i < instance_extension_count; ++i){
        instance_extensions[i] = rb_vk_g_config.instance.extensions[i].cstr;
    }
    instance_extensions[instance_extension_count++] = rb_vk_surface_extension_name(window.kind);
    RB_VK_CHECK(vkCreateInstance(
        &(VkInstanceCreateInfo)
        {
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,

            .enabledLayerCount      = validation_layer_count,
            .ppEnabledLayerNames    = cstr_array_from_string8_array(s.arena,
                validation_layer_count,
                rb_vk_g_config.validation_layers),

            .enabledExtensionCount      = instance_extension_count,
            .ppEnabledExtensionNames    = instance_extensions,

            .pApplicationInfo = &(VkApplicationInfo)
            {
                .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pApplicationName   = rb_vk_g_config.instance.application_name.cstr,
                .applicationVersion = rb_vk_g_config.instance.application_version,
                .pEngineName        = rb_vk_g_config.instance.engine_name.cstr,
                .engineVersion      = rb_vk_g_config.instance.engine_version,
                .apiVersion         = rb_vk_g_config.instance.api_version,
            },
            .pNext = debug_utils_info_ptr,
        },
        NULL, &rb_vk_g_ctx->instance));

    volkLoadInstance(rb_vk_g_ctx->instance);

    if(debug_utils_info_ptr){
        if(vkCreateDebugUtilsMessengerEXT){
            RB_VK_CHECK(vkCreateDebugUtilsMessengerEXT(rb_vk_g_ctx->instance, debug_utils_info_ptr, NULL, &rb_vk_g_ctx->debug_messenger));
        }
    }

    // --- Create Surface ---
    RB_VK_CHECK(rb_vk_surface_create(window, &rb_vk_g_ctx->surface));
    // --- Create Device ---
    {
        RB_VK_Device *device = &rb_vk_g_ctx->device;
        *device = rb_vk_pick_best_device(&rb_vk_g_config, rb_vk_g_ctx->instance, rb_vk_g_ctx->surface);
        device->device_properties = PUSH_STRUCT(arena, VkPhysicalDeviceProperties);

        vkGetPhysicalDeviceProperties(device->vk_gpu, device->device_properties);
        DOT_PRINT("Vulkan version: %u.%u.%u",
                  VK_VERSION_MAJOR(device->device_properties->apiVersion),
                  VK_VERSION_MINOR(device->device_properties->apiVersion),
                  VK_VERSION_PATCH(device->device_properties->apiVersion));

        b32 shared_present_graphics_queues = device->graphics_queue_idx == device->present_queue_idx;

        VkDeviceQueueCreateInfo queue_infos[2];
        u32 queue_count = 0;
        const float priority = 1.0f;
        queue_infos[queue_count++] = (VkDeviceQueueCreateInfo) {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = device->graphics_queue_idx,
            .queueCount       = 1,
            .pQueuePriorities = &priority,
        };

        if(!shared_present_graphics_queues){
            queue_infos[queue_count++] = (VkDeviceQueueCreateInfo) {
                .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = device->present_queue_idx,
                .queueCount       = 1,
                .pQueuePriorities = &priority,
            };
        }

        b8 feature_enabled[RB_VK_DeviceFeature_COUNT];
        rb_vk_physical_device_all_required_extensions(&rb_vk_g_config, device->vk_gpu, feature_enabled);

        u32 extension_count = 0;
        const char *extensions[RB_VK_DeviceFeature_COUNT];
        VkBaseOutStructure *features = NULL;
        for(u32 i = 0; i < RB_VK_DeviceFeature_COUNT; ++i){
            const RB_VK_DeviceFeature *feature = &rb_vk_g_config.device.features[i];
            if(!feature_enabled[i]){
                continue;
            }
            if(feature->extension.size){
                extensions[extension_count++] = feature->extension.cstr;
            }
            if(feature->features){
                VkBaseOutStructure *next = feature->features;
                next->pNext = features;
                features = next;
            }
        }
        device->mesh_shaders  = feature_enabled[RB_VK_DeviceFeature_MeshShader];
        device->memory_budget = feature_enabled[RB_VK_DeviceFeature_MemoryBudget];
        device->device_address_commands = feature_enabled[RB_VK_DeviceFeature_DeviceAddressCommands];
        if(!device->device_address_commands){
            DOT_WARNING("VK_KHR_device_address_commands unsupported, falling back to resolving GPU addresses to VkBuffers");
        }

        VkDeviceCreateInfo device_create_info = {
            .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pQueueCreateInfos       = queue_infos,
            .queueCreateInfoCount    = queue_count,
            .pEnabledFeatures        = &rb_vk_g_config.device.core_features,
            .ppEnabledExtensionNames = extensions,
            .enabledExtensionCount   = extension_count,
            .pNext = features,
        };

        RB_VK_CHECK(vkCreateDevice(device->vk_gpu, &device_create_info, NULL, &device->vk_device));
        volkLoadDevice(device->vk_device);

        vkGetDeviceQueue(device->vk_device, device->graphics_queue_idx, 0, &device->graphics_queue);

        {
            u32 family_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device->vk_gpu, &family_count, NULL);
            VkQueueFamilyProperties *families = PUSH_ARRAY(s.arena, VkQueueFamilyProperties, family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(device->vk_gpu, &family_count, families);
            b32 has_timestamps = families[device->graphics_queue_idx].timestampValidBits > 0;
            device->timestamp_query_count = has_timestamps ? desc->timestamp_query_max : 0;
            device->timestamp_period_ns   = device->device_properties->limits.timestampPeriod;
        }
        if(shared_present_graphics_queues){
            device->present_queue = device->graphics_queue;
        } else {
            DOT_ERROR("Different present and graphics queues unsupported");
            // vkGetDeviceQueue(device->vk_device, device->graphics_queue_idx, 0, &device->present_queue);
        }
    }
    // (claude) Must run before any rb_texture_heap_create.
    rb_vk_texture_memory_select();

    {
        RB_VK_SwapchainDetails details = {
            .preferred_format       = rb_vk_format_from_format_kind(desc->swapchain_format),
            .preferred_present_mode = rb_vk_present_mode_from_present_mode_kind(desc->present_mode),
        };
        rb_vk_physical_device_swapchain_support(rb_vk_g_ctx->device.vk_gpu, rb_vk_g_ctx->surface, &details);
        rb_vk_g_ctx->swapchain.image_format      = details.best_surface_format.format;
        rb_vk_g_ctx->swapchain.recreate_required = true;
    }

    {
        VkFormat preferred = rb_vk_format_from_format_kind(desc->depth_format);
        VkFormatProperties props = {0};
        vkGetPhysicalDeviceFormatProperties(rb_vk_g_ctx->device.vk_gpu, preferred, &props);
        VkFormatFeatureFlags need = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        if(DOT_BITS_MATCH(props.optimalTilingFeatures, need)){
            rb_vk_g_ctx->depth_format = preferred;
        }else{
            DOT_WARNING("Depth format %S unsupported, falling back to D16_UNORM", string8_from_RB_FormatKind[desc->depth_format]);
            rb_vk_g_ctx->depth_format = VK_FORMAT_D16_UNORM;
        }
    }

    {
        VkDevice device = rb_vk_g_ctx->device.vk_device;
        {
            rb_vk_g_ctx->present_context_count = desc->frame_overlap;
            rb_vk_g_ctx->present_contexts      = PUSH_ARRAY(ctx_arena, RB_VK_PresentContext, rb_vk_g_ctx->present_context_count);
            for(u32 i = 0; i < rb_vk_g_ctx->present_context_count; ++i){
                RB_VK_PresentContext *context = &rb_vk_g_ctx->present_contexts[i];
                RB_VK_CHECK(vkCreateFence(device,
                    &(VkFenceCreateInfo){
                        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
                    },
                    NULL, &context->presented));

                VkSemaphoreCreateInfo semaphore_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
                RB_VK_CHECK(vkCreateSemaphore(device, &semaphore_info, NULL, &context->acquired));
                RB_VK_CHECK(vkCreateSemaphore(device, &semaphore_info, NULL, &context->rendered));
            }
        }
        {
            rb_vk_g_ctx->heap_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_PROPERTIES_EXT;
            VkPhysicalDeviceProperties2 properties2 = {0};
            properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            properties2.pNext = &rb_vk_g_ctx->heap_properties;
            vkGetPhysicalDeviceProperties2(rb_vk_g_ctx->device.vk_gpu, &properties2);
        }
    }

    scratch_end(s);
}

internal RB_DeviceInfo
rb_vk_device_info(void)
{
    RB_DeviceInfo rb_info = {0};
    VkPhysicalDeviceProperties *properties = rb_vk_g_ctx->device.device_properties;
    rb_info.has_integrated_gpu = cast(b8) rb_vk_g_ctx->device.is_integrated_gpu;
    rb_info.vendor_id          = properties->vendorID;
    rb_info.driver_version     = properties->driverVersion;
    rb_info.api_version        = properties->apiVersion;
    rb_info.swapchain_format   = rb_vk_format_kind_from_vk_format(rb_vk_g_ctx->swapchain.image_format);
    rb_info.depth_format       = rb_vk_format_kind_from_vk_format(rb_vk_g_ctx->depth_format);
    rb_info.texture_heap_alignment  = rb_vk_g_ctx->device.texture_heap_alignment;
    rb_info.texture_descriptor_size = rb_vk_g_ctx->heap_properties.imageDescriptorSize;
    rb_info.sampler_descriptor_size = rb_vk_g_ctx->heap_properties.samplerDescriptorSize;
    rb_info.timestamp_period_ns = rb_vk_g_ctx->device.timestamp_query_count ? rb_vk_g_ctx->device.timestamp_period_ns : 0.0f;
    rb_info.mesh_shaders        = rb_vk_g_ctx->device.mesh_shaders;
    rb_info.name = string8_from_cstring(properties->deviceName);
    return(rb_info);
}

internal i32x2
rb_vk_drawable_extent(void)
{
    VkExtent2D extent = rb_vk_g_ctx->swapchain.extent;
    return((i32x2){ .x = cast(i32) extent.width, .y = cast(i32) extent.height });
}

internal void
rb_vk_wait_idle(void)
{
    RB_VK_CHECK(vkDeviceWaitIdle(rb_vk_g_ctx->device.vk_device));
    rb_vk_present_contexts_wait();
}

internal RB_RenderView
rb_vk_render_view_create(RB_Texture texture_h, RB_RenderViewDesc desc)
{
    RB_VK_Texture *texture = RB_HANDLE_UNPACK(RB_VK_Texture*, texture_h);
    DOT_ASSERT(texture->vk_usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT),
        "Texture '%s' isn't renderable - create it with RB_TextureUsageBit_RenderTarget or a depth/stencil format", texture->debug_name.buff);
    DOT_ASSERT(desc.mip_level < texture->mip_levels, "Render view mip %u out of range", desc.mip_level);
    DOT_ASSERT(desc.slice < texture->array_layers, "Render view slice %u out of range", desc.slice);

    RB_VK_RenderView *view = rb_vk_g_ctx->render_view_free;
    if(view){
        SLL_STACK_POP(rb_vk_g_ctx->render_view_free);
        MEMORY_ZERO_STRUCT(view);
    }else{
        view = PUSH_STRUCT(rb_vk_g_ctx->permanent_arena, RB_VK_RenderView);
    }
    view->vk_image  = texture->vk_image;
    view->vk_format = texture->vk_format;
    view->vk_aspect = texture->vk_aspect;
    view->extent    = (VkExtent2D){
        DOT_MAX(1u, texture->vk_extent3d.width  >> desc.mip_level),
        DOT_MAX(1u, texture->vk_extent3d.height >> desc.mip_level),
    };

    VkImageViewCreateInfo view_info = {0};
    view_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image                           = texture->vk_image;
    view_info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format                          = texture->vk_format;
    view_info.subresourceRange.aspectMask     = texture->vk_aspect;
    view_info.subresourceRange.baseMipLevel   = desc.mip_level;
    view_info.subresourceRange.levelCount     = 1;
    view_info.subresourceRange.baseArrayLayer = desc.slice;
    view_info.subresourceRange.layerCount     = 1;
    RB_VK_CHECK(vkCreateImageView(rb_vk_g_ctx->device.vk_device, &view_info, NULL, &view->vk_image_view));
    rb_vk_resource_set_name(VK_OBJECT_TYPE_IMAGE_VIEW, cast(u64)view->vk_image_view, string8_from_cstring(texture->debug_name.buff));
    RB_RenderView h = RB_HANDLE_PACK(RB_RenderView, view);
    return(h);
}

internal void
rb_vk_render_view_destroy(RB_RenderView handle)
{
    if(!RB_HANDLE_VALID(handle)){ return; }
    RB_VK_RenderView *view = RB_HANDLE_UNPACK(RB_VK_RenderView*, handle);

    DOT_ASSERT(!view->swapchain_image, "Swapchain render views are owned by the backend");
    vkDestroyImageView(rb_vk_g_ctx->device.vk_device, view->vk_image_view, NULL);
    SLL_STACK_PUSH(rb_vk_g_ctx->render_view_free, view);
}

////////////////////////////////////////////////////////////////
/// Command recording, timeline completion, dynamic rendering (Phase 3)

internal RB_TimelineSemaphore
rb_vk_timeline_create(u64 initial_value)
{
    VkSemaphoreTypeCreateInfo type_info = {0};
    type_info.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    type_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    type_info.initialValue  = initial_value;

    VkSemaphoreCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    create_info.pNext = &type_info;

    VkSemaphore vk_sema = VK_NULL_HANDLE;
    RB_VK_CHECK(vkCreateSemaphore(rb_vk_g_ctx->device.vk_device, &create_info, NULL, &vk_sema));
    RB_TimelineSemaphore sema = RB_HANDLE_PACK(RB_TimelineSemaphore, vk_sema);
    return(sema);
}

internal void
rb_vk_timeline_destroy(RB_TimelineSemaphore semaphore)
{
    if(!RB_HANDLE_VALID(semaphore)){ return; }
    vkDestroySemaphore(rb_vk_g_ctx->device.vk_device, RB_HANDLE_UNPACK(RB_VK_TimelineSemaphore, semaphore), NULL);
}

internal u64
rb_vk_timeline_completed_value(RB_TimelineSemaphore semaphore)
{
    u64 value = 0;
    vkGetSemaphoreCounterValue(rb_vk_g_ctx->device.vk_device, RB_HANDLE_UNPACK(RB_VK_TimelineSemaphore, semaphore), &value);
    return(value);
}

internal void
rb_vk_timeline_wait(RB_TimelinePoint point)
{
    RB_VK_TimelineSemaphore vk_sema = RB_HANDLE_UNPACK(RB_VK_TimelineSemaphore, point.semaphore);
    VkSemaphoreWaitInfo wait_info = {0};
    wait_info.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    wait_info.semaphoreCount = 1;
    wait_info.pSemaphores    = &vk_sema;
    wait_info.pValues        = &point.value;
    RB_VK_CHECK(vkWaitSemaphores(rb_vk_g_ctx->device.vk_device, &wait_info, U64_MAX));
}

internal RB_SwapchainFrame
rb_vk_swapchain_frame_acquire(RB_CommandBuffer cmd_h)
{
    RB_VK_CommandBuffer *cmd = RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, cmd_h);
    RB_VK_Swapchain *swapchain = &rb_vk_g_ctx->swapchain;
    VkDevice device = rb_vk_g_ctx->device.vk_device;
    DOT_ASSERT(!swapchain->acquired, "Swapchain image already acquired - present it with rb_submit_and_present first");
    rb_vk_assert_outside_suspended_pass(cmd);

    VkExtent2D surface_extent = rb_vk_surface_extent();
    if(surface_extent.width != swapchain->extent.width || surface_extent.height != swapchain->extent.height){
        swapchain->recreate_required = true;
    }

    RB_VK_PresentContext *context = &rb_vk_g_ctx->present_contexts[rb_vk_g_ctx->next_present_context];
    RB_VK_CHECK(vkWaitForFences(device, 1, &context->presented, VK_TRUE, U64_MAX));

    u32 image_index = 0;
    bool should_draw = true;
    for(;;){
        if(swapchain->recreate_required){
            surface_extent = rb_vk_surface_extent();
            if(surface_extent.width == 0 || surface_extent.height == 0){
                should_draw = false;
                break;
            }
            rb_vk_swapchain_recreate();
        }
        if(swapchain->extent.width == 0 || swapchain->extent.height == 0){
            should_draw = false;
            break;
        }
        VkResult result = vkAcquireNextImageKHR(device, swapchain->swapchain, U64_MAX, context->acquired, VK_NULL_HANDLE, &image_index);
        if(result == VK_ERROR_OUT_OF_DATE_KHR){
            swapchain->recreate_required = true;
            continue;
        }
        if(result != VK_SUBOPTIMAL_KHR){ RB_VK_CHECK(result); }
        swapchain->recreate_required = result == VK_SUBOPTIMAL_KHR;
        break;
    }
    RB_SwapchainFrame frame = {0};
    if(should_draw){
        DOT_ASSERT(image_index < swapchain->swapchain_image_count, "Swapchain returned image %u out of %u", image_index, swapchain->swapchain_image_count);

        RB_VK_SwapchainImage *image = &swapchain->swapchain_images[image_index];
        rb_vk_image_transition(cmd->vk_cmd, image->vk_image, VK_IMAGE_ASPECT_COLOR_BIT, image->vk_layout, VK_IMAGE_LAYOUT_GENERAL);
        image->vk_layout = VK_IMAGE_LAYOUT_GENERAL;

        swapchain->image_index         = image_index;
        swapchain->present_context     = context;
        swapchain->acquired            = true;
        swapchain->transition_commands = cmd;
        cmd->holds_swapchain           = true;

        frame.view   = RB_HANDLE_PACK(RB_RenderView, &swapchain->render_views[image_index]);
        frame.extent = (i32x2){ .x = cast(i32) swapchain->extent.width, .y = cast(i32) swapchain->extent.height };
    }
    return(frame);
}

internal RB_CommandPool
rb_vk_command_pool_create(void)
{
    RB_VK_CommandPool *pool = rb_vk_g_ctx->command_pool_free;
    if(pool){
        SLL_STACK_POP(rb_vk_g_ctx->command_pool_free);
        MEMORY_ZERO_STRUCT(pool);
    }else{
        pool = PUSH_STRUCT(rb_vk_g_ctx->permanent_arena, RB_VK_CommandPool);
    }
    VkCommandPoolCreateInfo create_info = {0};
    create_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    create_info.queueFamilyIndex = rb_vk_g_ctx->device.graphics_queue_idx;
    RB_VK_CHECK(vkCreateCommandPool(rb_vk_g_ctx->device.vk_device, &create_info, NULL, &pool->vk_pool));
    RB_CommandPool h = RB_HANDLE_PACK(RB_CommandPool, pool);
    return(h);
}

internal void
rb_vk_command_pool_destroy(RB_CommandPool pool_h)
{
    RB_VK_CommandPool *pool = RB_HANDLE_UNPACK(RB_VK_CommandPool*, pool_h);
    if(!pool){ return; }
    VkDevice device = rb_vk_g_ctx->device.vk_device;
    for(RB_VK_CommandBuffer *cmd = pool->first; cmd; cmd = cmd->next){
        if(cmd->timestamp_pool){ vkDestroyQueryPool(device, cmd->timestamp_pool, NULL); }
    }
    vkDestroyCommandPool(device, pool->vk_pool, NULL);
    SLL_STACK_PUSH(rb_vk_g_ctx->command_pool_free, pool);
}

internal void
rb_vk_command_pool_reset(RB_CommandPool pool_h)
{
    RB_VK_CommandPool *pool = RB_HANDLE_UNPACK(RB_VK_CommandPool*, pool_h);
    RB_VK_CHECK(vkResetCommandPool(rb_vk_g_ctx->device.vk_device, pool->vk_pool, 0));
    pool->next_free = pool->first;
}

internal RB_CommandBuffer
rb_vk_commands_begin(RB_CommandPool pool_h)
{
    RB_VK_CommandPool *pool = RB_HANDLE_UNPACK(RB_VK_CommandPool*, pool_h);
    VkDevice device = rb_vk_g_ctx->device.vk_device;

    RB_VK_CommandBuffer *cmd = pool->next_free;
    if(cmd){
        pool->next_free = cmd->next;
    }else{
        cmd = PUSH_STRUCT(rb_vk_g_ctx->permanent_arena, RB_VK_CommandBuffer);
        VkCommandBufferAllocateInfo alloc_info = {0};
        alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool        = pool->vk_pool;
        alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;
        RB_VK_CHECK(vkAllocateCommandBuffers(device, &alloc_info, &cmd->vk_cmd));

        if(rb_vk_g_ctx->device.timestamp_query_count){
            VkQueryPoolCreateInfo query_info = {0};
            query_info.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
            query_info.queryType  = VK_QUERY_TYPE_TIMESTAMP;
            query_info.queryCount = rb_vk_g_ctx->device.timestamp_query_count;
            RB_VK_CHECK(vkCreateQueryPool(device, &query_info, NULL, &cmd->timestamp_pool));
            cmd->timestamp_dsts = PUSH_ARRAY(rb_vk_g_ctx->permanent_arena, RB_GpuAddress, rb_vk_g_ctx->device.timestamp_query_count);
        }
        SLL_QUEUE_PUSH(pool->first, pool->last, cmd);
    }

    VkCommandBufferBeginInfo begin_info = {0};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    RB_VK_CHECK(vkBeginCommandBuffer(cmd->vk_cmd, &begin_info));

    // (claude) Host-side reset: vkCmdResetQueryPool here would break a resumed render pass.
    if(cmd->timestamp_pool){
        vkResetQueryPool(device, cmd->timestamp_pool, 0, rb_vk_g_ctx->device.timestamp_query_count);
    }
    cmd->timestamp_count   = 0;
    cmd->render_pass_count = 0;
    cmd->recorded_outside_pass = false;
    cmd->begins_resuming   = false;
    cmd->ends_suspending   = false;
    cmd->in_render_pass    = false;
    cmd->pass_suspending   = false;
    cmd->holds_swapchain   = false;
    cmd->active_color_attachment_count = 0;
    cmd->active_depth_attachment       = NULL;
    cmd->active_stencil_attachment     = NULL;

    RB_CommandBuffer h = RB_HANDLE_PACK(RB_CommandBuffer, cmd);
    return(h);
}

internal VkPipelineStageFlags2
rb_vk_pipeline_stage_flags2_from_rb_stage(RB_StageFlags stage)
{
    VkPipelineStageFlags2 flags = 0;
    if(DOT_BITS_ANY(stage, RB_StageBit_Indirect))          flags |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
    if(DOT_BITS_ANY(stage, RB_StageBit_IndexInput))        flags |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
    if(DOT_BITS_ANY(stage, RB_StageBit_Vertex))            flags |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    if(DOT_BITS_ANY(stage, RB_StageBit_Task))              flags |= VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT;
    if(DOT_BITS_ANY(stage, RB_StageBit_Mesh))              flags |= VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT;
    if(DOT_BITS_ANY(stage, RB_StageBit_DepthStencilTests)) flags |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    if(DOT_BITS_ANY(stage, RB_StageBit_Fragment))          flags |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    if(DOT_BITS_ANY(stage, RB_StageBit_ColorOutput))       flags |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    if(DOT_BITS_ANY(stage, RB_StageBit_Compute))           flags |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    if(DOT_BITS_ANY(stage, RB_StageBit_Transfer))          flags |= VK_PIPELINE_STAGE_2_COPY_BIT;
    if(DOT_BITS_ANY(stage, RB_StageBit_Host))              flags |= VK_PIPELINE_STAGE_2_HOST_BIT;
    if(DOT_BITS_ANY(stage, RB_StageBit_AllCommands))       flags |= VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    return(flags);
}

internal VkAccessFlags2
rb_vk_access_flags_from_rb_access(RB_AccessFlags access)
{
    VkAccessFlags2 flags = 0;
    if(DOT_BITS_ANY(access, RB_AccessBit_TransferRead))      flags |= VK_ACCESS_2_TRANSFER_READ_BIT;
    if(DOT_BITS_ANY(access, RB_AccessBit_TransferWrite))     flags |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
    if(DOT_BITS_ANY(access, RB_AccessBit_ShaderRead))        flags |= VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    if(DOT_BITS_ANY(access, RB_AccessBit_ShaderWrite))       flags |= VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
    if(DOT_BITS_ANY(access, RB_AccessBit_ColorRead))         flags |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
    if(DOT_BITS_ANY(access, RB_AccessBit_ColorWrite))        flags |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    if(DOT_BITS_ANY(access, RB_AccessBit_DepthStencilRead))  flags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    if(DOT_BITS_ANY(access, RB_AccessBit_DepthStencilWrite)) flags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    if(DOT_BITS_ANY(access, RB_AccessBit_IndirectRead))      flags |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
    if(DOT_BITS_ANY(access, RB_AccessBit_IndexRead))         flags |= VK_ACCESS_2_INDEX_READ_BIT;
    if(DOT_BITS_ANY(access, RB_AccessBit_HostRead))          flags |= VK_ACCESS_2_HOST_READ_BIT;
    return(flags);
}

internal void
rb_vk_barrier(RB_CommandBuffer cmd_h, RB_StageFlags before_stage, RB_AccessFlags before_access, RB_StageFlags after_stage, RB_AccessFlags after_access)
{
    RB_VK_CommandBuffer *cmd = RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, cmd_h);
    rb_vk_assert_outside_suspended_pass(cmd);

    VkMemoryBarrier2 barrier = {0};
    barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    barrier.srcStageMask  = rb_vk_pipeline_stage_flags2_from_rb_stage(before_stage);
    barrier.srcAccessMask = rb_vk_access_flags_from_rb_access(before_access);
    barrier.dstStageMask  = rb_vk_pipeline_stage_flags2_from_rb_stage(after_stage);
    barrier.dstAccessMask = rb_vk_access_flags_from_rb_access(after_access);

    VkDependencyInfo dep_info = {0};
    dep_info.sType              = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep_info.memoryBarrierCount = 1;
    dep_info.pMemoryBarriers    = &barrier;
    vkCmdPipelineBarrier2(cmd->vk_cmd, &dep_info);
}

internal void
rb_vk_set_viewport(RB_CommandBuffer cmd_h, RB_Viewport viewport)
{
    RB_VK_CommandBuffer *cmd = RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, cmd_h);
    VkViewport vk_viewport = {0};
    vk_viewport.x        = cast(f32) viewport.viewport_rect.x;
    vk_viewport.y        = cast(f32) viewport.viewport_rect.y;
    vk_viewport.width    = cast(f32) viewport.viewport_rect.w;
    vk_viewport.height   = cast(f32) viewport.viewport_rect.h;
    vk_viewport.minDepth = viewport.min_depth;
    vk_viewport.maxDepth = viewport.max_depth;
    vkCmdSetViewport(cmd->vk_cmd, 0, 1, &vk_viewport);
}

internal void
rb_vk_set_scissor(RB_CommandBuffer cmd_h, RB_Extent2D16 scissor)
{
    RB_VK_CommandBuffer *cmd = RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, cmd_h);
    VkRect2D vk_scissor = {0};
    vk_scissor.offset = (VkOffset2D){ scissor.x, scissor.y };
    vk_scissor.extent = (VkExtent2D){ cast(u32) scissor.w, cast(u32) scissor.h };
    vkCmdSetScissor(cmd->vk_cmd, 0, 1, &vk_scissor);
}

internal void
rb_vk_set_depth_stencil_state(RB_CommandBuffer cmd_h, RB_DepthStencilState state)
{
    RB_VK_CommandBuffer *cmd = RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, cmd_h);
    VkCommandBuffer vk_cmd = cmd->vk_cmd;

    vkCmdSetDepthTestEnable(vk_cmd, state.depth_enable ? VK_TRUE : VK_FALSE);
    vkCmdSetDepthWriteEnable(vk_cmd, state.depth_write_enable ? VK_TRUE : VK_FALSE);
    vkCmdSetDepthCompareOp(vk_cmd, rb_vk_compare_op_from_compare_op_kind(state.depth_comparison));

    vkCmdSetStencilTestEnable(vk_cmd, state.stencil_enable ? VK_TRUE : VK_FALSE);
    RB_StencilState *faces[2] = { &state.front, &state.back };
    VkStencilFaceFlags face_bits[2] = { VK_STENCIL_FACE_FRONT_BIT, VK_STENCIL_FACE_BACK_BIT };
    for(u32 i = 0; i < 2; ++i){
        RB_StencilState *s = faces[i];
        vkCmdSetStencilOp(vk_cmd, face_bits[i],
            rb_vk_stencil_op_from_stencil_op_kind(s->fail),
            rb_vk_stencil_op_from_stencil_op_kind(s->pass),
            rb_vk_stencil_op_from_stencil_op_kind(s->depth_fail),
            rb_vk_compare_op_from_compare_op_kind(s->compare_op));
        vkCmdSetStencilCompareMask(vk_cmd, face_bits[i], s->compare_mask);
        vkCmdSetStencilWriteMask(vk_cmd, face_bits[i], s->write_mask);
        vkCmdSetStencilReference(vk_cmd, face_bits[i], s->reference_mask);
    }
}

internal VkAttachmentLoadOp
rb_vk_attachment_load_op_from_rb_load_op(RB_LoadOpKind kind)
{
    switch(kind){
    case RB_LoadOpKind_Load:     return VK_ATTACHMENT_LOAD_OP_LOAD;
    case RB_LoadOpKind_Clear:    return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case RB_LoadOpKind_DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

internal VkAttachmentStoreOp
rb_vk_attachment_store_op_from_rb_store_op(RB_StoreOpKind kind)
{
    return kind == RB_StoreOpKind_Store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

internal void
rb_vk_render_pass_begin(RB_CommandBuffer cmd_h, RB_RenderingDesc desc, RB_RenderingFlags flags)
{
    RB_VK_CommandBuffer *cmd = RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, cmd_h);
    VkCommandBuffer vk_cmd = cmd->vk_cmd;
    DOT_ASSERT(!cmd->in_render_pass, "Render pass already open");

    b32 resuming   = DOT_BITS_ANY(flags, RB_RenderingBit_Resuming);
    b32 suspending = DOT_BITS_ANY(flags, RB_RenderingBit_Suspending);
    if(resuming){
        DOT_ASSERT(cmd->ends_suspending || (cmd->render_pass_count == 0 && !cmd->recorded_outside_pass),
            "A resuming render pass must directly follow a suspended one");
        if(cmd->render_pass_count == 0){ cmd->begins_resuming = true; }
    }else{
        DOT_ASSERT(!cmd->ends_suspending, "A suspended render pass must be followed by a resuming one");
    }

    VkRenderingAttachmentInfo color_infos[RB_IMAGE_OUTPUTS_MAX] = {0};
    u32 color_attachement_count = 0;
    for(; color_attachement_count < RB_IMAGE_OUTPUTS_MAX; ++color_attachement_count){
        RB_ColorAttachment *att = &desc.color_attachments[color_attachement_count];
        if(!RB_HANDLE_VALID(att->view)){ break; }

        RB_VK_RenderView *view = RB_HANDLE_UNPACK(RB_VK_RenderView*, att->view);
        cmd->active_color_attachments[color_attachement_count] = view;
        DOT_ASSERT(!view->swapchain_image || view->swapchain_image->vk_layout == VK_IMAGE_LAYOUT_GENERAL, "Rendering to a swapchain image that wasn't acquired");

        color_infos[color_attachement_count].sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        color_infos[color_attachement_count].imageView   = view->vk_image_view;
        color_infos[color_attachement_count].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        color_infos[color_attachement_count].loadOp      = rb_vk_attachment_load_op_from_rb_load_op(att->load_op);
        color_infos[color_attachement_count].storeOp     = rb_vk_attachment_store_op_from_rb_store_op(att->store_op);
        color_infos[color_attachement_count].clearValue.color = (VkClearColorValue){ .float32 = {att->clear_color.r, att->clear_color.g, att->clear_color.b, att->clear_color.a} };
    }
    cmd->active_color_attachment_count = color_attachement_count;

    RB_VK_RenderView *dview = RB_HANDLE_UNPACK(RB_VK_RenderView*, desc.depth_attachment.view);
    RB_VK_RenderView *sview = RB_HANDLE_UNPACK(RB_VK_RenderView*, desc.stencil_attachment.view);
    cmd->active_depth_attachment   = dview;
    cmd->active_stencil_attachment = sview;

    VkRenderingAttachmentInfo depth_info = {0};
    depth_info.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depth_info.imageView   = dview ? dview->vk_image_view : VK_NULL_HANDLE;
    depth_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    depth_info.loadOp      = rb_vk_attachment_load_op_from_rb_load_op(desc.depth_attachment.load_op);
    depth_info.storeOp     = rb_vk_attachment_store_op_from_rb_store_op(desc.depth_attachment.store_op);
    depth_info.clearValue.depthStencil.depth = desc.depth_attachment.clear_value;

    VkRenderingAttachmentInfo stencil_info = {0};
    stencil_info.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    stencil_info.imageView   = sview ? sview->vk_image_view : VK_NULL_HANDLE;
    stencil_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    stencil_info.loadOp      = rb_vk_attachment_load_op_from_rb_load_op(desc.stencil_attachment.load_op);
    stencil_info.storeOp     = rb_vk_attachment_store_op_from_rb_store_op(desc.stencil_attachment.store_op);
    stencil_info.clearValue.depthStencil.stencil = desc.stencil_attachment.clear_value;

    RB_VK_RenderView *area_view = color_attachement_count ? cmd->active_color_attachments[0] : (dview ? dview : sview);
    DOT_ASSERT(area_view, "A render pass needs at least one attachment");
    VkExtent2D area = area_view->extent;
    if(desc.render_extent.x || desc.render_extent.y){
        area = (VkExtent2D){ cast(u32) desc.render_extent.x, cast(u32) desc.render_extent.y };
    }

    VkRenderingInfo rendering_info = {0};
    rendering_info.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.flags                = (suspending ? VK_RENDERING_SUSPENDING_BIT : 0) | (resuming ? VK_RENDERING_RESUMING_BIT : 0);
    rendering_info.renderArea.extent    = area;
    rendering_info.layerCount           = 1;
    rendering_info.colorAttachmentCount = color_attachement_count;
    rendering_info.pColorAttachments    = color_infos;
    rendering_info.pDepthAttachment     = dview ? &depth_info : NULL;
    rendering_info.pStencilAttachment   = sview ? &stencil_info : NULL;
    vkCmdBeginRendering(vk_cmd, &rendering_info);

    cmd->in_render_pass     = true;
    cmd->pass_suspending    = cast(b8) suspending;
    cmd->ends_suspending    = false;
    cmd->render_pass_count += 1;

    RB_Viewport viewport = {0};
    viewport.viewport_rect = (RB_Extent2D16){ .w = cast(u16) area.width, .h = cast(u16) area.height };
    viewport.max_depth     = 1.0f;
    rb_vk_set_viewport(cmd_h, viewport);
    rb_vk_set_scissor(cmd_h, viewport.viewport_rect);
    rb_vk_set_depth_stencil_state(cmd_h, (RB_DepthStencilState){ .depth_comparison = RB_CompareOpKind_Always });
}

internal void
rb_vk_render_pass_end(RB_CommandBuffer cmd_h)
{
    RB_VK_CommandBuffer *cmd = RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, cmd_h);
    DOT_ASSERT(cmd->in_render_pass, "No render pass open");
    vkCmdEndRendering(cmd->vk_cmd);
    cmd->in_render_pass  = false;
    cmd->ends_suspending = cmd->pass_suspending;
    cmd->active_color_attachment_count = 0;
    cmd->active_depth_attachment       = NULL;
    cmd->active_stencil_attachment     = NULL;
}

internal void
rb_vk_bind_pso(RB_CommandBuffer cmd_h, RB_Pipeline pso_h)
{
    RB_VK_CommandBuffer *cmd = RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, cmd_h);
    RB_VK_Pipeline *pso = RB_HANDLE_UNPACK(RB_VK_Pipeline*, pso_h);
    vkCmdBindPipeline(cmd->vk_cmd, pso->vk_bind_point, pso->vk_pipeline);
}

internal void
rb_vk_root_push(RB_VK_CommandBuffer *cmd, RB_GpuAddress root)
{
    VkPushDataInfoEXT info = {0};
    info.sType        = VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT;
    info.data.address = &root;
    info.data.size    = sizeof(root);
    vkCmdPushDataEXT(cmd->vk_cmd, &info);
}

internal void
rb_vk_index_buffer_bind(RB_VK_CommandBuffer *cmd, RB_GpuRange indices, RB_IndexKind index_kind)
{
    VkIndexType index_type = index_kind == RB_IndexKind_U16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
    if(!rb_vk_g_ctx->device.device_address_commands){
        VkDeviceSize offset = 0;
        VkBuffer vk_buffer = rb_vk_buffer_from_gpu_address(indices.gpu, &offset);
        vkCmdBindIndexBuffer(cmd->vk_cmd, vk_buffer, offset, index_type);
        return;
    }
    VkBindIndexBuffer3InfoKHR bind_info = {0};
    bind_info.sType                = VK_STRUCTURE_TYPE_BIND_INDEX_BUFFER_3_INFO_KHR;
    bind_info.addressRange.address = indices.gpu;
    bind_info.addressRange.size    = cast(u64)indices.size;
    bind_info.addressFlags         = VK_ADDRESS_COMMAND_FULLY_BOUND_BIT_KHR;
    bind_info.indexType            = index_type;
    vkCmdBindIndexBuffer3KHR(cmd->vk_cmd, &bind_info);
}

internal void
rb_vk_draw(RB_CommandBuffer cmd_h, RB_GpuAddress root, u32 vertex_count, u32 instance_count, u32 first_vertex, u32 first_instance)
{
    RB_VK_CommandBuffer *cmd = RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, cmd_h);
    rb_vk_root_push(cmd, root);
    vkCmdDraw(cmd->vk_cmd, vertex_count, instance_count, first_vertex, first_instance);
}

internal void
rb_vk_draw_indexed(RB_CommandBuffer cmd_h, RB_GpuAddress root, RB_GpuRange indices, RB_IndexKind index_kind, u32 index_count, u32 instance_count, u32 first_index, i32 vertex_offset, u32 first_instance)
{
    RB_VK_CommandBuffer *cmd = RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, cmd_h);
    rb_vk_root_push(cmd, root);
    rb_vk_index_buffer_bind(cmd, indices, index_kind);
    vkCmdDrawIndexed(cmd->vk_cmd, index_count, instance_count, first_index, vertex_offset, first_instance);
}

internal void
rb_vk_draw_indirect(RB_CommandBuffer cmd_h, RB_GpuAddress root, RB_GpuRange args, u32 draw_count, u32 stride)
{
    RB_VK_CommandBuffer *cmd = RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, cmd_h);
    rb_vk_root_push(cmd, root);

    u32 draw_stride = stride ? stride : sizeof(RB_DrawIndirectArgs);
    if(!rb_vk_g_ctx->device.device_address_commands){
        VkDeviceSize offset = 0;
        VkBuffer vk_buffer = rb_vk_buffer_from_gpu_address(args.gpu, &offset);
        vkCmdDrawIndirect(cmd->vk_cmd, vk_buffer, offset, draw_count, draw_stride);
        return;
    }
    VkDrawIndirect2InfoKHR info = {0};
    info.sType                = VK_STRUCTURE_TYPE_DRAW_INDIRECT_2_INFO_KHR;
    info.addressRange.address = args.gpu;
    info.addressRange.size    = cast(u64)args.size;
    info.addressRange.stride  = draw_stride;
    info.addressFlags         = VK_ADDRESS_COMMAND_FULLY_BOUND_BIT_KHR;
    info.drawCount            = draw_count;
    vkCmdDrawIndirect2KHR(cmd->vk_cmd, &info);
}

internal void
rb_vk_draw_indexed_indirect(RB_CommandBuffer cmd_h, RB_GpuAddress root, RB_GpuRange indices, RB_IndexKind index_kind, RB_GpuRange args, u32 draw_count, u32 stride)
{
    RB_VK_CommandBuffer *cmd = RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, cmd_h);
    rb_vk_root_push(cmd, root);
    rb_vk_index_buffer_bind(cmd, indices, index_kind);

    u32 draw_stride = stride ? stride : sizeof(RB_DrawIndexedIndirectArgs);
    if(!rb_vk_g_ctx->device.device_address_commands){
        VkDeviceSize offset = 0;
        VkBuffer vk_buffer = rb_vk_buffer_from_gpu_address(args.gpu, &offset);
        vkCmdDrawIndexedIndirect(cmd->vk_cmd, vk_buffer, offset, draw_count, draw_stride);
        return;
    }
    VkDrawIndirect2InfoKHR info = {0};
    info.sType                = VK_STRUCTURE_TYPE_DRAW_INDIRECT_2_INFO_KHR;
    info.addressRange.address = args.gpu;
    info.addressRange.size    = cast(u64)args.size;
    info.addressRange.stride  = draw_stride;
    info.addressFlags         = VK_ADDRESS_COMMAND_FULLY_BOUND_BIT_KHR;
    info.drawCount            = draw_count;
    vkCmdDrawIndexedIndirect2KHR(cmd->vk_cmd, &info);
}

internal void
rb_vk_draw_meshlets(RB_CommandBuffer cmd_h, RB_GpuAddress root, u32 group_x, u32 group_y, u32 group_z)
{
    DOT_ASSERT(rb_vk_g_ctx->device.mesh_shaders, "Meshlet draws need VK_EXT_mesh_shader");
    RB_VK_CommandBuffer *cmd = RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, cmd_h);
    rb_vk_root_push(cmd, root);
    vkCmdDrawMeshTasksEXT(cmd->vk_cmd, group_x, group_y, group_z);
}

internal void
rb_vk_draw_meshlets_indirect(RB_CommandBuffer cmd_h, RB_GpuAddress root, RB_GpuRange args, u32 draw_count, u32 stride)
{
    DOT_ASSERT(rb_vk_g_ctx->device.mesh_shaders, "Meshlet draws need VK_EXT_mesh_shader");
    RB_VK_CommandBuffer *cmd = RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, cmd_h);
    rb_vk_root_push(cmd, root);

    u32 draw_stride = stride ? stride : sizeof(RB_DrawMeshletsIndirectArgs);
    if(!rb_vk_g_ctx->device.device_address_commands){
        VkDeviceSize offset = 0;
        VkBuffer vk_buffer = rb_vk_buffer_from_gpu_address(args.gpu, &offset);
        vkCmdDrawMeshTasksIndirectEXT(cmd->vk_cmd, vk_buffer, offset, draw_count, draw_stride);
        return;
    }
    VkDrawIndirect2InfoKHR info = {0};
    info.sType                = VK_STRUCTURE_TYPE_DRAW_INDIRECT_2_INFO_KHR;
    info.addressRange.address = args.gpu;
    info.addressRange.size    = cast(u64)args.size;
    info.addressRange.stride  = draw_stride;
    info.addressFlags         = VK_ADDRESS_COMMAND_FULLY_BOUND_BIT_KHR;
    info.drawCount            = draw_count;
    vkCmdDrawMeshTasksIndirect2EXT(cmd->vk_cmd, &info);
}

internal void
rb_vk_dispatch(RB_CommandBuffer cmd_h, RB_GpuAddress root, u32 group_x, u32 group_y, u32 group_z)
{
    RB_VK_CommandBuffer *cmd = RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, cmd_h);
    rb_vk_assert_outside_suspended_pass(cmd);
    rb_vk_root_push(cmd, root);
    vkCmdDispatch(cmd->vk_cmd, group_x, group_y, group_z);
}

internal void
rb_vk_dispatch_indirect(RB_CommandBuffer cmd_h, RB_GpuAddress root, RB_GpuRange args)
{
    RB_VK_CommandBuffer *cmd = RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, cmd_h);
    rb_vk_assert_outside_suspended_pass(cmd);
    rb_vk_root_push(cmd, root);

    if(!rb_vk_g_ctx->device.device_address_commands){
        VkDeviceSize offset = 0;
        VkBuffer vk_buffer = rb_vk_buffer_from_gpu_address(args.gpu, &offset);
        vkCmdDispatchIndirect(cmd->vk_cmd, vk_buffer, offset);
        return;
    }
    VkDispatchIndirect2InfoKHR info = {0};
    info.sType                = VK_STRUCTURE_TYPE_DISPATCH_INDIRECT_2_INFO_KHR;
    info.addressRange.address = args.gpu;
    info.addressRange.size    = cast(u64)args.size;
    info.addressFlags         = VK_ADDRESS_COMMAND_FULLY_BOUND_BIT_KHR;
    vkCmdDispatchIndirect2KHR(cmd->vk_cmd, &info);
}

internal void
rb_vk_write_timestamp(RB_CommandBuffer cmd_h, RB_GpuAddress dst, RB_StageFlags stage)
{
    RB_VK_CommandBuffer *cmd = RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, cmd_h);
    if(!cmd->timestamp_pool){ return; }
    DOT_ASSERT(cmd->timestamp_count < rb_vk_g_ctx->device.timestamp_query_count, "More than RB_DeviceDesc.timestamp_query_max (%u) timestamps in one command buffer", rb_vk_g_ctx->device.timestamp_query_count);
    DOT_ASSERT((dst & 7) == 0, "Timestamp destination must be 8-byte aligned");
    if(!cmd->in_render_pass){ rb_vk_assert_outside_suspended_pass(cmd); }

    cmd->timestamp_dsts[cmd->timestamp_count] = dst;
    vkCmdWriteTimestamp2(cmd->vk_cmd, rb_vk_pipeline_stage_flags2_from_rb_stage(stage), cmd->timestamp_pool, cmd->timestamp_count);
    cmd->timestamp_count += 1;
}

internal void
rb_vk_timestamps_copy(VkCommandBuffer vk_cmd, RB_VK_CommandBuffer *owner)
{
    for(u32 i = 0; i < owner->timestamp_count; ++i){
        VkQueryResultFlags result_flags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT;
        if(!rb_vk_g_ctx->device.device_address_commands){
            VkDeviceSize offset = 0;
            VkBuffer vk_buffer = rb_vk_buffer_from_gpu_address(owner->timestamp_dsts[i], &offset);
            vkCmdCopyQueryPoolResults(vk_cmd, owner->timestamp_pool, i, 1, vk_buffer, offset, sizeof(u64), result_flags);
            continue;
        }
        VkStridedDeviceAddressRangeKHR dst_range = {0};
        dst_range.address = owner->timestamp_dsts[i];
        dst_range.size    = sizeof(u64);
        dst_range.stride  = sizeof(u64);
        vkCmdCopyQueryPoolResultsToMemoryKHR(vk_cmd, owner->timestamp_pool, i, 1, &dst_range, VK_ADDRESS_COMMAND_FULLY_BOUND_BIT_KHR,
            result_flags);
    }
}

internal void
rb_vk_commands_end_for_submit(RB_CommandBuffer *cmds, u32 count, VkCommandBufferSubmitInfo *out_infos)
{
    u32 deferred_from = 0;
    for(u32 i = 0; i < count; ++i){
        RB_VK_CommandBuffer *cmd = RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, cmds[i]);
        DOT_ASSERT(!cmd->in_render_pass, "Submitting a command buffer with a render pass still open");
        DOT_ASSERT(cmd->begins_resuming == (i > 0 && (RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, cmds[i-1]))->ends_suspending),
            "Command buffer %u: a resuming render pass must be the first pass of the buffer right after the suspending one, in the same submit", i);
        DOT_ASSERT(!cmd->ends_suspending || i + 1 < count, "The last command buffer of a submit ends with a suspended render pass");

        if(!cmd->ends_suspending){
            b32 copied = false;
            for(u32 j = deferred_from; j <= i; ++j){
                RB_VK_CommandBuffer *owner = RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, cmds[j]);
                rb_vk_timestamps_copy(cmd->vk_cmd, owner);
                copied |= owner->timestamp_count > 0;
            }
            deferred_from = i + 1;
            if(copied){
                VkMemoryBarrier2 barrier = {0};
                barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
                barrier.srcStageMask  = VK_PIPELINE_STAGE_2_COPY_BIT;
                barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
                barrier.dstStageMask  = VK_PIPELINE_STAGE_2_HOST_BIT;
                barrier.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
                VkDependencyInfo dep_info = {0};
                dep_info.sType              = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                dep_info.memoryBarrierCount = 1;
                dep_info.pMemoryBarriers    = &barrier;
                vkCmdPipelineBarrier2(cmd->vk_cmd, &dep_info);
            }
        }
        RB_VK_CHECK(vkEndCommandBuffer(cmd->vk_cmd));

        out_infos[i] = (VkCommandBufferSubmitInfo){
	        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
	        .commandBuffer = cmd->vk_cmd,
        };
    }
}


internal void
rb_vk_submit(RB_SubmitDesc *desc)
{
    u32 count = desc->command_buffer_count;
    DOT_ASSERT(count > 0, "Submitting needs at least one command buffer");
    for(u32 i = 0; i < count; ++i){
        DOT_ASSERT(!RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, desc->command_buffers[i])->holds_swapchain, "A command buffer passed to rb_swapchain_frame_acquire must be submitted with rb_submit_and_present");
    }
    Scratch scratch = scratch_begin(0,0);
    VkCommandBufferSubmitInfo *cmd_infos = PUSH_ARRAY(scratch.arena, VkCommandBufferSubmitInfo, count);
    rb_vk_commands_end_for_submit(desc->command_buffers, count, cmd_infos);

    VkSemaphoreSubmitInfo signal_info = {0};
    signal_info.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signal_info.semaphore = RB_HANDLE_UNPACK(RB_VK_TimelineSemaphore, desc->completion.semaphore);
    signal_info.value     = desc->completion.value;
    signal_info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    VkSubmitInfo2 submit = {0};
    submit.sType                     = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit.commandBufferInfoCount    = count;
    submit.pCommandBufferInfos       = cmd_infos;
    submit.signalSemaphoreInfoCount  = 1;
    submit.pSignalSemaphoreInfos     = &signal_info;
    RB_VK_CHECK(vkQueueSubmit2(rb_vk_g_ctx->device.graphics_queue, 1, &submit, VK_NULL_HANDLE));
    scratch_end(scratch);
}

internal void
rb_vk_submit_and_present(RB_SubmitDesc *desc)
{
    RB_VK_Swapchain *swapchain = &rb_vk_g_ctx->swapchain;
    DOT_ASSERT(swapchain->acquired && swapchain->transition_commands && swapchain->present_context, "Presenting without an acquired swapchain image");
    RB_VK_PresentContext *context = swapchain->present_context;
    RB_VK_SwapchainImage *swapchain_image = &swapchain->swapchain_images[swapchain->image_index];

    u32 count = desc->command_buffer_count;
    DOT_ASSERT(count > 0, "Presenting needs at least one command buffer");

#if DOT_ENABLED(DOT_DEBUG)
    b32 contains_swapchain = false;
    for(u32 i = 0; i < count; ++i){
        RB_VK_CommandBuffer *cmd = RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, desc->command_buffers[i]);
        contains_swapchain |= cmd == swapchain->transition_commands;
    }
    DOT_ASSERT(contains_swapchain, "The presenting submission must include the command buffer passed to rb_swapchain_frame_acquire");
#endif

    RB_VK_CommandBuffer *last = RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, desc->command_buffers[count - 1]);
    DOT_ASSERT(!last->in_render_pass && !last->ends_suspending, "Present needs every render pass ended");
    rb_vk_image_transition(last->vk_cmd, swapchain_image->vk_image, VK_IMAGE_ASPECT_COLOR_BIT, swapchain_image->vk_layout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    swapchain_image->vk_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    Scratch scratch = scratch_begin(0,0);
    VkCommandBufferSubmitInfo *cmd_infos = PUSH_ARRAY(scratch.arena, VkCommandBufferSubmitInfo, count);
    rb_vk_commands_end_for_submit(desc->command_buffers, count, cmd_infos);

    VkSemaphoreSubmitInfo wait_info = {
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = context->acquired,
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    };

    VkSemaphoreSubmitInfo signal_infos[] = {
        {
            .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = context->rendered,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        },
        {
            .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = RB_HANDLE_UNPACK(RB_VK_TimelineSemaphore, desc->completion.semaphore),
            .value     = desc->completion.value,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        }
    };

    {
        VkSubmitInfo2 submit = {0};
        submit.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submit.waitSemaphoreInfoCount   = 1;
        submit.pWaitSemaphoreInfos      = &wait_info;
        submit.commandBufferInfoCount   = count;
        submit.pCommandBufferInfos      = cmd_infos;
        submit.signalSemaphoreInfoCount = 2;
        submit.pSignalSemaphoreInfos    = signal_infos;
        RB_VK_CHECK(vkQueueSubmit2(rb_vk_g_ctx->device.graphics_queue, 1, &submit, VK_NULL_HANDLE));

        RB_VK_CHECK(vkResetFences(rb_vk_g_ctx->device.vk_device, 1, &context->presented));
        VkSwapchainPresentFenceInfoKHR fence_info = {
            .sType          = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_KHR,
            .swapchainCount = 1,
            .pFences        = &context->presented,
        };
        VkPresentInfoKHR presentInfo = {
	        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
	        .pNext = &fence_info,
	        .pSwapchains = &swapchain->swapchain,
	        .swapchainCount = 1,
	        .pWaitSemaphores = &context->rendered,
	        .waitSemaphoreCount = 1,
	        .pImageIndices = &swapchain->image_index,
	    };
	    VkResult present_result = vkQueuePresentKHR(rb_vk_g_ctx->device.graphics_queue, &presentInfo);
	    if(present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR){
	        swapchain->recreate_required = true;
	    }else{
	        RB_VK_CHECK(present_result);
	    }
	}
    swapchain->transition_commands->holds_swapchain = false;
    swapchain->transition_commands = NULL;
    swapchain->present_context     = NULL;
    swapchain->acquired            = false;
    rb_vk_g_ctx->next_present_context = (rb_vk_g_ctx->next_present_context + 1) % rb_vk_g_ctx->present_context_count;
    scratch_end(scratch);
}

internal void
rb_vk_end()
{
    VkDevice device = rb_vk_g_ctx->device.vk_device;

    vkDeviceWaitIdle(device);
    rb_vk_present_contexts_wait();

    for(u32 i = 0; i < rb_vk_g_ctx->present_context_count; ++i){
        RB_VK_PresentContext *context = &rb_vk_g_ctx->present_contexts[i];
        vkDestroyFence(device, context->presented, NULL);
        vkDestroySemaphore(device, context->acquired, NULL);
        vkDestroySemaphore(device, context->rendered, NULL);
    }

    rb_vk_swapchain_images_destroy();

    vkDestroySwapchainKHR(device, rb_vk_g_ctx->swapchain.swapchain, NULL);
    vkDestroySurfaceKHR(rb_vk_g_ctx->instance, rb_vk_g_ctx->surface, NULL);
    vkDestroyDevice(rb_vk_g_ctx->device.vk_device, NULL);
    if(RB_VK_DEBUG_UTILS_ENABLE){
        if(vkDestroyDebugUtilsMessengerEXT){
            vkDestroyDebugUtilsMessengerEXT(rb_vk_g_ctx->instance, rb_vk_g_ctx->debug_messenger, NULL);
        }
    }
    vkDestroyInstance(rb_vk_g_ctx->instance, NULL);
}

internal RB_SizeAlign
rb_vk_texture_size_align(RB_TextureDesc *desc)
{
    VkImageCreateInfo image_create_info = rb_vk_image_create_info_from_texture_desc(desc);

    VkDeviceImageMemoryRequirements device_image_reqs = {0};
    device_image_reqs.sType       = VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS;
    device_image_reqs.pCreateInfo = &image_create_info;

    VkMemoryRequirements2 mem_reqs2 = {0};
    mem_reqs2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    vkGetDeviceImageMemoryRequirements(rb_vk_g_ctx->device.vk_device, &device_image_reqs, &mem_reqs2);

    RB_SizeAlign size_align = {0};
    size_align.size  = cast(i64)mem_reqs2.memoryRequirements.size;
    size_align.align = cast(i64)mem_reqs2.memoryRequirements.alignment;
    return(size_align);
}


internal RB_VK_GpuMemoryProperties
rb_vk_gpu_memory_properties_from_memory_kind(RB_MemoryKind mem_kind)
{
    VkBufferUsageFlags superset_usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
        | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
        | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
        | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    RB_VK_GpuMemoryProperties props = {0};
    props.usage = superset_usage;
    switch(mem_kind){
    default: DOT_ERROR("Invalid gpu alloc memory kind %d", mem_kind);
    case RB_MemoryKind_GpuOnly:
        props.required  = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        props.forbidden = rb_vk_g_ctx->device.is_integrated_gpu ? 0 : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    break;
    case RB_MemoryKind_CpuToGpu:
        props.required  = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        props.preferred = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    break;
    case RB_MemoryKind_Readback:
        props.required  = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            | VK_MEMORY_PROPERTY_HOST_CACHED_BIT
            | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        props.forbidden = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    break;
    case RB_MemoryKind_TextureDescriptorHeap:
    case RB_MemoryKind_SamplerDescriptorHeap:
        props.required  = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        props.preferred = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        props.usage    |= VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT;
    break;
    };

    return(props);
}

internal u32
rb_vk_find_memory_type(u32 type_bits, RB_VK_GpuMemoryProperties properties, u64 size)
{
    VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {0};
    budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;

    VkPhysicalDeviceMemoryProperties2 memory_properties2 = {0};
    memory_properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    memory_properties2.pNext = rb_vk_g_ctx->device.memory_budget ? &budget : NULL;
    vkGetPhysicalDeviceMemoryProperties2(rb_vk_g_ctx->device.vk_gpu, &memory_properties2);
    VkPhysicalDeviceMemoryProperties memory_properties = memory_properties2.memoryProperties;

    i64 best = -1;
    for(u32 i = 0; i < memory_properties.memoryTypeCount; ++i){
        if(type_bits != 0 && (type_bits & (1u << i)) == 0){
            continue;
        }
        VkMemoryPropertyFlags flags = memory_properties.memoryTypes[i].propertyFlags;

        if(DOT_BITS_ANY(flags, RB_VK_MEMORY_TYPE_ALWAYS_FORBIDDEN))
            continue;

        if(DOT_BITS_ANY(flags, properties.forbidden))
            continue;

        if(!DOT_BITS_MATCH(flags, properties.required))
            continue;

        if(properties.preferred && DOT_BITS_MATCH(flags, properties.preferred)){
            u32 heap_idx = memory_properties.memoryTypes[i].heapIndex;
            VkDeviceSize available = memory_properties.memoryHeaps[heap_idx].size;
            if(rb_vk_g_ctx->device.memory_budget){
                VkDeviceSize used     = budget.heapUsage[heap_idx];
                VkDeviceSize capacity = budget.heapBudget[heap_idx];
                available = used < capacity ? capacity - used : 0;
            }
            if(available >= size){
                return cast(u32)i;
            }
            continue;
        }

        if(best < 0)
            best = cast(u32)i;
    }

    if(best < 0){
        return U32_MAX;
    }
    return(cast(u32)best);
}

internal i32
rb_vk_heap_range_compare(void *element, void *key)
{
    VkDeviceAddress base    = (cast(RB_VK_GpuHeapRange*) element)->base;
    VkDeviceAddress address = *cast(VkDeviceAddress*) key;
    return(base < address ? -1 : base > address ? 1 : 0);
}

internal void
rb_vk_heap_range_insert(RB_VK_GpuHeap *heap)
{
    RB_VK_GpuHeapRange range = { .base = heap->gpu_base, .heap = heap };
    BINARY_SEARCH_INSERT(rb_vk_g_ctx->heap_ranges, &rb_vk_g_ctx->heap_range_count, RB_VK_GPU_HEAP_MAX, &range, &range.base, rb_vk_heap_range_compare);
}

internal void
rb_vk_heap_range_remove(RB_VK_GpuHeap *heap)
{
    i64 idx = BINARY_SEARCH_FIND(rb_vk_g_ctx->heap_ranges, rb_vk_g_ctx->heap_range_count, &heap->gpu_base, rb_vk_heap_range_compare);
    if(idx < 0 || rb_vk_g_ctx->heap_ranges[idx].heap != heap){ return; }
    BINARY_SEARCH_REMOVE_AT(rb_vk_g_ctx->heap_ranges, &rb_vk_g_ctx->heap_range_count, idx);
}

// (jd) The API only ever hands us a raw RB_GpuAddress, so the legacy commands recover the
// owning heap here: the offset is just the distance from that heap's base.
internal VkBuffer
rb_vk_buffer_from_gpu_address(RB_GpuAddress address, VkDeviceSize *out_offset)
{
    i64 idx = BINARY_SEARCH_LAST_LE(rb_vk_g_ctx->heap_ranges, rb_vk_g_ctx->heap_range_count, &address, rb_vk_heap_range_compare);
    if(idx >= 0){
        RB_VK_GpuHeap *heap = rb_vk_g_ctx->heap_ranges[idx].heap;
        if(address < heap->gpu_base + cast(u64)heap->size){
            *out_offset = address - heap->gpu_base;
            return(heap->vk_buffer);
        }
    }
    DOT_ERROR("GPU address 0x%llx doesn't belong to any live heap", address);
}

internal RB_GpuHeap
rb_vk_gpu_heap_create(RB_MemoryKind kind, i64 bytes)
{
    VkDevice vk_device = rb_vk_g_ctx->device.vk_device;
    RB_VK_GpuMemoryProperties props = rb_vk_gpu_memory_properties_from_memory_kind(kind);
    b8 descriptor_heap = (kind == RB_MemoryKind_TextureDescriptorHeap) || (kind == RB_MemoryKind_SamplerDescriptorHeap);
    b8 host_visible = (kind == RB_MemoryKind_CpuToGpu) || (kind == RB_MemoryKind_Readback) || descriptor_heap;

    RB_VK_GpuHeap *vk_gpu_heap = rb_vk_g_ctx->head;
    if(vk_gpu_heap){
        SLL_STACK_POP(rb_vk_g_ctx->head);
        MEMORY_ZERO_STRUCT(vk_gpu_heap);
    }else{
        vk_gpu_heap = PUSH_STRUCT(rb_vk_g_ctx->permanent_arena, RB_VK_GpuHeap);
    }
    vk_gpu_heap->kind = kind;
    vk_gpu_heap->size = bytes;

    u64 buffer_size = bytes ? cast(u64)bytes : 1;
    u64 base_alignment = 1;
    if(descriptor_heap){
        RB_VK_DescriptorHeapLayout layout = rb_vk_descriptor_heap_layout(kind);
        base_alignment = DOT_MAX(layout.heap_alignment, 1);
        buffer_size    = ALIGN_POW2(buffer_size, layout.reserved_alignment) + layout.reserved_size + base_alignment - 1;
    }

    VkBufferCreateInfo buffer_create_info = {0};
    buffer_create_info.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size        = buffer_size;
    buffer_create_info.usage       = props.usage;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkDeviceBufferMemoryRequirements device_buffer_reqs = {0};
    device_buffer_reqs.sType       = VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS;
    device_buffer_reqs.pCreateInfo = &buffer_create_info;

    VkMemoryRequirements2 mem_reqs2 = {0};
    mem_reqs2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;

    vkGetDeviceBufferMemoryRequirements(vk_device, &device_buffer_reqs, &mem_reqs2);

    u32 memory_type_idx = rb_vk_find_memory_type(mem_reqs2.memoryRequirements.memoryTypeBits, props, mem_reqs2.memoryRequirements.size);
    if(memory_type_idx == U32_MAX){
        DOT_ERROR("No suitable memory type for gpu alloc kind %d", kind);
    }

    VkMemoryAllocateFlagsInfo alloc_flags_info = {0};
    alloc_flags_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    alloc_flags_info.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo alloc_info = {0};
    alloc_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.pNext           = &alloc_flags_info;
    alloc_info.memoryTypeIndex = memory_type_idx;
    alloc_info.allocationSize  = mem_reqs2.memoryRequirements.size;

    RB_VK_CHECK(vkAllocateMemory(vk_device, &alloc_info, NULL, &vk_gpu_heap->vk_memory));
    vk_gpu_heap->alloc_size = cast(i64)alloc_info.allocationSize;

    RB_VK_CHECK(vkCreateBuffer(vk_device, &buffer_create_info, NULL, &vk_gpu_heap->vk_buffer));
    RB_VK_CHECK(vkBindBufferMemory(vk_device, vk_gpu_heap->vk_buffer, vk_gpu_heap->vk_memory, 0));

    VkBufferDeviceAddressInfo bda_info = {0};
    bda_info.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bda_info.buffer = vk_gpu_heap->vk_buffer;
    VkDeviceAddress buffer_address = vkGetBufferDeviceAddress(vk_device, &bda_info);
    vk_gpu_heap->gpu_base = ALIGN_POW2(buffer_address, base_alignment);

    rb_vk_heap_range_insert(vk_gpu_heap);

    if(host_visible){
        RB_VK_CHECK(vkMapMemory(vk_device, vk_gpu_heap->vk_memory, 0, VK_WHOLE_SIZE, 0, &vk_gpu_heap->cpu_base));
        vk_gpu_heap->cpu_base = cast(u8*) vk_gpu_heap->cpu_base + (vk_gpu_heap->gpu_base - buffer_address);
    }

    RB_GpuHeap ret = {0};
    ret.h = cast(u64)vk_gpu_heap;
    return(ret);
}

internal RB_GpuAddress
rb_vk_gpu_heap_address(RB_GpuHeap heap)
{
    RB_VK_GpuHeap *vk_gpu_heap = RB_HANDLE_UNPACK(RB_VK_GpuHeap*, heap);
    return(vk_gpu_heap->gpu_base);
}

internal void
rb_vk_gpu_heap_write(RB_GpuHeap heap, i64 offset, void *data, i64 size)
{
    RB_VK_GpuHeap *vk_gpu_heap = RB_HANDLE_UNPACK(RB_VK_GpuHeap*, heap);
    DOT_ASSERT(vk_gpu_heap->cpu_base, "Heap isn't host-visible (only CpuToGpu heaps can be written)");
    DOT_ASSERT(offset + size <= vk_gpu_heap->alloc_size);
    MEMORY_COPY(cast(u8*)vk_gpu_heap->cpu_base + offset, data, cast(u64)size);
}

internal VkDeviceMemory
rb_vk_image_memory_alloc(VkImage vk_image)
{
    VkDevice vk_device = rb_vk_g_ctx->device.vk_device;
    VkMemoryRequirements2 reqs = rb_vk_image_memory_requirements(vk_device, vk_image);

    RB_VK_GpuMemoryProperties props = rb_vk_gpu_memory_properties_from_memory_kind(RB_MemoryKind_GpuOnly);
    u32 memory_type_idx = rb_vk_find_memory_type(reqs.memoryRequirements.memoryTypeBits, props, reqs.memoryRequirements.size);
    if(memory_type_idx == U32_MAX){
        DOT_ERROR("No suitable device-local memory type for image");
    }

    VkMemoryAllocateInfo alloc_info = {0};
    alloc_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.memoryTypeIndex = memory_type_idx;
    alloc_info.allocationSize  = reqs.memoryRequirements.size;

    VkDeviceMemory vk_memory = VK_NULL_HANDLE;
    RB_VK_CHECK(vkAllocateMemory(vk_device, &alloc_info, NULL, &vk_memory));
    RB_VK_CHECK(vkBindImageMemory(vk_device, vk_image, vk_memory, 0));
    return(vk_memory);
}

// (claude) Memory requirements of an unsupported create-info are undefined; gate every probe on this.
internal b8
rb_vk_image_create_info_supported(VkImageCreateInfo *info)
{
    VkPhysicalDeviceImageFormatInfo2 format_info = {0};
    format_info.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
    format_info.format = info->format;
    format_info.type   = info->imageType;
    format_info.tiling = info->tiling;
    format_info.usage  = info->usage;
    format_info.flags  = info->flags;

    VkImageFormatProperties2 format_props = {0};
    format_props.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;

    if(vkGetPhysicalDeviceImageFormatProperties2(rb_vk_g_ctx->device.vk_gpu, &format_info, &format_props) != VK_SUCCESS){
        return(false);
    }

    VkImageFormatProperties *limits = &format_props.imageFormatProperties;
    return(info->extent.width  <= limits->maxExtent.width
        && info->extent.height <= limits->maxExtent.height
        && info->extent.depth  <= limits->maxExtent.depth
        && info->mipLevels     <= limits->maxMipLevels
        && info->arrayLayers   <= limits->maxArrayLayers);
}

internal void
rb_vk_texture_memory_select(void)
{
    // (jd) TODO: Add more probe tyeps as we go based on needs
    const VkImageCreateInfo probes[] = {
        {   .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType   = VK_IMAGE_TYPE_2D,
            .format      = VK_FORMAT_R8G8B8A8_UNORM,
            .extent      = {1024, 1024, 1},
            .mipLevels   = 1,
            .arrayLayers = 1,
            .samples     = VK_SAMPLE_COUNT_1_BIT,
            .tiling      = VK_IMAGE_TILING_OPTIMAL,
            .usage       = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT
                         | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                         | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT },
        {   .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType   = VK_IMAGE_TYPE_2D,
            .format      = VK_FORMAT_D32_SFLOAT_S8_UINT,
            .extent      = {1024, 1024, 1},
            .mipLevels   = 1,
            .arrayLayers = 1,
            .samples     = VK_SAMPLE_COUNT_1_BIT,
            .tiling      = VK_IMAGE_TILING_OPTIMAL,
            .usage       = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT },
    };

    u32 type_bits = U32_MAX;
    u64 align     = 16;
    for(u64 i = 0; i < DOT_ARRAY_COUNT(probes); ++i){
        VkImageCreateInfo info = probes[i];
        if(!rb_vk_image_create_info_supported(&info)){ continue; }

        VkDeviceImageMemoryRequirements device_image_reqs = {0};
        device_image_reqs.sType       = VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS;
        device_image_reqs.pCreateInfo = &info;

        VkMemoryRequirements2 mem_reqs2 = {0};
        mem_reqs2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
        vkGetDeviceImageMemoryRequirements(rb_vk_g_ctx->device.vk_device, &device_image_reqs, &mem_reqs2);

        type_bits &= mem_reqs2.memoryRequirements.memoryTypeBits;
        align      = DOT_MAX(align, mem_reqs2.memoryRequirements.alignment);
    }

    if(type_bits == 0){
        DOT_ERROR("No single memory type accepts every texture format - color and depth/stencil images cannot share one texture heap on this GPU");
    }

    RB_VK_GpuMemoryProperties props = rb_vk_gpu_memory_properties_from_memory_kind(RB_MemoryKind_GpuOnly);
    u32 memory_type_idx = rb_vk_find_memory_type(type_bits, props, 0);
    if(memory_type_idx == U32_MAX){
        DOT_ERROR("No device-local memory type among those every texture format accepts");
    }

    rb_vk_g_ctx->device.texture_memory_type    = memory_type_idx;
    rb_vk_g_ctx->device.texture_heap_alignment = align;
}

internal RB_TextureHeap
rb_vk_texture_heap_create(i64 bytes)
{
    VkMemoryAllocateInfo alloc_info = {0};
    alloc_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.memoryTypeIndex = rb_vk_g_ctx->device.texture_memory_type;
    alloc_info.allocationSize  = cast(u64)bytes;

    VkDeviceMemory vk_memory = VK_NULL_HANDLE;
    RB_VK_CHECK(vkAllocateMemory(rb_vk_g_ctx->device.vk_device, &alloc_info, NULL, &vk_memory));

    RB_TextureHeap heap = RB_HANDLE_PACK(RB_TextureHeap, vk_memory);
    return(heap);
}

internal void
rb_vk_texture_heap_destroy(RB_TextureHeap heap)
{
    if(!RB_HANDLE_VALID(heap)){ return; }
    vkFreeMemory(rb_vk_g_ctx->device.vk_device, RB_HANDLE_UNPACK(RB_VK_TextureHeap, heap), NULL);
}

internal void
rb_vk_copy_memory(RB_CommandBuffer cmd_h, RB_GpuRange src, RB_GpuRange dst)
{
    RB_VK_CommandBuffer *cmd = RB_HANDLE_UNPACK(RB_VK_CommandBuffer *, cmd_h);
    rb_vk_assert_outside_suspended_pass(cmd);
    DOT_ASSERT(src.size <= dst.size, "copy_memory destination (%llu B) smaller than source (%llu B)", dst.size, src.size);

    if(!rb_vk_g_ctx->device.device_address_commands){
        VkDeviceSize src_offset = 0;
        VkDeviceSize dst_offset = 0;
        VkBuffer src_buffer = rb_vk_buffer_from_gpu_address(src.gpu, &src_offset);
        VkBuffer dst_buffer = rb_vk_buffer_from_gpu_address(dst.gpu, &dst_offset);

        VkBufferCopy2 buffer_region = {0};
        buffer_region.sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
        buffer_region.srcOffset = src_offset;
        buffer_region.dstOffset = dst_offset;
        buffer_region.size      = cast(u64)src.size;

        VkCopyBufferInfo2 buffer_copy_info = {0};
        buffer_copy_info.sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
        buffer_copy_info.srcBuffer   = src_buffer;
        buffer_copy_info.dstBuffer   = dst_buffer;
        buffer_copy_info.regionCount = 1;
        buffer_copy_info.pRegions    = &buffer_region;
        vkCmdCopyBuffer2(cmd->vk_cmd, &buffer_copy_info);
        return;
    }

    VkDeviceMemoryCopyKHR region = {0};
    region.sType            = VK_STRUCTURE_TYPE_DEVICE_MEMORY_COPY_KHR;
    region.srcRange.address = src.gpu;
    region.srcRange.size    = cast(u64)src.size;
    region.srcFlags         = VK_ADDRESS_COMMAND_FULLY_BOUND_BIT_KHR;
    region.dstRange.address = dst.gpu;
    region.dstRange.size    = cast(u64)src.size;
    region.dstFlags         = VK_ADDRESS_COMMAND_FULLY_BOUND_BIT_KHR;

    VkCopyDeviceMemoryInfoKHR copy_info = {0};
    copy_info.sType       = VK_STRUCTURE_TYPE_COPY_DEVICE_MEMORY_INFO_KHR;
    copy_info.regionCount = 1;
    copy_info.pRegions    = &region;
    vkCmdCopyMemoryKHR(cmd->vk_cmd, &copy_info);
}

internal VkDeviceMemoryImageCopyKHR
rb_vk_texture_copy_region(RB_VK_Texture *texture, RB_GpuRange memory, RB_TextureCopyDesc copy)
{
    u32 mip_w = DOT_MAX(1u, texture->vk_extent3d.width  >> copy.mip_level);
    u32 mip_h = DOT_MAX(1u, texture->vk_extent3d.height >> copy.mip_level);
    u32 mip_d = DOT_MAX(1u, texture->vk_extent3d.depth  >> copy.mip_level);
    DOT_ASSERT(copy.mip_level < texture->mip_levels, "Texture copy mip %u out of range", copy.mip_level);
    DOT_ASSERT(copy.slice < texture->array_layers, "Texture copy slice %u out of range", copy.slice);

    VkDeviceMemoryImageCopyKHR region = {
        .sType                           = VK_STRUCTURE_TYPE_DEVICE_MEMORY_IMAGE_COPY_KHR,
        .addressRange.address            = memory.gpu,
        .addressRange.size               = cast(u64)memory.size,
        .addressFlags                    = VK_ADDRESS_COMMAND_FULLY_BOUND_BIT_KHR,
        .imageSubresource.aspectMask     = texture->vk_aspect,
        .imageSubresource.mipLevel       = copy.mip_level,
        .imageSubresource.baseArrayLayer = copy.slice,
        .imageSubresource.layerCount     = 1,
        .imageOffset = {copy.offset.x, copy.offset.y, copy.offset.z},
        .imageExtent = {
            copy.extent.x ? cast(u32)copy.extent.x : mip_w,
            copy.extent.y ? cast(u32)copy.extent.y : mip_h,
            copy.extent.z ? cast(u32)copy.extent.z : mip_d,
        },
    };
    return(region);
}

// (jd) Same region, expressed for the classic buffer <-> image copies.
internal VkBufferImageCopy2
rb_vk_texture_buffer_copy_region(VkDeviceMemoryImageCopyKHR *region, VkDeviceSize buffer_offset)
{
    VkBufferImageCopy2 buffer_region = {0};
    buffer_region.sType            = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
    buffer_region.bufferOffset     = buffer_offset;
    buffer_region.imageSubresource = region->imageSubresource;
    buffer_region.imageOffset      = region->imageOffset;
    buffer_region.imageExtent      = region->imageExtent;
    return(buffer_region);
}

internal void
rb_vk_copy_memory_to_texture(RB_CommandBuffer cmd_h, RB_GpuRange src, RB_Texture dst, RB_TextureCopyDesc copy)
{
    RB_VK_CommandBuffer *cmd = RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, cmd_h);
    rb_vk_assert_outside_suspended_pass(cmd);
    RB_VK_Texture *texture = RB_HANDLE_UNPACK(RB_VK_Texture*, dst);

    VkDeviceMemoryImageCopyKHR region = rb_vk_texture_copy_region(texture, src, copy);
    region.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    if(!rb_vk_g_ctx->device.device_address_commands){
        VkDeviceSize offset = 0;
        VkBuffer vk_buffer = rb_vk_buffer_from_gpu_address(src.gpu, &offset);
        VkBufferImageCopy2 buffer_region = rb_vk_texture_buffer_copy_region(&region, offset);

        VkCopyBufferToImageInfo2 buffer_copy_info = {0};
        buffer_copy_info.sType          = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
        buffer_copy_info.srcBuffer      = vk_buffer;
        buffer_copy_info.dstImage       = texture->vk_image;
        buffer_copy_info.dstImageLayout = VK_IMAGE_LAYOUT_GENERAL;
        buffer_copy_info.regionCount    = 1;
        buffer_copy_info.pRegions       = &buffer_region;
        vkCmdCopyBufferToImage2(cmd->vk_cmd, &buffer_copy_info);
        return;
    }

    VkCopyDeviceMemoryImageInfoKHR copy_info = {0};
    copy_info.sType       = VK_STRUCTURE_TYPE_COPY_DEVICE_MEMORY_IMAGE_INFO_KHR;
    copy_info.image       = texture->vk_image;
    copy_info.regionCount = 1;
    copy_info.pRegions    = &region;
    vkCmdCopyMemoryToImageKHR(cmd->vk_cmd, &copy_info);
}

internal void
rb_vk_copy_texture_to_memory(RB_CommandBuffer cmd_h, RB_Texture src, RB_GpuRange dst, RB_TextureCopyDesc copy)
{
    RB_VK_CommandBuffer *cmd = RB_HANDLE_UNPACK(RB_VK_CommandBuffer*, cmd_h);
    rb_vk_assert_outside_suspended_pass(cmd);
    RB_VK_Texture *texture = RB_HANDLE_UNPACK(RB_VK_Texture*, src);
    DOT_ASSERT(texture->vk_usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT, "Texture '%s' can't be copied from (depth/stencil textures have no transfer usage)", texture->debug_name.buff);

    VkDeviceMemoryImageCopyKHR region = rb_vk_texture_copy_region(texture, dst, copy);
    region.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    if(!rb_vk_g_ctx->device.device_address_commands){
        VkDeviceSize offset = 0;
        VkBuffer vk_buffer = rb_vk_buffer_from_gpu_address(dst.gpu, &offset);
        VkBufferImageCopy2 buffer_region = rb_vk_texture_buffer_copy_region(&region, offset);

        VkCopyImageToBufferInfo2 buffer_copy_info = {0};
        buffer_copy_info.sType          = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2;
        buffer_copy_info.srcImage       = texture->vk_image;
        buffer_copy_info.srcImageLayout = VK_IMAGE_LAYOUT_GENERAL;
        buffer_copy_info.dstBuffer      = vk_buffer;
        buffer_copy_info.regionCount    = 1;
        buffer_copy_info.pRegions       = &buffer_region;
        vkCmdCopyImageToBuffer2(cmd->vk_cmd, &buffer_copy_info);
        return;
    }

    VkCopyDeviceMemoryImageInfoKHR copy_info = {0};
    copy_info.sType       = VK_STRUCTURE_TYPE_COPY_DEVICE_MEMORY_IMAGE_INFO_KHR;
    copy_info.image       = texture->vk_image;
    copy_info.regionCount = 1;
    copy_info.pRegions    = &region;
    vkCmdCopyImageToMemoryKHR(cmd->vk_cmd, &copy_info);
}

internal void
rb_vk_gpu_heap_destroy(RB_GpuHeap heap)
{
    if(heap.h == 0){return;}
    RB_VK_GpuHeap *vk_gpu_heap = RB_HANDLE_UNPACK(RB_VK_GpuHeap*, heap);

    rb_vk_heap_range_remove(vk_gpu_heap);

    if(vk_gpu_heap->vk_buffer != VK_NULL_HANDLE){
        vkDestroyBuffer(rb_vk_g_ctx->device.vk_device, vk_gpu_heap->vk_buffer, NULL);
    }
    vkFreeMemory(rb_vk_g_ctx->device.vk_device, vk_gpu_heap->vk_memory, NULL); // (jd) implicitly unmaps

    SLL_STACK_PUSH(rb_vk_g_ctx->head, vk_gpu_heap);
}
