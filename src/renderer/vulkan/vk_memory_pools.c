CONST_INT_BLOCK {
    VK_MEMORY_POOLS_GPU_ONLY_SIZE = MB(512),
    VK_MEMORY_POOLS_STAGING_SIZE  = MB(64),
    VK_MEMORY_POOLS_READBACK_SIZE = MB(32),
};

internal VkMemory_Alloc
vk_memory_pools_bump(VkMemory_Pools *pools, VkMemoryRequirements reqs, VkMemory_PoolsKind memory_kind)
{
    VkMemory_Alloc memory_alloc = {0};
    switch (memory_kind) {
        case VkMemory_PoolsKind_GpuOnly:
            if (reqs.memoryTypeBits & (1u << pools->gpu_only_type)){
                u64 aligned_memory = ALIGN_POW2(pools->gpu_only_used, reqs.alignment);
                memory_alloc.offset = aligned_memory;
                memory_alloc.memory = pools->gpu_only_mem;
                pools->gpu_only_used += aligned_memory + reqs.size;
                DOT_PRINT("USEEEEEED %M", pools->gpu_only_used);
                DOT_PRINT("have %M", pools->gpu_only_size);
                if(pools->gpu_only_used > pools->gpu_only_size){
                    DOT_ERROR("Out of GPU only memory!");
                }
            }
        break;
        case VkMemory_PoolsKind_Staging:
            if (reqs.memoryTypeBits & (1u << pools->staging_type)){
                u64 aligned_memory = ALIGN_POW2(pools->staging_used, reqs.alignment);
                pools->staging_used += aligned_memory;
                memory_alloc.offset = aligned_memory;
                memory_alloc.memory = pools->staging_mem;
                if(pools->staging_used > pools->staging_size){
                    DOT_ERROR("Out of Staging memory!");
                }
            }
        break;
        case VkMemory_PoolsKind_Readback:
            if (reqs.memoryTypeBits & (1u << pools->readback_type)){
                u64 aligned_memory = ALIGN_POW2(pools->readback_used, reqs.alignment);
                pools->readback_used += aligned_memory;
                memory_alloc.offset = aligned_memory;
                memory_alloc.memory = pools->readback_mem;
                if(pools->readback_used > pools->readback_size){
                    DOT_ERROR("Out of Readback memory!");
                }
            }
        break;
        case VkMemory_PoolsKind_None:
            DOT_WARNING("Invalid memory type, returning 0");
        default:
        break;
    }
    return memory_alloc;
}

internal u32
vk_memory_pools_find_memory_type(
    const VkPhysicalDeviceMemoryProperties *memory_properties,
    u32 type_bits,
    VkMemoryPropertyFlags required,
    VkMemoryPropertyFlags preferred,
    VkMemoryPropertyFlags forbidden)
{
    i64 best = -1;
    for(u32 i = 0; i < memory_properties->memoryTypeCount; ++i){
        if (!(type_bits & (1u << i)))
            continue;

        VkMemoryPropertyFlags flags = memory_properties->memoryTypes[i].propertyFlags;

        if((flags & forbidden) != 0)
            continue;

        if((flags & required) != required)
            continue;

        // Prefer types that include all preferred flags
        if((flags & preferred) == preferred){
            return i;
        }

        // Fallback: first that matches required
        if(best < 0)
            best = cast(i64)i;
    }

    if(best < 0){
        return U32_MAX;
    }
    return cast(u32)best;
}

