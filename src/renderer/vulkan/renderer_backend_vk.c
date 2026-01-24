#include <vulkan/vulkan_core.h>
#include "vk_helper.h"

internal RendererBackendVk*
renderer_backend_as_vk(RendererBackend* base){
    DOT_ASSERT(base);
    DOT_ASSERT(base->backend_kind == RENDERER_BACKEND_VK);
    return cast(RendererBackendVk*) base;
}

internal RendererBackendVk*
renderer_backend_vk_create(Arena* arena){
    RendererBackendVk* backend = PUSH_STRUCT(arena, RendererBackendVk);
    RendererBackend* base = &backend->base;
    base->backend_kind = RENDERER_BACKEND_VK;
    base->Init = renderer_backend_vk_init;
    base->Shutdown = renderer_backend_vk_shutdown;
    base->arena = arena; // NOTE: Should probably use make arena from...
    return backend;
}

internal const RendererBackendVKSettings*
renderer_backend_vk_settings() {
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
            .instance_extension_count = ARRAY_COUNT(instance_exts),
        },
        .device_settings = {
            .device_extension_names = device_exts,
            .device_extension_count = ARRAY_COUNT(device_exts),
        },
        .layer_settings = {
            .layer_names = vk_layers,
            .layer_count = ARRAY_COUNT(vk_layers),
        },
        .swapchain_settings = {
            .preferred_format       = VK_FORMAT_B8G8R8A8_SRGB,
            .preferred_colorspace   = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
            .preferred_present_mode = VK_PRESENT_MODE_MAILBOX_KHR,
        },
    };
    return &vk_settings;
}

internal inline VKAPI_ATTR u32 VKAPI_CALL
renderere_backend_vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {
    UNUSED(message_type); UNUSED(user_data);
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

internal inline bool
vk_physical_device_all_required_extensions(VkPhysicalDevice device){
    TempArena temp = thread_ctx_get_temp(NULL);
    u32 extension_count;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);
    array(VkExtensionProperties) available_extensions = PUSH_ARRAY(temp.arena, VkExtensionProperties, extension_count);
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, available_extensions);

    bool all_found = true;
    const RendererBackendVKSettings* vk_settings = renderer_backend_vk_settings();
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
    temp_arena_restore(&temp);
    return all_found;
}

internal inline bool
vk_instance_all_required_extensions(){
    TempArena temp = thread_ctx_get_temp(NULL);
    u32 extension_count;
    vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);
    array(VkExtensionProperties) available_extensions = PUSH_ARRAY(temp.arena, VkExtensionProperties, extension_count);
    vkEnumerateInstanceExtensionProperties(NULL, &extension_count, available_extensions);

    bool all_found = true;
    const RendererBackendVKSettings* vk_settings = renderer_backend_vk_settings();
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
    temp_arena_restore(&temp);
    return all_found;
}

typedef struct SwapchainDetails{
    VkSurfaceFormatKHR               best_surface_format;
    VkPresentModeKHR                 best_present_mode;
    VkExtent2D                       surface_extent;
    VkSurfaceTransformFlagBitsKHR    current_transform;
    u32                              image_count;
}SwapchainDetails;

