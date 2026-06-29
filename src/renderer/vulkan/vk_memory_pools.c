internal void
rn_vk_memory_ring_buffer_pop(RN_VK_Memory_RingBuffer *ring_buffer, RN_VK_GpuAllocHandle h)
{
    RN_VK_GpuAlloc *alloc = cast(RN_VK_GpuAlloc*)h;
    DOT_ASSERT(ring_buffer->tail == alloc->offset - alloc->pad, "Incorrect ring buffer pop order %u %u %u", ring_buffer->tail, alloc->offset, alloc->pad);
    // ring_buffer->tail = (alloc->offset - alloc->pad) + alloc->size;
    ring_buffer->tail = alloc->offset + alloc->size;
    rn_vk_gpu_alloc_free(&ring_buffer->next_free, alloc);
}

internal void
rn_vk_memory_pools_staging_ring_buffer_pop(RN_VK_Memory *pools, RN_VK_StagingAlloc *staging_alloc)
{
    RN_VK_Memory_RingBuffer *staging_buffer = rn_vk_memory_ring_buffer_get(pools, RN_VK_GpuAllocKind_Staging);

    rn_vk_memory_ring_buffer_pop(staging_buffer, staging_alloc->handle);
}

internal RN_VK_GpuAlloc *
rn_vk_memory_ring_buffer_push_(Arena *arena, RN_VK_Memory_RingBuffer *ring_buffer, u64 size)
{
    // u64 pop_marker = ring_buffer->head;
    u64 aligned_memory = ALIGN_POW2(ring_buffer->head, cast(u64)16);
    u32 pad = cast(u32) (aligned_memory - ring_buffer->head);
    if(ring_buffer->head + size > ring_buffer->backing.size){
        aligned_memory = 0;
        ring_buffer->head = 0;
        if(size > ring_buffer->tail){
            DOT_ERROR("Out of Staging memory, consider increasing it!");
        }
    }
    if(aligned_memory < ring_buffer->tail && aligned_memory + size > ring_buffer->tail){
        DOT_ERROR("Out of Staging memory, consider increasing it!");
    }
 
    RN_VK_GpuAlloc *alloc = rn_vk_gpu_alloc_alloc(arena, &ring_buffer->next_free);
    alloc->vk_buffer = ring_buffer->backing.vk_buffer;
    alloc->pool_kind = RN_VK_GpuAllocKind_Staging;
    alloc->size = size;
    alloc->offset = aligned_memory;
    alloc->pad = pad;

    ring_buffer->head = aligned_memory + size;
    return(alloc);
}

internal RN_VK_Memory_Pool *
rn_vk_memory_pool_get(RN_VK_Memory *pools, RN_VK_GpuAllocKind kind)
{
    RN_VK_Memory_GpuBuffer *buffer = pools->memory_kinds[kind];
    DOT_ASSERT(buffer->pool_kind == kind);
    DOT_ASSERT(buffer->alloc_strategy_kind == rn_vk_g_memory_pools_alloc_strategy[kind]);
    RN_VK_Memory_Pool *pool = DOT_CONTAINER_OF(buffer, RN_VK_Memory_Pool, backing);
    return pool;
}

internal RN_VK_Memory_RingBuffer *
rn_vk_memory_ring_buffer_get(RN_VK_Memory *pools, RN_VK_GpuAllocKind kind)
{
    RN_VK_Memory_GpuBuffer *buffer = pools->memory_kinds[kind];
    DOT_ASSERT(buffer->pool_kind == kind);
    DOT_ASSERT(buffer->alloc_strategy_kind == rn_vk_g_memory_pools_alloc_strategy[kind]);
    RN_VK_Memory_RingBuffer *staging_buffer = DOT_CONTAINER_OF(buffer, RN_VK_Memory_RingBuffer, backing);
    return staging_buffer;
}

