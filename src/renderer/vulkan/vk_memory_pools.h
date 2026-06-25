#ifndef VK_MEMORY_POOLS_H
#define VK_MEMORY_POOLS_H

typedef enum RN_VK_MemoryPoolsKind{
    RN_VK_MemoryPoolsKind_None,
    RN_VK_MemoryPoolsKind_GpuOnly,
    RN_VK_MemoryPoolsKind_Staging, // (jd) NOTE: This should probably be a pool per thread
    RN_VK_MemoryPoolsKind_Readback,
}RN_VK_MemoryPoolsKind;

typedef struct RN_VK_Memory_Pool{
    // VkBuffer vk_buffer;
    u32 vk_memory_type_idx;
    VkDeviceMemory vk_memory;
    VkDeviceSize   size;
    u64            used;
}RN_VK_Memory_Pool;

typedef struct RN_VK_Memory_RingBuffer{
    VkBuffer vk_buffer;
    u32 vk_memory_type_idx;
    VkDeviceMemory vk_memory;
    VkDeviceSize   size;
    u64            head;
    u64            tail;
}RN_VK_Memory_RingBuffer;

typedef struct RN_VK_MemoryPools{
    VkDevice vk_device;

    RN_VK_Memory_Pool         gpu_pool;
    RN_VK_Memory_RingBuffer   staging_buffer;
    RN_VK_Memory_RingBuffer   readback_buffer;
}RN_VK_MemoryPools;

// (jd) NOTE: Maybe should just store ptr to memory pools and infer vkBuffer from pool_kind
typedef struct RN_VK_MemoryAlloc{
    VkBuffer vk_buffer;
    RN_VK_MemoryPoolsKind pool_kind;
    // VkDeviceMemory vk_memory;
    u64 pop_marker;
    u64 offset;
    u64 size;
}RN_VK_MemoryAlloc;

typedef struct RN_VK_Device RN_VK_Device;
internal RN_VK_MemoryPools rn_vk_memory_pools_create(RN_VK_Device *device);
internal void               rn_vk_memory_pools_destoy(RN_VK_Device *device, RN_VK_MemoryPools *pools);

internal RN_VK_MemoryAlloc  rn_vk_memory_pools_staging_ring_buffer_push(RN_VK_MemoryPools *pools, u64 size, void *data);
internal void               rn_vk_memory_pools_staging_ring_buffer_pop(RN_VK_MemoryPools *pools, const RN_VK_MemoryAlloc *mem_alloc);

internal RN_VK_MemoryAlloc  rn_vk_memory_ring_buffer_push(RN_VK_Memory_RingBuffer *ring_buffer, u64 size);
internal void               rn_vk_memory_ring_buffer_pop(RN_VK_Memory_RingBuffer *ring_buffer, const RN_VK_MemoryAlloc *mem_alloc);

internal RN_VK_MemoryAlloc  rn_vk_memory_gpu_image_alloc(RN_VK_MemoryPools *pools, VkImage vk_image);
internal RN_VK_MemoryAlloc  rn_vk_memory_gpu_buffer_alloc(RN_VK_MemoryPools *pools, VkBuffer vk_buffer);

internal u32 rn_vk_memory_pools_find_memory_type(
    const VkPhysicalDeviceMemoryProperties *memory_properties,
    u32 type_bits,
    VkMemoryPropertyFlags required,
    VkMemoryPropertyFlags preferred,
    VkMemoryPropertyFlags forbidden);

#endif // !VK_MEMORY_POOLS_H
