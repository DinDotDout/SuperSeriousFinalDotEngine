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
#endif

#define VkSurfaceFormat2KHRParams(...) \
    (VkSurfaceFormat2KHR){ \
        .sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR, \
        __VA_ARGS__ \
    }

#define VkSurfaceCapabilities2KHRParams(...) \
    (VkSurfaceCapabilities2KHR){ \
        .sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR, \
        __VA_ARGS__ \
    }

#define VkPhysicalDeviceSurfaceInfo2KHRParams(...) \
    (VkPhysicalDeviceSurfaceInfo2KHR){ \
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR, \
        __VA_ARGS__ \
    }
