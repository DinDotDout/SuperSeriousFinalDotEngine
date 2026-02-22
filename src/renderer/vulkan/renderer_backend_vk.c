internal RendererBackendVk*
renderer_backend_as_vk(RendererBackend *base)
{
    DOT_ASSERT(base);
    DOT_ASSERT(base->backend_kind == RendererBackendKind_Vk);
    return cast(RendererBackendVk*) base;
}

// NOTE: Should we just cache as
// g_vk_ctx = backend and ommit passing the parameter everywhere?
internal RendererBackendVk*
renderer_backend_vk_create(Arena *arena)
{
    RendererBackendVk *backend = PUSH_STRUCT(arena, RendererBackendVk);
    RendererBackend *base = &backend->base;
    base->backend_kind = RendererBackendKind_Vk;
    base->init         = renderer_backend_vk_init;
    base->shutdown     = renderer_backend_vk_shutdown;
    base->begin_frame  = renderer_backend_vk_begin_frame;
    base->end_frame    = renderer_backend_vk_end_frame;
    base->clear_bg     = renderer_backend_vk_clear_bg;
    base->load_shader_module = renderer_backend_vk_load_shader_from_file_buffer;
    base->unload_shader_module = renderer_backend_vk_unload_shader_module;
    return backend;
}

// NOTE: Might want to remove const to make some things tweakable
internal const RBVK_Settings*
renderer_backend_vk_settings()
{
    static const String8 instance_extension_names[] = {
        String8Lit(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME),
        String8Lit(VK_KHR_SURFACE_EXTENSION_NAME),
#ifdef VK_EXT_DEBUG_UTILS_ENABLE
        String8Lit(VK_EXT_DEBUG_UTILS_EXTENSION_NAME),
#endif
        String8Lit(DOT_VK_SURFACE),
    };

    static const VkPhysicalDeviceDescriptorBufferFeaturesEXT desc_buffer_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
        .pNext = NULL,
        .descriptorBuffer = VK_TRUE,
    };

    static const VkPhysicalDeviceVulkan13Features features13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = cast(void*) &desc_buffer_features,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = true,
    };
    static const VkPhysicalDeviceVulkan12Features features12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = cast(void*) &features13,
        .bufferDeviceAddress = true,
        .descriptorIndexing = true,
        .descriptorBindingPartiallyBound = true,
        .descriptorBindingVariableDescriptorCount = true,
        .runtimeDescriptorArray = true,
    };

    static const String8 device_extension_names[] = {
        String8Lit(VK_KHR_SWAPCHAIN_EXTENSION_NAME),
        String8Lit(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME),
        // String8Lit(VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME),
    };

    static const String8 layer_names[] = {
#ifdef VALIDATION_LAYERS_ENABLE
        String8Lit("VK_LAYER_KHRONOS_validation"),
#endif
    };

    static const RBVK_Settings vk_settings = {
        .instance_settings = {
            .instance_extension_names = instance_extension_names,
            .instance_extension_count = ARRAY_COUNT(instance_extension_names),
        },
        .device_settings = {
            .device_extension_names = device_extension_names,
            .device_extension_count = ARRAY_COUNT(device_extension_names),
            .device_features        = &features12,
        },
        .layer_settings = {
            .layer_names = layer_names,
            .layer_count = ARRAY_COUNT(layer_names),
        },
        .swapchain_settings = {
            .preferred_format       = VK_FORMAT_B8G8R8A8_SRGB,
            .preferred_colorspace   = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
            .preferred_present_mode = VK_PRESENT_MODE_MAILBOX_KHR,
        },
        .frame_settings = {
            .frame_overlap = vk_settings.swapchain_settings.preferred_present_mode ==
                VK_PRESENT_MODE_MAILBOX_KHR ? 3 : 2,
        }
    };
    return &vk_settings;
}

internal inline VKAPI_ATTR u32 VKAPI_CALL
renderere_backend_vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data)
{
    UNUSED(message_type); UNUSED(user_data);
    if(message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT){
        DOT_WARNING("validation layer: %s", callback_data->pMessage);
    }
    return VK_FALSE;
}

internal DOT_ShaderModuleHandle
rbvk_dot_shader_module_from_vk_shader_module(VkShaderModule vk_sm)
{
    DOT_ShaderModuleHandle dot_smh = {
        .handle = cast(u64) vk_sm,
    };
    return dot_smh;
}