// NOTE: We are for now requesting specifically VRAM memory, meaning this will fail on integrated GPU's
// we could eventually store the kind of GPU we selected and change the bit type configuration
internal VkResult
vk_memory_pools_create(
    struct RBVK_Device *device,
    VkMemory_Pools *pools)
{
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(device->gpu, &memory_properties);

    pools->gpu_only_size = VK_MEMORY_POOLS_GPU_ONLY_SIZE;
    pools->staging_size  = VK_MEMORY_POOLS_STAGING_SIZE;
    pools->readback_size = VK_MEMORY_POOLS_READBACK_SIZE;

    // --- 1) Choose memory types ---
    // GPU-only: DEVICE_LOCAL, not necessarily host visible
    {
        u32 type_bits = U32_MAX;
        pools->gpu_only_type = vk_memory_pools_find_memory_type(
            &memory_properties,
            type_bits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            (device->is_integrated_gpu ? 0 : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        );
        DOT_ASSERT(pools->gpu_only_type != U32_MAX, "Could not find gpu memory type");
    }

    // Upload: HOST_VISIBLE + HOST_COHERENT, prefer non-DEVICE_LOCAL on dGPU
    {
        u32 type_bits = U32_MAX;
        pools->staging_type = vk_memory_pools_find_memory_type(
            &memory_properties,
            type_bits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            0,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        DOT_ASSERT(pools->staging_type != U32_MAX, "Could not find staging memory type");
    }

    // Readback: HOST_VISIBLE + HOST_COHERENT + HOST_CACHED if possible
    {
        u32 type_bits = U32_MAX;
        pools->readback_type = vk_memory_pools_find_memory_type(
            &memory_properties,
            type_bits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
            0
        );
        DOT_ASSERT(pools->readback_type != U32_MAX, "Could not find readback memory type");
    }

    // --- 2) Allocate big blocks ---
    // GPU-only: we just allocate raw memory block (you’ll suballocate buffers/images later)
    {
        VkMemoryAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize  = pools->gpu_only_size,
            .memoryTypeIndex = pools->gpu_only_type,
        };

        VK_CHECK(vkAllocateMemory(device->device, &alloc_info, NULL, &pools->gpu_only_mem));
    }

    // Upload: create one big buffer and back it with upload memory
    {
        // Create buffer with dummy memory type first to get requirements
        VkBufferCreateInfo buff_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size  = pools->staging_size,
            .usage = (VkBufferUsageFlags)
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };

        VkBuffer temp_buff;
        VK_CHECK(vkCreateBuffer(device->device, &buff_info, NULL, &temp_buff));

        VkMemoryRequirements memory_requirements;
        vkGetBufferMemoryRequirements(device->device, temp_buff, &memory_requirements);
        vkDestroyBuffer(device->device, temp_buff, NULL);
        pools->staging_size = memory_requirements.size;

        VkMemoryAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize  = memory_requirements.size,
            .memoryTypeIndex = pools->staging_type,
        };

        VK_CHECK(vkAllocateMemory(device->device, &alloc_info, NULL, &pools->staging_mem));
        buff_info.size = memory_requirements.size;
        VK_CHECK(vkCreateBuffer(device->device, &buff_info, NULL, &pools->staging_buffer));
        VK_CHECK(vkBindBufferMemory(device->device, pools->staging_buffer, pools->staging_mem, 0));
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

        VK_CHECK(vkCreateBuffer(device->device, &buff_info, NULL, &temp_buff));

        VkMemoryRequirements memory_requirements;
        vkGetBufferMemoryRequirements(device->device, temp_buff, &memory_requirements);
        vkDestroyBuffer(device->device, temp_buff, NULL);

        pools->readback_size = memory_requirements.size;

        VkMemoryAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize  = memory_requirements.size,
            .memoryTypeIndex = pools->readback_type,
        };

        VK_CHECK(vkAllocateMemory(device->device, &alloc_info, NULL, &pools->readback_mem));
        buff_info.size = memory_requirements.size;
        VK_CHECK(vkCreateBuffer(device->device, &buff_info, NULL, &pools->readback_buffer));
        VK_CHECK(vkBindBufferMemory(device->device, pools->readback_buffer, pools->readback_mem, 0));
    }

    return VK_SUCCESS;
}

internal void
vk_memory_pools_destroy(struct RBVK_Device *device, VkMemory_Pools *pools)
{
    if(pools->staging_buffer)
        vkDestroyBuffer(device->device, pools->staging_buffer, NULL);
    if(pools->readback_buffer)
        vkDestroyBuffer(device->device, pools->readback_buffer, NULL);

    if(pools->gpu_only_mem)
        vkFreeMemory(device->device, pools->gpu_only_mem, NULL);
    if(pools->staging_mem)
        vkFreeMemory(device->device, pools->staging_mem, NULL);
    if(pools->readback_mem)
        vkFreeMemory(device->device, pools->readback_mem, NULL);
}
