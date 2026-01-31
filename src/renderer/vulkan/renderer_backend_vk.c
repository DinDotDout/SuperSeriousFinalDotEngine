#include "renderer/vulkan/renderer_backend_vk.h"
#include <vulkan/vulkan_core.h>
#include "base/dot.h"
#include "vk_helper.h"

internal RendererBackendVk*
renderer_backend_as_vk(RendererBackend *base){
    DOT_ASSERT(base);
    DOT_ASSERT(base->backend_kind == RENDERER_BACKEND_VK);
    return cast(RendererBackendVk*) base;
}

internal RendererBackendVk*
renderer_backend_vk_create(Arena* arena){
    RendererBackendVk *backend = PUSH_STRUCT(arena, RendererBackendVk);
    RendererBackend *base = &backend->base;
    base->backend_kind = RENDERER_BACKEND_VK;
    base->init         = renderer_backend_vk_init;
    base->shutdown     = renderer_backend_vk_shutdown;
    base->draw         = renderer_backend_vk_draw;
    return backend;
}

// NOTE: Might want to remove const to make some things tweakable
internal const RendererBackendVk_Settings*
renderer_backend_vk_settings() {
    static const String8 instance_exts[] = {
        String8Lit(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME),
        String8Lit(VK_KHR_SURFACE_EXTENSION_NAME),
#ifdef VK_EXT_DEBUG_UTILS_ENABLE
        String8Lit(VK_EXT_DEBUG_UTILS_EXTENSION_NAME),
#endif
        String8Lit(DOT_VK_SURFACE),
    };

    static const String8 device_exts[] = {
        String8Lit(VK_KHR_SWAPCHAIN_EXTENSION_NAME),
    };

    static const String8 vk_layers[] = {
#ifdef VALIDATION_LAYERS_ENABLE
        String8Lit("VK_LAYER_KHRONOS_validation"),
#endif
    };

    static const RendererBackendVk_Settings vk_settings = {
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
        .frame_settings = {
            .frame_overlap = 2,
        }
    };
    return &vk_settings;
}

internal inline VKAPI_ATTR u32 VKAPI_CALL
renderere_backend_vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data){
    UNUSED(message_type); UNUSED(user_data);
    if(message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT){
        DOT_PRINT("validation layer: %s\n", callback_data->pMessage);
    }
    return VK_FALSE;
}