internal VkShaderModule
rbvk_vk_shader_module_from_dot_shader_module(DOT_ShaderModuleHandle dot_smh)
{
    VkShaderModule vk_sm = cast(VkShaderModule)dot_smh.handle[0];
    return vk_sm;
}

internal DOT_ShaderModuleHandle
renderer_backend_vk_load_shader_from_file_buffer(RendererBackend *base_ctx, DOT_FileBuffer file_buffer)
{
    RendererBackendVk *vk_ctx = renderer_backend_as_vk(base_ctx);
    RBVK_FileBuffer vk_file_buffer = {
        .buff = cast(u32*)file_buffer.buff,
        .size = file_buffer.size,
    };

    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = NULL,
        .codeSize = vk_file_buffer.size,
        .pCode = vk_file_buffer.buff,
    };

    VkShaderModule vk_shader_module = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(vk_ctx->device.device, &create_info, NULL, &vk_shader_module));
    DOT_ShaderModuleHandle dot_shader_module_handle = rbvk_dot_shader_module_from_vk_shader_module(vk_shader_module);
    return dot_shader_module_handle;
}

internal void
renderer_backend_vk_unload_shader_module(RendererBackend *base_ctx, DOT_ShaderModuleHandle shader_module_handle)
{
    RendererBackendVk *vk_ctx = renderer_backend_as_vk(base_ctx);
    VkShaderModule vk_sm = rbvk_vk_shader_module_from_dot_shader_module(shader_module_handle);
    vkDestroyShaderModule(vk_ctx->device.device, vk_sm, NULL);
}

internal RBVK_Image
rbvk_create_image(RendererBackendVk *ctx, VkImageCreateInfo *image_info){
    RBVK_Image image = {0};
    image.image_format = image_info->format;
    image.extent = image_info->extent;

    VkDevice device = ctx->device.device;
    VK_CHECK(vkCreateImage(device, image_info, NULL, &image.image));
    VkMemoryRequirements reqs;
    vkGetImageMemoryRequirements(device, image.image, &reqs);
    image.alloc = vk_memory_pools_bump(&ctx->memory_pools, reqs, VkMemory_PoolsKind_GpuOnly);

    VK_CHECK(vkBindImageMemory(device, image.image, image.alloc.memory, image.alloc.offset));
    VkImageViewCreateInfo image_view_create_info = {
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .image    = image.image,
        .format   = image_info->format,
        .subresourceRange = {
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        }
    };

    vkCreateImageView(device, &image_view_create_info, NULL, &image.image_view);
    return image;
}

internal void
rbvk_destroy_image(RendererBackendVk *ctx, RBVK_Image *image){
    VkDevice device = ctx->device.device;
    vkDestroyImageView(device, image->image_view, NULL);
    // vkUnmapMemory(device, image->alloc.memory);
    vkDestroyImage(device, image->image, NULL);
}

