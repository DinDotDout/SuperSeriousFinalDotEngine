internal void
rn_vk_memory_ring_buffer_pop(RN_VK_GpuAllocHandle h)
{
    RN_VK_GpuAlloc *alloc = cast(RN_VK_GpuAlloc*)h;
    RN_VK_Memory_RingBuffer *ring_buffer = rn_vk_memory_ring_buffer_from_gpu_buffer(alloc->buffer);
    DOT_ASSERT(ring_buffer->tail == alloc->offset - alloc->pad, "Incorrect ring buffer pop order %u %u %u", ring_buffer->tail, alloc->offset, alloc->pad);
    ring_buffer->tail = alloc->offset + alloc->size;
    FreeListFree(&ring_buffer->backing.free_list, alloc);
}

internal void
rn_vk_memory_pools_staging_ring_buffer_pop(RN_VK_StagingAlloc *staging_alloc)
{
    rn_vk_memory_ring_buffer_pop(staging_alloc->handle);
}

internal RN_VK_Memory_RingBuffer *
rn_vk_memory_ring_buffer_from_gpu_buffer(RN_VK_Memory_GpuBuffer *buffer)
{
    DOT_ASSERT(cast(u32)buffer->alloc_strategy_kind == rn_vk_g_memory_pools_alloc_strategy[buffer->memory_kind]);
    RN_VK_Memory_RingBuffer *ring_buffer = DOT_CONTAINER_OF(buffer, RN_VK_Memory_RingBuffer, backing);
    return ring_buffer;
}

internal RN_VK_Memory_Pool *
rn_vk_memory_pool_from_gpu_buffer(RN_VK_Memory_GpuBuffer *buffer)
{
    DOT_ASSERT(cast(u32)buffer->alloc_strategy_kind == rn_vk_g_memory_pools_alloc_strategy[buffer->memory_kind]);
    RN_VK_Memory_Pool *pool = DOT_CONTAINER_OF(buffer, RN_VK_Memory_Pool, backing);
    return pool;
}

internal RN_VK_Memory_Pool *
rn_vk_memory_pool_get_from_memory_kind(RN_VK_Memory *memory, RN_VK_GpuMemoryKind kind)
{
    RN_VK_Memory_GpuBuffer *buffer = memory->pools[kind];
    DOT_ASSERT(buffer->memory_kind == kind);
    DOT_ASSERT(cast(u32)buffer->alloc_strategy_kind == rn_vk_g_memory_pools_alloc_strategy[kind]);
    RN_VK_Memory_Pool *pool = DOT_CONTAINER_OF(buffer, RN_VK_Memory_Pool, backing);
    return pool;
}

internal RN_VK_Memory_RingBuffer *
rn_vk_memory_ring_buffer_get_from_memory_kind(RN_VK_Memory *memory, RN_VK_GpuMemoryKind kind)
{
    RN_VK_Memory_GpuBuffer *buffer = memory->pools[kind];
    DOT_ASSERT(buffer->memory_kind == kind);
    DOT_ASSERT(cast(u32)buffer->alloc_strategy_kind == rn_vk_g_memory_pools_alloc_strategy[kind]);
    RN_VK_Memory_RingBuffer *staging_buffer = DOT_CONTAINER_OF(buffer, RN_VK_Memory_RingBuffer, backing);
    return staging_buffer;
}

internal RN_VK_StagingAlloc
rn_vk_memory_pools_staging_ring_buffer_push(Arena *arena, RN_VK_Memory *memory, u64 size, void *data)
{
    RN_VK_Memory_RingBuffer *staging_buffer = rn_vk_memory_ring_buffer_get_from_memory_kind(memory, RN_VK_GpuMemoryKind_Staging);

    u64 aligned_memory = ALIGN_POW2(staging_buffer->head, cast(u64)16);
    u32 pad = cast(u32) (aligned_memory - staging_buffer->head);
    if(aligned_memory + size > staging_buffer->backing.size){
        aligned_memory = 0;
        staging_buffer->head = 0;
        if(size > staging_buffer->tail){
            DOT_ERROR("Out of Staging memory, consider increasing it!");
        }
    }
    if(aligned_memory < staging_buffer->tail && aligned_memory + size > staging_buffer->tail){
        DOT_ERROR("Out of Staging memory, consider increasing it!");
    }
    staging_buffer->head = aligned_memory + size;
 
    RN_VK_GpuAlloc *alloc = FreeListGetOrCreate(arena, &staging_buffer->backing.free_list, RN_VK_GpuAlloc);
    alloc->buffer       = &staging_buffer->backing;
    //alloc->vk_buffer    = ring_buffer->backing.vk_buffer;
    alloc->size         = size;
    alloc->offset       = aligned_memory;
    alloc->pad          = pad;

    void *destination_data = cast(u8*)staging_buffer->backing.mapped + alloc->offset;
    if(data){
        DOT_PRINT("alloc size %M", alloc->size);
        MEMORY_COPY_NO_ALIAS(destination_data, data, size);
    }

    RN_VK_StagingAlloc staging = {0};
    staging.handle = cast(RN_VK_GpuAllocHandle) alloc;
    staging.vk_buffer = staging_buffer->backing.vk_buffer;

    return(staging);
}

