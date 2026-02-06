#ifndef VK_HELPER_H
#define VK_HELPER_H
#include <vulkan/vk_enum_string_helper.h>

typedef struct VkSwapchainDetails{
    VkSurfaceFormatKHR            best_surface_format;
    VkPresentModeKHR              best_present_mode;
    VkExtent2D                    surface_extent;
    VkSurfaceTransformFlagBitsKHR current_transform;
    u32                           image_count;
}VkSwapchainDetails;

typedef struct VkCandidateDeviceInfo{
    VkPhysicalDevice gpu;
    u16 graphics_family;
    u16 present_family;
    i32 score;
    b8 shared_present_graphics_queues;
}VkCandidateDeviceInfo;

#ifdef NDEBUG
#define VK_CHECK(x) x
#else
#define VK_CHECK(x) \
    do { \
        VkResult err = x; \
        if(err < 0){ \
            printf("Detected Vulkan error: %s\n", string_VkResult(err)); \
            abort(); \
        } \
    } while (0)
#endif

#define VkSurfaceFormat2KHRParams(...) \
    (VkSurfaceFormat2KHR){ \
        .sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR, \
        __VA_ARGS__ \
    }

#define VkSurfaceCapabilities2KHRParams(...) \
    (VkSurfaceCapabilities2KHR){ \
        .sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR, \
        __VA_ARGS__ \
    }

#define VkPhysicalDeviceSurfaceInfo2KHRParams(...) \
    (VkPhysicalDeviceSurfaceInfo2KHR){ \
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR, \
        __VA_ARGS__ \
    }

