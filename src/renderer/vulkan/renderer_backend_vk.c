global RendererBackendVk *g_vk_ctx;

internal RendererBackendVk*
renderer_backend_as_vk(RendererBackend *base)
{
    DOT_ASSERT(base);
    DOT_ASSERT(base->backend_kind == RendererBackendKind_Vk);
    return cast(RendererBackendVk*) base;
}

internal RendererBackendVk*
renderer_backend_vk_create(Arena *arena, RendererBackendConfig *backend_config)
{
    renderer_backend_vk_merge_settings(backend_config);
    Arena *backend_arena = ARENA_ALLOC(
        .parent = arena,
        .reserve_size = backend_config->backend_memory_size,);
    RendererBackendVk *backend = PUSH_STRUCT(backend_arena, RendererBackendVk);
    RendererBackend *base = &backend->base;
    base->backend_kind    = RendererBackendKind_Vk;
    base->permanent_arena = backend_arena;
#define FN(ret, name, args) base->name = renderer_backend_vk_##name;
    RENDERER_BACKEND_FN_LIST
#undef FN

    g_vk_ctx = backend;
    return backend;
}

internal void
renderer_backend_vk_merge_settings(RendererBackendConfig *backend_config)
{
    VK_SETTINGS.frame_settings.frame_overlap = backend_config->frame_overlap;
    VK_SETTINGS.swapchain_settings.preferred_present_mode = vk_helper_present_mode_kind_to_vk_present_mode_khr(backend_config->present_mode);
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
        // os_print_stacktrace();
    }
    return VK_FALSE;
}

