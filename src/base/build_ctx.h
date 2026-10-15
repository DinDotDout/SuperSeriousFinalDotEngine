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
#   define DOT_OS_WINDOWS 1
#elif defined(__unix__) || defined(__APPLE__) || defined(__linux__)
#   define DOT_OS_POSIX 1
#else
#   error "Unsupported platform"
#endif

////////////////////////////////////////////////////////////////
//
// Compiler

#if defined(_MSC_VER)
#   define DOT_COMPILER_CLANG 0
#   define DOT_COMPILER_GCC 0
#   define DOT_COMPILER_MSVC 1
#elif defined(__clang__)
#   define DOT_COMPILER_GCC 0
#   define DOT_COMPILER_MSVC 0
#   define DOT_COMPILER_CLANG 1
#elif defined(__GNUC__)
#   define DOT_COMPILER_MSVC 0
#   define DOT_COMPILER_CLANG 0
#   define DOT_COMPILER_GCC 1
#else
#   error Unknown compiler
#endif

#if DOT_COMPILER_CLANG || DOT_COMPILER_CLANG
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

////////////////////////////////////////////////////////////////
///
/// Compiler diagnostics

#if DOT_COMPILER_CLANG
#   define DOT_DIAGNOSTIC_PUSH      _Pragma("clang diagnostic push")
#   define DOT_DIAGNOSTIC_POP       _Pragma("clang diagnostic pop")
#   define DOT_DIAGNOSTIC_IGNORE(w) _Pragma(DOT_STR(clang diagnostic ignored w))
#   define DOT_DIAGNOSTIC_ERROR(w)  _Pragma(DOT_STR(clang diagnostic error w))
#elif DOT_COMPILER_GCC
#   define DOT_DIAGNOSTIC_PUSH      _Pragma("GCC diagnostic push")
#   define DOT_DIAGNOSTIC_POP       _Pragma("GCC diagnostic pop")
#   define DOT_DIAGNOSTIC_IGNORE(w) _Pragma(DOT_STR(GCC diagnostic ignored error w))
#   define DOT_DIAGNOSTIC_ERROR(w)  _Pragma(DOT_STR(GCC diagnostic error w))
#elif DOT_COMPILER_MSVC
#   define DOT_DIAGNOSTIC_PUSH      __pragma(warning(push))
#   define DOT_DIAGNOSTIC_POP       __pragma(warning(pop))
#   define DOT_DIAGNOSTIC_IGNORE(w) __pragma(warning(disable: w))
#   define DOT_DIAGNOSTIC_ERROR(w)  __pragma(warning(error: w))
#else
#   define DOT_DIAGNOSTIC_PUSH
#   define DOT_DIAGNOSTIC_POP
#   define DOT_DIAGNOSTIC_IGNORE(w)
#   define DOT_DIAGNOSTIC_ERROR(w)
#endif

#endif // !BUILD_CTX_H