internal RN_VK_StagingAlloc
rn_vk_memory_pools_staging_ring_buffer_push(Arena *arena, RN_VK_Memory *pools, u64 size, void *data)
{
    RN_VK_Memory_RingBuffer *staging_buffer = rn_vk_memory_ring_buffer_get(pools, RN_VK_GpuAllocKind_Staging);

    RN_VK_GpuAlloc *alloc = rn_vk_memory_ring_buffer_push_(arena, staging_buffer, size);
    void *destination_data;
    if(data){
        RN_VK_CHECK(vkMapMemory(pools->vk_device, staging_buffer->backing.vk_memory, alloc->offset, alloc->size, 0, &destination_data));
        DOT_PRINT("alloc size %M", alloc->size); 
        MEMORY_COPY_NO_ALIAS(destination_data, data, size);
        vkUnmapMemory(pools->vk_device, staging_buffer->backing.vk_memory);
    }

    RN_VK_StagingAlloc staging = {0};
    staging.handle = cast(RN_VK_GpuAllocHandle) alloc;
    staging.size = alloc->size;
    staging.offset = alloc->offset;
    staging.vk_buffer = alloc->vk_buffer;

    return(staging);
}

internal RN_VK_GpuAllocHandle
rn_vk_memory_gpu_buffer_alloc(Arena *arena, RN_VK_Memory *pools, VkBuffer vk_buffer)
{
    RN_VK_Memory_Pool *gpu_pool = rn_vk_memory_pool_get(pools, RN_VK_GpuAllocKind_GpuOnly);

    VkMemoryRequirements2 reqs2 = vk_buffer_memory_requirements(pools->vk_device, vk_buffer);
    VkMemoryRequirements reqs   = reqs2.memoryRequirements;

    if((reqs.memoryTypeBits & (1u << gpu_pool->backing.vk_memory_type_idx)) == 0){
        DOT_ERROR("Invalid image memory type");
    }

    u64 aligned_memory = ALIGN_POW2(gpu_pool->used, reqs.alignment);
    u64 needed = (aligned_memory - gpu_pool->used) + reqs.size;
    gpu_pool->used += needed;

    if(gpu_pool->used > gpu_pool->backing.size){
        DOT_ERROR("Out of GPU only memory, consider increasing it!. Current %M/%M, needed %M", gpu_pool->used, gpu_pool->backing.size, needed);
    }
    DOT_PRINT("Total memory %M/%M, needed used %M", gpu_pool->used, gpu_pool->backing.size, needed);

    RN_VK_GpuAlloc *alloc = rn_vk_gpu_alloc_alloc(arena, &gpu_pool->next_free);
    alloc->size = reqs.size;
    alloc->pool_kind = RN_VK_GpuAllocKind_GpuOnly;
    alloc->offset = aligned_memory;

    RN_VK_CHECK(vkBindBufferMemory(pools->vk_device, vk_buffer, gpu_pool->backing.vk_memory, alloc->offset));
    return(cast(RN_VK_GpuAllocHandle)alloc);
}

internal RN_VK_GpuAllocHandle
rn_vk_memory_gpu_image_alloc(Arena *arena, RN_VK_Memory *pools, VkImage vk_image)
{
    RN_VK_Memory_Pool *gpu_pool = rn_vk_memory_pool_get(pools, RN_VK_GpuAllocKind_GpuOnly);

    VkMemoryRequirements2 reqs2 = vk_image_memory_requirements(pools->vk_device, vk_image);
    VkMemoryRequirements reqs   = reqs2.memoryRequirements;

    if((reqs.memoryTypeBits & (1u << gpu_pool->backing.vk_memory_type_idx)) == 0){
        DOT_ERROR("Invalid image memory type");
    }
    u64 aligned_memory = ALIGN_POW2(gpu_pool->used, reqs.alignment);
    u64 needed = (aligned_memory - gpu_pool->used) + reqs.size;
    u32 pad = cast(u32)(aligned_memory - gpu_pool->used);
    gpu_pool->used += needed;

    if(gpu_pool->used > gpu_pool->backing.size){
        DOT_ERROR("Out of GPU only memory, consider increasing it!. Current %M/%M, needed %M", gpu_pool->used, gpu_pool->backing.size, needed);
    }
    DOT_PRINT("Total memory %M/%M, needed %M", gpu_pool->used, gpu_pool->backing.size, needed);

    RN_VK_GpuAlloc *alloc = rn_vk_gpu_alloc_alloc(arena, &gpu_pool->next_free);
    alloc->size = reqs.size;
    alloc->pool_kind = RN_VK_GpuAllocKind_GpuOnly;
    alloc->offset = aligned_memory;
    alloc->pad = pad;

    RN_VK_CHECK(vkBindImageMemory(pools->vk_device, vk_image, gpu_pool->backing.vk_memory, alloc->offset));
    return(cast(RN_VK_GpuAllocHandle)alloc);
}

