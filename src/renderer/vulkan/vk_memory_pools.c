DOT_CONST_INT_BLOCK {
    VK_MEMORY_POOLS_GPU_ONLY_SIZE = MB(512),
    VK_MEMORY_POOLS_STAGING_SIZE  = MB(64),
    VK_MEMORY_POOLS_READBACK_SIZE = MB(32),
};


// internal VkBuffer
// RBVKMemory_Pools(RBVKMemory_Pools *pools, VkMemory_PoolsKind buffer_kind)
// {
//     switch(buffer_kind){
//         case VkMemory_PoolsKind_Staging: return pools->staging_buffer.vk_buffer;
//         case VkMemory_PoolsKind_Readback: return pools->readback_buffer.vk_buffer;
//         default: DOT_ERROR("Invalid buffer kind");
//     }
// }

internal VkMemory_Alloc
rbvk_memory_ring_buffer_push(RBVKMemory_RingBuffer *ring_buffer, u64 size)
{
    u64 pop_marker = ring_buffer->head;
    u64 offset = ring_buffer->head;
    if(ring_buffer->head + size > ring_buffer->size){
        offset = 0;
        ring_buffer->head = 0;
        if(size > ring_buffer->tail){
            DOT_ERROR("Out of Staging memory, consider increasing it!");
        }
    }
    if (offset < ring_buffer->tail && offset + size > ring_buffer->tail) {
        DOT_ERROR("Out of Staging memory, consider increasing it!");
    }
 
    VkMemory_Alloc alloc = {
        .vk_buffer = ring_buffer->vk_buffer,
        .pool_kind = VkMemory_PoolsKind_Staging,
        .vk_memory = ring_buffer->vk_memory,
        .size = size,
        .offset = ring_buffer->head,
        .pop_marker = pop_marker,
    };
    ring_buffer->head = offset + size;
    return(alloc);
}

internal void
rbvk_memory_ring_buffer_pop(RBVKMemory_RingBuffer *ring_buffer, const VkMemory_Alloc *alloc)
{
    DOT_ASSERT(ring_buffer->tail == alloc->pop_marker, "Incorrect ring buffer pop order");
    ring_buffer->tail = alloc->offset + alloc->size;
}

internal void
rbvk_memory_pools_staging_ring_buffer_pop(RBVKMemory_Pools *pools, const VkMemory_Alloc *alloc)
{
    rbvk_memory_ring_buffer_pop(&pools->staging_buffer, alloc);
}

internal VkMemory_Alloc
rbvk_memory_pools_staging_ring_buffer_push(RBVKMemory_Pools *pools, u64 size, void *data)
{
    RBVKMemory_RingBuffer *staging_buffer = &pools->staging_buffer;
    VkMemory_Alloc alloc = rbvk_memory_ring_buffer_push(staging_buffer, size);
    void *destination_data;
    if(data){
        VK_CHECK(vkMapMemory(pools->vk_device, alloc.vk_memory, alloc.offset, alloc.size, 0, &destination_data));
        MEMORY_COPY_NO_ALIAS(destination_data, data, alloc.size);
        vkUnmapMemory(pools->vk_device, alloc.vk_memory);
    }
    return(alloc);
}

internal VkMemory_Alloc
rbvk_memory_gpu_buffer_alloc(RBVKMemory_Pools *pools, VkBuffer vk_buffer)
{
    VkMemoryRequirements2 reqs2 = vk_buffer_memory_requirements(pools->vk_device, vk_buffer);
    VkMemoryRequirements reqs = reqs2.memoryRequirements;
    VkMemory_Alloc alloc = { .size = reqs.size, .pool_kind = VkMemory_PoolsKind_GpuOnly };
    if((reqs.memoryTypeBits & (1u << pools->gpu_pool.vk_memory_type_idx)) != 0){
        u64 aligned_memory = ALIGN_POW2(pools->gpu_pool.used, reqs.alignment);
        alloc.offset = aligned_memory;
        alloc.vk_memory = pools->gpu_pool.vk_memory;
        pools->gpu_pool.used += aligned_memory + reqs.size;
        if(pools->gpu_pool.used > pools->gpu_pool.size){
            DOT_ERROR("Out of GPU only memory, consider increasing it!");
        }
    }else{
        DOT_ERROR("Invalid buffer memory type");
    }
    return(alloc);
}

