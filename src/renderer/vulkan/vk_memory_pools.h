#ifndef VK_MEMORY_POOLS_H
#define VK_MEMORY_POOLS_H

typedef DOT_ENUM(u8, VkMemory_PoolsKind){
    VkMemory_PoolsKind_None,
    VkMemory_PoolsKind_GpuOnly,
    VkMemory_PoolsKind_Staging,
    VkMemory_PoolsKind_Readback,
};

typedef struct VkMemory_Pools{
    u32 gpu_only_type;
    VkDeviceMemory gpu_only_mem;
    VkDeviceSize   gpu_only_size;
    u64            gpu_only_used;

    u32 staging_type;
    VkDeviceMemory staging_mem;
    VkDeviceSize   staging_size;
    VkBuffer       staging_buffer;
    u64            staging_used;

    u32 readback_type;
    VkDeviceMemory readback_mem;
    VkDeviceSize   readback_size;
    VkBuffer       readback_buffer;
    u64            readback_used;
}VkMemory_Pools;

typedef struct VkMemory_Alloc{
    VkDeviceMemory memory;
    u64 offset;
}VkMemory_Alloc;

struct RBVK_Device;
internal VkMemory_Alloc vk_memory_pools_bump(VkMemory_Pools *pools, VkMemoryRequirements reqs, VkMemory_PoolsKind memory_kind);
internal VkResult vk_memory_pools_create(struct RBVK_Device *device, VkMemory_Pools *pools);
internal void vk_memory_pools_destoy(struct RBVK_Device *device, VkMemory_Pools *pools);

internal u32 vk_memory_pools_find_memory_type(
    const VkPhysicalDeviceMemoryProperties *memory_properties,
    u32 type_bits,
    VkMemoryPropertyFlags required,
    VkMemoryPropertyFlags preferred,
    VkMemoryPropertyFlags forbidden);

#endif // !VK_MEMORY_POOLS_H
