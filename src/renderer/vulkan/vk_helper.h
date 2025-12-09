#ifndef VK_HELPER_H
#define VK_HELPER_H
#include <vulkan/vk_enum_string_helper.h>

#ifdef NDEBUG
#define VkCheck(x) x
#else
#define VkCheck(x) \
    do { \
        VkResult err = x; \
        if (err < 0) { \
            printf("Detected Vulkan error: %s\n", string_VkResult(err)); \
            abort(); \
        } \
    } while (0)
#endif
#endif