// internal RN_VK_GpuMemoryKind
// rn_vk_gpu_memory_kind_from_resource_usage(RN_ResourceUsageKind usage_kind)
// {
//     switch (usage_kind){
//     default: DOT_ERROR("Invalid usage kind %u", usage_kind);
//     case RN_ResourceUsageKind_Dynamic   : return RN_VK_GpuMemoryKind_Dynamic;
//     case RN_ResourceUsageKind_Readback  : return RN_VK_GpuMemoryKind_Readback;
//     case RN_ResourceUsageKind_GPUOnly   : return RN_VK_GpuMemoryKind_GpuOnly;
//     }
// }

internal RN_VK_GpuAllocHandle
rn_vk_memory_pools_dynamic_buffer_push(Arena *arena, RN_VK_Memory *memory, u64 size)
{
    //RN_VK_GpuMemoryKind memory_kind = rn_vk_gpu_memory_kind_from_resource_usage(memory_kind);
    RN_VK_Memory_Pool *gpu_pool = rn_vk_memory_pool_get_from_memory_kind(memory, RN_ResourceUsageKind_Dynamic);

    u64 aligned_memory = ALIGN_POW2(gpu_pool->used, cast(u64)16);
    u32 pad = cast(u32) (aligned_memory - gpu_pool->used);

    gpu_pool->used = aligned_memory + size;
    if(gpu_pool->used > gpu_pool->backing.size){
        DOT_ERROR("Out of Staging memory, consider increasing it!");
    }

    RN_VK_GpuAlloc *alloc = FreeListGetOrCreate(arena, &gpu_pool->backing.free_list, RN_VK_GpuAlloc);
    alloc->buffer       = &gpu_pool->backing;
    //alloc->vk_buffer    = gpu_pool->backing.vk_buffer;
    alloc->size         = size;
    alloc->offset       = aligned_memory;
    alloc->pad          = pad;

    return(cast(RN_VK_GpuAllocHandle)alloc);
}

internal RN_VK_GpuAllocHandle
rn_vk_memory_gpu_buffer_alloc(Arena *arena, RN_VK_Memory *memory, VkBuffer vk_buffer)
{
    //RN_VK_GpuMemoryKind memory_kind = rn_vk_gpu_memory_kind_from_resource_usage(usage_kind);

    RN_VK_Memory_Pool *gpu_pool = rn_vk_memory_pool_get_from_memory_kind(memory, RN_ResourceUsageKind_GPUOnly);

    VkMemoryRequirements2 reqs2 = vk_buffer_memory_requirements(memory->vk_device, vk_buffer);
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

    RN_VK_GpuAlloc *alloc = FreeListGetOrCreate(arena, &gpu_pool->backing.free_list, RN_VK_GpuAlloc);
    alloc->size     = reqs.size;
    alloc->buffer   = &gpu_pool->backing;
    alloc->offset   = aligned_memory;

    RN_VK_CHECK(vkBindBufferMemory(memory->vk_device, vk_buffer, gpu_pool->backing.vk_memory, alloc->offset));
    return(cast(RN_VK_GpuAllocHandle)alloc);
}

