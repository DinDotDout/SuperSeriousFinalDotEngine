#if defined(_WIN32)
    #define DOT_OS_WINDOWS
#elif defined(__unix__) || defined(__APPLE__) || defined(__linux__)
    #define DOT_OS_POSIX
#else
    #error "Unsupported platform"
#endif

#if defined(DOT_OS_POSIX)
    #include "os/os_linux.h"
#elif defined(DOT_OS_WINDOWS)
    #include "os/os_windows.h"
#endif

CONST_INT_BLOCK{
    PLATFORM_REGULAR_PAGE_SIZE = KB(4),
    PLATFORM_LARGE_PAGE_SIZE = MB(2),
};

////////////////////////////////////////////////////////////////
//
// Compiler
//
#if DOT_COMPILER_MSVC
    #if defined(__SANITIZE_ADDRESS__)
        #define DOT_ASAN_ENABLED
        #define NO_ASAN __declspec(no_sanitize_address)
    #else
        #define NO_ASAN
    #endif
#elif DOT_COMPILER_GCC
    #if defined(__has_feature)
        #if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
            #define DOT_ASAN_ENABLED
        #endif
    #endif
#endif

#if defined(DOT_ASAN_ENABLED)
void __asan_poison_memory_region(void const volatile *addr, size_t size);
void __asan_unpoison_memory_region(void const volatile *addr, size_t size);
#define ASAN_POISON(addr, size)   __asan_poison_memory_region((addr), (size))
#define ASAN_UNPOISON(addr, size) __asan_unpoison_memory_region((addr), (size))
#define NO_ASAN __attribute__((no_sanitize("all")))

// NOTE: Replaces OS functionality by regular malloc so that asan can track allocations
#define os_reserve(size) malloc((size))
#define os_release(ptr, size) \
    do{ \
        (void)(size); \
        free((ptr)); \
    }while (0)
#define os_commit(ptr, size) ((void)0)
#else
#define ASAN_POISON(addr, size)   ((void)0)
#define ASAN_UNPOISON(addr, size) ((void)0)
#define NO_ASAN
#endif

////////////////////////////////////////////////////////////////
//
// Metrics
//
internal u64
platform_cpu_read_timer(void){
    return __rdtsc(); // NOTE: Only works on x86, update to support ARM
}

internal u64
platform_cpu_estimate_freq(){
    u64 msec_to_wait = 100;
    u64 os_freq = platform_os_get_timer_freq();

    u64 cpu_start = platform_cpu_read_timer();
    u64 os_start = platform_os_read_timer();
    u64 os_end = 0;
    u64 os_elapsed = 0;
    u64 wait_time_msec = msec_to_wait * os_freq / 1000;
    while(os_elapsed < wait_time_msec){
        os_end = platform_os_read_timer();
        os_elapsed = os_end - os_start;
    }
    u64 cpu_end = platform_cpu_read_timer();
    u64 cpu_elapsed = cpu_end - cpu_start;
    u64 cpu_freq = 0;
    if(os_elapsed){
        cpu_freq = os_freq * cpu_elapsed / os_elapsed;
    }

    // printf("os elapsed: %lu\n", os_elapsed);
    // printf("cpu elapsed: %lu\n", cpu_elapsed);
    // printf("cpu freq: %lu\n", cpu_freq);
    return cpu_freq;
}