internal DOT_ShaderModuleHandle
rbvk_dot_shader_module_from_vk_shader_module(VkShaderModule vk_sm)
{
    DOT_ShaderModuleHandle dot_smh = {
        .handle[0] = cast(u64) vk_sm,
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
renderer_backend_vk_load_shader_from_file_buffer(DOT_FileBuffer file_buffer)
{
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
    VK_CHECK(vkCreateShaderModule(g_vk_ctx->device.device, &create_info, NULL, &vk_shader_module));
    DOT_ShaderModuleHandle dot_shader_module_handle = rbvk_dot_shader_module_from_vk_shader_module(vk_shader_module);
    return dot_shader_module_handle;
}

internal void
renderer_backend_vk_unload_shader_module(DOT_ShaderModuleHandle shader_module_handle)
{
    VkShaderModule vk_sm = rbvk_vk_shader_module_from_dot_shader_module(shader_module_handle);
    vkDestroyShaderModule(g_vk_ctx->device.device, vk_sm, NULL);
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

typedef struct RBVK_Buffer {
    VkBuffer        buffer;
    VkDeviceMemory  memory;
    VkDeviceSize    offset;
    VkDeviceSize    size;
} RBVK_Buffer;

internal RBVK_Buffer
rbvk_create_buffer(
    RendererBackendVk *ctx,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemory_PoolsKind pool_kind)
{
    RBVK_Buffer buf = {0};
    buf.size = size;

    VkDevice device = ctx->device.device;

    // Create the buffer
    VkBufferCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VK_CHECK(vkCreateBuffer(device, &info, NULL, &buf.buffer));

    // Allocate memory
    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(device, buf.buffer, &reqs);

    VkMemory_Alloc alloc = vk_memory_pools_bump(&ctx->memory_pools, reqs, pool_kind);

    buf.memory = alloc.memory;
    buf.offset = alloc.offset;

    VK_CHECK(vkBindBufferMemory(device, buf.buffer, buf.memory, buf.offset));

    return buf;
}

internal void
rbvk_destroy_image(RendererBackendVk *ctx, RBVK_Image *image){
    VkDevice device = ctx->device.device;
    vkDestroyImageView(device, image->image_view, NULL);
    // vkUnmapMemory(device, image->alloc.memory);
    vkDestroyImage(device, image->image, NULL);
}

internal void
renderer_backend_vk_init(DOT_Window* window)
{
#ifdef DOT_USE_VOLK
    volkInitialize();
#endif
    Arena *ctx_arena = g_vk_ctx->base.permanent_arena;
    // g_vk_ctx->vk_allocator = VkAllocatorParams(ctx_arena);
    TempArena temp = threadctx_get_temp(0,0);
    if(!vk_helper_all_layers(&VK_SETTINGS)){
        DOT_ERROR("Could not find all requested layers");
    }

    if(!vk_helper_instance_all_required_extensions(&VK_SETTINGS)){
        DOT_ERROR("Could not find all requested instance extensions");
    }

    // --- Create Instance ---
    {
        VkInstanceCreateInfo instance_create_info = {
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .ppEnabledLayerNames     = string8_array_to_str_array(
                temp.arena,
                VK_SETTINGS.validation_layers.count,
                VK_SETTINGS.validation_layers.data),
            .enabledLayerCount       = VK_SETTINGS.validation_layers.count,
            .ppEnabledExtensionNames = string8_array_to_str_array(
                temp.arena,
                VK_SETTINGS.instance_settings.instance_extensions.count,
                VK_SETTINGS.instance_settings.instance_extensions.data),
            .enabledExtensionCount = VK_SETTINGS.instance_settings.instance_extensions.count,
            .pApplicationInfo = &(VkApplicationInfo){
                .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pApplicationName   = cast(char*)VK_SETTINGS.instance_settings.application_name.str,
                .applicationVersion = VK_SETTINGS.instance_settings.application_version,
                .pEngineName        = cast(char*)VK_SETTINGS.instance_settings.engine_name.str,
                .engineVersion      = VK_SETTINGS.instance_settings.engine_version,
                .apiVersion         = VK_SETTINGS.instance_settings.api_version,
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

        VK_CHECK(vkCreateInstance(&instance_create_info, NULL, &g_vk_ctx->instance));
#ifdef DOT_USE_VOLK
        volkLoadInstance(g_vk_ctx->instance);
#endif
        if(VK_EXT_DEBUG_UTILS_ENABLE){

#ifndef DOT_USE_VOLK
            PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT =
                (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(g_vk_ctx->instance, "vkCreateDebugUtilsMessengerEXT");
#endif
            if(vkCreateDebugUtilsMessengerEXT){
                VK_CHECK(vkCreateDebugUtilsMessengerEXT(g_vk_ctx->instance, &debug_utils_info, NULL, &g_vk_ctx->debug_messenger));
            }
        }
    }

    // --- Create Surface --- 
    dot_window_create_surface(window, &g_vk_ctx->base);
 
    // --- Create Device --- 
    {
        VkHelper_CandidateDeviceInfo candidate_device_info = vk_helper_pick_best_device(&VK_SETTINGS, g_vk_ctx->instance, g_vk_ctx->surface);
        if(candidate_device_info.score == -1){
            DOT_ERROR("Could not find a suitable device");
        }

        RBVK_Device* device = &g_vk_ctx->device;
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
                VK_SETTINGS.device_settings.device_extension_count,
                VK_SETTINGS.device_settings.device_extension_names),
            .enabledExtensionCount   = VK_SETTINGS.device_settings.device_extension_count,
            .pNext = VK_SETTINGS.device_settings.device_features,
        };

        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(candidate_device_info.gpu, &properties);
        DOT_PRINT("Vulkan version: %u.%u.%u", VK_VERSION_MAJOR(properties.apiVersion), VK_VERSION_MINOR(properties.apiVersion), VK_VERSION_PATCH(properties.apiVersion));
        VK_CHECK(vkCreateDevice(candidate_device_info.gpu, &device_create_info, NULL, &device->device));

#ifdef DOT_USE_VOLK
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
        RBVK_Device *device = &g_vk_ctx->device;
        vk_memory_pools_create(device, &g_vk_ctx->memory_pools);
    }
    // TODO: We will probably need to move this to its own function to allow recreation
    // --- Create Swapchain ---
    {
        RBVK_Swapchain* swapchain = &g_vk_ctx->swapchain;
        VkHelper_SwapchainDetails details = {0};
        vk_helper_physical_device_swapchain_support(&VK_SETTINGS, g_vk_ctx->device.gpu, g_vk_ctx->surface, window, &details);
        swapchain->extent = details.surface_extent;
        swapchain->image_format = details.best_surface_format.format;
        swapchain->image_datas.count = details.image_count;

        VkExtent2D window_extents = {window->window->w, window->window->h};
        VkSwapchainCreateInfoKHR swapchain_create_info = {
            .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface          = g_vk_ctx->surface,
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
        if(g_vk_ctx->device.graphics_queue_idx != g_vk_ctx->device.present_queue_idx){
            DOT_ERROR("Different present and graphics queues unsupported");
            u32 queues[] = {g_vk_ctx->device.graphics_queue_idx, g_vk_ctx->device.present_queue_idx};
            swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            swapchain_create_info.pQueueFamilyIndices = queues;
            swapchain_create_info.queueFamilyIndexCount = ARRAY_COUNT(queues);
        }
        VK_CHECK(vkCreateSwapchainKHR(g_vk_ctx->device.device, &swapchain_create_info, NULL, &swapchain->swapchain));
        VkExtent3D draw_image_extent = vk_helper_extent3d_from_extent2d(window_extents);
        g_vk_ctx->draw_extent = window_extents;
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

        g_vk_ctx->draw_image = rbvk_create_image(g_vk_ctx, &image_info);
        {
            //  --- Create Image Datas ---
            VkDevice device = g_vk_ctx->device.device;
            vkGetSwapchainImagesKHR(device, swapchain->swapchain, &swapchain->image_datas.count, NULL);
            array(VkImage) swapchain_images = PUSH_ARRAY(temp.arena, VkImage, swapchain->image_datas.count);
            vkGetSwapchainImagesKHR(device, swapchain->swapchain, &swapchain->image_datas.count, swapchain_images);
            swapchain->image_datas.data = PUSH_ARRAY(ctx_arena, RBVK_SwapchainImageData, swapchain->image_datas.count);

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

            VkSemaphoreCreateInfo semaphore_create_info = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                .flags = 0,
                .pNext = NULL,
            };
                for(u32 i = 0; i < swapchain->image_datas.count; ++i){
                    RBVK_SwapchainImageData *framedata = &swapchain->image_datas.data[i];
                    // RBVK_SwapchainImageData *framedata = &SLICE_GET(swapchain->image_datas, i);
                    framedata->image = swapchain_images[i];
                    image_view_create_info.image = framedata->image;

                    VK_CHECK(vkCreateImageView(device, &image_view_create_info, NULL, &framedata->image_view));
                    VK_CHECK(vkCreateSemaphore(device, &semaphore_create_info, NULL, &framedata->image_semaphore));
                }
        }
    }
    // --- Create Frame Structures ---
    {
        u8 frame_overlap = VK_SETTINGS.frame_settings.frame_overlap;
        VkDevice device = g_vk_ctx->device.device;
        // WARN: Any allocation that can be recreated may be need to be pushed onto its own arena alloc ctx to avoid leaking
        g_vk_ctx->frame_data_count = frame_overlap;
        g_vk_ctx->frame_datas = PUSH_ARRAY(ctx_arena, RBVK_FrameData, frame_overlap);
        // --- Create Commands Buffers ---
        {
            VkCommandPoolCreateInfo command_pool_info = {
                .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = g_vk_ctx->device.graphics_queue_idx,
            };
            VkCommandBufferAllocateInfo cmd_alloc_info = {
                .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            };
            for(u8 i = 0; i < frame_overlap; ++i){
                RBVK_FrameData *frame_data = &g_vk_ctx->frame_datas[i];
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
                RBVK_FrameData *frame_data = &g_vk_ctx->frame_datas[i];
                VK_CHECK(vkCreateFence(device, &fence_create_info, NULL, &frame_data->render_fence));
                // VK_CHECK(vkCreateSemaphore(device, &semaphore_create_info, NULL, &frame_data->render_semaphore));
                VK_CHECK(vkCreateSemaphore(device, &semaphore_create_info, NULL, &frame_data->acquire_semaphore));
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

            vkCreateDescriptorPool(device, &pool_info, NULL, &g_vk_ctx->descriptor_pool);
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
            VK_CHECK(vkCreateDescriptorSetLayout(device, &bindless_info, NULL, &g_vk_ctx->bindless_layout));

            // Compute layout
            VkDescriptorSetLayoutCreateInfo compute_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = ARRAY_COUNT(bindings_compute),
                .pBindings = bindings_compute,
            };
            VK_CHECK(vkCreateDescriptorSetLayout(device, &compute_info, NULL, &g_vk_ctx->compute_layout));

            VkDescriptorSetLayout layouts[] = {
                g_vk_ctx->compute_layout,
                // g_vk_ctx->bindless_layout,
            };

            VkDescriptorSetAllocateInfo alloc_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = g_vk_ctx->descriptor_pool,
                .descriptorSetCount = ARRAY_COUNT(layouts),
                .pSetLayouts = layouts,
            };

            g_vk_ctx->descriptor_set_count = ARRAY_COUNT(layouts);
            VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, g_vk_ctx->descriptor_sets));
        }
    }
    // {
    //     VkDevice device = g_vk_ctx->device.device;
    //     VkPipelineLayoutCreateInfo pipeline_compute_layout = {
    //      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    //      .pNext = NULL,
    //      .pSetLayouts = &g_vk_ctx->compute_layout,
    //      .setLayoutCount = 1,
    //     };
    //
    //  VK_CHECK(vkCreatePipelineLayout(device, &pipeline_compute_layout, NULL, &g_vk_ctx->gradient_pipeline_layout));
    //     VkShaderModule compute_draw_shader = rbvk_vk_shader_module_from_dot_shader_module(test_shader_module->shader_module_handle);
    //  VkPipelineShaderStageCreateInfo stageinfo = {
    //      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    //      .pNext = NULL,
    //      .stage = VK_SHADER_STAGE_COMPUTE_BIT,
    //      .module = compute_draw_shader,
    //      .pName = "main",
    //  };
    //
    //  VkComputePipelineCreateInfo compute_pipeline_create_info = {
    //      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
    //      .pNext = NULL,
    //      .layout = g_vk_ctx->gradient_pipeline_layout,
    //      .stage = stageinfo,
    //  };
    //
    //  VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &compute_pipeline_create_info, NULL, &g_vk_ctx->gradient_pipeline));
    // }
    temp_arena_restore(temp);
}

void 
renderer_create_postprocess_module(DOT_ShaderModuleHandle shader_module_h)
{
        VkDescriptorSetLayout layouts[] = {
            g_vk_ctx->compute_layout,
            // g_vk_ctx->bindless_layout,
        };
        VkDevice device = g_vk_ctx->device.device;
        VkPipelineLayoutCreateInfo pipeline_compute_layout = {
	        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
	        .setLayoutCount = ARRAY_COUNT(layouts),
	        .pSetLayouts = layouts,
        };

	    VK_CHECK(vkCreatePipelineLayout(device, &pipeline_compute_layout, NULL, &g_vk_ctx->gradient_pipeline_layout));
        VkShaderModule compute_draw_shader = rbvk_vk_shader_module_from_dot_shader_module(shader_module_h);
	    VkPipelineShaderStageCreateInfo stageinfo = {
	        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
	        .pNext = NULL,
	        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
	        .module = compute_draw_shader,
	        .pName = "main",
	    };

	    VkComputePipelineCreateInfo compute_pipeline_create_info = {
	        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
	        .pNext = NULL,
	        .layout = g_vk_ctx->gradient_pipeline_layout,
	        .stage = stageinfo,
	    };

	    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &compute_pipeline_create_info, NULL, &g_vk_ctx->gradient_pipeline));
        VkDescriptorImageInfo imgInfo = {
	        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
	        .imageView = g_vk_ctx->draw_image.image_view,
	    };

	    VkWriteDescriptorSet drawImageWrite = {
	        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	        .pNext = NULL,
	        .dstBinding = 0,
	        .dstSet = g_vk_ctx->descriptor_sets[0],
	        .descriptorCount = 1,
	        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
	        .pImageInfo = &imgInfo,
	    };
	    vkUpdateDescriptorSets(g_vk_ctx->device.device, 1, &drawImageWrite, 0, NULL);

}

