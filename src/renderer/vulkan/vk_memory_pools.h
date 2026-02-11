#ifndef VK_MEMORY_POOLS_H
#define VK_MEMORY_POOLS_H

// Minimal Vulkan C code to create big memory pools.
// Assumes you already have: VkPhysicalDevice gpu, VkDevice device.
typedef struct VkMemory_Pools{
    u32 gpu_only_type;
    VkDeviceMemory gpu_only_mem;
    VkDeviceSize   gpu_only_size;

    u32 upload_type;
    VkDeviceMemory upload_mem;
    VkDeviceSize   upload_size;
    VkBuffer       upload_buffer;   // single big buffer

    u32 readback_type;
    VkDeviceMemory readback_mem;
    VkDeviceSize   readback_size;
    VkBuffer       readback_buffer; // single big buffer
}VkMemory_Pools;

// Helper: find memory type index with required flags (and optional forbidden flags)
internal u32 
vk_memory_pools_find_memory_type(
    const VkPhysicalDeviceMemoryProperties *memory_properties,
    u32 typeBits,
    VkMemoryPropertyFlags required,
    VkMemoryPropertyFlags preferred,
    VkMemoryPropertyFlags forbidden)
{
    i32 best = -1;

    for (u32 i = 0; i < memory_properties->memoryTypeCount; ++i){
        if (!(typeBits & (1u << i)))
            continue;

        VkMemoryPropertyFlags flags = memory_properties->memoryTypes[i].propertyFlags;

        if ((flags & forbidden) != 0)
            continue;

        if ((flags & required) != required)
            continue;

        // Prefer types that include all preferred flags
        if ((flags & preferred) == preferred) {
            return i;
        }

        // Fallback: first that matches required
        if (best < 0)
            best = (i32)i;
    }

    if (best < 0) {
        // In production, handle this properly
        // Here we just return 0 to avoid UB
        return 0;
    }
    return (u32)best;
}

// Main: create big pools
internal VkResult
vk_memory_pools_create(
    VkPhysicalDevice gpu,
    VkDevice device,
    VkMemory_Pools *pools)
{
    memset(pools, 0, sizeof(*pools));

    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(gpu, &memory_properties);

    // --- 1) Choose memory types ---

    // GPU-only: DEVICE_LOCAL, not necessarily host visible
    {
        u32 typeBits = 0xFFFFFFFFu; // we’ll refine per resource later
        pools->gpu_only_type = vk_memory_pools_find_memory_type(
            &memory_properties,
            typeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0
        );
    }

    // Upload: HOST_VISIBLE + HOST_COHERENT, prefer non-DEVICE_LOCAL on dGPU
    {
        u32 typeBits = 0xFFFFFFFFu;
        pools->upload_type = vk_memory_pools_find_memory_type(
            &memory_properties,
            typeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            0,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT  // We should only do this for discrete GPUs
        );
    }

    // Readback: HOST_VISIBLE + HOST_COHERENT + HOST_CACHED if possible
    {
        u32 typeBits = 0xFFFFFFFFu;
        pools->readback_type = vk_memory_pools_find_memory_type(
            &memory_properties,
            typeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
            0
        );
    }

    // --- 2) Allocate big blocks ---

    // You can tune these sizes later
    pools->gpu_only_size = MB(512);
    pools->upload_size   = MB(64);
    pools->readback_size = MB(32);

    // GPU-only: we just allocate raw memory block (you’ll suballocate buffers/images later)
    {
        VkMemoryAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize  = pools->gpu_only_size,
            .memoryTypeIndex = pools->gpu_only_type,
        };

        VK_CHECK(vkAllocateMemory(device, &alloc_info, NULL, &pools->gpu_only_mem));
    }

    // Upload: create one big buffer and back it with upload memory
    {
        // Create buffer with dummy memory type first to get requirements
        VkBufferCreateInfo buff_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size  = pools->upload_size,
            .usage = (VkBufferUsageFlags)
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };

        VkBuffer temp_buff;
        VK_CHECK(vkCreateBuffer(device, &buff_info, NULL, &temp_buff));

        VkMemoryRequirements memory_requirements;
        vkGetBufferMemoryRequirements(device, temp_buff, &memory_requirements);
        vkDestroyBuffer(device, temp_buff, NULL);

        // Adjust size to requirement
        pools->upload_size = memory_requirements.size;

        VkMemoryAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize  = memory_requirements.size,
            .memoryTypeIndex = pools->upload_type,
        };

        VK_CHECK(vkAllocateMemory(device, &alloc_info, NULL, &pools->upload_mem));
        buff_info.size = memory_requirements.size;
        VK_CHECK(vkCreateBuffer(device, &buff_info, NULL, &pools->upload_buffer));
        VK_CHECK(vkBindBufferMemory(device, pools->upload_buffer, pools->upload_mem, 0));
    }

    // Readback: one big buffer backed by readback memory
    {
        VkBuffer temp_buff;
        VkBufferCreateInfo buff_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size  = pools->readback_size,
            .usage = (VkBufferUsageFlags)
                VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };

        VK_CHECK(vkCreateBuffer(device, &buff_info, NULL, &temp_buff));

        VkMemoryRequirements memory_requirements;
        vkGetBufferMemoryRequirements(device, temp_buff, &memory_requirements);
        vkDestroyBuffer(device, temp_buff, NULL);

        pools->readback_size = memory_requirements.size;

        VkMemoryAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize  = memory_requirements.size,
            .memoryTypeIndex = pools->readback_type,
        };

        VK_CHECK(vkAllocateMemory(device, &alloc_info, NULL, &pools->readback_mem));
        buff_info.size = memory_requirements.size;
        VK_CHECK(vkCreateBuffer(device, &buff_info, NULL, &pools->readback_buffer));
        VK_CHECK(vkBindBufferMemory(device, pools->readback_buffer, pools->readback_mem, 0));
    }

    return VK_SUCCESS;
}

internal void
vk_memory_pools_destoy(VkDevice device, VkMemory_Pools *pools)
{
    if (pools->upload_buffer)
        vkDestroyBuffer(device, pools->upload_buffer, NULL);
    if (pools->readback_buffer)
        vkDestroyBuffer(device, pools->readback_buffer, NULL);

    if (pools->gpu_only_mem)
        vkFreeMemory(device, pools->gpu_only_mem, NULL);
    if (pools->upload_mem)
        vkFreeMemory(device, pools->upload_mem, NULL);
    if (pools->readback_mem)
        vkFreeMemory(device, pools->readback_mem, NULL);

    memset(pools, 0, sizeof(*pools));
}

#endif // !VK_MEMORY_POOLS_H