internal void
renderer_backend_vk_init(RendererBackend* base_ctx, DOT_Window* window){
    RendererBackendVk *vk_ctx = renderer_backend_as_vk(base_ctx);
    Arena* ctx_arena = vk_ctx->base.permanent_arena;
    // vk_ctx->vk_allocator = VkAllocatorParams(ctx_arena);
    TempArena temp = threadctx_get_temp(NULL, 0);
    const RendererBackendVk_Settings* vk_settings = renderer_backend_vk_settings();
    if(!vk_all_layers(vk_settings)){
        DOT_ERROR("Could not find all requested layers");
    }

    if(!vk_instance_all_required_extensions(vk_settings)){
        DOT_ERROR("Could not find all requested instance extensions");
    }

    // --- Create Instance ---
    {
        VkInstanceCreateInfo instance_create_info = {
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .ppEnabledLayerNames     = string8_array_to_str_array(
                temp.arena,
                vk_settings->layer_settings.layer_count,
                vk_settings->layer_settings.layer_names),
            .enabledLayerCount       = vk_settings->layer_settings.layer_count,
            .ppEnabledExtensionNames = string8_array_to_str_array(
                temp.arena,
                vk_settings->instance_settings.instance_extension_count,
                vk_settings->instance_settings.instance_extension_names),
            .enabledExtensionCount   = vk_settings->instance_settings.instance_extension_count,
            .pApplicationInfo        = &(VkApplicationInfo){
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
                .messageSeverity =
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                .messageType     =
                    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                .pfnUserCallback = renderere_backend_vk_debug_callback,
          };
          instance_create_info.pNext = &debug_utils_info;
        }

        VK_CHECK(vkCreateInstance(&instance_create_info, NULL, &vk_ctx->instance));

        if(VK_EXT_DEBUG_UTILS_ENABLE){
            PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT =
                (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vk_ctx->instance, "vkCreateDebugUtilsMessengerEXT");

            if(vkCreateDebugUtilsMessengerEXT) {
                VK_CHECK(vkCreateDebugUtilsMessengerEXT(vk_ctx->instance, &debug_utils_info, NULL, &vk_ctx->debug_messenger));
            }
        }
    }

    // --- Create Surface --- 
    dot_window_create_surface(window, base_ctx);
 
    // --- Create Device --- 
    {
        VkCandidateDeviceInfo candidate_device_info = vk_pick_best_device(vk_settings, vk_ctx->instance, vk_ctx->surface, window);
        RendererBackendVk_Device* device = &vk_ctx->device;
        device->gpu                      = candidate_device_info.gpu;
        device->graphics_queue_idx       = candidate_device_info.graphics_family;
        device->present_queue_idx        = candidate_device_info.present_family;

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
            // .pEnabledFeatures        = &(VkPhysicalDeviceFeatures){}, // 
            .ppEnabledExtensionNames = string8_array_to_str_array(
                temp.arena,
                vk_settings->device_settings.device_extension_count,
                vk_settings->device_settings.device_extension_names),
            .enabledExtensionCount   = vk_settings->device_settings.device_extension_count,
        };

        VK_CHECK(vkCreateDevice(candidate_device_info.gpu, &device_create_info, NULL, &device->device));
        vkGetDeviceQueue(device->device, candidate_device_info.graphics_family, 0, &device->graphics_queue);
        if(shared_present_graphics_queues){
            device->present_queue = device->graphics_queue;
        } else {
            DOT_ERROR("Different present and graphics queues unsupported");
            vkGetDeviceQueue(device->device, candidate_device_info.present_family, 0, &device->present_queue);
        }
    }
    // TODO: We will probably need to move this to its own function to allow recreation
    // --- Create Swapchain ---
    {
        RendererBackendVk_Swapchain* swapchain = &vk_ctx->swapchain;
        VkSwapchainDetails details = {0};
        vk_physical_device_swapchain_support(vk_settings, vk_ctx->device.gpu, vk_ctx->surface, window, &details);
        swapchain->extent = details.surface_extent;
        swapchain->image_format = details.best_surface_format.format;
        swapchain->images_count = details.image_count;

        VkSwapchainCreateInfoKHR swapchain_create_info = {
            .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface          = vk_ctx->surface,
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
        if(vk_ctx->device.graphics_queue_idx != vk_ctx->device.present_queue_idx){
            DOT_ERROR("Different present and graphics queues unsupported");
            u32 queues[] = {vk_ctx->device.graphics_queue_idx, vk_ctx->device.present_queue_idx};
            swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            swapchain_create_info.pQueueFamilyIndices = queues;
            swapchain_create_info.queueFamilyIndexCount = ARRAY_COUNT(queues);
        }
        VK_CHECK(vkCreateSwapchainKHR(vk_ctx->device.device, &swapchain_create_info, NULL, &swapchain->swapchain));
 
        //  --- Create Images ---
        {
            VkDevice device = vk_ctx->device.device;
            vkGetSwapchainImagesKHR(device, swapchain->swapchain, &swapchain->images_count, NULL);
            swapchain->images = PUSH_ARRAY(ctx_arena, VkImage, swapchain->images_count);
            vkGetSwapchainImagesKHR(device, swapchain->swapchain, &swapchain->images_count, swapchain->images);
        }
        //  --- Create Image Views ---
        {
            VkDevice device = vk_ctx->device.device;
            swapchain->image_views = PUSH_ARRAY(ctx_arena, VkImageView, swapchain->images_count);
            VkImageViewCreateInfo image_create_info = {
                .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
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

            for(u32 i = 0; i < swapchain->images_count; ++i){
                image_create_info.image = swapchain->images[i];
                VK_CHECK(vkCreateImageView(device, &image_create_info, NULL, &swapchain->image_views[i]));
            }
        }
    }
    // --- Create Frame Structures ---
    {
        u8 frame_overlap = vk_settings->frame_settings.frame_overlap;
        VkDevice device = vk_ctx->device.device;
        // WARN: Any allocation that can be recreated may be need to be pushed onto its own arena alloc ctx to avoid leaking
        vk_ctx->frame_count = frame_overlap;
        vk_ctx->frames = PUSH_ARRAY(ctx_arena, RendererBackendVk_FrameData, frame_overlap);
        // --- Create Commands ---
        {
            VkCommandPoolCreateInfo command_pool_info = {
                .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = vk_ctx->device.graphics_queue_idx,
            };
            VkCommandBufferAllocateInfo cmd_alloc_info = {
                .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            };
            for(u8 i = 0; i < frame_overlap; ++i){
                RendererBackendVk_FrameData *frame_data = &vk_ctx->frames[i];
                VK_CHECK(vkCreateCommandPool(device, &command_pool_info, NULL, &frame_data->command_pool));
                cmd_alloc_info.commandPool = frame_data->command_pool;
                VK_CHECK(vkAllocateCommandBuffers(device, &cmd_alloc_info, &frame_data->command_buffer));
            }
        }
        // --- Init Sync Structures ---
        {
            VkFenceCreateInfo fence_create_info = {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = VK_FENCE_CREATE_SIGNALED_BIT,
                .pNext = NULL,
            };
            VkSemaphoreCreateInfo semaphore_create_info = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                .flags = 0,
                .pNext = NULL,
            };
            for(u8 i = 0; i < frame_overlap; ++i){
                RendererBackendVk_FrameData *frame_data = &vk_ctx->frames[i];
                VK_CHECK(vkCreateFence(device, &fence_create_info, NULL, &frame_data->render_fence));
                VK_CHECK(vkCreateSemaphore(device, &semaphore_create_info, NULL, &frame_data->render_semaphore));
                VK_CHECK(vkCreateSemaphore(device, &semaphore_create_info, NULL, &frame_data->swapchain_semaphore));
            }
        }
    }
    temp_arena_restore(&temp);
}