internal void
renderer_backend_vk_shutdown()
{
    VkDevice device = g_vk_ctx->device.device;

    vkDeviceWaitIdle(device);
    {
        vkDestroyDescriptorSetLayout(device, g_vk_ctx->compute_layout, NULL);
        vkDestroyDescriptorSetLayout(device, g_vk_ctx->bindless_layout, NULL);
        vkDestroyDescriptorPool(device, g_vk_ctx->descriptor_pool, NULL);
    }

    for(u32 i = 0; i < VK_SETTINGS.frame_settings.frame_overlap; ++i){
        RBVK_FrameData* frame_data = &g_vk_ctx->frame_datas[i];
        vkDestroyCommandPool(device, frame_data->command_pool, NULL);
        vkDestroyFence(device, frame_data->render_fence, NULL);
        vkDestroySemaphore(device, frame_data->acquire_semaphore, NULL);
    }
    for(u32 i = 0; i < g_vk_ctx->swapchain.image_datas.count; ++i){
        vkDestroyImageView(device, g_vk_ctx->swapchain.image_datas.data[i].image_view, NULL);
        vkDestroySemaphore(device, g_vk_ctx->swapchain.image_datas.data[i].image_semaphore, NULL);
    }

    vkDestroyPipelineLayout(device, g_vk_ctx->gradient_pipeline_layout, NULL);
    vkDestroyPipeline(device, g_vk_ctx->gradient_pipeline, NULL);

    rbvk_destroy_image(g_vk_ctx, &g_vk_ctx->draw_image);
    vkDestroySwapchainKHR(device, g_vk_ctx->swapchain.swapchain, NULL);
    vk_memory_pools_destroy(&g_vk_ctx->device, &g_vk_ctx->memory_pools);
    vkDestroySurfaceKHR(g_vk_ctx->instance, g_vk_ctx->surface, NULL);
    vkDestroyDevice(g_vk_ctx->device.device, NULL);
    if(VK_EXT_DEBUG_UTILS_ENABLE){
#ifndef DOT_USE_VOLK
        PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT =
            (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(g_vk_ctx->instance, "vkDestroyDebugUtilsMessengerEXT");
#endif
        if(vkDestroyDebugUtilsMessengerEXT){
            vkDestroyDebugUtilsMessengerEXT(g_vk_ctx->instance, g_vk_ctx->debug_messenger, NULL);
        }
    }
    vkDestroyInstance(g_vk_ctx->instance, NULL);
}

internal void
renderer_backend_vk_clear_bg(u8 current_frame, vec3 color)
{
	RBVK_FrameData *frame_data = &g_vk_ctx->frame_datas[current_frame];
	VkCommandBuffer cmd = frame_data->frame_command_buffer;
	(void)color;
	RBVK_Image *draw_image = &g_vk_ctx->draw_image;
	(void) draw_image;

	VkClearColorValue clear_value = {
	    .float32 = { color.r, color.g,color.b, 0 },
	};
	(void) clear_value;
	VkImageSubresourceRange clear_range = vk_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
	(void) clear_range;
	// vkCmdClearColorImage(cmd, draw_image->image, VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1, &clear_range);


    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, g_vk_ctx->gradient_pipeline);
	// bind the descriptor set containing the draw image for the compute pipeline
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, g_vk_ctx->gradient_pipeline_layout, 0, 1, g_vk_ctx->descriptor_sets, 0, NULL);
	// execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
	vkCmdDispatch(cmd, ceil(g_vk_ctx->draw_extent.width / 16.0), ceil(g_vk_ctx->draw_extent.height / 16.0), 1);
}

