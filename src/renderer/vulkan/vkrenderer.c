#include "vkrenderer.h"
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

internal const DOT_RendererBackendVKSettings* DOT_VKSettings() {
    static const char* instance_exts[] = {
        VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME,
    #ifdef DOT_VK_EXT_DEBUG_UTILS_ENABLE
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    #endif
        RGFW_VK_SURFACE,
    };

    static const char* device_exts[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    static const char* vk_layers[] = {
    #ifdef DOT_VALIDATION_LAYERS_ENABLE
            "VK_LAYER_KHRONOS_validation",
    #endif
    };

    static const DOT_RendererBackendVKSettings vk_settings = {
        .instance_settings = {
            .instance_extension_names = instance_exts,
            .instance_extension_count = ArrayCount(instance_exts),
        },
        .device_extension_names = device_exts,
        .device_extension_count = ArrayCount(device_exts),
        .layer_names = vk_layers,
        .layer_count = ArrayCount(vk_layers),
    };
    return &vk_settings;
}

internal inline VKAPI_ATTR u32 VKAPI_CALL DOT_VkDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {
    Unused(message_type); Unused(user_data);
    if(message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT){
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
    const DOT_RendererBackendVKSettings* vk_settings = DOT_VKSettings();
    for(u64 i = 0; i < vk_settings->device_extension_count; ++i){
        for(u64 j = 0; j < extension_count; ++j){
            if(strcmp(vk_settings->device_extension_names[i], available_extensions[j].extensionName) == 0){
                DOT_PRINT("Found exntesion \"%s\"", vk_settings->device_extension_names[i]);
                found = true;
                break;
            }
        }
        if(!found){
            DOT_WARNING("Requested extension %s not found", vk_settings->device_extension_names[i]);
            break;
        }
    }
    TempArena_Restore(&temp);
    return found;
}

internal inline b8 DOT_VkDeviceSwapchainSupport(VkPhysicalDevice gpu, VkSurfaceKHR surface){
    TempArena temp = Memory_GetScratch(NULL);
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

    for(u32 it = 0; it < swapchain_support_details.format_count; ++it){
        swapchain_support_details.surface_formats[it] = VkSurfaceFormat2KHRParams();
    }

    // 2KHR version expect this instead of just surface....
    VkPhysicalDeviceSurfaceInfo2KHR surface_info = VkPhysicalDeviceSurfaceInfo2KHRParams(.surface = surface);
    // --- Query Surface Capabilities ---
    vkGetPhysicalDeviceSurfaceCapabilities2KHR(gpu, &surface_info, &swapchain_support_details.surface_capabilities);
 
    // --- Query Surface Formats ---
    VkCheck(vkGetPhysicalDeviceSurfaceFormats2KHR(gpu, &surface_info, &swapchain_support_details.format_count, NULL));
    swapchain_support_details.surface_formats = PushArray(temp.arena, VkSurfaceFormat2KHR, swapchain_support_details.format_count);
    vkGetPhysicalDeviceSurfaceFormats2KHR(gpu, &surface_info, &swapchain_support_details.format_count, swapchain_support_details.surface_formats);
 
    // --- Query Present Modes ---
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &swapchain_support_details.present_modes_count, NULL);
    swapchain_support_details.present_modes = PushArray(temp.arena, VkPresentModeKHR, swapchain_support_details.present_modes_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &swapchain_support_details.present_modes_count, swapchain_support_details.present_modes);

    VkExtent2D surface_extent = swapchain_support_details.surface_capabilities.surfaceCapabilities.currentExtent; // ...
    if(surface_extent.width == U32_MAX){
        DOT_ERROR("Hmmmm might need explicit HDPI handling");
        // i32 w,h;
        // DOT_Window_GetSize(window, w, h);
        // surface_extent.width = w;
        // surface_extent.height = h;
        //
        // surface_extent.width = Clamp(surface_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        // surface_extent.height = Clamp(surface_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }

    array(VkSurfaceFormat2KHR) surface_formats = swapchain_support_details.surface_formats;
    VkSurfaceFormat2KHR desired_format = swapchain_support_details.surface_formats[0]; // Keep this one if none found
    for(u32 i = 0; i < swapchain_support_details.format_count; ++i){
        VkSurfaceFormat2KHR desiredVk_format = surface_formats[i];
        if (desiredVk_format.surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && desiredVk_format.surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            desired_format = desiredVk_format;
            break;
        }
    }
    (void)desired_format;

    array(VkPresentModeKHR) present_modes = swapchain_support_details.present_modes;
    VkPresentModeKHR desired_present_mode = VK_PRESENT_MODE_FIFO_KHR; // Keep this one if none found for now
    for(u32 i = 0; i < swapchain_support_details.present_modes_count; ++i){
        if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR){
            desired_present_mode = present_modes[i];
            break;
        }
    }
    (void)desired_present_mode;
 
    TempArena_Restore(&temp);
    return swapchain_support_details.format_count > 0 && swapchain_support_details.present_modes_count > 0;
}

internal inline DOT_CandidateDeviceInfo DOT_VkPickBestDevice(VkInstance instance, VkSurfaceKHR surface){
    TempArena temp = Memory_GetScratch(NULL);
    u32 device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, NULL);
    if(device_count == 0){
        DOT_ERROR("No Vulkan-capable GPUs found");
    }

    array(VkPhysicalDevice) devices = PushArray(temp.arena, VkPhysicalDevice, device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices);
    DOT_PRINT("device count %i", device_count);

    DOT_CandidateDeviceInfo best_device = {0};
    best_device.score = -1;

    for(u32 i = 0; i < device_count; i++){
        VkPhysicalDevice dev = devices[i];
        // We to first ensure we have what we want before even rating the device
        if(!DOT_VkDeviceAllRequiredExtensions(dev) && DOT_VkDeviceSwapchainSupport(dev, surface)){
            continue;
        }
        VkPhysicalDeviceProperties props;
        VkPhysicalDeviceFeatures feats;
        vkGetPhysicalDeviceProperties(dev, &props);
        vkGetPhysicalDeviceFeatures(dev, &feats);

        u32 queue_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queue_count, NULL);
        array(VkQueueFamilyProperties) queue_properties = PushArray(temp.arena, VkQueueFamilyProperties, queue_count);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queue_count, queue_properties);

        int graphics_idx = -1;
        int present_idx = -1;
        bool shared_present_graphics_queues = false;
        for(u32 q = 0; q < queue_count; q++){
            b32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, q, surface, &present_support);
            if((queue_properties[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present_support){
                shared_present_graphics_queues  = true;
                graphics_idx = q;
                present_idx  = q;
                break;
            }
        }
        if(!shared_present_graphics_queues){
            for(u32 q = 0; q < queue_count; q++){
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
        if(graphics_idx < 0 || present_idx < 0){
            continue;
        }

        int score = -1;
        if(props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 2000;
        // if(props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 100;
        if(feats.geometryShader) score += 10;
        DOT_PRINT("graphics family: %d; present family: %i; score: %i", graphics_idx, present_idx, score);
        if(score > best_device.score) {
            best_device.gpu = dev;
            best_device.graphics_family = cast(u16)graphics_idx;
            best_device.present_family = cast(u16)present_idx;
            best_device.score = score;
            best_device.shared_present_graphics_queues = shared_present_graphics_queues;
        }
    }

    if(best_device.score < 0){
        DOT_ERROR("No suitable GPU found");
    }

    DOT_PRINT("best_device  graphics family: %d; present family: %i; score: %i", best_device.graphics_family, best_device.present_family, best_device.score);
    TempArena_Restore(&temp);
    return best_device;
}

internal inline bool DOT_VkAllLayers(){
    TempArena temp = Memory_GetScratch(NULL);
    u32 available_layer_count = 0;
    vkEnumerateInstanceLayerProperties(&available_layer_count, NULL);
    array(VkLayerProperties) available_layers = PushArray(temp.arena, VkLayerProperties, available_layer_count);
    vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers);

    const DOT_RendererBackendVKSettings* vk_settings = DOT_VKSettings();
    bool found = true;
    for(u64 i = 0; i < vk_settings->layer_count; ++i){
        found = false;
        for(u64 j = 0; j < available_layer_count; ++j){
            if(strcmp(vk_settings->layer_names[i], available_layers[j].layerName) == 0){
                DOT_PRINT("Found Layer %s", vk_settings->layer_names[i]);
                found = true;
                break;
            }
        }
        if(!found){
            DOT_WARNING("Requested layer %s not found", vk_settings->layer_names[i]);
            break;
        }
    }
    TempArena_Restore(&temp);
    return found;
}

internal void DOT_RendererBackendVk_InitVulkan(DOT_RendererBackendBase* ctx, DOT_Window* window){
    DOT_RendererBackendVk *renderer_ctx = DOT_RendererBackendBase_AsVk(ctx);
    TempArena temp = Memory_GetScratch(NULL);
    if(!DOT_VkAllLayers()){
        DOT_ERROR("Could not find all requested layers");
    }

    const DOT_RendererBackendVKSettings* vk_settings = DOT_VKSettings();
    // --- Create Instance ---
    {
        VkInstanceCreateInfo instance_create_info = {
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .ppEnabledLayerNames     = vk_settings->layer_names,
            .enabledLayerCount       = vk_settings->layer_count,
            .ppEnabledExtensionNames = vk_settings->instance_settings.instance_extension_names,
            .enabledExtensionCount   = vk_settings->instance_settings.instance_extension_count,
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

        if(vkCreateDebugUtilsMessengerEXT) {
            VkCheck(vkCreateDebugUtilsMessengerEXT(renderer_ctx->instance, &debug_utils_info, NULL, &renderer_ctx->debug_messenger));
        }
#endif
    }

    // --- Create Surface --- 
    DOT_Window_CreateSurface(window, ctx);
 
    // --- Create Device --- 
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

        if(!renderer_device.shared_present_graphics_queues){
            queue_infos[queue_count++] = (VkDeviceQueueCreateInfo) {
                .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = candidate_device_info.present_family,
                .queueCount       = 1,
                .pQueuePriorities = &priority,
            };
        }

        VkDeviceCreateInfo device_create_info = {
            .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pQueueCreateInfos       = queue_infos,
            .queueCreateInfoCount    = queue_count,
            .pEnabledFeatures        = &(VkPhysicalDeviceFeatures){},
            .ppEnabledExtensionNames = vk_settings->device_extension_names,
            .enabledExtensionCount   = vk_settings->device_extension_count,
        };

        VkCheck(vkCreateDevice(candidate_device_info.gpu, &device_create_info, NULL, &renderer_device.device));

        vkGetDeviceQueue(renderer_device.device, candidate_device_info.graphics_family, 0, &renderer_device.graphics_queue);
        if(renderer_device.shared_present_graphics_queues){
            renderer_device.present_queue = renderer_device.graphics_queue;
        } else {
            DOT_ERROR("Different present and graphics queues unsupported");
            vkGetDeviceQueue(renderer_device.device, candidate_device_info.present_family, 0, &renderer_device.present_queue);
        }
        renderer_ctx->device = renderer_device;
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
    if(vkDestroyDebugUtilsMessengerEXT){
        vkDestroyDebugUtilsMessengerEXT(renderer_ctx->instance, renderer_ctx->debug_messenger, NULL);
    }
#endif
    vkDestroyInstance(renderer_ctx->instance, NULL);
}