internal void renderer_backend_vk_init(RendererBackend *base_ctx, DOT_Window* window)
{
#ifdef USE_VOLK
        volkInitialize();
#endif
    RendererBackendVk *vk_ctx = renderer_backend_as_vk(base_ctx);
    Arena* ctx_arena = vk_ctx->base.permanent_arena;
    // vk_ctx->vk_allocator = VkAllocatorParams(ctx_arena);
    TempArena temp = threadctx_get_temp(NULL, 0);
    const RBVK_Settings* vk_settings = renderer_backend_vk_settings();
    if(!vk_helper_all_layers(vk_settings)){
        DOT_ERROR("Could not find all requested layers");
    }

    if(!vk_helper_instance_all_required_extensions(vk_settings)){
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
#ifdef USE_VOLK
        volkLoadInstance(vk_ctx->instance);
#endif
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
        VkHelper_CandidateDeviceInfo candidate_device_info = vk_helper_pick_best_device(vk_settings, vk_ctx->instance, vk_ctx->surface, window);
        RBVK_Device* device = &vk_ctx->device;
        device->gpu                = candidate_device_info.gpu;
        device->graphics_queue_idx = candidate_device_info.graphics_family;
        device->present_queue_idx  = candidate_device_info.present_family;
        device->is_integrated_gpu  = candidate_device_info.is_integrated_gpu;

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
            .ppEnabledExtensionNames = string8_array_to_str_array(
                temp.arena,
                vk_settings->device_settings.device_extension_count,
                vk_settings->device_settings.device_extension_names),
            .enabledExtensionCount   = vk_settings->device_settings.device_extension_count,
            .pNext = vk_settings->device_settings.device_features,
        };

        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(candidate_device_info.gpu, &properties);
        DOT_PRINT("Vulkan version: %u.%u.%u", VK_VERSION_MAJOR(properties.apiVersion), VK_VERSION_MINOR(properties.apiVersion), VK_VERSION_PATCH(properties.apiVersion));
        VK_CHECK(vkCreateDevice(candidate_device_info.gpu, &device_create_info, NULL, &device->device));

#ifdef USE_VOLK
        volkLoadDevice(device->device);
#endif
        vkGetDeviceQueue(device->device, candidate_device_info.graphics_family, 0, &device->graphics_queue);
        if(shared_present_graphics_queues){
            device->present_queue = device->graphics_queue;
        } else {
            DOT_ERROR("Different present and graphics queues unsupported");
            vkGetDeviceQueue(device->device, candidate_device_info.present_family, 0, &device->present_queue);
        }
    }
    {
        RBVK_Device *device = &vk_ctx->device;
        vk_memory_pools_create(device, &vk_ctx->memory_pools);
    }
    // TODO: We will probably need to move this to its own function to allow recreation
    // --- Create Swapchain ---
    {
        RBVK_Swapchain* swapchain = &vk_ctx->swapchain;
        VkHelper_SwapchainDetails details = {0};
        vk_helper_physical_device_swapchain_support(vk_settings, vk_ctx->device.gpu, vk_ctx->surface, window, &details);
        swapchain->extent = details.surface_extent;
        swapchain->image_format = details.best_surface_format.format;
        swapchain->images_count = details.image_count;

        VkExtent2D window_extents = {window->window->w, window->window->h};
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
            .imageExtent      = window_extents,
        };
        if(vk_ctx->device.graphics_queue_idx != vk_ctx->device.present_queue_idx){
            DOT_ERROR("Different present and graphics queues unsupported");
            u32 queues[] = {vk_ctx->device.graphics_queue_idx, vk_ctx->device.present_queue_idx};
            swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            swapchain_create_info.pQueueFamilyIndices = queues;
            swapchain_create_info.queueFamilyIndexCount = ARRAY_COUNT(queues);
        }
        VK_CHECK(vkCreateSwapchainKHR(vk_ctx->device.device, &swapchain_create_info, NULL, &swapchain->swapchain));
        VkExtent3D draw_image_extent = vk_helper_extent3d_from_extent2d(window_extents);
        vk_ctx->draw_extent = window_extents;
        VkImageCreateInfo image_info = {
            .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext       = NULL,
            .imageType   = VK_IMAGE_TYPE_2D,
            .format      = VK_FORMAT_R16G16B16A16_SFLOAT,
            .extent      = draw_image_extent,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,

            .mipLevels = 1,
            .arrayLayers = 1,

            // for MSAA. we will not be using it by default, so default it to 1
            // sample per pixel.
            .samples = VK_SAMPLE_COUNT_1_BIT,
            // optimal tiling, which means the image is stored on the best gpu format
            .tiling = VK_IMAGE_TILING_OPTIMAL, // VK_IMAGE_TILING_LINEAR (for staging / copying)
            .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                     VK_IMAGE_USAGE_STORAGE_BIT |
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        };

        vk_ctx->draw_image = rbvk_create_image(vk_ctx, &image_info);
 
        //  --- Create Swapchain Images ---
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
            VkImageViewCreateInfo image_view_create_info = {
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
                image_view_create_info.image = swapchain->images[i];
                VK_CHECK(vkCreateImageView(device, &image_view_create_info, NULL, &swapchain->image_views[i]));
            }
        }
    }
    // --- Create Frame Structures ---
    {
        u8 frame_overlap = vk_settings->frame_settings.frame_overlap;
        VkDevice device = vk_ctx->device.device;
        // WARN: Any allocation that can be recreated may be need to be pushed onto its own arena alloc ctx to avoid leaking
        vk_ctx->frame_data_count = frame_overlap;
        vk_ctx->frame_datas = PUSH_ARRAY(ctx_arena, RBVK_FrameData, frame_overlap);
        // --- Create Commands Buffers ---
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
                RBVK_FrameData *frame_data = &vk_ctx->frame_datas[i];
                VK_CHECK(vkCreateCommandPool(device, &command_pool_info, NULL, &frame_data->command_pool));
                cmd_alloc_info.commandPool = frame_data->command_pool;
                VK_CHECK(vkAllocateCommandBuffers(device, &cmd_alloc_info, &frame_data->frame_command_buffer));
                frame_data->frame_arena = ARENA_ALLOC(.parent = ctx_arena, .reserve_size = KB(32));
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
                RBVK_FrameData *frame_data = &vk_ctx->frame_datas[i];
                VK_CHECK(vkCreateFence(device, &fence_create_info, NULL, &frame_data->render_fence));
                VK_CHECK(vkCreateSemaphore(device, &semaphore_create_info, NULL, &frame_data->render_semaphore));
                VK_CHECK(vkCreateSemaphore(device, &semaphore_create_info, NULL, &frame_data->swapchain_semaphore));
            }
        }
        // --- Create descriptors
        {
            VkDescriptorPoolSize pool_sizes[] = {
                {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 4096},
                {.type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = 4096},
                {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 512},
                {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1},
                // Keeping this for the first draw compute pass on the tutorial
                {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1},
            };

            VkDescriptorPoolCreateInfo pool_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .flags = 0,
                .poolSizeCount = ARRAY_COUNT(pool_sizes),
                .pPoolSizes = pool_sizes,
                .maxSets = 2,
            };

            vkCreateDescriptorPool(device, &pool_info, NULL, &vk_ctx->descriptor_pool);
            VkDescriptorSetLayoutBinding bindings[] = {
                { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                    .descriptorCount = 4096, .stageFlags = VK_SHADER_STAGE_ALL },
                { .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                    .descriptorCount = 4096, .stageFlags = VK_SHADER_STAGE_ALL },
                { .binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .descriptorCount = 512, .stageFlags = VK_SHADER_STAGE_ALL },
                { .binding = 3, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_ALL },
            };

            VkDescriptorSetLayoutBinding bindings_compute[] = {
                { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_ALL },
            };
            // Bindless layout
            VkDescriptorSetLayoutCreateInfo bindless_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = ARRAY_COUNT(bindings),
                .pBindings = bindings,
            };

            VK_CHECK(vkCreateDescriptorSetLayout(device, &bindless_info, NULL, &vk_ctx->bindless_layout));

            // Compute layout
            VkDescriptorSetLayoutCreateInfo compute_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = ARRAY_COUNT(bindings_compute),
                .pBindings = bindings_compute,
            };

            VK_CHECK(vkCreateDescriptorSetLayout(device, &compute_info, NULL, &vk_ctx->compute_layout));
            VkDescriptorSetLayout layouts[] = {
                vk_ctx->bindless_layout,
                vk_ctx->compute_layout
            };

            VkDescriptorSetAllocateInfo alloc_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = vk_ctx->descriptor_pool,
                .descriptorSetCount = ARRAY_COUNT(layouts),
                .pSetLayouts = layouts,
            };

            VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, &vk_ctx->descriptor_sets));

        }
    }
    temp_arena_restore(temp);
}