internal void
renderer_backend_vk_begin_frame(u8 current_frame)
{
    RBVK_FrameData *frame_data = &g_vk_ctx->frame_datas[current_frame];
    ARENA_RESET(frame_data->frame_arena);

    VkDevice device = g_vk_ctx->device.device;
    VK_CHECK(vkWaitForFences(device, 1, &frame_data->render_fence, true, TO_NSEC(1)));
    VK_CHECK(vkResetFences(device, 1, &frame_data->render_fence));

    VK_CHECK(vkAcquireNextImageKHR(device, g_vk_ctx->swapchain.swapchain,
        TO_NSEC(1), frame_data->acquire_semaphore,
        NULL, &frame_data->swapchain_image_idx));

    VkCommandBuffer cmd = frame_data->frame_command_buffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo cmd_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));
    RBVK_Image *draw_image = &g_vk_ctx->draw_image;
    VkImage swapchain_image = g_vk_ctx->swapchain.image_datas.data[frame_data->swapchain_image_idx].image;
    vk_helper_transition_image(cmd, swapchain_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    vk_helper_transition_image(cmd, draw_image->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
}

internal void
renderer_backend_vk_end_frame(u8 current_frame)
{
    RBVK_FrameData *frame_data = &g_vk_ctx->frame_datas[current_frame];
    VkCommandBuffer cmd = frame_data->frame_command_buffer;

    RBVK_Image *draw_image = &g_vk_ctx->draw_image;
    RBVK_SwapchainImageData *image_data = &g_vk_ctx->swapchain.image_datas.data[frame_data->swapchain_image_idx];
    VkImage swapchain_image = image_data->image;
    VkSemaphore swapchain_semaphore = image_data->image_semaphore;
 
    // Copy draw image into swapchain
    {
        vk_helper_transition_image(cmd, draw_image->image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        vk_helper_transition_image(cmd, swapchain_image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        vk_helper_copy_image_to_image(cmd, draw_image->image, swapchain_image, g_vk_ctx->draw_extent, g_vk_ctx->swapchain.extent);
        vk_helper_transition_image(cmd, swapchain_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }

	VK_CHECK(vkEndCommandBuffer(cmd));
    VkCommandBufferSubmitInfo cmd_info = vk_command_buffer_submit_info(cmd);

    // Present image
    {
        VkSemaphoreSubmitInfo wait_info = vk_semaphore_submit_info(
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
            frame_data->acquire_semaphore);
        VkSemaphoreSubmitInfo signal_info = vk_semaphore_submit_info(
            VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, 
            swapchain_semaphore);

        VkSubmitInfo2 submit = vk_submit_info(&cmd_info, &signal_info, &wait_info);
        VK_CHECK(vkQueueSubmit2(g_vk_ctx->device.graphics_queue, 1, &submit,
                                frame_data->render_fence));

        VkPresentInfoKHR presentInfo = {
	        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
	        .pNext = NULL,
	        .pSwapchains = &g_vk_ctx->swapchain.swapchain,
	        .swapchainCount = 1,
	        .pWaitSemaphores = &swapchain_semaphore,
	        .waitSemaphoreCount = 1,
	        .pImageIndices = &frame_data->swapchain_image_idx,
	    };
	    VK_CHECK(vkQueuePresentKHR(g_vk_ctx->device.graphics_queue, &presentInfo));
	}
}

/* ================================================================== */
/*  Overlay – Vulkan backend implementation                           */
/*                                                                    */
/*  Renders a 2-D overlay onto draw_image (before the blit to the     */
/*  swapchain).  All GPU memory comes from the existing VkMemory_Pools */
/*  (gpu_only for the font image, staging for vertex/index data).     */
/* ================================================================== */


#define RBVK_OVERLAY_MAX_VERTEX_BUFFER  (512 * 1024)
#define RBVK_OVERLAY_MAX_ELEMENT_BUFFER (128 * 1024)

typedef struct RBVK_OverlayPushConstants {
    f32 projection[4][4];
} RBVK_OverlayPushConstants;

typedef struct RBVK_OverlayState {
    VkPipeline           pipeline;
    VkPipelineLayout     pipeline_layout;
    VkDescriptorPool     descriptor_pool;
    VkDescriptorSetLayout global_set_layout;
    VkDescriptorSetLayout material_set_layout;
    VkDescriptorSet      descriptor_sets[2];
    u32                   descriptor_set_count;
    VkSampler            font_sampler;
    VkImage              font_image;
    VkImageView          font_image_view;
    VkShaderModule       vert_shader;
    VkShaderModule       frag_shader;
    VkRenderPass         render_pass;
    VkFramebuffer        framebuffer; /* single fb for draw_image */

    /* Offsets into the staging pool buffer for vertex/index data */
    u64                  vertex_offset;
    u64                  index_offset;
} RBVK_OverlayState;

global RBVK_OverlayState g_rbvk_overlay;

/* ------------------------------------------------------------------ */
/*  Render pass (overlay on draw_image, GENERAL → GENERAL)            */
/* ------------------------------------------------------------------ */

internal void
rbvk_overlay_create_render_pass(RBVK_OverlayState *overlay_state)
{
    VkDevice device = g_vk_ctx->device.device;
    VkFormat color_format = g_vk_ctx->draw_image.image_format;

    VkAttachmentDescription color_attachment = {
        .format         = color_format,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_GENERAL,
        .finalLayout    = VK_IMAGE_LAYOUT_GENERAL,
    };

    VkAttachmentReference color_ref = {
        .attachment = 0,
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &color_ref,
    };

    VkSubpassDependency dependency = {
        .srcSubpass    = VK_SUBPASS_EXTERNAL,
        .dstSubpass    = 0,
        .srcStageMask  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo rp_info = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &color_attachment,
        .subpassCount    = 1,
        .pSubpasses      = &subpass,
        .dependencyCount = 1,
        .pDependencies   = &dependency,
    };
    VK_CHECK(vkCreateRenderPass(device, &rp_info, NULL, &overlay_state->render_pass));
}

/* ------------------------------------------------------------------ */
/*  Single framebuffer (wrapping draw_image)                          */
/* ------------------------------------------------------------------ */

internal void
rbvk_overlay_create_framebuffer(RBVK_OverlayState *overlay_state)
{
    VkDevice device = g_vk_ctx->device.device;
    RBVK_Image *draw = &g_vk_ctx->draw_image;

    VkFramebufferCreateInfo fb_info = {
        .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass      = overlay_state->render_pass,
        .attachmentCount = 1,
        .pAttachments    = &draw->image_view,
        .width           = g_vk_ctx->draw_extent.width,
        .height          = g_vk_ctx->draw_extent.height,
        .layers          = 1,
    };
    VK_CHECK(vkCreateFramebuffer(device, &fb_info, NULL, &overlay_state->framebuffer));
}

/* ------------------------------------------------------------------ */
/*  Font atlas → VkImage from gpu_only pool + staging upload          */
/* ------------------------------------------------------------------ */

internal void
rbvk_overlay_upload_font(RBVK_OverlayState *overlay_state, const void *pixels, int atlas_w, int atlas_h)
{
    VkDevice device = g_vk_ctx->device.device;
    VkMemory_Pools *pools = &g_vk_ctx->memory_pools;
    VkDeviceSize image_size = (VkDeviceSize)atlas_w * atlas_h * 4;
 
    // TODO: USE rbvk_create_image
    /* --- Create font image and bind to gpu_only pool --- */
    VkImageCreateInfo img_info = {
        .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType   = VK_IMAGE_TYPE_2D,
        .format      = VK_FORMAT_R8G8B8A8_UNORM,
        .extent      = { (u32)atlas_w, (u32)atlas_h, 1 },
        .mipLevels   = 1,
        .arrayLayers = 1,
        .samples     = VK_SAMPLE_COUNT_1_BIT,
        .tiling      = VK_IMAGE_TILING_OPTIMAL,
        .usage       = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VK_CHECK(vkCreateImage(device, &img_info, NULL, &overlay_state->font_image));

    VkMemoryRequirements img_reqs;
    vkGetImageMemoryRequirements(device, overlay_state->font_image, &img_reqs);

    VkMemory_Alloc font_alloc = vk_memory_pools_bump(pools, img_reqs, VkMemory_PoolsKind_GpuOnly);
    VK_CHECK(vkBindImageMemory(device, overlay_state->font_image, font_alloc.memory, font_alloc.offset));

    /* --- Copy pixels through the staging buffer --- */
    {
        /* Map a region of the staging buffer and write pixels */
        void *mapped;
        VK_CHECK(vkMapMemory(device, pools->staging_mem, 0, image_size, 0, &mapped));
        MEM_COPY(mapped, pixels, (usize)image_size);
        vkUnmapMemory(device, pools->staging_mem);

        /* One-shot command buffer for the upload */
        VkCommandPool tmp_pool;
        VkCommandPoolCreateInfo pool_ci = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = g_vk_ctx->device.graphics_queue_idx,
        };
        VK_CHECK(vkCreateCommandPool(device, &pool_ci, NULL, &tmp_pool));

        VkCommandBuffer cmd;
        VkCommandBufferAllocateInfo cmd_alloc = {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = tmp_pool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        VK_CHECK(vkAllocateCommandBuffers(device, &cmd_alloc, &cmd));

        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));

        VkImageMemoryBarrier barrier_to_dst = {
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = 0,
            .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = overlay_state->font_image,
            .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        };
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, NULL, 0, NULL, 1, &barrier_to_dst);

        VkBufferImageCopy region = {
            .bufferOffset      = 0, /* start of the staging buffer */
            .bufferRowLength   = 0,
            .bufferImageHeight = 0,
            .imageSubresource  = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
            .imageOffset       = {0, 0, 0},
            .imageExtent       = { (u32)atlas_w, (u32)atlas_h, 1 },
        };
        vkCmdCopyBufferToImage(cmd, pools->staging_buffer, overlay_state->font_image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        VkImageMemoryBarrier barrier_to_read = {
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = overlay_state->font_image,
            .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        };
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, NULL, 0, NULL, 1, &barrier_to_read);

        VK_CHECK(vkEndCommandBuffer(cmd));

        VkSubmitInfo submit = {
            .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers    = &cmd,
        };
        VK_CHECK(vkQueueSubmit(g_vk_ctx->device.graphics_queue, 1, &submit, VK_NULL_HANDLE));
        VK_CHECK(vkQueueWaitIdle(g_vk_ctx->device.graphics_queue));

        vkDestroyCommandPool(device, tmp_pool, NULL);
    }

    /* --- Image view --- */
    VkImageViewCreateInfo view_info = {
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image    = overlay_state->font_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format   = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
    };
    VK_CHECK(vkCreateImageView(device, &view_info, NULL, &overlay_state->font_image_view));

    /* --- Sampler --- */
    VkSamplerCreateInfo sampler_info = {
        .sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter    = VK_FILTER_LINEAR,
        .minFilter    = VK_FILTER_LINEAR,
        .mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    };
    VK_CHECK(vkCreateSampler(device, &sampler_info, NULL, &overlay_state->font_sampler));
}

/* ------------------------------------------------------------------ */
/*  Descriptor set (combined image sampler for font texture)          */
/* ------------------------------------------------------------------ */

internal void
rbvk_overlay_create_descriptors(RBVK_OverlayState *overlay_state)
{
    VkDevice device = g_vk_ctx->device.device;

    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings    = &(VkDescriptorSetLayoutBinding) {
            .binding         = 0,
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
        },

    };
    VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_info, NULL, &overlay_state->material_set_layout));

    VkDescriptorSetLayoutCreateInfo ubo_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &(VkDescriptorSetLayoutBinding){
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,

        },
    };
    VK_CHECK(vkCreateDescriptorSetLayout(device, &ubo_layout_info, NULL, &overlay_state->global_set_layout));
    VkDescriptorPoolSize pool_sizes[] = {
        { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1 },          // for set 0
        { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1 },  // for set 1
    };
    VkDescriptorPoolCreateInfo pool_info = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = ARRAY_COUNT(pool_sizes),
        .pPoolSizes    = pool_sizes,
        .maxSets       = 2,
    };

    VkDescriptorSetLayout layouts[] = {
        overlay_state->global_set_layout,
        overlay_state->material_set_layout,
    };
    VK_CHECK(vkCreateDescriptorPool(device, &pool_info, NULL, &overlay_state->descriptor_pool));
    VkDescriptorSetAllocateInfo dset_alloc = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = overlay_state->descriptor_pool,
        .descriptorSetCount = ARRAY_COUNT(layouts),
        .pSetLayouts        = layouts,
    };

    overlay_state->descriptor_set_count = ARRAY_COUNT(layouts);
    VK_CHECK(vkAllocateDescriptorSets(device, &dset_alloc, overlay_state->descriptor_sets));

    VkWriteDescriptorSet write = {
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = overlay_state->descriptor_sets[1],
        .dstBinding      = 0,
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo      = &(VkDescriptorImageInfo){
            .sampler     = overlay_state->font_sampler,
            .imageView   = overlay_state->font_image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
    };
    vkUpdateDescriptorSets(device, 1, &write, 0, NULL);

    typedef struct {
        f32 projection[4][4];
        // mat4 projection;
    } OverlayUBO;

    // VkBufferCreateInfo buf_info = {
    //     .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    //     .size = sizeof(OverlayUBO),
    //     .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    //     .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    // };
    //
    //
    // TODO: Update the 500 to match viewport
    OverlayUBO ubo_data = {0};
    ubo_data.projection[0][0] =  2.0f / 500;
    ubo_data.projection[1][1] = -2.0f / 500;
    ubo_data.projection[2][2] = -1.0f;
    ubo_data.projection[3][0] = -1.0f;
    ubo_data.projection[3][1] =  1.0f;
    ubo_data.projection[3][3] =  1.0f;

    RBVK_Buffer overlay_ubo_buffer = rbvk_create_buffer(g_vk_ctx,
                       sizeof(OverlayUBO),
                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                       VkMemory_PoolsKind_Staging);
    // VkDeviceMemory       ubo_memory;
    // VkDeviceSize         ubo_offset;

    void* mapped;
    vkMapMemory(device,
        overlay_ubo_buffer.memory,
        overlay_ubo_buffer.offset,
        sizeof(ubo_data),
        0,
        &mapped);

    memcpy(mapped, &ubo_data, sizeof(ubo_data));
    vkUnmapMemory(device, overlay_ubo_buffer.memory);

    VkDescriptorBufferInfo buffer_info = {
        .buffer = overlay_ubo_buffer.buffer,
        .offset = 0,
        .range  = sizeof(OverlayUBO),
    };

    VkWriteDescriptorSet write_ubo = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = g_rbvk_overlay.descriptor_sets[0],      // set = 0
        .dstBinding = 0,                   // binding = 0
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &buffer_info,
    };

    vkUpdateDescriptorSets(device, 1, &write_ubo, 0, NULL);
}

