#include <vulkan/vulkan_core.h>
#include "vk_helper.h"

internal DOT_RendererBackendVk* DOT_RendererBackendBase_AsVk(DOT_RendererBackendBase* base){
    DOT_ASSERT(base);
    DOT_ASSERT(base->backend_kind == DOT_RENDERER_BACKEND_VK);
    return cast(DOT_RendererBackendVk*) base;
}

internal DOT_RendererBackendVk* DOT_RendererBackendVk_Create(Arena* arena){
    DOT_RendererBackendVk* backend = PushStruct(arena, DOT_RendererBackendVk);
    DOT_RendererBackendBase* base = &backend->base;
    base->backend_kind = DOT_RENDERER_BACKEND_VK;
    base->Init = DOT_RendererBackendVk_InitVulkan;
    base->Shutdown = DOT_RendererBackendVk_ShutdownVulkan;
    base->arena = arena; // NOTE: Should probably use make arena from...
    return backend;
}

internal inline VKAPI_ATTR u32 VKAPI_CALL DOT_VkDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {
    Unused(message_type); Unused(user_data);
    if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        DOT_PRINT("validation layer: %s\n", callback_data->pMessage);
    }
    return VK_FALSE;
}


typedef struct DOT_CandidateDeviceInfo {
    VkPhysicalDevice gpu;
    u16 graphics_family;
    u16 present_family;
    i32 score;
    b8 shared_present_graphics_queues;
} DOT_CandidateDeviceInfo;

internal inline bool DOT_VkDeviceAllRequiredExtensions(VkPhysicalDevice device){
    TempArena temp = Memory_GetScratch(NULL);
    u32 extension_count;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);
    array(VkExtensionProperties) available_extensions = PushArray(temp.arena, VkExtensionProperties, extension_count);
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, available_extensions);
    bool found = true;
    for EachElement(i, dot_renderer_backend_device_extension_names){
    for EachIndex(j, extension_count){
    if (strcmp(dot_renderer_backend_device_extension_names[i], available_extensions[j].extensionName) == 0){
        DOT_PRINT("found %s", dot_renderer_backend_device_extension_names[i]);
        found = true;
        break;
    }
}
if (!found){
    DOT_WARNING("Requested layer %s not found", dot_vk_layers[i]);
    break;
}

    }
TempArena_Restore(&temp);
return found;
}

internal inline DOT_CandidateDeviceInfo DOT_VkPickBestDevice(VkInstance instance, VkSurfaceKHR surface){
    TempArena temp = Memory_GetScratch(NULL);
    u32 device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, NULL);
    if (device_count == 0) {
        DOT_ERROR("No Vulkan-capable GPUs found");
    }

    array(VkPhysicalDevice) devices = PushArray(temp.arena, VkPhysicalDevice, device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices);
    DOT_PRINT("device count %i", device_count);

    DOT_CandidateDeviceInfo best_device = {0};
    best_device.score = -1;

    for (u32 i = 0; i < device_count; i++){
        VkPhysicalDevice dev = devices[i];
        if (!DOT_VkDeviceAllRequiredExtensions(dev)){
            continue;
        }
        VkPhysicalDeviceProperties props;
        VkPhysicalDeviceFeatures feats;
        vkGetPhysicalDeviceProperties(dev, &props);
        vkGetPhysicalDeviceFeatures(dev, &feats);

        u32 queue_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queue_count, NULL);
        array(VkQueueFamilyProperties) qprops = PushArray(temp.arena, VkQueueFamilyProperties, queue_count);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queue_count, qprops);

        int graphics_idx = -1;
        int present_idx = -1;
        bool shared_present_graphics_queues = false;
        for (u32 q = 0; q < queue_count; q++){
            b32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, q, surface, &present_support);
            if ((qprops[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present_support) {
                shared_present_graphics_queues  = true;
                graphics_idx = q;
                present_idx  = q;
                break;
            }
        }
        if (!shared_present_graphics_queues){
            for (u32 q = 0; q < queue_count; q++) {
                if (qprops[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    graphics_idx = q;
                }
                u32 present_support = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(dev, q, surface, &present_support);
                if (present_support) {
                    present_idx = q;
                }
            }
        }
        if (graphics_idx < 0 || present_idx < 0){
            continue;
        }

        int score = -1;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 2000;
        // if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 100;
        if (feats.geometryShader) score += 10;
        DOT_PRINT("graphics family: %d; present family: %i; score: %i", graphics_idx, present_idx, score);
        if (score > best_device.score) {
            best_device.gpu = dev;
            best_device.graphics_family = cast(u16)graphics_idx;
            best_device.present_family = cast(u16)present_idx;
            best_device.score = score;
            best_device.shared_present_graphics_queues = shared_present_graphics_queues;
        }
    }

    if (best_device.score < 0) {
        DOT_ERROR("No suitable GPU found");
    }
    DOT_PRINT("best_device  graphics family: %d; present family: %i; score: %i", best_device.graphics_family, best_device.present_family, best_device.score);
    TempArena_Restore(&temp);
    return best_device;
}

