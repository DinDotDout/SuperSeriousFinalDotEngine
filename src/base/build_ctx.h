#ifndef BUILD_CTX_H
#define BUILD_CTX_H

#if defined(NDEBUG)
#   define DOT_DEBUG 0
#else
#   define DOT_DEBUG 1
#endif

#ifndef DOT_DEBUG_ALLOCATORS
#   define DOT_DEBUG_ALLOCATORS 0
#endif

#if defined(_WIN32)
#   define DOT_OS_WINDOWS   1
#   define DOT_OS_LINUX     0
#   define DOT_OS_MAC       0
#elif defined(__linux__)
#   define DOT_OS_WINDOWS   0
#   define DOT_OS_LINUX     1
#   define DOT_OS_MAC       0
#elif defined(__APPLE__)
#   define DOT_OS_WINDOWS   0
#   define DOT_OS_LINUX     0
#   define DOT_OS_MAC       1
#else
#   error "Unsupported OS"
#endif
#define DOT_OS_POSIX (DOT_OS_LINUX || DOT_OS_MAC)

////////////////////////////////////////////////////////////////
//
// Compiler

#if defined(__clang__)
#   define DOT_COMPILER_GCC 0
#   define DOT_COMPILER_MSVC 0
#   define DOT_COMPILER_CLANG 1
#elif defined(_MSC_VER)
#   define DOT_COMPILER_CLANG 0
#   define DOT_COMPILER_GCC 0
#   define DOT_COMPILER_MSVC 1
#elif defined(__GNUC__)
#   define DOT_COMPILER_MSVC 0
#   define DOT_COMPILER_CLANG 0
#   define DOT_COMPILER_GCC 1
#else
#   error Unknown compiler
#endif

#if DOT_COMPILER_CLANG || DOT_COMPILER_GCC
#   if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64)
#      define DOT_ARCH_X64 1
#   elif defined(i386) || defined(__i386) || defined(__i386__)
#      define DOT_ARCH_X86 1
#   elif defined(__aarch64__)
#      define DOT_ARCH_ARM64 1
#   elif defined(__arm__)
#      define DOT_ARCH_ARM32 1
#   else
#      error Architecture not supported.
#   endif
#endif

#if DOT_COMPILER_MSVC
#   if defined(_M_AMD64)
#      define DOT_ARCH_X64 1
#   elif defined(_M_IX86)
#      define DOT_ARCH_X86 1
#   elif defined(_M_ARM64)
#      define DOT_ARCH_ARM64 1
#   elif defined(_M_ARM)
#      define DOT_ARCH_ARM32 1
#   else
#       error Architecture not supported.
#   endif
#endif

#if DOT_ARCH_X64 || DOT_ARCH_X86
enum
{
    ARCH_CACHE_LINE_SIZE    = 64,
    ARCH_FALSE_SHARING_SIZE = 128, // the L2 spatial prefetcher pulls lines in 128B aligned pairs
};
#elif DOT_ARCH_ARM64 && DOT_OS_MAC
enum
{
    ARCH_CACHE_LINE_SIZE    = 128,
    ARCH_FALSE_SHARING_SIZE = 128,
};
#else
enum
{
    ARCH_CACHE_LINE_SIZE    = 64,
    ARCH_FALSE_SHARING_SIZE = 64,
};
#endif

////////////////////////////////////////////////////////////////
///
/// Compiler diagnostics

#define DOT_PRAGMA(x) _Pragma(#x)

#if DOT_COMPILER_CLANG
#   define DOT_DIAGNOSTIC_PUSH          _Pragma("clang diagnostic push")
#   define DOT_DIAGNOSTIC_POP           _Pragma("clang diagnostic pop")
#   define DOT_DIAGNOSTIC_IGNORE(w)     DOT_PRAGMA(clang diagnostic ignored w)
#   define DOT_DIAGNOSTIC_ERROR(w)      DOT_PRAGMA(clang diagnostic error w)
#   define DOT_DIAGNOSTIC_PUSH_SILENT   DOT_DIAGNOSTIC_PUSH DOT_DIAGNOSTIC_IGNORE("-Weverything")
#elif DOT_COMPILER_GCC
#   define DOT_DIAGNOSTIC_PUSH          _Pragma("GCC diagnostic push")
#   define DOT_DIAGNOSTIC_POP           _Pragma("GCC diagnostic pop")
#   define DOT_DIAGNOSTIC_IGNORE(w)     DOT_PRAGMA(GCC diagnostic ignored error w)
#   define DOT_DIAGNOSTIC_ERROR(w)      DOT_PRAGMA(GCC diagnostic error w)
#   define DOT_DIAGNOSTIC_PUSH_SILENT   DOT_DIAGNOSTIC_PUSH DOT_PRAGMA(GCC diagnostic ignored "-Wall") DOT_PRAGMA(GCC diagnostic ignored "-Wextra")
#elif DOT_COMPILER_MSVC
#   define DOT_DIAGNOSTIC_PUSH          __pragma(warning(push))
#   define DOT_DIAGNOSTIC_POP           __pragma(warning(pop))
#   define DOT_DIAGNOSTIC_IGNORE(w)     __pragma(warning(disable: w))
#   define DOT_DIAGNOSTIC_ERROR(w)      __pragma(warning(error: w))
#   define DOT_DIAGNOSTIC_PUSH_SILENT   __pragma(warning(push, 0))
#else
#   define DOT_DIAGNOSTIC_PUSH
#   define DOT_DIAGNOSTIC_POP
#   define DOT_DIAGNOSTIC_IGNORE(w)
#   define DOT_DIAGNOSTIC_ERROR(w)
#   define DOT_DIAGNOSTIC_PUSH_SILENT
#endif

////////////////////////////////////////////////////////////////
///
/// Address sanitizer

#if defined(__SANITIZE_ADDRESS__)
#   define DOT_ASAN_ENABLED 1
#elif defined(__has_feature)
#   if __has_feature(address_sanitizer)
#       define DOT_ASAN_ENABLED 1
#   endif
#endif
#ifndef DOT_ASAN_ENABLED
#   define DOT_ASAN_ENABLED 0
#endif

#if DOT_ASAN_ENABLED
#   include <stddef.h>
    void __asan_poison_memory_region(void const volatile *addr, size_t size);
    void __asan_unpoison_memory_region(void const volatile *addr, size_t size);
#   define ASAN_POISON(addr, size)      __asan_poison_memory_region((addr), (size))
#   define ASAN_UNPOISON(addr, size)    __asan_unpoison_memory_region((addr), (size))
#   if DOT_COMPILER_MSVC
#       define NO_ASAN                  __declspec(no_sanitize_address)
#   else
#       define NO_ASAN                  __attribute__((no_sanitize("all")))
#   endif
#else
#   define ASAN_POISON(addr, size)      ((void)0)
#   define ASAN_UNPOISON(addr, size)    ((void)0)
#   define NO_ASAN
#endif

#endif // !BUILD_CTX_H