/* ------------------------------------------------------------------ */
/*  Graphics pipeline                                                 */
/* ------------------------------------------------------------------ */

internal void
rbvk_overlay_create_pipeline(RBVK_OverlayState *overlay_state)
{
    VkDevice device = g_vk_ctx->device.device;

    VkShaderModuleCreateInfo vert_create_info = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(nuklearshaders_nuklear_vert_spv),
        .pCode    = cast(u32*) nuklearshaders_nuklear_vert_spv,
    };
    VK_CHECK(vkCreateShaderModule(device, &vert_create_info, NULL, &overlay_state->vert_shader));

    VkShaderModuleCreateInfo frag_create_info = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(nuklearshaders_nuklear_frag_spv),
        .pCode    = cast(u32*) nuklearshaders_nuklear_frag_spv,
    };
    VK_CHECK(vkCreateShaderModule(device, &frag_create_info, NULL, &overlay_state->frag_shader));

    VkPipelineShaderStageCreateInfo stages[2] = {
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_VERTEX_BIT,
            .module = overlay_state->vert_shader,
            .pName  = "main",
        },
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = overlay_state->frag_shader,
            .pName  = "main",
        },
    };

    VkVertexInputBindingDescription vtx_binding = {
        .binding   = 0,
        .stride    = sizeof(OverlayVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    VkVertexInputAttributeDescription attrs[3] = {
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT,
          .offset = offsetof(OverlayVertex, position) },
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT,
          .offset = offsetof(OverlayVertex, uv) },
        { .location = 2, .binding = 0, .format = VK_FORMAT_R8G8B8A8_UINT,
          .offset = offsetof(OverlayVertex, col) },
    };

    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &vtx_binding,
        .vertexAttributeDescriptionCount = ARRAY_COUNT(attrs),
        .pVertexAttributeDescriptions    = attrs,
    };

    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset     = 0,
        .size       = sizeof(RBVK_OverlayPushConstants),
    };

    VkDescriptorSetLayout set_layouts[] = {
        overlay_state->global_set_layout,   // set 0
        overlay_state->material_set_layout,  // set 1
    };

    VkPipelineLayoutCreateInfo pipe_layout_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = ARRAY_COUNT(set_layouts),
        .pSetLayouts            = set_layouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push_range,
    };

    VK_CHECK(vkCreatePipelineLayout(device, &pipe_layout_info, NULL, &overlay_state->pipeline_layout));

    VkGraphicsPipelineCreateInfo pipeline_create_infos = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount          = ARRAY_COUNT(stages),
        .pStages             = stages,
        .pVertexInputState   = &vertex_input,
        .pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo) {
            .sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        },
        .pViewportState = &(VkPipelineViewportStateCreateInfo){
            .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount  = 1,
        } ,
        .pRasterizationState = &(VkPipelineRasterizationStateCreateInfo){
            .sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode    = VK_CULL_MODE_NONE,
            .frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .lineWidth   = 1.0f,
        },
        .pMultisampleState   = &(VkPipelineMultisampleStateCreateInfo){
            .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        },
        .pDepthStencilState  = &(VkPipelineDepthStencilStateCreateInfo){
            .sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable  = VK_FALSE,
            .depthWriteEnable = VK_FALSE,
        },
        .pColorBlendState    = &(VkPipelineColorBlendStateCreateInfo){
            .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments    = &(VkPipelineColorBlendAttachmentState){
                .blendEnable         = VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp        = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp        = VK_BLEND_OP_ADD,
                .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            },
        },
        .pDynamicState       = &(VkPipelineDynamicStateCreateInfo) {
            .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = ARRAY_COUNT(dynamic_states),
            .pDynamicStates    = dynamic_states,
        },
        .layout              = overlay_state->pipeline_layout,
        .renderPass          = overlay_state->render_pass,
        .subpass             = 0,
    };
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_create_infos, NULL, &overlay_state->pipeline));
}

