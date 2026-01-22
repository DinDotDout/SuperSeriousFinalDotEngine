#include <vulkan/vulkan_core.h>
#include "vk_helper.h"

internal RendererBackendVk* RendererBackendBase_AsVk(RendererBackendBase* base){
    DOT_ASSERT(base);
    DOT_ASSERT(base->backend_kind == RENDERER_BACKEND_VK);
    return cast(RendererBackendVk*) base;
}

internal RendererBackendVk* RendererBackendVk_Create(Arena* arena){
    RendererBackendVk* backend = PushStruct(arena, RendererBackendVk);
    RendererBackendBase* base = &backend->base;
    base->backend_kind = RENDERER_BACKEND_VK;
    base->Init = RendererBackendVk_Init;
    base->Shutdown = RendererBackendVk_Shutdown;
    base->arena = arena; // NOTE: Should probably use make arena from...
    return backend;
}

internal const RendererBackendVKSettings* RendererBackendVk_Settings() {
    static const char* instance_exts[] = {
        VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME,
    #ifdef VK_EXT_DEBUG_UTILS_ENABLE
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    #endif
        DOT_VK_SURFACE,
    };

    static const char* device_exts[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    static const char* vk_layers[] = {
    #ifdef VALIDATION_LAYERS_ENABLE
            "VK_LAYER_KHRONOS_validation",
    #endif
    };

    static const RendererBackendVKSettings vk_settings = {
        .instance_settings = {
            .instance_extension_names = instance_exts,
            .instance_extension_count = ArrayCount(instance_exts),
        },
        .device_settings = {
            .device_extension_names = device_exts,
            .device_extension_count = ArrayCount(device_exts),
        },
        .layer_settings = {
            .layer_names = vk_layers,
            .layer_count = ArrayCount(vk_layers),
        },
        .swapchain_settings = {
            .preferred_format       = VK_FORMAT_B8G8R8A8_SRGB,
            .preferred_colorspace   = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
            .preferred_present_mode = VK_PRESENT_MODE_MAILBOX_KHR,
        },
    };
    return &vk_settings;
}

internal inline VKAPI_ATTR u32 VKAPI_CALL RendererBackendVk_DebugCallback(
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

typedef struct CandidateDeviceInfo{
    VkPhysicalDevice gpu;
    u16 graphics_family;
    u16 present_family;
    i32 score;
    b8 shared_present_graphics_queues;
}CandidateDeviceInfo;

internal inline bool VkPhysicalDeviceAllRequiredExtensions(VkPhysicalDevice device){
    TempArena temp = Memory_GetScratch(NULL);
    u32 extension_count;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);
    array(VkExtensionProperties) available_extensions = PushArray(temp.arena, VkExtensionProperties, extension_count);
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, available_extensions);

    bool all_found = true;
    const RendererBackendVKSettings* vk_settings = RendererBackendVk_Settings();
    for(u64 i = 0; i < vk_settings->device_settings.device_extension_count; ++i){
        bool found = false;
        const char* device_extension_name = vk_settings->device_settings.device_extension_names[i];
        for(u64 j = 0; j < extension_count; ++j){
            if(strcmp(device_extension_name, available_extensions[j].extensionName) == 0){
                DOT_PRINT("Found extension \"%s\"", device_extension_name);
                found = true;
                break;
            }
        }
        if(!found){  // Might aswell list all missing physical dev extensions
            all_found = false;
            DOT_WARNING("Requested extension %s not found", device_extension_name);
            break;
        }
    }
    TempArena_Restore(&temp);
    return all_found;
}

