#define STB_SPRINTF_DECORATE(name) dot_##name
#define STB_SPRINTF_IMPLEMENTATION
// #include "../extra/stb_sprintf.h"

#include <vulkan/vk_enum_string_helper.h>

#ifdef NDEBUG
#define VkCheck(x) x
#define PRINT_D(message, ...)
#else
#define VkCheck(x)                                                                                \
    do {                                                                                           \
        VkResult err = x;                                                                          \
        if (err < 0) {                                                                             \
            printf("Detected Vulkan error: %s\n", string_VkResult(err));                       \
            abort();                                                                               \
        }                                                                                          \
    } while (0)
#endif