internal VkMemory_Alloc
rbvk_memory_gpu_image_alloc(RBVKMemory_Pools *pools, VkImage vk_image)
{
    VkMemoryRequirements2 reqs2 = vk_image_memory_requirements(pools->vk_device, vk_image);
    VkMemoryRequirements reqs = reqs2.memoryRequirements;
    VkMemory_Alloc alloc = { .size = reqs.size, .pool_kind = VkMemory_PoolsKind_GpuOnly };
    if(reqs.memoryTypeBits & (1u << pools->gpu_pool.vk_memory_type_idx)){
        u64 aligned_memory = ALIGN_POW2(pools->gpu_pool.used, reqs.alignment);
        alloc.offset = aligned_memory;
        alloc.vk_memory = pools->gpu_pool.vk_memory;
        pools->gpu_pool.used += aligned_memory + reqs.size;
        if(pools->gpu_pool.used > pools->gpu_pool.size){
            DOT_ERROR("Out of GPU only memory, consider increasing it!");
        }
    }else{
        DOT_ERROR("Invalid image memory type");
    }
    VK_CHECK(vkBindImageMemory(pools->vk_device, vk_image, alloc.vk_memory, alloc.offset));
    return(alloc);
}

// internal VkMemory_Alloc
// vk_memory_pools_bump(RBVKMemory_Pools *pools, VkMemoryRequirements reqs, VkMemory_PoolsKind memory_kind)
// {
//     VkMemory_Alloc alloc = { .size = reqs.size, .pool_kind = memory_kind };
//     switch(memory_kind){
//         case VkMemory_PoolsKind_GpuOnly:
//             if(reqs.memoryTypeBits & (1u << pools->gpu_only_type)){
//                 u64 aligned_memory = ALIGN_POW2(pools->gpu_only_used, reqs.alignment);
//                 alloc.offset = aligned_memory;
//                 alloc.vk_memory = pools->gpu_only_mem;
//                 pools->gpu_only_used += aligned_memory + reqs.size;
//                 if(pools->gpu_only_used > pools->gpu_only_size){
//                     DOT_ERROR("Out of GPU only memory, consider increasing it!");
//                 }
//             }
//         // break;
//         // case VkMemory_PoolsKind_Staging:{
//         //     RBVKMemory_RingBuffer *staging_buffer = &pools->staging_buffer;
//         //     // if(reqs.memoryTypeBits & DOT_BIT(pools->staging_type)){
//         //         u64 aligned_memory = ALIGN_POW2(staging_buffer->head, reqs.alignment);
//         //         staging_buffer->head += aligned_memory;
//         //         alloc.offset = aligned_memory;
//         //         alloc.vk_memory = staging_buffer->vk_memory;
//         //
//         //         if(staging_buffer->head > staging_buffer->size){
//         //             staging_buffer->head = 0;
//         //             aligned_memory = ALIGN_POW2(staging_buffer->head, reqs.alignment);
//         //             staging_buffer->head += aligned_memory;
//         //             alloc.offset = aligned_memory;
//         //             alloc.vk_memory = staging_buffer->vk_memory;
//         //             if(staging_buffer->head > staging_buffer->tail){
//         //                 DOT_ERROR("Out of Staging memory, consider increasing it!");
//         //             }
//         //         }
//         //         // alloc.vk_buffer = staging_buffer->vk_buffer;
//         //     }
//         // break;
//         // case VkMemory_PoolsKind_Readback:{
//         //     // if(reqs.memoryTypeBits & (1u << pools->readback_type)){
//         //         u64 aligned_memory = ALIGN_POW2(pools->readback_used, reqs.alignment);
//         //         pools->readback_used += aligned_memory;
//         //         alloc.offset = aligned_memory;
//         //         alloc.vk_memory = pools->readback_mem;
//         //         if(pools->readback_used > pools->readback_size){
//         //             DOT_ERROR("Out of Readback memory, consider increasing it!");
//         //         }
//         //         // alloc.vk_buffer = pools->readback_buffer;
//         //     }
//         // break;
//         // case VkMemory_PoolsKind_None:
//         default:
//             alloc.size = 0;
//             DOT_WARNING("Invalid memory type %u, returning 0", memory_kind);
//         break;
//     }
//     return(alloc);
// }

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

        if(DOT_BITS_ANY(flags, forbidden))
            continue;

        if(!DOT_BITS_MATCH(flags, required))
            continue;

        // Prefer types that include all preferred flags
        if(DOT_BITS_MATCH(flags, preferred)){
            return cast(u32)i;
        }

        // Fallback: first that matches required
        if(best < 0)
            best = cast(u32)i;
    }

    if(best < 0){
        return U32_MAX;
    }
    return(cast(u32)best);
}