internal void
renderer_backend_vk_shutdown(RendererBackend* base_ctx)
{
    const RBVK_Settings *settings = renderer_backend_vk_settings();
    RendererBackendVk *vk_ctx = renderer_backend_as_vk(base_ctx);
    VkDevice device = vk_ctx->device.device;

    vkDeviceWaitIdle(device);
    {
        vkDestroyDescriptorSetLayout(device, vk_ctx->compute_layout, NULL);
        vkDestroyDescriptorSetLayout(device, vk_ctx->bindless_layout, NULL);
        vkDestroyDescriptorPool(device, vk_ctx->descriptor_pool, NULL);
    }

    for(u32 i = 0; i < settings->frame_settings.frame_overlap; ++i){
        RBVK_FrameData* frame_data = &vk_ctx->frame_datas[i];
        vkDestroyCommandPool(device, frame_data->command_pool, NULL);
        vkDestroyFence(device, frame_data->render_fence, NULL);
        vkDestroySemaphore(device, frame_data->render_semaphore, NULL);
        vkDestroySemaphore(device, frame_data->swapchain_semaphore, NULL);
    }
    for(u32 i = 0; i < vk_ctx->swapchain.images_count; ++i){
        vkDestroyImageView(device, vk_ctx->swapchain.image_views[i], NULL);
    }
    rbvk_destroy_image(vk_ctx, &vk_ctx->draw_image);
    vkDestroySwapchainKHR(device, vk_ctx->swapchain.swapchain, NULL);
    vk_memory_pools_destroy(&vk_ctx->device, &vk_ctx->memory_pools);
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
renderer_backend_vk_clear_bg(RendererBackend *base_ctx, u8 current_frame, vec3 color)
{
    RendererBackendVk *vk_ctx = renderer_backend_as_vk(base_ctx);
    RBVK_FrameData *frame_data = &vk_ctx->frame_datas[current_frame];
    VkCommandBuffer cmd = frame_data->frame_command_buffer;
    RBVK_Image *draw_image = &vk_ctx->draw_image;

    VkClearColorValue clear_value = {
        .float32 = { color.r, color.g,color.b, 0 },
    };
	VkImageSubresourceRange clear_range = vk_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
	vkCmdClearColorImage(cmd, draw_image->image, VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1, &clear_range);
}

internal void
rbvk_draw_background(VkCommandBuffer cmd, VkImage image, u64 frame)
{
    f64 flash = fabs(sin(frame / 2000.f));
    VkClearColorValue clear_value = {
        .float32 = {0.0f, 0.0f, flash, 1.0f},
    };
	VkImageSubresourceRange clear_range = vk_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
	vkCmdClearColorImage(cmd, image, VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1, &clear_range);
}

internal void
renderer_backend_vk_begin_frame(RendererBackend *base_ctx, u8 current_frame)
{
    RendererBackendVk *vk_ctx = renderer_backend_as_vk(base_ctx);
    RBVK_FrameData *frame_data = &vk_ctx->frame_datas[current_frame];
    arena_reset(frame_data->frame_arena);
    VkDevice device = vk_ctx->device.device;
    VK_CHECK(vkWaitForFences(device, 1, &frame_data->render_fence, true, TO_USEC(1)));
    VK_CHECK(vkResetFences(device, 1, &frame_data->render_fence));

    VK_CHECK(vkAcquireNextImageKHR(device, vk_ctx->swapchain.swapchain,
                                   TO_USEC(1), frame_data->swapchain_semaphore,
                                   NULL, &frame_data->swapchain_image_idx));

    VkCommandBuffer cmd = frame_data->frame_command_buffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo cmd_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));
    RBVK_Image *draw_image = &vk_ctx->draw_image;
    vk_helper_transition_image(cmd, draw_image->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
}

