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
enum
{
    RBVK_TEXURE_MAX = 512,
    RBVK_BUFFER_MAX = 4096,
    RBVK_SAMPLER_MAX = 128,

    RBVK_CTX_RESCOURCE_POOL = 128,
    RBVK_RESOURCE_CLEANUP_CTX_TEXTURES  = 24,
    RBVK_RESOURCE_CLEANUP_CTX_BUFFERS   = 24,
    RBVK_RESOURCE_CLEANUP_CTX_SAMPLERS  = 24,

    RENDER_BACKEND_ALLOC_SIZE =
        (RBVK_CTX_RESCOURCE_POOL * RBVK_RESOURCE_CLEANUP_CTX_TEXTURES * RBVK_RESOURCE_CLEANUP_CTX_BUFFERS * RBVK_RESOURCE_CLEANUP_CTX_SAMPLERS * sizeof(PoolHandle))
        // (jd) RBVK_Texture isn't yet included here, maybe we need to make render_backend_vk_types
        // + RBVK_TEXURE_MAX * sizeof(RBVK_Texture)
        // + RBVK_BUFFER_MAX * sizeof(RBVK_Buffer)
        // + RBVK_SAMPLER_MAX * sizeof(RBVK_Sampler)
};

typedef struct RBVK_VulkanConfig{
    struct {
        struct{
            String8 *data;
            usize    count;
        }extensions;

        String8 application_name;
        u32     application_version;

        String8 engine_name;
        u32     engine_version;

        u32     api_version;
    }instance;

    struct{
        struct{
            String8 *data;
            usize    count;
        }extensions;

        const void *features; // pNext chain
    } device;

    struct{
        String8 *data;
        usize    count;
    }validation_layers;

}RBVK_VulkanConfig;

typedef struct RBVK_RenderSettings{
    struct {
        VkFormat        preferred_format;
        VkColorSpaceKHR preferred_colorspace;
        VkPresentModeKHR preferred_present_mode;
    } swapchain;

    struct {
        u8 frame_overlap;
    } frame;
}RBVK_RendererSettings;

read_only global RBVK_VulkanConfig g_rbvk_vk_config = {
    .instance = {
        .application_name    = String8Lit("dot_engine"),
        .application_version = VK_MAKE_VERSION(1,0,0),
        .engine_name         = String8Lit("dot_engine"),
        .engine_version      = VK_MAKE_VERSION(1,0,0),
        .api_version         = VK_API_VERSION_1_4,
        .extensions = SLICE_INIT(String8, {
            String8Lit(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME),
            String8Lit(VK_KHR_SURFACE_EXTENSION_NAME),
#ifdef VK_EXT_DEBUG_UTILS_ENABLE
            String8Lit(VK_EXT_DEBUG_UTILS_EXTENSION_NAME),
#endif
            String8Lit(DOT_VK_SURFACE),
        }),
    },

    .validation_layers = SLICE_INIT(String8, {
#ifdef VALIDATION_LAYERS_ENABLE
        String8Lit("VK_LAYER_KHRONOS_validation"),
#endif
    }),

    .device = {
        .extensions = SLICE_INIT(String8, {
            String8Lit(VK_KHR_SWAPCHAIN_EXTENSION_NAME),
            String8Lit(VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME),
            String8Lit(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME),
            String8Lit(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME),
            String8Lit(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME),
        }),

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

// TODO: This should all come from general renderer settings, and be filled with those rather than providing them here
RBVK_RenderSettings g_rbvk_render_settings = {
    .swapchain = {
        .preferred_format       = VK_FORMAT_B8G8R8A8_SRGB,
        .preferred_colorspace   = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .preferred_present_mode = VK_PRESENT_MODE_FIFO_KHR,
    },
    .frame = {
        .frame_overlap = 3,
    },
};

#endif // !RENDER_BACKEND_CONFING_H