/* ------------------------------------------------------------------ */
/*  Reserve vertex / index regions from the staging pool              */
/* ------------------------------------------------------------------ */

internal void
rbvk_overlay_reserve_buffers(RBVK_OverlayState *s)
{
    VkMemory_Pools *pools = &g_vk_ctx->memory_pools;

    /* Vertex region */
    VkMemoryRequirements vtx_reqs = {
        .size           = RBVK_OVERLAY_MAX_VERTEX_BUFFER,
        .alignment      = 16,
        .memoryTypeBits = (1u << pools->staging_type),
    };
    VkMemory_Alloc vtx_alloc = vk_memory_pools_bump(pools, vtx_reqs, VkMemory_PoolsKind_Staging);
    s->vertex_offset = vtx_alloc.offset;
    /* Manually advance past the allocation (pool bump for staging doesn't add size) */
    pools->staging_used = vtx_alloc.offset + RBVK_OVERLAY_MAX_VERTEX_BUFFER;

    /* Index region */
    VkMemoryRequirements idx_reqs = {
        .size           = RBVK_OVERLAY_MAX_ELEMENT_BUFFER,
        .alignment      = 16,
        .memoryTypeBits = (1u << pools->staging_type),
    };
    VkMemory_Alloc idx_alloc = vk_memory_pools_bump(pools, idx_reqs, VkMemory_PoolsKind_Staging);
    s->index_offset = idx_alloc.offset;
    pools->staging_used = idx_alloc.offset + RBVK_OVERLAY_MAX_ELEMENT_BUFFER;
}

