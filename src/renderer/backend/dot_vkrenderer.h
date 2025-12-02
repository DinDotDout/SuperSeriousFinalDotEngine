typedef struct DOT_RendererBackendDevice {
        VkDevice device;
        VkPhysicalDevice gpu;
        VkQueue graphics_queue;
        VkQueue present_queue;
        b8 shared_present_graphics_queues; // NOTE: keep for now but can be removed
} DOT_RendererBackendDevice;

typedef struct DOT_RendererBackendVk {
        DOT_RendererBackendBase base;
#ifndef NDEBUG
        VkDebugUtilsMessengerEXT debug_messenger; // This may trip us up if hot reloading different build types?
#endif
        DOT_RendererBackendDevice device;
        VkInstance instance;
        VkSurfaceKHR surface;
} DOT_RendererBackendVk;

// typedef struct {
//     const char** extensions;
//     size_t extension_count;
//     const char** validation_layers;
//     size_t layer_count;
// } VulkanConfig;

#ifndef NDEBUG
#define DOT_VALIDATION_LAYERS_ENABLE
#define DOT_VK_EXT_DEBUG_UTILS_ENABLE
#endif

const char* dot_renderer_backend_extension_names[] = {
        VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef DOT_VK_EXT_DEBUG_UTILS_ENABLE
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
        RGFW_VK_SURFACE, // NOTE: This should be queried from DOT_Window
};

const char* dot_renderer_backend_device_extension_names[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

const char* dot_vk_layers[] = {
#ifdef DOT_VALIDATION_LAYERS_ENABLE
        "VK_LAYER_KHRONOS_validation",
#endif
};

internal DOT_RendererBackendVk* DOT_RendererBackendBase_AsVk(DOT_RendererBackendBase* base);
internal DOT_RendererBackendVk* DOT_RendererBackendVk_Create(Arena* arena);
internal void DOT_RendererBackendVk_InitVulkan(DOT_RendererBackendBase* ctx, DOT_Window* window);
internal void DOT_RendererBackendVk_ShutdownVulkan(DOT_RendererBackendBase* ctx);