internal void
renderer_backend_vk_shutdown(RendererBackend* base_ctx){
    RendererBackendVk *vk_ctx = renderer_backend_as_vk(base_ctx);
    VkDevice device = vk_ctx->device.device;
    const RendererBackendVk_Settings *settings = renderer_backend_vk_settings();
    vkDeviceWaitIdle(device);
    for(u32 i = 0; i < settings->frame_settings.frame_overlap; ++i){
        RendererBackendVk_FrameData* frame_data = &vk_ctx->frames[i];
        vkDestroyCommandPool(device, frame_data->command_pool, NULL);
        vkDestroyFence(device, frame_data->render_fence, NULL);
        vkDestroySemaphore(device, frame_data->render_semaphore, NULL);
        vkDestroySemaphore(device, frame_data->swapchain_semaphore, NULL);
    }
    for(u32 i = 0; i < vk_ctx->swapchain.images_count; ++i){
        vkDestroyImageView(device, vk_ctx->swapchain.image_views[i], NULL);
    }
    vkDestroySwapchainKHR(device, vk_ctx->swapchain.swapchain, NULL);
    vkDestroySurfaceKHR(vk_ctx->instance, vk_ctx->surface, NULL);
    vkDestroyDevice(vk_ctx->device.device, NULL);
    if(VK_EXT_DEBUG_UTILS_ENABLE){
        PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT =
            (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(vk_ctx->instance, "vkDestroyDebugUtilsMessengerEXT");
        if(vkDestroyDebugUtilsMessengerEXT){
            vkDestroyDebugUtilsMessengerEXT(vk_ctx->instance, vk_ctx->debug_messenger, NULL);
        }
    }
    vkDestroyInstance(vk_ctx->instance, NULL);
}

internal void
renderer_backend_vk_draw(RendererBackend* base_ctx, u8 current_frame){
    UNUSED(base_ctx); UNUSED(current_frame);
    RendererBackendVk *vk_ctx = renderer_backend_as_vk(base_ctx);
    RendererBackendVk_FrameData *frame_data = &vk_ctx->frames[current_frame];
    VkDevice device = vk_ctx->device.device;
    VK_CHECK(vkWaitForFences(device, 1, &frame_data->render_fence, true, TO_USEC(1)));
    VK_CHECK(vkResetFences(device, 1, &frame_data->render_fence));

    u32 swapchainImageIndex;
	VK_CHECK(vkAcquireNextImageKHR(device, vk_ctx->swapchain.swapchain, TO_USEC(1), frame_data->swapchain_semaphore, NULL, &swapchainImageIndex));
}