/* ------------------------------------------------------------------ */
/*  Public backend interface                                          */
/* ------------------------------------------------------------------ */

internal void
renderer_backend_vk_overlay_init(const void *font_pixels, int font_w, int font_h)
{
    MEMORY_ZERO_STRUCT(&g_rbvk_overlay);
    rbvk_overlay_create_render_pass(&g_rbvk_overlay);
    rbvk_overlay_create_framebuffer(&g_rbvk_overlay);
    rbvk_overlay_upload_font(&g_rbvk_overlay, font_pixels, font_w, font_h);
    rbvk_overlay_create_descriptors(&g_rbvk_overlay);
    rbvk_overlay_create_pipeline(&g_rbvk_overlay);
    rbvk_overlay_reserve_buffers(&g_rbvk_overlay);
}

internal void
renderer_backend_vk_overlay_render(u8 frame_idx, OverlayDrawList *draw_list)
{
    RBVK_FrameData *fd = &g_vk_ctx->frame_datas[frame_idx];
    VkCommandBuffer cmd = fd->frame_command_buffer;
    VkDevice device = g_vk_ctx->device.device;
    VkMemory_Pools *pools = &g_vk_ctx->memory_pools;

    /* Upload vertex data into the staging buffer region */
    if (draw_list->vertex_size > 0) {
        void *mapped;
        VK_CHECK(vkMapMemory(device, pools->staging_mem, g_rbvk_overlay.vertex_offset,
                             draw_list->vertex_size, 0, &mapped));
        MEM_COPY(mapped, draw_list->vertices, draw_list->vertex_size);
        vkUnmapMemory(device, pools->staging_mem);
    }

    /* Upload index data into the staging buffer region */
    if (draw_list->index_size > 0) {
        void *mapped;
        VK_CHECK(vkMapMemory(device, pools->staging_mem, g_rbvk_overlay.index_offset,
                             draw_list->index_size, 0, &mapped));
        MEM_COPY(mapped, draw_list->indices, draw_list->index_size);
        vkUnmapMemory(device, pools->staging_mem);
    }

    /* Begin overlay render pass on draw_image */
    VkRenderPassBeginInfo rp_begin = {
        .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass  = g_rbvk_overlay.render_pass,
        .framebuffer = g_rbvk_overlay.framebuffer,
        .renderArea  = { {0, 0}, g_vk_ctx->draw_extent },
    };
    vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_rbvk_overlay.pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        g_rbvk_overlay.pipeline_layout, 0, 2, g_rbvk_overlay.descriptor_sets, 0, NULL);

    /* Bind vertex/index buffers from the staging pool */
    VkDeviceSize vb_offset = g_rbvk_overlay.vertex_offset;
    vkCmdBindVertexBuffers(cmd, 0, 1, &pools->staging_buffer, &vb_offset);
    vkCmdBindIndexBuffer(cmd, pools->staging_buffer, g_rbvk_overlay.index_offset, VK_INDEX_TYPE_UINT16);

    /* Push orthographic projection */
    {
        RBVK_OverlayPushConstants pc;
        MEMORY_ZERO_STRUCT(&pc);
        pc.projection[0][0] =  2.0f / (f32)draw_list->width;
        pc.projection[1][1] = -2.0f / (f32)draw_list->height;
        pc.projection[2][2] = -1.0f;
        pc.projection[3][0] = -1.0f;
        pc.projection[3][1] =  1.0f;
        pc.projection[3][3] =  1.0f;
        vkCmdPushConstants(cmd, g_rbvk_overlay.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(RBVK_OverlayPushConstants), &pc);
    }

    VkViewport vp = { 0, 0, (f32)draw_list->width, (f32)draw_list->height, 0.0f, 1.0f };
    vkCmdSetViewport(cmd, 0, 1, &vp);

    /* Draw commands */
    u32 index_offset = 0;
    for (u32 i = 0; i < draw_list->cmd_count; ++i) {
        OverlayDrawCmd *dc = &draw_list->cmds[i];
        if (!dc->elem_count) continue;

        VkRect2D scissor = {
            .offset = {
                .x = MAX((i32)dc->clip_x, 0),
                .y = MAX((i32)dc->clip_y, 0),
            },
            .extent = {
                .width  = (u32)dc->clip_w,
                .height = (u32)dc->clip_h,
            },
        };
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdDrawIndexed(cmd, dc->elem_count, 1, index_offset, 0, 0);
        index_offset += dc->elem_count;
    }

    vkCmdEndRenderPass(cmd);
}

