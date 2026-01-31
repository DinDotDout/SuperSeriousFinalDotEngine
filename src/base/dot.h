#ifndef DOT
#define DOT
// NOTE: Should i prefix all of this with some macro and alloc prefix stripping?

////////////////////////////////////////////////////////////////
//
// Needed headers

#define _GNU_SOURCE
#include <stdlib.h>
//
#include <stdbool.h>
// fprintf, FILE
#include <stdio.h>
// memset
#include <string.h>
// va_start, va_end
#include <stdarg.h>

// #define STB_SPRINTF_STATIC
// #define STB_SPRINTF_NOUNALIGNED
// #define STB_SPRINTF_DECORATE(name) dot_##name
// #include "third_party/stb_sprintf.h"

////////////////////////////////////////////////////////////////
//
// Compiler

#if defined(_MSC_VER)
#define DOT_COMPILER_MSVC 1
#elif defined(__GNUC__)
#define DOT_COMPILER_GCC 1
#elif defined(__clang__)
#define DOT_COMPILER_CLANG 1
#else
#error Unknown compiler
#endif

////////////////////////////////////////////////////////////////
//
// OS

#if defined(_WIN32)
#define DOT_OS_WINDOWS
#else
#define DOT_OS_POSIX
#endif

////////////////////////////////////////////////////////////////
//
// Nice qualifiers

#define internal static
#define local_persist static
#define global static

////////////////////////////////////////////////////////////////
//
// Sane type renames

// size
#include <stddef.h>
// precise int
typedef size_t usize;
#include <stdint.h>

#ifndef DOT_INT_SKIP
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#endif
typedef uintptr_t uptr;

typedef u32 b32;
typedef u8 b8;
typedef float f32;
typedef double f64;

#define U64_MAX 0xffffffffffffffffull
#define U32_MAX 0xffffffff
#define U16_MAX 0xffff
#define U8_MAX  0xff

#define I64_MAX (i64)0x7fffffffffffffffll
#define I32_MAX (i32)0x7fffffff
#define I16_MAX (i16)0x7fff
#define I8_MAX  (i8)0x7f

#define I64_MIN (i64)0x8000000000000000ll
#define I32_MIN (i32)0x80000000
#define I16_MIN (i16)0x8000
#define I8_MIN  (i8)0x80

////////////////////////////////////////////////////////////////
//
// TIME
#define TO_USEC(t) ((t)*(u64)1000000000)
#define TO_MSEC(t) ((t)*(u64)1000000)

////////////////////////////////////////////////////////////////
//
// Array + size
#define VA_ARG_COUNT_T(T, ...) \
    (sizeof((T[]){ __VA_ARGS__ }) / sizeof(T))

#define ARRAY_PARAM(T, ...) \
    (T[]){__VA_ARGS__}, (u32) VA_ARG_COUNT_T(T, __VA_ARGS__)

////////////////////////////////////////////////////////////////
//
// Array

#define ARRAY_COUNT(arr) sizeof(arr) / sizeof(arr[0])

// This is for when allocating from an arena and we have an actual array
#define array(T) T*

////////////////////////////////////////////////////////////////
//
// Debug

#if defined(DOT_COMPILER_MSVC)
#define DEBUG_BREAK __debugbreak()
#elif defined(DOT_COMPILER_GCC)
#include <signal.h>
#define DEBUG_BREAK raise(SIGTRAP)
#elif defined(DOT_COMPILER_CLANG)
#define DEBUG_BREAK __builtin_debugtrap()
#else
#define DEBUG_BREAK ((void)0) // Fallback: no-op
#endif

////////////////////////////////////////////////////////////////
//
// String concat

#define DOT_STR_HELPER(x) #x
#define DOT_STR(x) DOT_STR_HELPER(x)

#define DOT_CONCAT_2(x, y) x ## y
#define DOT_CONCAT(x, y) DOT_CONCAT_2(x, y)

////////////////////////////////////////////////////////////////
//
// Source location helper

#define DEBUG_LOC_ARG __FILE__, __LINE__
#define DEBUG_LOC_FMT "%s:%d"

////////////////////////////////////////////////////////////////
//
// Static Debug

#define DOT_STATIC_ASSERT(x) \
typedef int DOT_CONCAT(DOT_STATIC_ASSERT_, __COUNTER__) [(x) ? 1 : -1]

////////////////////////////////////////////////////////////////
//
// Debug utils

#if defined(DOT_COMPILER_MSVC)
  #include <sal.h>
  #define PRINTF_LIKE(fmtpos, argpos) 
  #define PRINTF_STRING _Printf_format_string_
#elif defined(DOT_COMPILER_GCC) || defined(DOT_COMPILER_CLANG)
  #define PRINTF_LIKE(fmtpos, argpos) __attribute__((format(printf, fmtpos, argpos)))
  #define PRINTF_STRING
#else
  #define PRINTF_LIKE(fmtpos, argpos)
  #define PRINTF_STRING
#endif

