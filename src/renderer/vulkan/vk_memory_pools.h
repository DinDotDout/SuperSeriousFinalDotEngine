#ifndef VK_MEMORY_POOLS_H
#define VK_MEMORY_POOLS_H

typedef enum VkMemory_PoolsKind{
    VkMemory_PoolsKind_None,
    VkMemory_PoolsKind_GpuOnly,
    VkMemory_PoolsKind_Staging, // (jd) NOTE: This should probably be a pool per thread
    VkMemory_PoolsKind_Readback,
}VkMemory_PoolsKind;

typedef struct RBVK_Device RBVK_Device;

// We need a ring buffer for staging resources
typedef struct RBVKMemory_Pool{
    // VkBuffer vk_buffer;
    u32 vk_memory_type_idx;
    VkDeviceMemory vk_memory;
    VkDeviceSize   size;
    u64            used;
}RBVKMemory_Pool;

typedef struct RBVKMemory_RingBuffer{
    VkBuffer vk_buffer;
    u32 vk_memory_type_idx;
    VkDeviceMemory vk_memory;
    VkDeviceSize   size;
    u64            head;
    u64            tail;
}RBVKMemory_RingBuffer;

typedef struct RBVKMemory_Pools{
    VkDevice vk_device;

    RBVKMemory_Pool gpu_pool;
    // u32 gpu_only_type;
    // VkDeviceMemory gpu_only_mem;
    // VkDeviceSize   gpu_only_size;
    // VkDeviceSize   gpu_only_used;

    RBVKMemory_RingBuffer staging_buffer;
    // u32 staging_type;
    // VkDeviceMemory staging_mem;
    // VkDeviceSize   staging_size;
    // VkDeviceSize   staging_used;
    // VkBuffer       staging_buffer;

    RBVKMemory_RingBuffer readback_buffer;
    // u32 readback_type;
    // VkDeviceMemory readback_mem;
    // VkDeviceSize   readback_size;
    // VkDeviceSize   readback_used;
    // VkBuffer       readback_buffer;
}RBVKMemory_Pools;

typedef struct VkMemory_Alloc{
    VkBuffer vk_buffer;
    VkMemory_PoolsKind pool_kind;
    VkDeviceMemory vk_memory;
    u64 pop_marker;
    u64 offset;
    u64 size;
}VkMemory_Alloc;

typedef struct RBVK_Texture RBVK_Texture;
typedef struct RBVK_Buffer RBVK_Buffer;

// internal VkBuffer vk_memory_pools_get_vk_buffer_from_kind(RBVKMemory_Pools *pools, VkMemory_PoolsKind buffer_kind);

// (jd) NOTE: This two must be run consecutively. Will add some check jsut in case
// internal VkMemory_Alloc rbvk_memory_pools_staging_ring_buffer_push(RBVKMemory_Pools *pools, u64 size);
internal VkMemory_Alloc rbvk_memory_pools_staging_ring_buffer_push(RBVKMemory_Pools *pools, u64 size, void *data);
internal void rbvk_memory_pools_staging_ring_buffer_pop(RBVKMemory_Pools *pools, const VkMemory_Alloc *mem_alloc);

internal VkMemory_Alloc rbvk_memory_ring_buffer_push(RBVKMemory_RingBuffer *ring_buffer, u64 size);
internal void rbvk_memory_ring_buffer_pop(RBVKMemory_RingBuffer *ring_buffer, const VkMemory_Alloc *mem_alloc);

internal VkMemory_Alloc rbvk_memory_gpu_image_alloc(RBVKMemory_Pools *pools, VkImage vk_image);
internal VkMemory_Alloc rbvk_memory_gpu_buffer_alloc(RBVKMemory_Pools *pools, VkBuffer vk_buffer);

// internal VkMemory_Alloc vk_memory_pools_bump(RBVKMemory_Pools *pools, VkMemoryRequirements reqs, VkMemory_PoolsKind memory_kind);
internal RBVKMemory_Pools vk_memory_pools_create(RBVK_Device *device);
internal void vk_memory_pools_destoy(RBVK_Device *device, RBVKMemory_Pools *pools);

internal u32 vk_memory_pools_find_memory_type(
    const VkPhysicalDeviceMemoryProperties *memory_properties,
    u32 type_bits,
    VkMemoryPropertyFlags required,
    VkMemoryPropertyFlags preferred,
    VkMemoryPropertyFlags forbidden);

#endif // !VK_MEMORY_POOLS_H