internal void
renderer_backend_vk_overlay_shutdown(void)
{
    VkDevice device = g_vk_ctx->device.device;
    vkDeviceWaitIdle(device);

    vkDestroyPipeline(device, g_rbvk_overlay.pipeline, NULL);
    vkDestroyPipelineLayout(device, g_rbvk_overlay.pipeline_layout, NULL);
    vkDestroyShaderModule(device, g_rbvk_overlay.vert_shader, NULL);
    vkDestroyShaderModule(device, g_rbvk_overlay.frag_shader, NULL);

    vkDestroyDescriptorPool(device, g_rbvk_overlay.descriptor_pool, NULL);
    vkDestroyDescriptorSetLayout(device, g_rbvk_overlay.material_set_layout, NULL);
    vkDestroyDescriptorSetLayout(device, g_rbvk_overlay.global_set_layout, NULL);

    vkDestroySampler(device, g_rbvk_overlay.font_sampler, NULL);
    vkDestroyImageView(device, g_rbvk_overlay.font_image_view, NULL);
    vkDestroyImage(device, g_rbvk_overlay.font_image, NULL);
    /* font image memory is owned by the gpu_only pool – not freed here */

    vkDestroyFramebuffer(device, g_rbvk_overlay.framebuffer, NULL);
    vkDestroyRenderPass(device, g_rbvk_overlay.render_pass, NULL);
    /* vertex/index memory is owned by the staging pool – not freed here */

    MEMORY_ZERO_STRUCT(&g_rbvk_overlay);
}
