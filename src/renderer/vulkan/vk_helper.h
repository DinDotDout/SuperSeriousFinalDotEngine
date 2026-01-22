#ifndef VK_HELPER_H
#define VK_HELPER_H
#include <vulkan/vk_enum_string_helper.h>

#ifdef NDEBUG
#define VkCheck(x) x
#else
#define VkCheck(x) \
    do { \
        VkResult err = x; \
        if(err < 0){ \
            printf("Detected Vulkan error: %s\n", string_VkResult(err)); \
            abort(); \
        } \
    } while (0)
#endif

#define VkSurfaceFormat2KHRParams(...) \
    (VkSurfaceFormat2KHR){ \
        .sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR, \
        __VA_ARGS__ \
    }

#define VkSurfaceCapabilities2KHRParams(...) \
    (VkSurfaceCapabilities2KHR){ \
        .sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR, \
        __VA_ARGS__ \
    }

#define VkPhysicalDeviceSurfaceInfo2KHRParams(...) \
    (VkPhysicalDeviceSurfaceInfo2KHR){ \
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR, \
        __VA_ARGS__ \
    }

internal const char* VkSystemAllocationScopeName(VkSystemAllocationScope s) {
    switch(s){
        case VK_SYSTEM_ALLOCATION_SCOPE_COMMAND:
            return "COMMAND";
        case VK_SYSTEM_ALLOCATION_SCOPE_OBJECT:
            return "OBJECT";
        case VK_SYSTEM_ALLOCATION_SCOPE_CACHE:
            return "CACHE";
        case VK_SYSTEM_ALLOCATION_SCOPE_DEVICE:
            return "DEVICE";
        case VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE:
            return "INSTANCE";
        default:
            return "UNKNOWN";
    }
}

internal inline void* VkAlloc(void* data, usize size, usize alignment, VkSystemAllocationScope scope){
    Unused(data); Unused(size); Unused(alignment); Unused(scope);
    DOT_PRINT("VK Alloc: size=%M; scope=\n", size, VkSystemAllocationScopeName(scope));

    Arena* arena = cast(Arena*)data;
    void* mem = Arena_Push(arena, size, alignment, __FILE__, __LINE__);
    Arena_PrintDebug(arena);
    return mem;
}

internal inline void* VkRealloc(void* data, void* old_mem, usize size, usize alignment, VkSystemAllocationScope scope){
    Unused(data); Unused(size); Unused(alignment); Unused(scope); Unused(old_mem);
    DOT_PRINT("VK Realloc: size=%M; scope=%s", size, VkSystemAllocationScopeName(scope) );
    Arena* arena = cast(Arena*)data;
    void* mem = Arena_Push(arena, size, alignment, __FILE__, __LINE__);
    Arena_PrintDebug(arena);
    return mem;
}

internal inline void VkFree(void* data, void* mem){
    Unused(data); Unused(mem);
    Arena* arena = cast(Arena*)data;
    DOT_PRINT("VK free");
    Arena_PrintDebug(arena);
}

internal inline void VkInternalAlloc(void* data, usize size, VkInternalAllocationType alloc_type, VkSystemAllocationScope scope){
    Unused(data); Unused(size); Unused(scope); Unused(alloc_type);
    DOT_PRINT( "VK Internal Alloc: size=%M; scope=%s", size, VkSystemAllocationScopeName(scope) );
    // Arena* arena = cast(Arena*)data;
    // Arena_PrintDebug(arena);
}

void VkInternalFree(void* data, usize size, VkInternalAllocationType alloc_type, VkSystemAllocationScope scope){
    Unused(data); Unused(size); Unused(scope); Unused(alloc_type);
    DOT_PRINT( "VK Internal Free: size=%M; scope=%s", size, VkSystemAllocationScopeName(scope) );
}

#define VkAllocatorParams(arena) (VkAllocationCallbacks){ \
    .pUserData = arena, \
    .pfnAllocation = VkAlloc, \
    .pfnReallocation = VkRealloc, \
    .pfnFree = VkFree, \
    .pfnInternalAllocation = VkInternalAlloc, \
    .pfnInternalFree = VkInternalFree, \
}

#endif // !VK_HELPER_H
