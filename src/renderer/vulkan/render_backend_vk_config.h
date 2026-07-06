#ifndef RENDER_BACKEND_CONFING_H
#define RENDER_BACKEND_CONFING_H

#if DOT_DEBUG
#   define VALIDATION_LAYERS_ENABLE 1
#   define VK_EXT_DEBUG_UTILS_ENABLE 1
#else
#   define VALIDATION_LAYERS_ENABLE 0
#   define VK_EXT_DEBUG_UTILS_ENABLE 0
#endif

// TODO:This things will be useful for debugging!
// Make stub and add to all those pfns in case we do not find one it just won't do anything:

// pfnSetDebugUtilsObjectNameEXT    = (PFN_vkSetDebugUtilsObjectNameEXT)    vkGetDeviceProcAddr(vulkan_device, "vkSetDebugUtilsObjectNameEXT");
// pfnCmdBeginDebugUtilsLabelEXT    = (PFN_vkCmdBeginDebugUtilsLabelEXT)    vkGetDeviceProcAddr(vulkan_device, "vkCmdBeginDebugUtilsLabelEXT");
// pfnCmdEndDebugUtilsLabelEXT      = (PFN_vkCmdEndDebugUtilsLabelEXT)      vkGetDeviceProcAddr(vulkan_device, "vkCmdEndDebugUtilsLabelEXT");

// (jd) TODO: Make this dynamic. (json or something)
//      Also these should really be renderer config

typedef struct RN_VK_VulkanConfig{
    struct {
        // SLICE(String8) extensions;
        u32     extension_count;
        String8 *extensions;

        String8 application_name;
        u32     application_version;

        String8 engine_name;
        u32     engine_version;

        u32     api_version;
    }instance;

    struct{
        u32 extension_count;
        String8 *extensions;
        const void *features; // pNext chain
    }device;

    u32     validation_layer_count;
    String8 *validation_layers;

}RN_VK_VulkanConfig;

typedef struct RN_VK_RenderSettings{
    struct {
        VkFormat        preferred_format;
        VkColorSpaceKHR preferred_colorspace;
        VkPresentModeKHR preferred_present_mode;
    } swapchain;

    struct {
        u8 frame_overlap;
    } frame;
}RN_VK_RenderSettings;

global String8 rn_vk_g_instance_extensions[] = {
    string8_lit(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME),
    string8_lit(VK_KHR_SURFACE_EXTENSION_NAME),
#ifdef VK_EXT_DEBUG_UTILS_ENABLE
    string8_lit(VK_EXT_DEBUG_UTILS_EXTENSION_NAME),
#endif
    string8_lit(DOT_VK_SURFACE),
};

global String8 rn_vk_g_validation_layers[] = {
#ifdef VALIDATION_LAYERS_ENABLE
        string8_lit("VK_LAYER_KHRONOS_validation"),
#endif
};

global String8 rn_vk_g_device_extensions[] = {
            string8_lit(VK_KHR_SWAPCHAIN_EXTENSION_NAME),
            string8_lit(VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME),
            string8_lit(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME),
            string8_lit(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME),
            string8_lit(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME),
};

read_only global RN_VK_VulkanConfig rn_vk_g_config = {
    .instance = {
        .application_name    = string8_lit("dot_engine"),
        .application_version = VK_MAKE_VERSION(1,0,0),
        .engine_name         = string8_lit("dot_engine"),
        .engine_version      = VK_MAKE_VERSION(1,0,0),
        .api_version         = VK_API_VERSION_1_4,

        .extension_count     = DOT_ARRAY_COUNT(rn_vk_g_instance_extensions),
        .extensions          = rn_vk_g_instance_extensions,
    },
    .validation_layer_count = DOT_ARRAY_COUNT(rn_vk_g_validation_layers),
    .validation_layers      = rn_vk_g_validation_layers,
    .device = {
        .extension_count    = DOT_ARRAY_COUNT(rn_vk_g_device_extensions),
        .extensions         = rn_vk_g_device_extensions,

        .features = &(VkPhysicalDeviceVulkan12Features){
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .bufferDeviceAddress = true,
            .descriptorIndexing = true,
            .descriptorBindingPartiallyBound = true,
            .descriptorBindingVariableDescriptorCount = true,
            .runtimeDescriptorArray = true,
            .pNext = &(VkPhysicalDeviceVulkan13Features){
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
                .synchronization2 = VK_TRUE,
                .dynamicRendering = true,
                .pNext = &(VkPhysicalDeviceDescriptorBufferFeaturesEXT){
                    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
                    .descriptorBuffer = VK_TRUE,
                    .pNext = &(VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT){
                        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT,
                        .graphicsPipelineLibrary = VK_TRUE,
                        .pNext = NULL,
                    },
                },
            },
        },
    },
};

#endif // !RENDER_BACKEND_CONFING_H