internal void
renderer_backend_vk_end_frame(RendererBackend *base_ctx, u8 current_frame)
{
    RendererBackendVk *vk_ctx = renderer_backend_as_vk(base_ctx);
    RBVK_FrameData *frame_data = &vk_ctx->frame_datas[current_frame];
    VkCommandBuffer cmd = frame_data->frame_command_buffer;

    RBVK_Image *draw_image = &vk_ctx->draw_image;
    VkImage swapchain_image = vk_ctx->swapchain.images[frame_data->swapchain_image_idx];
 
    // NOTE: Copy draw image into swapchain
    {
        vk_helper_transition_image(cmd, draw_image->image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        vk_helper_transition_image(cmd, swapchain_image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        vk_helper_copy_image_to_image(cmd, draw_image->image, swapchain_image, vk_ctx->draw_extent, vk_ctx->swapchain.extent);
        vk_helper_transition_image(cmd, swapchain_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }

	VK_CHECK(vkEndCommandBuffer(cmd));
    VkCommandBufferSubmitInfo cmd_info = vk_command_buffer_submit_info(cmd);

    // Presnet image
    {
        VkSemaphoreSubmitInfo wait_info = vk_semaphore_submit_info(
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
            frame_data->swapchain_semaphore
        );
        VkSemaphoreSubmitInfo signal_info = vk_semaphore_submit_info(
            VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, frame_data->render_semaphore);

        VkSubmitInfo2 submit = vk_submit_info(&cmd_info, &signal_info, &wait_info);

        // submit command buffer to the queue and execute it.
        //  _renderFence will now block until the graphic commands finish execution
        VK_CHECK(vkQueueSubmit2(vk_ctx->device.graphics_queue, 1, &submit,
                                frame_data->render_fence));

        VkPresentInfoKHR presentInfo = {
	        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
	        .pNext = NULL,
	        .pSwapchains = &vk_ctx->swapchain.swapchain,
	        .swapchainCount = 1,
	        .pWaitSemaphores = &frame_data->render_semaphore,
	        .waitSemaphoreCount = 1,
	        .pImageIndices = &frame_data->swapchain_image_idx,
	    };
	    VK_CHECK(vkQueuePresentKHR(vk_ctx->device.graphics_queue, &presentInfo));
	}
}