typedef enum DOT_LogLevelKind{
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ASSERT,
    LOG_LEVEL_COUNT,
}DOT_LogLevelKind;

global const char *print_debug_str[] = {
    [LOG_LEVEL_DEBUG]     = "",
    [LOG_LEVEL_ASSERT]    = "Assertion failed",
    [LOG_LEVEL_ERROR]     = "Error",
    [LOG_LEVEL_WARNING]   = "Warning",
};

DOT_STATIC_ASSERT(LOG_LEVEL_COUNT == ARRAY_COUNT(print_debug_str));

typedef struct DOT_PrintDebugParams{
    DOT_LogLevelKind print_debug_kind;
    const char *file;
    u32 line;
}DOT_PrintDebugParams;

#define DOT_MAX_LOG_LEVEL_LENGTH 256
internal void dot_print_debug_(const DOT_PrintDebugParams* params, PRINTF_STRING const char *fmt, ...);

#define DOT_PRINT_DEBUG_PARAMS_DEFAULT(...) \
&(DOT_PrintDebugParams) { \
    .print_debug_kind = LOG_LEVEL_DEBUG, \
    .file = __FILE__, \
    .line = __LINE__, \
    __VA_ARGS__}

// --- Error Macros ---
#define DOT_ERROR_IMPL(params, fmt, ...) \
do { \
    dot_print_debug_(params, fmt, ##__VA_ARGS__); \
    DEBUG_BREAK; \
    abort(); \
} while(0)

#define DOT_ERROR(...) DOT_ERROR_IMPL(DOT_PRINT_DEBUG_PARAMS_DEFAULT(.print_debug_kind = LOG_LEVEL_ERROR), __VA_ARGS__)
#define DOT_ERROR_FL(f, l, ...) DOT_ERROR_IMPL(DOT_PRINT_DEBUG_PARAMS_DEFAULT(.print_debug_kind = LOG_LEVEL_ERROR, .file = (f), .line = (l)), __VA_ARGS__)
#define DOT_TODO(msg) \
do { \
    dot_print_debug_(DOT_PRINT_DEBUG_PARAMS_DEFAULT(.print_debug_kind = LOG_LEVEL_ERROR), "TODO: %s", msg); \
    abort(); \
} while(0)

#ifndef NDEBUG

// --- Printing Macros ---
#define DOT_PRINT(...) dot_print_debug_(DOT_PRINT_DEBUG_PARAMS_DEFAULT(), __VA_ARGS__)
#define DOT_PRINT_FL(f, l, ...) dot_print_debug_(DOT_PRINT_DEBUG_PARAMS_DEFAULT(.file = (f), .line = (l)), __VA_ARGS__)
#define DOT_WARNING(...) dot_print_debug_(DOT_PRINT_DEBUG_PARAMS_DEFAULT(.print_debug_kind = LOG_LEVEL_WARNING), __VA_ARGS__)
#define DOT_WARNING_FL(f, l, ...) dot_print_debug_(DOT_PRINT_DEBUG_PARAMS_DEFAULT(.file = (f), .line = (l), .print_debug_kind = LOG_LEVEL_WARNING), __VA_ARGS__)

// --- Assertion Macros ---
// WARN: "##" allows us to not have and fmt but uses an extension
#define DOT_ASSERT_IMPL(cond, params, fmt, ...) \
do { \
    if(!(cond)){ \
        dot_print_debug_(params, "%s " fmt, #cond, ##__VA_ARGS__); \
        DEBUG_BREAK; \
    } \
} while(0)
#define DOT_ASSERT(cond, ...) DOT_ASSERT_IMPL((cond), DOT_PRINT_DEBUG_PARAMS_DEFAULT(.print_debug_kind = LOG_LEVEL_ASSERT), __VA_ARGS__)
#define DOT_ASSERT_FL(cond, f, l, ...) DOT_ASSERT_IMPL((cond), DOT_PRINT_DEBUG_PARAMS_DEFAULT(.print_debug_kind = LOG_LEVEL_ASSERT, .file = (f), .line = (l)), __VA_ARGS__)
#else
#define DOT_PRINT(...) ((void)0)
#define DOT_PRINT_FL(f, l, ...) ((void)0)
#define DOT_WARNING(...) ((void)0)
#define DOT_WARNING_FL(f, l, ...) ((void)0)
#define DOT_ASSERT(...) ((void)0)
#define DOT_ASSERT_FL(...) ((void)0)
#endif

////////////////////////////////////////////////////////////////
//
// Cast for easy cast grep

#define cast(t) (t)

////////////////////////////////////////////////////////////////
//
// B size utils

#define KB(x) ((x) * (u64)1024)
#define MB(x) ((KB(x)) * (u64)1024)
#define GB(x) ((MB(x)) * (u64)1024)

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define CLAMP(x, bot, top) (((x) < (bot)) ? (bot) : ((x) > (top)) ? (top) : (x))

////////////////////////////////////////////////////////////////
//
// Processor hints

#if defined(__GNUC__) || defined(__clang__)
#define DOT_LIKELY(x) __builtin_expect(!!(x), 1)
#define DOT_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define Dot_Likely(x) (x)
#define Dot_Unlikely(x) (x)
#endif


////////////////////////////////////////////////////////////////
//
// Memory

#if defined(DOT_COMPILER_MSVC)
#define ALIGNOF(T) __alignof(T)
#elif defined(DOT_COMPILER_CLANG)
#define ALIGNOF(T) __alignof__(T)
#elif DOT_COMPILER_GCC
#define ALIGNOF(T) __alignof__(T)
#else
#error alignof not defined for this compiler.
#endif

#define ALIGN_POW2(x, b) (((x) + (b) - 1) & (~((b) - 1)))
#define ALIGN_DOWN_POW2(x, b) ((x) & ~((b) - 1))

#define MEMORY_ZERO(s, z) memset((s), 0, (z))
#define MEMORY_ZERO_STRUCT(s) MEMORY_ZERO((s), sizeof(*(s)))
#define MEMORY_ZERO_ARRAY(a) MEMORY_ZERO((a), sizeof(a))
#define MEMORY_ZERO_TYPED(m, c) MEMORY_ZERO((m), sizeof(*(m)) * (c))

#define MEMORY_COMPARE(a, b, size) memcmp((a), (b), (size))
#define MEMORY_EQUAL(a, b, size) (memcmp((a), (b), (size)) == 0)

// Keep this around for when using sorting functions
#if defined(DOT_COMPILER_MSVC)
#define force_inline __forceinline __declspec(safebuffers)
#define COMPILER_RESET(ptr) __assume(ptr)
#elif defined(DOT_COMPILER_GCC) || defined(DOT_DOMPILER_CLANG)
#define force_inline __attribute__((always_inline))
#define COMPILER_RESET(ptr)
#endif


////////////////////////////////////////////////////////////////
//
// whatever

#define UNUSED(something) (void)something

////////////////////////////////////////////////////////////////
//
// Useful it from raddebugger

#define EACH_INDEX(it, count) (u64 it = 0; it < (count); ++it)
#define EACH_ELEMENT(it, array) (u64 it = 0; it < ARRAY_COUNT(array); ++it)
#define EACH_ENUM_VAL(type, it) \
(type it = (type)0; it < type##_COUNT; it = (type)(it + 1))
#define EACH_NON_ZERO_ENUM_VAL(type, it) \
(type it = (type)1; it < type##_COUNT; it = (type)(it + 1))

#define EACH_IN_RANGE(it, range) (u64 it = (range).min; it < (range).max; it += 1)
#define EACH_NODE(it, T, first) (T *it = first; it != 0; it = it->next)


////////////////////////////////////////////////////////////////
//
// Useful Mem copy from raddebugger

#define MEM_COPY(dst, src, size)    memmove((dst), (src), (size))
#define MEM_COPY_STRUCT(d,s)  MEM_COPY((d),(s),sizeof(*(d)))
#define MEM_COPY_ARRAY(d,s)   MEM_COPY((d),(s),sizeof(d))
// #define MemoryCopyTyped(d,s,c) MEM_COPY((d),(s),sizeof(*(d))*(c))


////////////////////////////////////////////////////////////////
//
// Defer for profile blocks, lock/unlock...
// This accepts expressions that will run before and after a scope block once

#define DEFER_LOOP(before, after) \
        for(int _once_defer_ = 0; _once_defer_ == 0;) \
                for(before; _once_defer_++ == 0; after)


#define DEFER_LOOP_COND(before, cond, after) \
        for(int _once_cond_defer_ = 0; _once_cond_defer_ == 0;) \
                for(before; _once_cond_defer_++ == 0 && (cond); after)

////////////////////////////////////////////////////////////////
//
// Threading

#if defined(DOT_COMPILER_MSVC)
#define thread_local __declspec(thread)
#elif defined(DOT_COMPILER_GCC) || defined(DOT_COMPILER_CLANG)
#define thread_local __thread
#else
#error "No thread-local storage keyword available for this compiler"
#endif

////////////////////////////////////////////////////////////////
//
// This runs after static initialization and before main

#if defined(DOT_COMPILER_MSVC)
    #pragma section(".CRT$XCU",read)
    #define CONSTRUCTOR2_(fn, p) \
        __declspec(allocate(".CRT$XCU")) void (*fn##_)(void) = fn; \
        __pragma(comment(linker,"/include:" p #fn "_")) \
        static void fn(void)
    #ifdef _WIN64
        #define CONSTRUCTOR(fn) CONSTRUCTOR2_(fn,"")
    #else
        #define CONSTRUCTOR(fn) CONSTRUCTOR2_(fn,"_")
    #endif
#else
    #define CONSTRUCTOR(fn) \
        __attribute__((constructor)) static void fn(void)
#endif


#if defined(NDEBUG)
    #define DOT_DEBUG 0
else
    #define DOT_DEBUG 1
#endif

#endif // !DOT_H