internal inline b8
vk_all_layers(const RBVK_Settings* vk_settings)
{
    TempArena temp = threadctx_get_temp(NULL, 0);
    u32 available_layer_count = 0;
    vkEnumerateInstanceLayerProperties(&available_layer_count, NULL);
    array(VkLayerProperties) available_layers = PUSH_ARRAY(temp.arena, VkLayerProperties, available_layer_count);
    vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers);
    b8 all_found = true;
    for(u64 i = 0; i < vk_settings->layer_settings.layer_count; ++i){
        b8 found = false;
        const String8 layer_name = vk_settings->layer_settings.layer_names[i];
        for(u64 j = 0; j < available_layer_count; ++j){
            if(string8_equal(layer_name, string8_cstring(available_layers[j].layerName))){
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

internal inline b8
vk_instance_all_required_extensions(const RBVK_Settings* vk_settings)
{
    TempArena temp = threadctx_get_temp(NULL, 0);
    u32 extension_count;
    vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);
    array(VkExtensionProperties) available_extensions = PUSH_ARRAY(temp.arena, VkExtensionProperties, extension_count);
    vkEnumerateInstanceExtensionProperties(NULL, &extension_count, available_extensions);

    b8 all_found = true;
    for(u64 i = 0; i < vk_settings->instance_settings.instance_extension_count; ++i){
        b8 found = false;
        const String8 instance_extension_name = vk_settings->instance_settings.instance_extension_names[i];
        for(u64 j = 0; j < extension_count; ++j){
            char* name = available_extensions[j].extensionName;
            if(string8_equal(instance_extension_name, string8_cstring(name))){
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

internal inline b8
vk_physical_device_all_required_extensions(const RBVK_Settings* vk_settings, VkPhysicalDevice device)
{
    TempArena temp = threadctx_get_temp(NULL, 0);
    u32 extension_count;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);
    array(VkExtensionProperties) available_extensions = PUSH_ARRAY(temp.arena, VkExtensionProperties, extension_count);
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, available_extensions);

    b8 all_found = true;
    for(u64 i = 0; i < vk_settings->device_settings.device_extension_count; ++i){
        b8 found = false;
        String8 device_extension_name = vk_settings->device_settings.device_extension_names[i];
        for(u64 j = 0; j < extension_count; ++j){
            if(string8_equal(device_extension_name, string8_cstring(available_extensions[j].extensionName)) == 0){
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

internal inline b8
vk_physical_device_swapchain_support(
    const RBVK_Settings *vk_settings,
    VkPhysicalDevice gpu,
    VkSurfaceKHR surface,
    DOT_Window* window,
    VkSwapchainDetails* details)
{
    TempArena temp = threadctx_get_temp(NULL, 0);
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
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormats2KHR(gpu, &surface_info, &swapchain_support_details.format_count, NULL));
    swapchain_support_details.surface_formats = PUSH_ARRAY(temp.arena, VkSurfaceFormat2KHR, swapchain_support_details.format_count);
    for(u32 it = 0; it < swapchain_support_details.format_count; ++it){
        swapchain_support_details.surface_formats[it] = VkSurfaceFormat2KHRParams();
    }
    vkGetPhysicalDeviceSurfaceFormats2KHR(gpu, &surface_info, &swapchain_support_details.format_count, swapchain_support_details.surface_formats);
 
    // --- Query Present Modes ---
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &swapchain_support_details.present_modes_count, NULL);
    swapchain_support_details.present_modes = PUSH_ARRAY(temp.arena, VkPresentModeKHR, swapchain_support_details.present_modes_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &swapchain_support_details.present_modes_count, swapchain_support_details.present_modes);
    b8 has_support = swapchain_support_details.format_count > 0 && swapchain_support_details.present_modes_count > 0;
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

        VkFormat         preferred_format       = vk_settings->swapchain_settings.preferred_format;
        VkColorSpaceKHR  preferred_colorspace   = vk_settings->swapchain_settings.preferred_colorspace;
        VkPresentModeKHR preferred_present_mode = vk_settings->swapchain_settings.preferred_present_mode;

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
        details->current_transform   = surface_capabilities.currentTransform;
    }
    temp_arena_restore(&temp);
    return has_support;
}

internal inline VkCandidateDeviceInfo
vk_pick_best_device(
    const RBVK_Settings *vk_settings,
    VkInstance instance,
    VkSurfaceKHR surface,
    DOT_Window *window)
{
    TempArena temp = threadctx_get_temp(NULL, 0);
    u32 device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, NULL);
    if (device_count == 0) {
        DOT_ERROR("No Vulkan-capable GPUs found");
    }

    array(VkPhysicalDevice) devices = PUSH_ARRAY(temp.arena, VkPhysicalDevice, device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices);
    DOT_PRINT("device count %i", device_count);

    VkCandidateDeviceInfo best_device = {0};
    best_device.score = -1;

    for (u32 i = 0; i < device_count; i++){
        VkPhysicalDevice dev = devices[i];
        // We to first ensure we have what we want before even rating the device
        if (!vk_physical_device_all_required_extensions(vk_settings, dev) ||
            !vk_physical_device_swapchain_support(vk_settings, dev, surface, window, NULL)){
            continue;
        }
        VkPhysicalDeviceProperties device_properties;
        VkPhysicalDeviceFeatures device_features;
        vkGetPhysicalDeviceProperties(dev, &device_properties);
        vkGetPhysicalDeviceFeatures(dev, &device_features);

        u32 queue_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queue_count, NULL);
        array(VkQueueFamilyProperties) queue_properties =
            PUSH_ARRAY(temp.arena, VkQueueFamilyProperties, queue_count);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queue_count,
                                                 queue_properties);

        int graphics_idx = -1;
        int present_idx = -1;
        b8 shared_present_graphics_queues = false;
        for (u32 q = 0; q < queue_count; q++){
            b32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, q, surface, &present_support);
            if ((queue_properties[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                present_support) {
                shared_present_graphics_queues = true;
                graphics_idx = q;
                present_idx = q;
                break;
            }
        }
        if (!shared_present_graphics_queues){
            for (u32 q = 0; q < queue_count; q++){
                if (queue_properties[q].queueFlags & VK_QUEUE_GRAPHICS_BIT){
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
        if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            score += 2000;
        // if(device_properties.deviceType ==
        // VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 100;
        if (device_features.geometryShader)
            score += 10;
        DOT_PRINT("graphics family: %d; present family: %i; score: %i",
                  graphics_idx, present_idx, score);
        if (score > best_device.score){
            best_device.gpu = dev;
            best_device.graphics_family = cast(u16) graphics_idx;
            best_device.present_family = cast(u16) present_idx;
            best_device.score = score;
            best_device.shared_present_graphics_queues =
                shared_present_graphics_queues;
        }
    }

    if (best_device.score < 0){
        DOT_ERROR("No suitable GPU found");
    }

    DOT_PRINT("best_device  graphics family: %d; present family: %i; score: %i",
              best_device.graphics_family, best_device.present_family,
              best_device.score);
    temp_arena_restore(&temp);
    return best_device;
}

internal VkImageSubresourceRange
vk_image_subresource_range(VkImageAspectFlags aspect_mask)
{
    VkImageSubresourceRange subImage = {
        .aspectMask = aspect_mask,
        .baseMipLevel = 0,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };
    return subImage;
}

// NOTE: Should make this a macro with default params?
internal void
vk_transition_image(
    VkCommandBuffer cmd,
    VkImage image,
    VkImageLayout current_layout,
    VkImageLayout new_layout)
{
    VkImageAspectFlags aspect_mask = (new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
    	? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    VkDependencyInfo dep_info = {
        .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &(VkImageMemoryBarrier2){
            .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .srcAccessMask    = VK_ACCESS_2_MEMORY_WRITE_BIT,
            .dstStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .dstAccessMask    = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
            .oldLayout        = current_layout,
            .newLayout        = new_layout,
            .image            = image,
            .subresourceRange = vk_image_subresource_range(aspect_mask),
        } ,
    };

    vkCmdPipelineBarrier2(cmd, &dep_info);
}


internal VkSemaphoreSubmitInfo
vk_semaphore_submit_info(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore)
{
	VkSemaphoreSubmitInfo submitInfo = {
	    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
	    .semaphore = semaphore,
	    .stageMask = stageMask,
	    .deviceIndex = 0,
	    .value = 1
    };

	return submitInfo;
}

internal VkCommandBufferSubmitInfo
vk_command_buffer_submit_info(VkCommandBuffer cmd)
{
	VkCommandBufferSubmitInfo info = {
	    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
	    .pNext = NULL,
	    .commandBuffer = cmd,
	    .deviceMask = 0,
    };
	return info;
}

internal VkSubmitInfo2
vk_submit_info(
    VkCommandBufferSubmitInfo* cmd,
    VkSemaphoreSubmitInfo* signal_semaphore_info,
    VkSemaphoreSubmitInfo* wait_semaphore_info)
{
    VkSubmitInfo2 info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = NULL,

        .waitSemaphoreInfoCount = wait_semaphore_info == NULL ? 0 : 1,
        .pWaitSemaphoreInfos = wait_semaphore_info,

        .signalSemaphoreInfoCount = signal_semaphore_info == NULL ? 0 : 1,
        .pSignalSemaphoreInfos = signal_semaphore_info,

        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = cmd,
    };

    return info;
}

internal void*
vk_alloc(void* data, usize size, usize alignment, VkSystemAllocationScope scope)
{
    UNUSED(data); UNUSED(size); UNUSED(alignment); UNUSED(scope);
    DOT_PRINT("VK Alloc: size=%M; scope=\n", size, string_VkSystemAllocationScope(scope));

    Arena* arena = cast(Arena*)data;
    void* mem = arena_push(arena, size, alignment, true, __FILE__, __LINE__);
    arena_print_debug(arena);
    return mem;
}

internal void*
vk_realloc(void* data, void* old_mem, usize size, usize alignment, VkSystemAllocationScope scope)
{
    UNUSED(data); UNUSED(size); UNUSED(alignment); UNUSED(scope); UNUSED(old_mem);
    DOT_PRINT("VK Realloc: size=%M; scope=%s", size, string_VkSystemAllocationScope(scope) );
    Arena* arena = cast(Arena*)data;
    void* mem = arena_push(arena, size, alignment, true, __FILE__, __LINE__);
    arena_print_debug(arena);
    return mem;
}

internal void
vk_free(void* data, void* mem)
{
    UNUSED(data); UNUSED(mem);
    Arena* arena = cast(Arena*)data;
    DOT_PRINT("VK free");
    arena_print_debug(arena);
}

internal void
vk_internal_alloc(void* data, usize size, VkInternalAllocationType alloc_type, VkSystemAllocationScope scope)
{
    UNUSED(data); UNUSED(size); UNUSED(scope); UNUSED(alloc_type);
    DOT_PRINT("VK Internal Alloc: size=%M; scope=%s", size, string_VkSystemAllocationScope(scope));
    // Arena* arena = cast(Arena*)data;
    // arena_print_debug(arena);
}

internal void
vk_internal_free(void* data, usize size, VkInternalAllocationType alloc_type, VkSystemAllocationScope scope)
{
    UNUSED(data); UNUSED(size); UNUSED(scope); UNUSED(alloc_type);
    DOT_PRINT( "VK Internal Free: size=%M; scope=%s", size, string_VkSystemAllocationScope(scope) );
}

#ifdef VK_USE_CUSTOM_ALLOCATOR
#define VkAllocatorParams(arena) (VkAllocationCallbacks){ \
    .pUserData = arena, \
    .pfnAllocation = vk_alloc, \
    .pfnReallocation = vk_realloc, \
    .pfnFree = vk_free, \
    .pfnInternalAllocation = vk_internal_alloc, \
    .pfnInternalFree = vk_internal_free, \
}
#else
#define VkAllocatorParams(arena) NULL
#endif // !VK_USE_CUSTOM_ALLOCATOR

#endif // !VK_HELPER_H