internal u32
rn_vk_memory_pools_find_memory_type(
    const VkPhysicalDeviceMemoryProperties *memory_properties,
    VkMemoryPropertyFlags required,
    VkMemoryPropertyFlags preferred,
    VkMemoryPropertyFlags forbidden)
{
    i64 best = -1;
    for(u32 i = 0; i < memory_properties->memoryTypeCount; ++i){
        VkMemoryPropertyFlags flags = memory_properties->memoryTypes[i].propertyFlags;

        if(DOT_BITS_ANY(flags, forbidden))
            continue;

        if(!DOT_BITS_MATCH(flags, required))
            continue;

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

internal RN_VK_Memory_GpuBuffer *
rn_vk_memory_gpu_alloc(Arena *arena, RN_VK_Device *device, RN_VK_GpuAllocKind kind, RN_VK_GpuAllocStrategyKind alloc_strategy, u64 alloc_size)
{
    VkMemoryPropertyFlags required  = 0;
    VkMemoryPropertyFlags preferred = 0;
    VkMemoryPropertyFlags forbidden = 0;
    VkBufferUsageFlags usage = 0;
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(device->vk_gpu, &memory_properties);

    DOT_ALLOW_PARTIAL_SWITCH;
    switch(kind){
    default: DOT_ERROR("Non existent memory kind");
    case RN_VK_GpuAllocKind_GpuOnly:
        required    = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        forbidden   = (device->is_integrated_gpu ? 0 : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    break;
    case RN_VK_GpuAllocKind_Staging:
        required    = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        forbidden   = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    break;
    case RN_VK_GpuAllocKind_Dyanmic:
        required    = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        preferred   = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        forbidden   = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    break;
    case RN_VK_GpuAllocKind_Readback:
        required    = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        preferred   = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        forbidden   = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    break;
    }
    DOT_RESTORE_PARTIAL_SWITCH;

    RN_VK_Memory_GpuBuffer *alloc_ctx = NULL;
    switch(alloc_strategy){
    default: DOT_ERROR("Innvalid alloc strategy %d", alloc_strategy);
    case RN_VK_GpuAllocStrategyKind_RingBuffer:{
        RN_VK_Memory_RingBuffer *ring_buffer = PUSH_STRUCT(arena, RN_VK_Memory_RingBuffer);
        alloc_ctx = &ring_buffer->backing;
    break;}
    case RN_VK_GpuAllocStrategyKind_Pool:{
        RN_VK_Memory_Pool *pool = PUSH_STRUCT(arena, RN_VK_Memory_Pool);
        alloc_ctx = &pool->backing;
    break;}
    }
    alloc_ctx->vk_memory_type_idx = rn_vk_memory_pools_find_memory_type(&memory_properties, required, preferred, forbidden);
    alloc_ctx->size = alloc_size;

    u64 required_alloc_size = alloc_size;
    if(usage){
        VkMemoryRequirements memory_requirements = {0};

        VkBufferCreateInfo buff_info = {0};
        buff_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buff_info.size  = alloc_size;
        buff_info.usage = usage;
        buff_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer temp_buff;
        RN_VK_CHECK(vkCreateBuffer(device->vk_device, &buff_info, NULL, &temp_buff));
        vkGetBufferMemoryRequirements(device->vk_device, temp_buff, &memory_requirements);
        vkDestroyBuffer(device->vk_device, temp_buff, NULL);
        required_alloc_size = memory_requirements.size;
    }

    VkMemoryAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize  = required_alloc_size;
    alloc_info.memoryTypeIndex = alloc_ctx->vk_memory_type_idx;
    RN_VK_CHECK(vkAllocateMemory(device->vk_device, &alloc_info, NULL, &alloc_ctx->vk_memory));

    if(usage){
        VkBufferCreateInfo buff_info = {0};
        buff_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buff_info.size  = alloc_size;
        buff_info.usage = usage;
        buff_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        RN_VK_CHECK(vkCreateBuffer(device->vk_device, &buff_info, NULL, &alloc_ctx->vk_buffer));
        RN_VK_CHECK(vkBindBufferMemory(device->vk_device, alloc_ctx->vk_buffer, alloc_ctx->vk_memory, 0));
    }
    return(alloc_ctx);
}

internal RN_VK_Memory
rn_vk_memory_pools_create(Arena *arena, RN_VK_Device *device)
{
    RN_VK_Memory pools = {0};
    pools.vk_device =  device->vk_device;
    for EACH_ENUM_VAL(RN_VK_GpuAllocKind, alloc_kind){
        RN_VK_GpuAllocStrategyKind alloc_strategy = rn_vk_g_memory_pools_alloc_strategy[alloc_kind];
        pools.memory_kinds[alloc_kind] = rn_vk_memory_gpu_alloc(arena, device, alloc_kind, alloc_strategy, rn_vk_g_memory_pools_memory_sizes[alloc_kind]);
    }
    // pools.gpu_pool.backing          = rn_vk_memory_gpu_alloc(device, RN_VK_GpuAllocKind_GpuOnly, RN_VK_MEMORY_POOLS_GPU_ONLY_SIZE);
    // pools.streaming_pool.backing    = rn_vk_memory_gpu_alloc(device, RN_VK_GpuAllocKind_Streaming, RN_VK_MEMORY_POOLS_STREAMING_SIZE);
    // pools.staging_buffer.backing    = rn_vk_memory_gpu_alloc(device, RN_VK_GpuAllocKind_Staging, RN_VK_MEMORY_POOLS_STAGING_SIZE);
    // pools.readback_buffer.backing   = rn_vk_memory_gpu_alloc(device, RN_VK_GpuAllocKind_Readback, RN_VK_MEMORY_POOLS_READBACK_SIZE);
    return(pools);
}

internal void
rn_vk_memory_pools_destroy(RN_VK_Memory *pools)
{
    for EACH_ENUM_VAL(RN_VK_GpuAllocKind, it){
        RN_VK_Memory_GpuBuffer *pool = pools->memory_kinds[it];
        if(pool->vk_buffer != VK_NULL_HANDLE){
            vkDestroyBuffer(pools->vk_device, pool->vk_buffer, NULL);
        }
        if(pool->vk_memory != VK_NULL_HANDLE){
            vkFreeMemory(pools->vk_device, pool->vk_memory, NULL);
        }
    }
    // if(pools->staging_buffer.backing.vk_buffer != VK_NULL_HANDLE)
    //     vkDestroyBuffer(device->vk_device, pools->staging_buffer.backing.vk_buffer, NULL);
    // if(pools->readback_buffer.backing.vk_buffer != VK_NULL_HANDLE)
    //     vkDestroyBuffer(device->vk_device, pools->readback_buffer.backing.vk_buffer, NULL);
    // if(pools->streaming_pool.backing.vk_buffer != VK_NULL_HANDLE)
    //     vkDestroyBuffer(device->vk_device, pools->streaming_pool.backing.vk_buffer, NULL);
    //
    // if(pools->gpu_pool.backing.vk_memory != VK_NULL_HANDLE)
    //     vkFreeMemory(device->vk_device, pools->gpu_pool.backing.vk_memory, NULL);
    // if(pools->staging_buffer.backing.vk_memory)
    //     vkFreeMemory(device->vk_device, pools->staging_buffer.backing.vk_memory, NULL);
    // if(pools->readback_buffer.backing.vk_memory != VK_NULL_HANDLE)
    //     vkFreeMemory(device->vk_device, pools->readback_buffer.backing.vk_memory, NULL);
    // if(pools->streaming_pool.backing.vk_memory != VK_NULL_HANDLE)
    //     vkFreeMemory(device->vk_device, pools->streaming_pool.backing.vk_memory, NULL);
}


internal RN_VK_GpuAlloc *
rn_vk_gpu_alloc_alloc(Arena *arena, RN_VK_GpuAlloc **head)
{
    RN_VK_GpuAlloc *node = *head;
    if (node) {
        *head = node->next_free;
    } else {
        node = PUSH_STRUCT(arena, RN_VK_GpuAlloc);
    }
    return node;
}

internal void
rn_vk_gpu_alloc_free(RN_VK_GpuAlloc **head, RN_VK_GpuAlloc *alloc)
{
    alloc->next_free = *head;
    *head = alloc;
}