internal inline
b8 vk_physical_device_swapchain_support(VkPhysicalDevice gpu, VkSurfaceKHR surface, DOT_Window* window, SwapchainDetails* details){
    TempArena temp = thread_ctx_get_temp(NULL);
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
    swapchain_support_details.surface_formats = PUSH_ARRAY(temp.arena, VkSurfaceFormat2KHR, swapchain_support_details.format_count);
    for(u32 it = 0; it < swapchain_support_details.format_count; ++it){
        swapchain_support_details.surface_formats[it] = VkSurfaceFormat2KHRParams();
    }
    vkGetPhysicalDeviceSurfaceFormats2KHR(gpu, &surface_info, &swapchain_support_details.format_count, swapchain_support_details.surface_formats);
 
    // --- Query Present Modes ---
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &swapchain_support_details.present_modes_count, NULL);
    swapchain_support_details.present_modes = PUSH_ARRAY(temp.arena, VkPresentModeKHR, swapchain_support_details.present_modes_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &swapchain_support_details.present_modes_count, swapchain_support_details.present_modes);
    bool has_support = swapchain_support_details.format_count > 0 && swapchain_support_details.present_modes_count > 0;
    if(has_support && details != NULL){
        VkSurfaceCapabilitiesKHR surface_capabilities = swapchain_support_details.surface_capabilities.surfaceCapabilities; // ...
        VkExtent2D surface_extent = surface_capabilities.currentExtent; // ...
        if(surface_extent.width == U32_MAX){ // Should we just do this outside this func?
            i32 w, h;
            dot_window_get_framebuffer_size(window, &w, &h);
            surface_extent.width  = w;
            surface_extent.height = h;

            surface_extent.width = CLAMP(surface_extent.width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
            surface_extent.height = CLAMP(surface_extent.height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
        }

        const RendererBackendVKSettings* settings = renderer_backend_vk_settings();
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
    temp_arena_restore(&temp);
    return has_support;
}

internal inline CandidateDeviceInfo
vk_pick_best_device(VkInstance instance, VkSurfaceKHR surface, DOT_Window* window){
    TempArena temp = thread_ctx_get_temp(NULL);
    u32 device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, NULL);
    if(device_count == 0){
        DOT_ERROR("No Vulkan-capable GPUs found");
    }

    array(VkPhysicalDevice) devices = PUSH_ARRAY(temp.arena, VkPhysicalDevice, device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices);
    DOT_PRINT("device count %i", device_count);

    CandidateDeviceInfo best_device = {0};
    best_device.score = -1;

    for(u32 i = 0; i < device_count; i++){
        VkPhysicalDevice dev = devices[i];
        // We to first ensure we have what we want before even rating the device
        if(!vk_physical_device_all_required_extensions(dev) || !vk_physical_device_swapchain_support(dev, surface, window, NULL)){
            continue;
        }
        VkPhysicalDeviceProperties device_properties;
        VkPhysicalDeviceFeatures device_features;
        vkGetPhysicalDeviceProperties(dev, &device_properties);
        vkGetPhysicalDeviceFeatures(dev, &device_features);

        u32 queue_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queue_count, NULL);
        array(VkQueueFamilyProperties) queue_properties = PUSH_ARRAY(temp.arena, VkQueueFamilyProperties, queue_count);
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
    temp_arena_restore(&temp);
    return best_device;
}

internal inline bool
vk_all_layers(){
    TempArena temp = thread_ctx_get_temp(NULL);
    u32 available_layer_count = 0;
    vkEnumerateInstanceLayerProperties(&available_layer_count, NULL);
    array(VkLayerProperties) available_layers = PUSH_ARRAY(temp.arena, VkLayerProperties, available_layer_count);
    vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers);

    const RendererBackendVKSettings* vk_settings = renderer_backend_vk_settings();
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
    temp_arena_restore(&temp);
    return all_found;
}

internal void
renderer_backend_vk_init(RendererBackend* ctx, DOT_Window* window){
    RendererBackendVk *renderer_ctx = renderer_backend_as_vk(ctx);
    // renderer_ctx->vk_allocator = VkAllcocatorParams(ctx->arena);
    TempArena temp = thread_ctx_get_temp(NULL);
    if(!vk_all_layers()){
        DOT_ERROR("Could not find all requested layers");
    }

    if(!vk_instance_all_required_extensions()){
        DOT_ERROR("Could not find all requested instance extensions");
    }

    const RendererBackendVKSettings* vk_settings = renderer_backend_vk_settings();
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
                .pfnUserCallback = renderere_backend_vk_debug_callback,
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
    dot_window_create_surface(window, ctx);
 
    // --- Create Device --- 
    {
        RendererBackendDevice* device = &renderer_ctx->device;
        CandidateDeviceInfo candidate_device_info = vk_pick_best_device(renderer_ctx->instance, renderer_ctx->surface, window);
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
        vk_physical_device_swapchain_support(renderer_ctx->device.gpu, renderer_ctx->surface, window, &details);
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
            swapchain_create_info.queueFamilyIndexCount = ARRAY_COUNT(queues);
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
            swapchain->images = PUSH_ARRAY(ctx->arena, VkImage, swapchain->images_count);
            vkGetSwapchainImagesKHR(device, swapchain->swapchain, &swapchain->images_count, swapchain->images);
            // VkImageView images_views;
        }
        //  --- Create Image Views ---
        {
            // VkSwapchainKHR* swapchain = &renderer_ctx->swapchain.swapchain;
            VkDevice device = renderer_ctx->device.device;
            // vkGetSwapchainImagesKHR(device, swapchain, &renderer_ctx->swapchain.images_count, NULL);
            // swapchain->image_views = PUSH_ARRAY(ctx->arena, VkImageView, swapchain->images_count);
            renderer_ctx->swapchain.image_views = PUSH_ARRAY(ctx->arena, VkImageView, swapchain->images_count);
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
    temp_arena_restore(&temp);
}

internal void
renderer_backend_vk_shutdown(RendererBackend* ctx) {
    RendererBackendVk *renderer_ctx = renderer_backend_as_vk(ctx);
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
