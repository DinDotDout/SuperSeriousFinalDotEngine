#ifndef VK_MEMORY_POOLS_H
#define VK_MEMORY_POOLS_H

typedef enum VkMemory_PoolsKind{
    VkMemory_PoolsKind_None,
    VkMemory_PoolsKind_GpuOnly,
    VkMemory_PoolsKind_Staging, // (jd) NOTE: This should probably be a pool per thread
    VkMemory_PoolsKind_Readback,
}VkMemory_PoolsKind;

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

    RBVKMemory_Pool         gpu_pool;
    RBVKMemory_RingBuffer   staging_buffer;
    RBVKMemory_RingBuffer   readback_buffer;

}RBVKMemory_Pools;

typedef struct VkMemory_Alloc{
    VkBuffer vk_buffer;
    VkMemory_PoolsKind pool_kind;
    VkDeviceMemory vk_memory;
    u64 pop_marker;
    u64 offset;
    u64 size;
}VkMemory_Alloc;

typedef struct RBVK_Device RBVK_Device;
internal RBVKMemory_Pools   vk_memory_pools_create(RBVK_Device *device);
internal void               vk_memory_pools_destoy(RBVK_Device *device, RBVKMemory_Pools *pools);

internal VkMemory_Alloc     rbvk_memory_pools_staging_ring_buffer_push(RBVKMemory_Pools *pools, u64 size, void *data);
internal void               rbvk_memory_pools_staging_ring_buffer_pop(RBVKMemory_Pools *pools, const VkMemory_Alloc *mem_alloc);

internal VkMemory_Alloc     rbvk_memory_ring_buffer_push(RBVKMemory_RingBuffer *ring_buffer, u64 size);
internal void               rbvk_memory_ring_buffer_pop(RBVKMemory_RingBuffer *ring_buffer, const VkMemory_Alloc *mem_alloc);

internal VkMemory_Alloc     rbvk_memory_gpu_image_alloc(RBVKMemory_Pools *pools, VkImage vk_image);
internal VkMemory_Alloc     rbvk_memory_gpu_buffer_alloc(RBVKMemory_Pools *pools, VkBuffer vk_buffer);

internal u32 vk_memory_pools_find_memory_type(
    const VkPhysicalDeviceMemoryProperties *memory_properties,
    u32 type_bits,
    VkMemoryPropertyFlags required,
    VkMemoryPropertyFlags preferred,
    VkMemoryPropertyFlags forbidden);

#endif // !VK_MEMORY_POOLS_H