internal inline bool DOT_VkAllLayers(){
    TempArena temp = Memory_GetScratch(NULL);
    u32 layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, NULL);
    array(VkLayerProperties) available_layers = PushArray(temp.arena, VkLayerProperties, layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers);
    bool found = true;
    for EachElement(i, dot_vk_layers){
    found = false;
    for EachIndex(j, layer_count){
    if (strcmp(dot_vk_layers[i], available_layers[j].layerName) == 0){
        DOT_PRINT("Found Layer %s", dot_vk_layers[i]);
        found = true;
        break;
    }
}
if (!found){
    DOT_WARNING("Requested layer %s not found", dot_vk_layers[i]);
    break;
}
    }
TempArena_Restore(&temp);
return found;
}

internal void DOT_RendererBackendVk_InitVulkan(DOT_RendererBackendBase* ctx, DOT_Window* window){
    DOT_RendererBackendVk *renderer_ctx = DOT_RendererBackendBase_AsVk(ctx);
    TempArena temp = Memory_GetScratch(NULL);
    if (!DOT_VkAllLayers()){
        DOT_ERROR("Could not find all requested layers");
    }

    // --- Create Instance ---
    {
        VkInstanceCreateInfo instance_create_info = {
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .ppEnabledLayerNames     = dot_vk_layers,
            .enabledLayerCount       = ArrayCount(dot_vk_layers),
            .ppEnabledExtensionNames = dot_renderer_backend_extension_names,
            .enabledExtensionCount   = ArrayCount(dot_renderer_backend_extension_names),
            .pApplicationInfo        =
            &(VkApplicationInfo){
                .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pApplicationName   = "dot_engine",
                .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                .pEngineName        = "dot_engine",
                .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
                .apiVersion         = VK_API_VERSION_1_4,
            },
        };

#ifndef DOT_VK_EXT_DEBUG_UTILS_ENABLE
        VkDebugUtilsMessengerCreateInfoEXT debug_utils_info = {
            .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = DOT_VkDebugCallback,
        };
        instance_create_info.pNext = &debug_utils_info;
#endif
        // u32 extension_count = 0;
        // vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);
        // array(VkExtensionProperties) extensions = PushArray(temp.arena, VkExtensionProperties, extension_count);
        // vkEnumerateInstanceExtensionProperties(NULL, &extension_count, extensions);

        VkCheck(vkCreateInstance(&instance_create_info, NULL, &renderer_ctx->instance));

#ifndef DOT_VK_EXT_DEBUG_UTILS_ENABLE
        PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = 
            (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(renderer_ctx->instance, "vkCreateDebugUtilsMessengerEXT");

        if (vkCreateDebugUtilsMessengerEXT) {
            VkCheck(vkCreateDebugUtilsMessengerEXT(renderer_ctx->instance, &debug_utils_info, NULL, &renderer_ctx->debug_messenger));
        }
#endif
    }

    // --- CREATE SURFACE --- 
    DOT_Window_CreateSurface(window, ctx);
 
    // --- CREATE DEVICE --- 
    {
        DOT_RendererBackendDevice renderer_device = {0};
        DOT_CandidateDeviceInfo candidate_device_info =
            DOT_VkPickBestDevice(renderer_ctx->instance, renderer_ctx->surface);

        renderer_device.gpu = candidate_device_info.gpu;
        renderer_device.shared_present_graphics_queues = candidate_device_info.shared_present_graphics_queues;

        VkDeviceQueueCreateInfo queue_infos[2];
        u32 queue_count = 0;
        const float priority = 1.0f;
        queue_infos[queue_count++] = (VkDeviceQueueCreateInfo) {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = candidate_device_info.graphics_family,
            .queueCount       = 1,
            .pQueuePriorities = &priority,
        };

        if (!renderer_device.shared_present_graphics_queues) {
            queue_infos[queue_count++] = (VkDeviceQueueCreateInfo) {
                .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = candidate_device_info.present_family,
                .queueCount       = 1,
                .pQueuePriorities = &priority,
            };
        }

        VkDeviceCreateInfo device_create_info = {
            .sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pQueueCreateInfos    = queue_infos,
            .queueCreateInfoCount = queue_count,
            .pEnabledFeatures     = &(VkPhysicalDeviceFeatures){},
        };

        VkCheck(vkCreateDevice(candidate_device_info.gpu, &device_create_info, NULL, &renderer_device.device));

        vkGetDeviceQueue(renderer_device.device, candidate_device_info.graphics_family, 0, &renderer_device.graphics_queue);
        if (renderer_device.shared_present_graphics_queues) {
            renderer_device.present_queue = renderer_device.graphics_queue;
        } else {
            DOT_ERROR("Different present and graphics queues unsupported");
            vkGetDeviceQueue(renderer_device.device, candidate_device_info.present_family, 0, &renderer_device.present_queue);
        }
        renderer_ctx->device = renderer_device;
    }
 
    // --- SURFACE CAPABILITIES ---
    {
        VkSurfaceCapabilities2KHR surface_capabilities = {
            .sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
        };
        VkPhysicalDeviceSurfaceInfo2KHR surface_info = {
            .sType   = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
            .surface = renderer_ctx->surface,
        };

        vkGetPhysicalDeviceSurfaceCapabilities2KHR(renderer_ctx->device.gpu, &surface_info, &surface_capabilities);
        struct DOT_VkSurfaceFormats{
            array(VkSurfaceFormat2KHR) surface_formats;
            u32 format_count;
        } formats;

        VkCheck(vkGetPhysicalDeviceSurfaceFormats2KHR(renderer_ctx->device.gpu, &surface_info, &formats.format_count, NULL));
        formats.surface_formats = PushArray(temp.arena, VkSurfaceFormat2KHR, formats.format_count);
        for EachIndex(it, formats.format_count){
            formats.surface_formats[it] = (VkSurfaceFormat2KHR) {
                .sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR
            };
        }
        vkGetPhysicalDeviceSurfaceFormats2KHR(renderer_ctx->device.gpu, &surface_info, &formats.format_count, formats.surface_formats);
    }
    TempArena_Restore(&temp);
}

internal void DOT_RendererBackendVk_ShutdownVulkan(DOT_RendererBackendBase* ctx) {
    DOT_RendererBackendVk *renderer_ctx = DOT_RendererBackendBase_AsVk(ctx);
    vkDestroySurfaceKHR(renderer_ctx->instance, renderer_ctx->surface, NULL);
    vkDestroyDevice(renderer_ctx->device.device, NULL);
#ifndef NDEBUG
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT =
        (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(renderer_ctx->instance, "vkDestroyDebugUtilsMessengerEXT");
    if (vkDestroyDebugUtilsMessengerEXT) {
        vkDestroyDebugUtilsMessengerEXT(renderer_ctx->instance, renderer_ctx->debug_messenger, NULL);
    }
#endif
    vkDestroyInstance(renderer_ctx->instance, NULL);
}
