#ifndef DOT_VK_RENDERER
#define DOT_VK_RENDERER

#ifndef NDEBUG
#define DOT_VALIDATION_LAYERS_ENABLE
#define DOT_VK_EXT_DEBUG_UTILS_ENABLE
#endif

typedef struct DOT_RendererBackendDevice {
        VkDevice device;
        VkPhysicalDevice gpu;
        VkQueue graphics_queue;
        VkQueue present_queue;
        b8 shared_present_graphics_queues; // NOTE: keep for now but can be removed
} DOT_RendererBackendDevice;

typedef struct DOT_RendererBackendVk {
        DOT_RendererBackendBase base;
#ifdef DOT_VK_EXT_DEBUG_UTILS_ENABLE
        VkDebugUtilsMessengerEXT debug_messenger; // This may trip us up if hot reloading different build types?
#endif
        DOT_RendererBackendDevice device;
        VkInstance instance;
        VkSurfaceKHR surface;
} DOT_RendererBackendVk;


typedef struct DOT_RendererBackendVKSettings{
    const char** instance_extension_names;
    const usize  instance_extension_count;

    const char **device_extension_names;
    const usize  device_extension_count;

    const char **layer_names;
    const usize  layer_count;
}DOT_RendererBackendVKSettings;


internal DOT_RendererBackendVk* DOT_RendererBackendBase_AsVk(DOT_RendererBackendBase* base);
internal DOT_RendererBackendVk* DOT_RendererBackendVk_Create(Arena* arena);
internal const DOT_RendererBackendVKSettings* DOT_VKSettings();

internal void DOT_RendererBackendVk_InitVulkan(DOT_RendererBackendBase* ctx, DOT_Window* window);
internal void DOT_RendererBackendVk_ShutdownVulkan(DOT_RendererBackendBase* ctx);
#endif