// split this into create pool and create ring buffer
internal RBVKMemory_Pools
vk_memory_pools_create(RBVK_Device *device)
{
    RBVKMemory_Pools pools = {.vk_device =  device->vk_device};
    //     .vk_device = device->vk_device,
    //     .gpu_only_size = VK_MEMORY_POOLS_GPU_ONLY_SIZE,
    //     .staging_size  = VK_MEMORY_POOLS_STAGING_SIZE,
    //     .readback_size = VK_MEMORY_POOLS_READBACK_SIZE,
    // };
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(device->vk_gpu, &memory_properties);

    {
        RBVKMemory_Pool gpu_pool = {
            .vk_memory_type_idx = vk_memory_pools_find_memory_type(
                &memory_properties,
                U32_MAX,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                (device->is_integrated_gpu ? 0 : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)),
            .size = VK_MEMORY_POOLS_GPU_ONLY_SIZE,
        };
        DOT_ASSERT(gpu_pool.vk_memory_type_idx != U32_MAX, "Could not find gpu memory type");

        VkMemoryAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize  = gpu_pool.size,
            .memoryTypeIndex = gpu_pool.vk_memory_type_idx,
        };
        VK_CHECK(vkAllocateMemory(device->vk_device, &alloc_info, NULL, &gpu_pool.vk_memory));
        pools.gpu_pool = gpu_pool;
    }

    // Upload: HOST_VISIBLE + HOST_COHERENT, prefer non-DEVICE_LOCAL on dGPU
    {
        RBVKMemory_RingBuffer staging_buffer = {
            .vk_memory_type_idx = vk_memory_pools_find_memory_type(
                &memory_properties,
                U32_MAX,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                0,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
            .size = VK_MEMORY_POOLS_STAGING_SIZE,

        };
        DOT_ASSERT(staging_buffer.vk_memory_type_idx != U32_MAX, "Could not find staging memory type");
        VkBufferCreateInfo buff_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size  = staging_buffer.size,
            .usage = (VkBufferUsageFlags)
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .size = staging_buffer.size,
        };

        VkMemoryRequirements memory_requirements; {
            VkBuffer temp_buff;
            VK_CHECK(vkCreateBuffer(device->vk_device, &buff_info, NULL, &temp_buff));
            vkGetBufferMemoryRequirements(device->vk_device, temp_buff, &memory_requirements);
            vkDestroyBuffer(device->vk_device, temp_buff, NULL);
        }
        VK_CHECK(vkAllocateMemory(
            device->vk_device,
            &(VkMemoryAllocateInfo){
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize  = memory_requirements.size,
                .memoryTypeIndex = staging_buffer.vk_memory_type_idx,
            },
            NULL,
            &staging_buffer.vk_memory));
        VK_CHECK(vkCreateBuffer(device->vk_device, &buff_info, NULL, &staging_buffer.vk_buffer));
        VK_CHECK(vkBindBufferMemory(device->vk_device, staging_buffer.vk_buffer, staging_buffer.vk_memory, 0));
        pools.staging_buffer = staging_buffer;
    }

    // Readback: HOST_VISIBLE + HOST_COHERENT + HOST_CACHED if possible
    {
        RBVKMemory_RingBuffer readback_buffer = {
            .vk_memory_type_idx = vk_memory_pools_find_memory_type(
                &memory_properties,
                U32_MAX,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                0),
            .size = VK_MEMORY_POOLS_READBACK_SIZE,

        };

        DOT_ASSERT(readback_buffer.vk_memory_type_idx != U32_MAX, "Could not find staging memory type");
        VkBufferCreateInfo buff_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size  = readback_buffer.size,
            .usage = (VkBufferUsageFlags)
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .size = readback_buffer.size,
        };

        VkMemoryRequirements memory_requirements; {
            VkBuffer temp_buff;
            VK_CHECK(vkCreateBuffer(device->vk_device, &buff_info, NULL, &temp_buff));
            vkGetBufferMemoryRequirements(device->vk_device, temp_buff, &memory_requirements);
            vkDestroyBuffer(device->vk_device, temp_buff, NULL);
        }
        VK_CHECK(vkAllocateMemory(
            device->vk_device,
            &(VkMemoryAllocateInfo){
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize  = memory_requirements.size,
                .memoryTypeIndex = readback_buffer.vk_memory_type_idx,
            },
            NULL,
            &readback_buffer.vk_memory));
        VK_CHECK(vkCreateBuffer(device->vk_device, &buff_info, NULL, &readback_buffer.vk_buffer));
        VK_CHECK(vkBindBufferMemory(device->vk_device, readback_buffer.vk_buffer, readback_buffer.vk_memory, 0));
        pools.readback_buffer = readback_buffer;
    }
    return(pools);
}

internal void
vk_memory_pools_destroy(RBVK_Device *device, RBVKMemory_Pools *pools)
{
    if(pools->staging_buffer.vk_buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(device->vk_device, pools->staging_buffer.vk_buffer, NULL);
    if(pools->readback_buffer.vk_buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(device->vk_device, pools->readback_buffer.vk_buffer, NULL);

    if(pools->gpu_pool.vk_memory != VK_NULL_HANDLE)
        vkFreeMemory(device->vk_device, pools->gpu_pool.vk_memory, NULL);
    if(pools->staging_buffer.vk_memory)
        vkFreeMemory(device->vk_device, pools->staging_buffer.vk_memory, NULL);
    if(pools->readback_buffer.vk_memory != VK_NULL_HANDLE)
        vkFreeMemory(device->vk_device, pools->readback_buffer.vk_memory, NULL);
}
