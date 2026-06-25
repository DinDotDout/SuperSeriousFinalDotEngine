// (joan) NOTE: split this per platform instead of keeping thing together
#include <sys/stat.h>
#if defined(DOT_OS_POSIX)
enum
{
    PLATFORM_REGULAR_PAGE_SIZE  = DOT_KB(4),
    PLATFORM_LARGE_PAGE_SIZE    = DOT_MB(2),
    PLATFORM_CACHE_LINE_SIZE    = 64,
};
#elif defined(DOT_OS_WINDOWS)
// NOTE: Windows reserve size is 64kb even though commit size is 4
enum
{
    PLATFORM_REGULAR_PAGE_SIZE  = DOT_KB(64), // Comit
    PLATFORM_LARGE_PAGE_SIZE    = DOT_MB(2),
    PLATFORM_CACHE_LINE_SIZE    = 64,
};
#endif

////////////////////////////////////////////////////////////////
//
// Compiler
//
#if DOT_COMPILER_MSVC
#   if defined(__SANITIZE_ADDRESS__)
#       define DOT_ASAN_ENABLED
#       define NO_ASAN __declspec(no_sanitize_address)
#   else
#       define NO_ASAN
#   endif
#elif DOT_COMPILER_GCC || DOT_COMPILER_CLANG
#   if defined(__has_feature)
#       if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
#           define DOT_ASAN_ENABLED
#       endif
#   endif
#endif

#if defined(DOT_ASAN_ENABLED)
    void __asan_poison_memory_region(void const volatile *addr, size_t size);
    void __asan_unpoison_memory_region(void const volatile *addr, size_t size);
#   define ASAN_POISON(addr, size)   __asan_poison_memory_region((addr), (size))
#   define ASAN_UNPOISON(addr, size) __asan_unpoison_memory_region((addr), (size))
#   define NO_ASAN __attribute__((no_sanitize("all")))
#else
#   define ASAN_POISON(addr, size)   ((void)0)
#   define ASAN_UNPOISON(addr, size) ((void)0)
#   define NO_ASAN
#endif

typedef struct String8 String8;
typedef struct Arena Arena;
internal bool platform_file_exists(String8 file_path);
internal String8 platform_read_entire_file(Arena *arena, String8 path);
b32 platform_file_is_newer(String8 a, String8 b);
 
u64 platform_get_time_ns(void);

////////////////////////////////////////////////////////////////
//
// Metrics
//
internal u64 platform_cpu_read_timer(void);
internal u64 platform_cpu_estimate_freq();