internal RN_VK_GpuAllocHandle
rn_vk_memory_gpu_image_alloc(Arena *arena, RN_VK_Memory *memory, VkImage vk_image)
{
    RN_VK_Memory_Pool *gpu_pool = rn_vk_memory_pool_get_from_memory_kind(memory, RN_VK_GpuMemoryKind_GpuOnly);

    VkMemoryRequirements2 reqs2 = vk_image_memory_requirements(memory->vk_device, vk_image);
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

    RN_VK_GpuAlloc *alloc = FreeListGetOrCreate(arena, &gpu_pool->backing.free_list, RN_VK_GpuAlloc);
    alloc->size             = reqs.size;
    alloc->buffer           = &gpu_pool->backing;
    alloc->offset           = aligned_memory;
    alloc->pad              = pad;

    RN_VK_CHECK(vkBindImageMemory(memory->vk_device, vk_image, gpu_pool->backing.vk_memory, alloc->offset));
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
rn_vk_memory_gpu_buffer_create(Arena *arena, RN_VK_Device *device, RN_VK_GpuMemoryKind memory_kind, RN_VK_GpuAllocStrategyKind alloc_strategy, u64 alloc_size)
{
    VkMemoryPropertyFlags required  = 0;
    VkMemoryPropertyFlags preferred = 0;
    VkMemoryPropertyFlags forbidden = 0;
    VkBufferUsageFlags usage = 0;
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(device->vk_gpu, &memory_properties);

    partial_switch_begin(memory_kind){
    default: DOT_ERROR("Non existent memory kind");
    case RN_VK_GpuMemoryKind_GpuOnly:
        required    = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        forbidden   = (device->is_integrated_gpu ? 0 : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    break;
    case RN_VK_GpuMemoryKind_Staging:
        required    = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        forbidden   = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    break;
    case RN_VK_GpuMemoryKind_Dynamic:
        required    = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        forbidden   = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
            | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
            | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
            | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    break;
    case RN_VK_GpuMemoryKind_Readback:
        required    = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            | VK_MEMORY_PROPERTY_HOST_CACHED_BIT
            | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT; // Avoid manually calling vkInvalidateMappedMemoryRanges
        // preferred   = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        forbidden   = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    break;
    }partial_switch_end;

    RN_VK_Memory_GpuBuffer *gpu_buffer = NULL;
    switch(alloc_strategy){
    default: DOT_ERROR("Innvalid alloc strategy %d", alloc_strategy);
    case RN_VK_GpuAllocStrategyKind_RingBuffer:{
        RN_VK_Memory_RingBuffer *ring_buffer = PUSH_STRUCT(arena, RN_VK_Memory_RingBuffer);
        gpu_buffer = &ring_buffer->backing;
    break;}
    case RN_VK_GpuAllocStrategyKind_Pool:{
        RN_VK_Memory_Pool *pool = PUSH_STRUCT(arena, RN_VK_Memory_Pool);
        gpu_buffer = &pool->backing;
    break;}
    }
    gpu_buffer->vk_memory_type_idx = rn_vk_memory_pools_find_memory_type(&memory_properties, required, preferred, forbidden);
    gpu_buffer->size = alloc_size;
    gpu_buffer->alloc_strategy_kind = alloc_strategy;
    gpu_buffer->memory_kind = memory_kind;

    u64 required_alloc_size = alloc_size;
    if(usage != 0){
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
    alloc_info.memoryTypeIndex = gpu_buffer->vk_memory_type_idx;
    RN_VK_CHECK(vkAllocateMemory(device->vk_device, &alloc_info, NULL, &gpu_buffer->vk_memory));

    if(usage){
        VkBufferCreateInfo buff_info = {0};
        buff_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buff_info.size  = alloc_size;
        buff_info.usage = usage;
        buff_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        RN_VK_CHECK(vkCreateBuffer(device->vk_device, &buff_info, NULL, &gpu_buffer->vk_buffer));
        RN_VK_CHECK(vkBindBufferMemory(device->vk_device, gpu_buffer->vk_buffer, gpu_buffer->vk_memory, 0));

        RN_VK_CHECK(vkMapMemory(device->vk_device, gpu_buffer->vk_memory, 0, alloc_size, 0, &gpu_buffer->mapped));
    }
    return(gpu_buffer);
}
void 
rn_vk_memory_gpu_buffer_destroy(VkDevice dev, RN_VK_Memory_GpuBuffer *buff)
{
    if(buff->vk_buffer != VK_NULL_HANDLE){
        vkDestroyBuffer(dev, buff->vk_buffer, NULL);
    }
    if(buff->vk_memory != VK_NULL_HANDLE){
        vkUnmapMemory(dev, buff->vk_memory);
        vkFreeMemory(dev, buff->vk_memory, NULL);
    }
}

internal RN_VK_Memory
rn_vk_memory_pools_create(Arena *arena, RN_VK_Device *device)
{
    RN_VK_Memory memory = {0};
    memory.vk_device =  device->vk_device;
    for EACH_ENUM_VAL(RN_VK_GpuMemoryKind, memory_kind){
        RN_VK_GpuAllocStrategyKind alloc_strategy = rn_vk_g_memory_pools_alloc_strategy[memory_kind];
        memory.pools[memory_kind] = rn_vk_memory_gpu_buffer_create(arena, device, memory_kind, alloc_strategy, rn_vk_g_memory_pools_memory_sizes[memory_kind]);
    }
    return(memory);
}

internal void
rn_vk_memory_pools_destroy(RN_VK_Memory *memory)
{
    for EACH_ENUM_VAL(RN_VK_GpuMemoryKind, it){
        RN_VK_Memory_GpuBuffer *buff = memory->pools[it];
        rn_vk_memory_gpu_buffer_destroy(memory->vk_device, buff);
    }
}