internal inline bool VkInstanceAllRequiredExtensions(){
    TempArena temp = Memory_GetScratch(NULL);
    u32 extension_count;
    vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);
    array(VkExtensionProperties) available_extensions = PushArray(temp.arena, VkExtensionProperties, extension_count);
    vkEnumerateInstanceExtensionProperties(NULL, &extension_count, available_extensions);

    bool all_found = true;
    const RendererBackendVKSettings* vk_settings = RendererBackendVk_Settings();
    for(u64 i = 0; i < vk_settings->instance_settings.instance_extension_count; ++i){
        bool found = false;
        const char* instance_extension_name = vk_settings->instance_settings.instance_extension_names[i];
        for(u64 j = 0; j < extension_count; ++j){
            if(strcmp(instance_extension_name, available_extensions[j].extensionName) == 0){
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
    TempArena_Restore(&temp);
    return all_found;
}

typedef struct SwapchainDetails{
    VkSurfaceFormatKHR               best_surface_format;
    VkPresentModeKHR                 best_present_mode;
    VkExtent2D                       surface_extent;
    VkSurfaceTransformFlagBitsKHR    current_transform;
    u32                              image_count;
}SwapchainDetails;

internal inline b8 VkPhysicalDeviceSwapchainSupport(VkPhysicalDevice gpu, VkSurfaceKHR surface, DOT_Window* window, SwapchainDetails* details){
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

    // 2KHR version expect this instead of just surface....
    VkPhysicalDeviceSurfaceInfo2KHR surface_info = VkPhysicalDeviceSurfaceInfo2KHRParams(.surface = surface);
    // --- Query Surface Capabilities ---
    vkGetPhysicalDeviceSurfaceCapabilities2KHR(gpu, &surface_info, &swapchain_support_details.surface_capabilities);
 
    // --- Query Surface Formats ---
    VkCheck(vkGetPhysicalDeviceSurfaceFormats2KHR(gpu, &surface_info, &swapchain_support_details.format_count, NULL));
    swapchain_support_details.surface_formats = PushArray(temp.arena, VkSurfaceFormat2KHR, swapchain_support_details.format_count);
    for(u32 it = 0; it < swapchain_support_details.format_count; ++it){
        swapchain_support_details.surface_formats[it] = VkSurfaceFormat2KHRParams();
    }
    vkGetPhysicalDeviceSurfaceFormats2KHR(gpu, &surface_info, &swapchain_support_details.format_count, swapchain_support_details.surface_formats);
 
    // --- Query Present Modes ---
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &swapchain_support_details.present_modes_count, NULL);
    swapchain_support_details.present_modes = PushArray(temp.arena, VkPresentModeKHR, swapchain_support_details.present_modes_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &swapchain_support_details.present_modes_count, swapchain_support_details.present_modes);
    bool has_support = swapchain_support_details.format_count > 0 && swapchain_support_details.present_modes_count > 0;
    if(has_support && details != NULL){
        VkSurfaceCapabilitiesKHR surface_capabilities = swapchain_support_details.surface_capabilities.surfaceCapabilities; // ...
        VkExtent2D surface_extent = surface_capabilities.currentExtent; // ...
        if(surface_extent.width == U32_MAX){ // Should we just do this outside this func?
            i32 w, h;
            DOT_Window_GetFrameBufferSize(window, &w, &h);
            surface_extent.width  = w;
            surface_extent.height = h;

            surface_extent.width = Clamp(surface_extent.width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
            surface_extent.height = Clamp(surface_extent.height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
        }

        const RendererBackendVKSettings* settings = RendererBackendVk_Settings();
        VkFormat         preferred_format             = settings->swapchain_settings.preferred_format;
        VkColorSpaceKHR  preferred_colorspace         = settings->swapchain_settings.preferred_colorspace;
        VkPresentModeKHR preferred_present_mode       = settings->swapchain_settings.preferred_present_mode;

        VkSurfaceFormat2KHR desired_format = swapchain_support_details.surface_formats[0]; // Keep this one if none found
        array(VkSurfaceFormat2KHR) surface_formats = swapchain_support_details.surface_formats;
        for(u32 i = 0; i < swapchain_support_details.format_count; ++i){
            VkSurfaceFormat2KHR desiredVk_format = surface_formats[i];
            if (desiredVk_format.surfaceFormat.format == preferred_format &&
                desiredVk_format.surfaceFormat.colorSpace == preferred_colorspace){
                desired_format = desiredVk_format;
                break;
            }
        }

        VkPresentModeKHR desired_present_mode = VK_PRESENT_MODE_FIFO_KHR; // Keep this one if none found for now
        array(VkPresentModeKHR) present_modes = swapchain_support_details.present_modes;
        for(u32 i = 0; i < swapchain_support_details.present_modes_count; ++i){
            if (present_modes[i] == preferred_present_mode){
                desired_present_mode = present_modes[i];
                break;
            }
        }
        details->best_present_mode   = desired_present_mode;
        details->best_surface_format = desired_format.surfaceFormat;
        details->surface_extent      = surface_extent;
        details->image_count         = surface_capabilities.minImageCount+1;
        if(surface_capabilities.maxImageCount > 0 && details->image_count > surface_capabilities.maxImageCount){
            details->image_count = surface_capabilities.maxImageCount;
        }
        DOT_PRINT("IMAGESS %u %u %u", details->image_count, surface_capabilities.minImageCount, surface_capabilities.maxImageCount);
        details->current_transform   = surface_capabilities.currentTransform;
    }
    TempArena_Restore(&temp);
    return has_support;
}

internal inline CandidateDeviceInfo VkPickBestDevice(VkInstance instance, VkSurfaceKHR surface, DOT_Window* window){
    TempArena temp = Memory_GetScratch(NULL);
    u32 device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, NULL);
    if(device_count == 0){
        DOT_ERROR("No Vulkan-capable GPUs found");
    }

    array(VkPhysicalDevice) devices = PushArray(temp.arena, VkPhysicalDevice, device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices);
    DOT_PRINT("device count %i", device_count);

    CandidateDeviceInfo best_device = {0};
    best_device.score = -1;

    for(u32 i = 0; i < device_count; i++){
        VkPhysicalDevice dev = devices[i];
        // We to first ensure we have what we want before even rating the device
        if(!VkPhysicalDeviceAllRequiredExtensions(dev) || !VkPhysicalDeviceSwapchainSupport(dev, surface, window, NULL)){
            continue;
        }
        VkPhysicalDeviceProperties device_properties;
        VkPhysicalDeviceFeatures device_features;
        vkGetPhysicalDeviceProperties(dev, &device_properties);
        vkGetPhysicalDeviceFeatures(dev, &device_features);

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
        if(device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 2000;
        // if(device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 100;
        if(device_features.geometryShader) score += 10;
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

internal inline bool VkAllLayers(){
    TempArena temp = Memory_GetScratch(NULL);
    u32 available_layer_count = 0;
    vkEnumerateInstanceLayerProperties(&available_layer_count, NULL);
    array(VkLayerProperties) available_layers = PushArray(temp.arena, VkLayerProperties, available_layer_count);
    vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers);

    const RendererBackendVKSettings* vk_settings = RendererBackendVk_Settings();
    bool all_found = true;
    for(u64 i = 0; i < vk_settings->layer_settings.layer_count; ++i){
        bool found = false;
        const char* layer_name = vk_settings->layer_settings.layer_names[i];
        for(u64 j = 0; j < available_layer_count; ++j){
            if(strcmp(layer_name, available_layers[j].layerName) == 0){
                DOT_PRINT("Found Layer %s", layer_name);
                found = true;
                break;
            }
        }
        if(!found){ // Might aswell list all missing layers
            all_found = false;
            DOT_WARNING("Requested layer %s not found", layer_name);
        }
    }
    TempArena_Restore(&temp);
    return all_found;
}

internal void RendererBackendVk_Init(RendererBackendBase* ctx, DOT_Window* window){
    RendererBackendVk *renderer_ctx = RendererBackendBase_AsVk(ctx);
    // renderer_ctx->vk_allocator = VkAllcocatorParams(ctx->arena);
    TempArena temp = Memory_GetScratch(NULL);
    if(!VkAllLayers()){
        DOT_ERROR("Could not find all requested layers");
    }

    if(!VkInstanceAllRequiredExtensions()){
        DOT_ERROR("Could not find all requested instance extensions");
    }

    const RendererBackendVKSettings* vk_settings = RendererBackendVk_Settings();
    // --- Create Instance ---
    {
        VkInstanceCreateInfo instance_create_info = {
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .ppEnabledLayerNames     = vk_settings->layer_settings.layer_names,
            .enabledLayerCount       = vk_settings->layer_settings.layer_count,
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

        VkDebugUtilsMessengerCreateInfoEXT debug_utils_info;
        if(VK_EXT_DEBUG_UTILS_ENABLE){
            debug_utils_info = (VkDebugUtilsMessengerCreateInfoEXT){
                .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                .pfnUserCallback = RendererBackendVk_DebugCallback,
            };
            instance_create_info.pNext = &debug_utils_info;
        }

        VkCheck(vkCreateInstance(&instance_create_info, NULL, &renderer_ctx->instance));

        if(VK_EXT_DEBUG_UTILS_ENABLE){
            PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT =
                (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(renderer_ctx->instance, "vkCreateDebugUtilsMessengerEXT");

            if(vkCreateDebugUtilsMessengerEXT) {
                VkCheck(vkCreateDebugUtilsMessengerEXT(renderer_ctx->instance, &debug_utils_info, NULL, &renderer_ctx->debug_messenger));
            }
        }
    }

    // --- Create Surface --- 
    DOT_Window_CreateSurface(window, ctx);
 
    // --- Create Device --- 
    {
        RendererBackendDevice* device = &renderer_ctx->device;
        CandidateDeviceInfo candidate_device_info = VkPickBestDevice(renderer_ctx->instance, renderer_ctx->surface, window);
        device->gpu = candidate_device_info.gpu;
        device->graphics_queue_idx = candidate_device_info.graphics_family;
        device->present_queue_idx = candidate_device_info.present_family;

        b8 shared_present_graphics_queues = candidate_device_info.graphics_family == candidate_device_info.present_family;

        VkDeviceQueueCreateInfo queue_infos[2];
        u32 queue_count = 0;
        const float priority = 1.0f;
        queue_infos[queue_count++] = (VkDeviceQueueCreateInfo) {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = candidate_device_info.graphics_family,
            .queueCount       = 1,
            .pQueuePriorities = &priority,
        };

        if(!shared_present_graphics_queues){
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
            .ppEnabledExtensionNames = vk_settings->device_settings.device_extension_names,
            .enabledExtensionCount   = vk_settings->device_settings.device_extension_count,
        };

        VkCheck(vkCreateDevice(candidate_device_info.gpu, &device_create_info, NULL, &device->device));
        vkGetDeviceQueue(device->device, candidate_device_info.graphics_family, 0, &device->graphics_queue);
        if(shared_present_graphics_queues){
            device->present_queue = device->graphics_queue;
        } else {
            DOT_ERROR("Different present and graphics queues unsupported");
            vkGetDeviceQueue(device->device, candidate_device_info.present_family, 0, &device->present_queue);
        }
    }
    // --- Create Swapchain ---
    {
        RendererBackendSwapchain* swapchain = &renderer_ctx->swapchain;
        SwapchainDetails details;
        VkPhysicalDeviceSwapchainSupport(renderer_ctx->device.gpu, renderer_ctx->surface, window, &details);
        VkSwapchainCreateInfoKHR swapchain_create_info = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface          = renderer_ctx->surface,
            .minImageCount    = details.image_count,
            .imageFormat      = details.best_surface_format.format,
            .imageColorSpace  = details.best_surface_format.colorSpace,
            .imageExtent      = details.surface_extent,
            .preTransform     = details.current_transform,
            .presentMode      = details.best_present_mode,
            .clipped          = VK_TRUE,
            .imageArrayLayers = 1,
            .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .oldSwapchain     = VK_NULL_HANDLE, // Will need to update this for recreation
        };
        if(renderer_ctx->device.graphics_queue_idx != renderer_ctx->device.present_queue_idx){
            DOT_ERROR("Different present and graphics queues unsupported");
            u32 queues[] = {renderer_ctx->device.graphics_queue_idx, renderer_ctx->device.present_queue_idx};
            swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            swapchain_create_info.pQueueFamilyIndices = queues;
            swapchain_create_info.queueFamilyIndexCount = ArrayCount(queues);
        }
        VkCheck(vkCreateSwapchainKHR(
            renderer_ctx->device.device, &swapchain_create_info,
            NULL, &swapchain->swapchain));

        swapchain->extent = details.surface_extent;
        swapchain->image_format = details.best_surface_format.format;
 
        //  --- Create Images ---
        {
            VkDevice device = renderer_ctx->device.device;
            vkGetSwapchainImagesKHR(device, swapchain->swapchain, &swapchain->images_count, NULL);
            swapchain->images = PushArray(ctx->arena, VkImage, swapchain->images_count);
            vkGetSwapchainImagesKHR(device, swapchain->swapchain, &swapchain->images_count, swapchain->images);
            // VkImageView images_views;
        }
        //  --- Create Image Views ---
        {
            // VkSwapchainKHR* swapchain = &renderer_ctx->swapchain.swapchain;
            VkDevice device = renderer_ctx->device.device;
            // vkGetSwapchainImagesKHR(device, swapchain, &renderer_ctx->swapchain.images_count, NULL);
            // swapchain->image_views = PushArray(ctx->arena, VkImageView, swapchain->images_count);
            renderer_ctx->swapchain.image_views = PushArray(ctx->arena, VkImageView, swapchain->images_count);
            for(u32 i = 0; i < swapchain->images_count; ++i){
                VkImageViewCreateInfo image_create_info = {
                    .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .image    = swapchain->images[i],
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format   = swapchain->image_format,
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
                    }
                };
                // VkCheck(vkCreateImageView(device, &image_create_info, NULL, &swapchain->image_views[i]));
                VkCheck(vkCreateImageView(device, &image_create_info, NULL, &renderer_ctx->swapchain.image_views[i]));
            }
            // vkGetSwapchainImagesKHR(device, swapchain, &renderer_ctx->swapchain.images_count, renderer_ctx->swapchain.images);
            // VkImageView images_views;
        }
    }
    TempArena_Restore(&temp);
}

internal void RendererBackendVk_Shutdown(RendererBackendBase* ctx) {
    RendererBackendVk *renderer_ctx = RendererBackendBase_AsVk(ctx);
    VkDevice device = renderer_ctx->device.device;
    for(u32 i = 0; i < renderer_ctx->swapchain.images_count; ++i){
        vkDestroyImageView(device, renderer_ctx->swapchain.image_views[i], NULL);
    }
    vkDestroySwapchainKHR(device, renderer_ctx->swapchain.swapchain, NULL);
    vkDestroySurfaceKHR(renderer_ctx->instance, renderer_ctx->surface, NULL);
    vkDestroyDevice(renderer_ctx->device.device, NULL);
    if(VK_EXT_DEBUG_UTILS_ENABLE){
        PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT =
            (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(renderer_ctx->instance, "vkDestroyDebugUtilsMessengerEXT");
        if(vkDestroyDebugUtilsMessengerEXT){
            vkDestroyDebugUtilsMessengerEXT(renderer_ctx->instance, renderer_ctx->debug_messenger, NULL);
        }
    }
    vkDestroyInstance(renderer_ctx->instance, NULL);
}
