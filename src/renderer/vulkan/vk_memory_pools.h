#ifndef VK_MEMORY_POOLS_H
#define VK_MEMORY_POOLS_H

// (jd) TODO: This will have to be abstracted to work with dx12 too eventually
// Probably eventually make a map based on consumer memory names / types
typedef enum RN_VK_GpuMemoryKind{
    RN_VK_GpuMemoryKind_GpuOnly,
    RN_VK_GpuMemoryKind_Dynamic,

    RN_VK_GpuMemoryKind_Staging,
    RN_VK_GpuMemoryKind_Readback,
    RN_VK_GpuMemoryKind_Count,
}RN_VK_GpuMemoryKind;

typedef enum RN_VK_GpuAllocStrategyKind{
    RN_VK_GpuAllocStrategyKind_RingBuffer,
    RN_VK_GpuAllocStrategyKind_Pool,
}RN_VK_GpuAllocStrategyKind;

read_only global u32 rn_vk_g_memory_pools_alloc_strategy[] = {
    [RN_VK_GpuMemoryKind_GpuOnly]    = RN_VK_GpuAllocStrategyKind_Pool,
    [RN_VK_GpuMemoryKind_Dynamic]    = RN_VK_GpuAllocStrategyKind_Pool,

    [RN_VK_GpuMemoryKind_Staging]    = RN_VK_GpuAllocStrategyKind_RingBuffer,
    [RN_VK_GpuMemoryKind_Readback]   = RN_VK_GpuAllocStrategyKind_RingBuffer,
};

read_only global u64 rn_vk_g_memory_pools_memory_sizes[] = {
    [RN_VK_GpuMemoryKind_GpuOnly] = DOT_MB(512),
    [RN_VK_GpuMemoryKind_Dynamic] = DOT_MB(64),

    [RN_VK_GpuMemoryKind_Staging] = DOT_MB(64),
    [RN_VK_GpuMemoryKind_Readback]= DOT_MB(32),
};


// TODO:I am keeping this for now, but we should probably store a ptr to the
// parent pool so that we can dispatch cleanup logic automatically on correct
// allocator rather than having the user know which it should use. Tho we don't
// free individually for now except on ring buffer

typedef struct RN_VK_FreeListGpuAlloc{
    LLHead head;
}RN_VK_FreeListGpuAlloc;

typedef struct RN_VK_Memory_GpuBuffer{
    RN_VK_GpuMemoryKind         memory_kind;
    RN_VK_GpuAllocStrategyKind  alloc_strategy_kind;
    VkDeviceMemory  vk_memory;
    VkBuffer        vk_buffer; // (jd) Empty if gpu only
    VkDeviceSize    size;
    u32 vk_memory_type_idx;
    RN_VK_FreeListGpuAlloc free_list;
}RN_VK_Memory_GpuBuffer;

typedef u64 RN_VK_GpuAllocHandle;
typedef struct RN_VK_StagingAlloc{
    VkBuffer vk_buffer;
    VkDeviceSize offset;

    RN_VK_GpuAllocHandle handle;
}RN_VK_StagingAlloc;


typedef struct RN_VK_Memory RN_VK_Memory;
typedef struct RN_VK_GpuAlloc{
    RN_VK_Memory_GpuBuffer *buffer;

    u64 size;
    u64 offset;
    u32 pad;

    VkBuffer vk_buffer; // (jd) Unused for Device local memory

    LLNode node;
}RN_VK_GpuAlloc;

typedef struct RN_VK_Memory_Pool{
    RN_VK_Memory_GpuBuffer backing;

    u64 used;
    // RN_VK_GpuAlloc *next_free;
    // RN_VK_FreeListGpuAlloc free_list;
}RN_VK_Memory_Pool;


typedef struct RN_VK_Memory_RingBuffer{
    RN_VK_Memory_GpuBuffer backing;

    u64 head;
    u64 tail; // (jd) used = head - tail

    LLNode node;
    // RN_VK_GpuAlloc *next_free;
    // RN_VK_FreeListGpuAlloc free_list;
}RN_VK_Memory_RingBuffer;

struct RN_VK_Memory{
    VkDevice vk_device;
    RN_VK_Memory_GpuBuffer *memory_kinds[RN_VK_GpuMemoryKind_Count];
};

typedef struct RN_VK_Device RN_VK_Device;
internal RN_VK_Memory  rn_vk_memory_pools_create(Arena *arena, RN_VK_Device *device);
internal void          rn_vk_memory_pools_destroy(RN_VK_Memory *pools);

internal RN_VK_Memory_GpuBuffer     *rn_vk_memory_gpu_alloc(Arena *arena, RN_VK_Device *device, RN_VK_GpuMemoryKind kind, RN_VK_GpuAllocStrategyKind alloc_strategy, u64 alloc_size);
internal RN_VK_Memory_Pool          *rn_vk_memory_pool_get(RN_VK_Memory *pools, RN_VK_GpuMemoryKind kind);
internal RN_VK_Memory_RingBuffer    *rn_vk_memory_ring_buffer_get(RN_VK_Memory *pools, RN_VK_GpuMemoryKind kind);
internal RN_VK_Memory_RingBuffer    *rn_vk_ring_buffer_from_buffer(RN_VK_Memory_GpuBuffer *buffer);
internal RN_VK_Memory_Pool          *rn_vk_memory_pool_from_buffer(RN_VK_Memory_GpuBuffer *buffer);

// We should probably hand out handles rather than allocate this on the user
internal RN_VK_StagingAlloc     rn_vk_memory_pools_staging_ring_buffer_push(Arena *arena, RN_VK_Memory *pools, u64 size, void *data);
internal void                   rn_vk_memory_pools_staging_ring_buffer_pop(RN_VK_StagingAlloc *staging_alloc);

internal RN_VK_GpuAlloc         *rn_vk_memory_ring_buffer_push_(Arena *arena, RN_VK_Memory_RingBuffer *ring_buffer, u64 size);

internal RN_VK_GpuAllocHandle  rn_vk_memory_gpu_image_alloc(Arena *arena, RN_VK_Memory *pools, VkImage vk_image);
internal RN_VK_GpuAllocHandle  rn_vk_memory_gpu_buffer_alloc(Arena *arena, RN_VK_Memory *pools, RN_ResourceUsageKind usage_kind, VkBuffer vk_buffer);


internal u32 rn_vk_memory_pools_find_memory_type(
    const VkPhysicalDeviceMemoryProperties *memory_properties,
    // u32 type_bits,
    VkMemoryPropertyFlags required,
    VkMemoryPropertyFlags preferred,
    VkMemoryPropertyFlags forbidden);

#endif // !VK_MEMORY_POOLS_H
