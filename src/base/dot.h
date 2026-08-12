#ifndef DOT_H
#define DOT_H

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

#include <threads.h>

#if DOT_COMPILER_CLANG || (DOT_COMPILER_GCC && __GNUC__ >= 7)
#   define DOT_FALLTHROUGH __attribute__((fallthrough))
#else
#   define DOT_FALLTHROUGH /* fallthrough */
#endif

#if DOT_COMPILER_CLANG
#   define DOT_ALLOW_PARTIAL_SWITCH \
        _Pragma("clang diagnostic push") \
        _Pragma("clang diagnostic ignored \"-Wswitch-enum\"")
        // _Pragma("clang diagnostic ignored \"-Wswitch\"")

#   define DOT_RESTORE_PARTIAL_SWITCH \
        _Pragma("clang diagnostic pop")

#elif DOT_COMPILER_GCC
#   define DOT_ALLOW_PARTIAL_SWITCH \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Wswitch-enum\"")
        // _Pragma("GCC diagnostic ignored \"-Wswitch\"")

#   define DOT_RESTORE_PARTIAL_SWITCH _Pragma("GCC diagnostic pop")
#elif DOT_COMPILER_MSVC
#   define DOT_ALLOW_PARTIAL_SWITCH \
        __pragma(warning(push)) \
        __pragma(warning(disable: 4061 4062 4063 4065))
#   define DOT_RESTORE_PARTIAL_SWITCH \
        __pragma(warning(pop))
#else
    // MSVC or unknown compiler 
#   define DOT_ALLOW_PARTIAL_SWITCH
#   define DOT_RESTORE_PARTIAL_SWITCH
#endif

#define partial_switch_begin(cond) \
        DOT_ALLOW_PARTIAL_SWITCH; \
        switch(cond)

#define partial_switch_end DOT_RESTORE_PARTIAL_SWITCH

////////////////////////////////////////////////////////////////
//
// Nice qualifiers

#define internal static
#define local_persist static
#define global static

#if DOT_COMPILER_MSVC || (DOT_COMPILER_CLANG && DOT_OS_WINDOWS)
#   pragma section(".rdata$", read)
#   define read_only __declspec(allocate(".rdata$"))
#elif DOT_COMPILER_CLANG && DOT_OS_POSIX
// #   define read_only __attribute__((section(".rodata")))
#   define read_only const
#else
#   define read_only
#endif
//
// Keep this around for when using sorting functions
#if DOT_COMPILER_MSVC
#   define force_inline __forceinline __declspec(safebuffers)
#   define COMPILER_RESET(ptr) __assume(ptr)
#elif DOT_COMPILER_GCC || DOT_COMPILER_CLANG
#   define force_inline __attribute__((always_inline))
#   define COMPILER_RESET(ptr)
#endif

#ifndef thread_local
#   if DOT_COMPILER_MSVC
#       define thread_local __declspec(thread)
#   elif DOT_COMPILER_GCC || DOT_COMPILER_CLANG
#       define thread_local __thread
#   else
#       error "No thread-local storage keyword available for this compiler"
#   endif
#endif

// #if defined(__GNUC__) && __GNUC__ < 5
//     #define thread_local __thread
// #elif DOT_COMPILER_MSVC
//     #define thread_local       __declspec(thread)
// #elif defined (__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
//     #define thread_local       _Thread_local
// #endif
//
// #ifndef STBI_THREAD_LOCAL
//     #if defined(__GNUC__)
//     #define thread_local       __thread
//     #endif
// #endif

////////////////////////////////////////////////////////////////
//
// Enum x Bitfield
//
// This is a way to get some consistency out of enum types in c
// We should probably only care about this if we need to serialize
// the data, otherwise we lose te ability to get warning out of
// unhandled switch statement cases

#define DOT_BIT(x)                  (1 << (x))
#define DOT_BITS_MATCH(flags, mask) (((flags) & (mask)) == (mask))
#define DOT_BITS_ANY(flags, mask)   (((flags) & (mask)) != 0)

// (jd) NOTE: Should probably use something for code gen but whatever
#define DOT_X_ENUM_ARG(prefix, v)           prefix##_##v,
#define DOT_X_ENUM_STR(prefix, v)           string8_lit(#v),
#define DOT_X_ENUM_VAL_FROM_STR(prefix, v)  if(string8_equal(string8_from_##prefix[prefix##_##v], str)) return prefix##_##v;
#define DOT_ENUM_REFLECT(enum_name, x_list) \
    typedef enum enum_name{x_list(DOT_X_ENUM_ARG) }enum_name; \
    static const String8 string8_from_##enum_name[] = { x_list(DOT_X_ENUM_STR) }; \
    static enum_name \
    enum_name##_from_string8(String8 str) \
    { \
        x_list(DOT_X_ENUM_VAL_FROM_STR) \
        DOT_ERROR("Invalid enum name"); \
    }

#define DOT_ENUM_REFLECT_TYPED(T, enum_name, x_list) \
    typedef T enum_name; \
    enum {x_list(DOT_X_ENUM_ARG) }; \
    static const String8 string8_from_##enum_name[] = { x_list(DOT_X_ENUM_STR) }; \
    static enum_name \
    enum_name##_from_string8(String8 str) \
    { \
        x_list(DOT_X_ENUM_VAL_FROM_STR) \
        DOT_ERROR("Invalid enum name"); \
    }

////////////////////////////////////////////////////////////////
//
// Sane type renames

// size_t
#include <stddef.h>
// precise int
typedef size_t usize;
#include <stdint.h>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef uintptr_t uptr;
typedef ptrdiff_t iptr;

typedef u32 b32;
typedef u16 b16;
typedef u8  b8;

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

#define TO_NSEC(t) ((t)*(u64)1000000000)
#define TO_USEC(t) ((t)*(u64)1000000)
#define TO_MSEC(t) ((t)*(u64)1000)

////////////////////////////////////////////////////////////////
//
// Array

#define DOT_ARRAY_COUNT(arr) sizeof(arr) / sizeof(arr[0])
#define DOT_ARRAY_COUNT_T(T, ...) (sizeof((T[]){__VA_ARGS__}) / sizeof(T))

#define DOT_ARRAY_SPREAD(T, ...) (T[])__VA_ARGS__, (u64) (sizeof((T[]){__VA_ARGS__}) / sizeof(T))
#define DOT_ARRAY_SPREAD_T(T, ...) (sizeof((T[]){__VA_ARGS__}) / sizeof(T)), (T[]){__VA_ARGS__}

// #define ARRAY_FIELDS(T, field, field_count, ...) \
//     .field = { __VA_ARGS__ }, \
//     .field_count = sizeof((T[]){ __VA_ARGS__ }) / sizeof(T)

// Some sugar to make some things a bit more evident
// or to indicate
#define array(T) T*

////////////////////////////////////////////////////////////////
//
// Debug

#if DOT_COMPILER_MSVC
#   define DEBUG_BREAK __debugbreak()
#elif DOT_COMPILER_GCC
#   include <signal.h>
#   define DEBUG_BREAK raise(SIGTRAP)
#elif DOT_COMPILER_CLANG
#   define DEBUG_BREAK __builtin_debugtrap()
#else
#   define DEBUG_BREAK ((void)0) // Fallback: no-op
#endif

////////////////////////////////////////////////////////////////
//
// String concat

#define DOT_STR_HELPER(x) #x
#define DOT_STR(x) DOT_STR_HELPER(x)

#define DOT_CONCAT_2(x, y) x ## y
#define DOT_CONCAT(x, y) DOT_CONCAT_2(x, y)


#define DOT_FILE_LINE (__FILE__ ": " DOT_STR(__LINE__))

////////////////////////////////////////////////////////////////
//
// Source location helper

#define DEBUG_LOC_ARG __FILE__, __LINE__
#define DEBUG_LOC_FMT "%s:%d"

////////////////////////////////////////////////////////////////
//
// Static Debug

#define DOT_STATIC_ASSERT(cond, ...) typedef int DOT_CONCAT(DOT_STATIC_ASSERT, __COUNTER__) [(cond) ? 1 : -1]
// (jd) This is used as a compile time assert expression. Can be composed with other expressions
// when we compile, signifying we didn't assert
#define DOT_STATIC_ASSERT_EXPR(cond, ...) (0 * sizeof(char[(cond) ? 1 : -1]))
#define DOT_STATIC_ASSERT_LITERAL_LEN(s, n) DOT_STATIC_ASSERT_EXPR(sizeof(s) - 1 == (n))

////////////////////////////////////////////////////////////////
//
// Debug utils

#if DOT_COMPILER_MSVC
#   include <sal.h>
#   define DOT_PRINTF_LIKE(fmtpos, argpos) 
#   define DOT_PRINTF_STRING _Printf_format_string_
#elif DOT_COMPILER_GCC || DOT_COMPILER_CLANG
#   define DOT_PRINTF_LIKE(fmtpos, argpos) __attribute__((format(printf, fmtpos, argpos)))
#   define DOT_PRINTF_STRING
#else
#   define DOT_PRINTF_LIKE(fmtpos, argpos)
#   define DOT_PRINTF_STRING
#endif

typedef enum DOT_LogLevelKind{
    DOT_LogLevelKind_Debug,
    DOT_LogLevelKind_Warning,
    DOT_LogLevelKind_Error,
    DOT_LogLevelKind_Assert,
    DOT_LogLevelKind_Count,
}DOT_LogLevelKind;

global const char *dot_log_level_kind_str[] = {
    [DOT_LogLevelKind_Debug]   = "",
    [DOT_LogLevelKind_Warning] = "Warning",
    [DOT_LogLevelKind_Assert]  = "Assertion failed",
    [DOT_LogLevelKind_Error]   = "Error",
};
DOT_STATIC_ASSERT(DOT_LogLevelKind_Count == DOT_ARRAY_COUNT(dot_log_level_kind_str));

typedef struct DOT_PrintDebugParams{
    DOT_LogLevelKind print_debug_kind;
    const char *file;
    u32 line;
}DOT_PrintDebugParams;

// Maybe the loging system should just be tied to a frame arena / thread arena
#define DOT_MAX_LOG_LEVEL_LENGTH 2048

internal void dot_set_log_level(DOT_LogLevelKind log_level);

// This causes -Wformat to jump due to custom format specifiers %S %M %m :(
// DOT_PRINTF_LIKE(2,3)
internal void dot_print_debug_(const DOT_PrintDebugParams* params, DOT_PRINTF_STRING const char *fmt, ...);

#define DOT_PRINT_DEBUG_PARAMS_DEFAULT(...) \
&(DOT_PrintDebugParams) { \
    .print_debug_kind = DOT_LogLevelKind_Debug, \
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

#define DOT_ERROR(...)          DOT_ERROR_IMPL(DOT_PRINT_DEBUG_PARAMS_DEFAULT(.print_debug_kind = DOT_LogLevelKind_Error), __VA_ARGS__)
#define DOT_ERROR_FL(f, l, ...) DOT_ERROR_IMPL(DOT_PRINT_DEBUG_PARAMS_DEFAULT(.print_debug_kind = DOT_LogLevelKind_Error, .file = (f), .line = (l)), __VA_ARGS__)
#define DOT_TODO(msg) \
do { \
    dot_print_debug_(DOT_PRINT_DEBUG_PARAMS_DEFAULT(.print_debug_kind = DOT_LogLevelKind_Error), "TODO: %s", msg); \
    abort(); \
} while(0)

#if DOT_DEBUG
// --- Printing Macros ---
#define DOT_PRINT(...)              dot_print_debug_(DOT_PRINT_DEBUG_PARAMS_DEFAULT(), __VA_ARGS__)
#define DOT_PRINT_FL(f, l, ...)     dot_print_debug_(DOT_PRINT_DEBUG_PARAMS_DEFAULT(.file = (f), .line = (l)), __VA_ARGS__)
#define DOT_WARNING(...)            dot_print_debug_(DOT_PRINT_DEBUG_PARAMS_DEFAULT(.print_debug_kind = DOT_LogLevelKind_Warning), __VA_ARGS__)
#define DOT_WARNING_FL(f, l, ...)   dot_print_debug_(DOT_PRINT_DEBUG_PARAMS_DEFAULT(.file = (f), .line = (l), .print_debug_kind = DOT_LogLevelKind_Warning), __VA_ARGS__)

// --- Assertion Macros ---
// WARN: "##" allows us to not have and fmt but uses an extension
#define DOT_ASSERT_IMPL(cond, params, fmt, ...) \
do { \
    if(!(cond)){ \
        dot_print_debug_(params, "%s " fmt, #cond, ##__VA_ARGS__); \
        DEBUG_BREAK; \
    } \
} while(0)
#define DOT_ASSERT(cond, ...)           DOT_ASSERT_IMPL((cond), DOT_PRINT_DEBUG_PARAMS_DEFAULT(.print_debug_kind = DOT_LogLevelKind_Error), __VA_ARGS__)
#define DOT_ASSERT_FL(cond, f, l, ...)  DOT_ASSERT_IMPL((cond), DOT_PRINT_DEBUG_PARAMS_DEFAULT(.print_debug_kind = DOT_LogLevelKind_Error, .file = (f), .line = (l)), __VA_ARGS__)
#else
#define DOT_PRINT(...) ((void)0)
#define DOT_PRINT_FL(f, l, ...) do{((void)f); ((void)l);}while(0)
#define DOT_WARNING(...) ((void)0)
#define DOT_WARNING_FL(f, l, ...) do{((void)f); ((void)l);}while(0)
#define DOT_ASSERT(...) ((void)0)
#define DOT_ASSERT_FL(cond, f, l, ...) do{((void)f); ((void)l);}while(0)
#endif

////////////////////////////////////////////////////////////////
//
// Cast for easy grep

#define cast(t) (t)

////////////////////////////////////////////////////////////////
//
// B size utils

#define DOT_KB(x) ((x) * (u64)1024)
#define DOT_MB(x) ((DOT_KB(x)) * (u64)1024)
#define DOT_GB(x) ((DOT_MB(x)) * (u64)1024)

////////////////////////////////////////////////////////////////
//
// MATH

#define DOT_MAX(a, b) ((a) > (b) ? (a) : (b))
#define DOT_MAX3(a, b, c) (DOT_MAX(DOT_MAX(a,b), c))
#define DOT_MIN(a, b) ((a) < (b) ? (a) : (b))
#define DOT_MIN3(a, b, c) (DOT_MIN(DOT_MIN(a,b), c))

#define DOT_CLAMP(x, bot, top) (((x) < (bot)) ? (bot) : ((x) > (top)) ? (top) : (x))
#define DOT_CLAMP_BOT(a, b) DOT_MAX(a, b)
#define DOT_CLAMP_TOP(a, b) DOT_MIN(a, b)

#define DOT_CLAMP01(x) (DOT_CLAMP((x),0,1))

#define DOT_ABS(x) ((x) < 0 ? -(x) : (x))

////////////////////////////////////////////////////////////////
//
// Processor hints

#if defined(__GNUC__) || defined(__clang__)
#   define DOT_LIKELY(x) __builtin_expect(!!(x), 1)
#   define DOT_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#   define DOT_LIKELY(x) (x)
#   define DOT_UNLIKELY(x) (x)
#endif

////////////////////////////////////////////////////////////////
//
// Memory

#if DOT_COMPILER_MSVC
#elif DOT_COMPILER_CLANG || DOT_COMPILER_GCC
#else
#endif

// _Alignof
#if DOT_COMPILER_MSVC
#   define DOT_ALIGNOF(T) __alignof(T)
#elif DOT_COMPILER_CLANG || DOT_COMPILER_GCC
#   define DOT_ALIGNOF(T) __alignof__(T)
#else
#   error alignof not defined for this compiler.
#endif

#if (DOT_COMPILER_GCC && __GNUC__ >= 4) || DOT_COMPILER_CLANG
#   define DOT_OFFSETOF(T,member) (__builtin_offsetof(T,member))
#else
#   define DOT_OFFSETOF(T, member) ((usize)&(((T*)0)->member))
#endif

#define DOT_CONTAINER_OF(ptr, type, member) ((type *)((u8 *)(ptr) - offsetof(type, member)))

#if DOT_COMPILER_CLANG || DOT_COMPILER_GCC
#   define TYPEOF(x) __typeof__(x)
#   define alignas(a) __attribute((aligned(a)))

#elif DOT_COMPILER_MSVC
#   define alignas(a) __declspec(align(a))
#   if _MSC_VER >= 1939 && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202000L
    // MSVC 19.39+ (VS 2022 17.9+)
#   define TYPEOF(x) __typeof__(x)
#   else
#   define TYPEOF(x) __typeof__(x)
//#   error "no typeof"

#   endif
#else
    // #define TYPEOF(x) "typeof is not supported on this compiler. Upgrade MSVC to 17.9+ or use GCC/Clang."
    // #error "typeof is not supported on this compiler. Upgrade MSVC to 17.9+ or use GCC/Clang."
#endif

#ifndef DOT_SWAP
#define DOT_SWAP(Type, a, b) do { Type tmp = (a); (a) = (b); (b) = tmp; } while (0)
#endif
#define ALIGN_POW2(x, align)        (((x) + (align) - 1) & (~((align) - 1)))
#define ALIGN_DOWN_POW2(x, align)   ((x) & ~((align) - 1))
#define IS_POW2(x)                  (((x) & ((x) - 1)) == 0)
#define MOD_POW2(x, n)              (((x) & ((n) - 1)))

#define MEMORY_ZERO(s, z)       memset((s), 0, (z))
#define MEMORY_ZERO_STRUCT(s)   MEMORY_ZERO((s), sizeof(*(s)))
#define MEMORY_ZERO_ARRAY(a)    MEMORY_ZERO((a), sizeof(a))
#define MEMORY_ZERO_TYPED(m, c) MEMORY_ZERO((m), sizeof(*(m)) * (c))

#define MEMORY_COMPARE(a, b, size)  memcmp((a), (b), (size))
#define MEMORY_EQUAL(a, b, size)    (MEMORY_COMPARE((a), (b), (size)) == 0)
#define MEMORY_EQUAL_STRUCT(a, b)   (MEMORY_COMPARE((a), (b), sizeof(*(a))) == 0)

// (jd) the comparisson here isn't doing anything. We just let the compiler validate the types
// Note that the -Wcompare-distinct-pointer-types is needed
#define VALIDATE_TYPE(x, T) ((void)((T*)0 == &(x)))

////////////////////////////////////////////////////////////////
//
// Useful Mem copy from raddebugger

#define MEMORY_COPY_NO_ALIAS(dst, src, size)    memcpy((dst), (src), (size))
#define MEMORY_COPY(dst, src, size)             memmove((dst), (src), (size))
#define MEMORY_COPY_STRUCT(dst,src)             MEMORY_COPY((dst),(src),sizeof(*(dst)))
#define MEMORY_COPY_ARRAY(dst,src)              MEMORY_COPY((dst),(src),sizeof(dst))
// #define MemoryCopyTyped(dst,src,c) MEMORY_COPY((dst),(ssrc),sizeof(*(dst))*(c))

////////////////////////////////////////////////////////////////
//
// Removalble debug field names

typedef struct String8 String8;
internal void dot_debug_name_set(u64 capacity, char *ptr, String8 src);

typedef struct DOT_DebugName16{
    char buff[16];
}DOT_DebugName16;

typedef struct DOT_DebugName8{
    char buff[8];
}DOT_DebugName;

#if DOT_DEBUG
#   define DOT_DebugName16(field) DOT_DebugName16 field
#   define DOT_DebugName8(field) DOT_DebugName8 field
#   define DOT_DebugNameSet(field, str8) dot_debug_name_set(sizeof(field.buff), field.buff, str8)
#else
#   define DOT_DebugName16(field)
#   define DOT_DebugName8(field)
#   define DOT_DebugNameSet(buff, str8) (void) str8
#endif


////////////////////////////////////////////////////////////////
//
// Unused

#define DOT_UNUSED(...) (void) sizeof((__VA_ARGS__, 0))

////////////////////////////////////////////////////////////////
//
// Useful it from raddebugger

#define EACH_INDEX(it, count) (u64 it = 0; it < (count); ++it)
#define EACH_ELEMENT(it, array) (u64 it = 0; it < DOT_ARRAY_COUNT(array); ++it)
#define EACH_ENUM_VAL(type, it) \
(type it = (type)0; it < type##_Count; it = (type)(it + 1))
#define EACH_NON_ZERO_ENUM_VAL(type, it) \
(type it = (type)1; it < type##_Count; it = (type)(it + 1))

#define EACH_IN_RANGE(it, range) (u64 it = (range).min; it < (range).max; it += 1)
#define EACH_NODE(it, T, first) (T *it = first; it != 0; it = it->next)

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
// C static init

#if DOT_COMPILER_MSVC
#   pragma section(".CRT$XCU",read)
#   define DOT_CONSTRUCTOR2_(fn, p) \
        static void fn(void); \
        __declspec(allocate(".CRT$XCU")) void (*fn##_)(void) = fn; \
        __pragma(comment(linker,"/include:" p #fn "_")) \
        static void fn(void)
#   define DOT_CONSTRUCTOR(fn) DOT_CONSTRUCTOR2_(fn,"")
#else
#   define DOT_CONSTRUCTOR(fn) \
        __attribute__((constructor)) static void fn(void)
#endif
//
// #if DOT_COMPILER_MSVC
//     typedef void (__cdecl *DOT_CRTINIT)(void);
//
//     #pragma section(".CRT$XCU", read)
//
//     #define DOT_CONSTRUCTOR(fn)                     \
//         static void __cdecl fn(void);               \
//         __declspec(allocate(".CRT$XCU"))            \
//         DOT_CRTINIT fn##_ = fn;                     \
//         static void __cdecl fn(void)
// #else
//     #define DOT_CONSTRUCTOR(fn)                     \
//         __attribute__((constructor)) static void fn(void)
// #endif

////////////////////////////////////////////////////////////////
//
// Custom section
// This requires a custom linker script akin to
//
// // plugins_section.ld
// SECTIONS
// {
//   .DOT_plugins : {
//     PROVIDE_HIDDEN(__start_DOT_plugins = .);
//     KEEP(*(SORT_BY_INIT_PRIORITY(.DOT_plugins.*)))
//     PROVIDE_HIDDEN(__stop_DOT_plugins = .);
//   }
// } INSERT AFTER .data;

// (jd) NOTE: This need to be placed in manually for the section we want above declare section
// #pragma section(".#name", read)
// #pragma comment(linker, "/merge:.#name=.data")

#if DOT_COMPILER_MSVC
// For some reason #name "$a" doesn't work with __declspec(allocate)
#   define SECTION_NAME_JOIN(a, b) DOT_STR(a##b)
// (jd) NOTE: `extern T __start_##name[]` never actually defines storage for the symbol (it's just
// a declaration), so the linker had nothing to resolve it to. Instead we place one real, zeroed
// T-sized marker at each end of the section ($a and $z) and derive __start_/__stop_ from their
// addresses.
//
// (jd) NOTE: Each `__declspec(allocate(...))` variable becomes its own COMDAT, and MSVC pads
// every COMDAT in one of these custom sections up to 16 bytes. That means consecutive items in
// "$m" actually sit `DOT_ALIGN_UP(sizeof(T), 16)` apart, not `sizeof(T)` apart. Iteration (see
// EACH_IN_SECTION) walks the array with `sizeof(T)`-strided pointer arithmetic, so T must already
// be a multiple of 16 bytes or the walk silently drifts off the real items. Use `alignas(16)` on T
// if this fires.
#   define SECTION_DECLARE(T, name) \
        DOT_STATIC_ASSERT(sizeof(T) % 16 == 0, "T must be 16-byte sized for MSVC section iteration to stay aligned"); \
        __pragma(data_seg(#name "$a")) \
        __pragma(data_seg(#name "$m")) \
        __pragma(data_seg(#name "$z")) \
        __pragma(data_seg()) \
        __declspec(allocate(SECTION_NAME_JOIN(name, $a))) static T DOT_CONCAT(__dot_section_begin_, name) = {0}; \
        __declspec(allocate(SECTION_NAME_JOIN(name, $z))) static T DOT_CONCAT(__dot_section_end_, name) = {0}; \
        static T *__start_##name = &DOT_CONCAT(__dot_section_begin_, name) + 1; \
        static T *__stop_##name  = &DOT_CONCAT(__dot_section_end_, name)

// priority doesn't work on windows. Nothing depends on it for now tho
#   define SECTION_ITEM(name, priority) \
        __declspec(allocate(SECTION_NAME_JOIN(name, $m)))
#else
#   define SECTION_DECLARE(T, name) \
        extern T __start_##name[]; \
        extern T __stop_##name[];

#   define SECTION_ITEM(name, priority) \
        DOT_STATIC_ASSERT(priority == 0, "priority != 0 doesn't work on windows"); \
        __attribute__((no_sanitize("address"), used, section("." #name "." #priority)))
#endif

#define REGISTER_IN_SECTION(name, type, symbol, ...) \
    SECTION_ITEM(name) static const type symbol = (type){ __VA_ARGS__ }

#define EACH_IN_SECTION(section_name, T, it) \
    (T *it = __start_##section_name; (it) < __stop_##section_name; (it)++)

// NOTE: revisit atomics, also grabbed this from raddbg for the stracktrace thing
#if DOT_COMPILER_MSVC
#   include <intrin.h>
#   if DOT_ARCH_X64
#       define ins_atomic_u32_eval_cond_assign(x,k,c)  _InterlockedCompareExchange((long *)(x), (k), (c))
#   else
#       error "Atomic intrinsics not defined for this compiler / architecture combination."
#   endif
#elif DOT_COMPILER_CLANG || DOT_COMPILER_GCC
#   include <immintrin.h>
#   include <emmintrin.h>
#   define ins_atomic_u32_eval_cond_assign(x,k,c)  ({ u32 _new = (c); __atomic_compare_exchange_n((u32 *)(x),&_new,(k),0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST); _new; })
#else
#    error "Atomic intrinsics not defined for this compiler / architecture."
#endif

////////////////////////////////////////////////////////////////
///
// Static hash
//

#define FNV1A_OFFSET 2166136261u
#define FNV1A_PRIME  16777619u

#define DOT_STATIC_ASSERT_EXPR(cond, ...) (0 * sizeof(char[(cond) ? 1 : -1]))
#define DOT_STATIC_ASSERT_LITERAL_LEN(s, n) DOT_STATIC_ASSERT_EXPR(sizeof(s) - 1 == (n))

#define DOT_FNV1A_STEP(h, c) ((h ^ (unsigned char)(c)) * FNV1A_PRIME)

#define DOT_FNV1A_1_UNCHECKED(s)  DOT_FNV1A_STEP(FNV1A_OFFSET, s[0])
#define DOT_FNV1A_2_UNCHECKED(s)  DOT_FNV1A_STEP(DOT_FNV1A_1_UNCHECKED(s), s[1])
#define DOT_FNV1A_3_UNCHECKED(s)  DOT_FNV1A_STEP(DOT_FNV1A_2_UNCHECKED(s), s[2])
#define DOT_FNV1A_4_UNCHECKED(s)  DOT_FNV1A_STEP(DOT_FNV1A_3_UNCHECKED(s), s[3])
#define DOT_FNV1A_5_UNCHECKED(s)  DOT_FNV1A_STEP(DOT_FNV1A_4_UNCHECKED(s), s[4])
#define DOT_FNV1A_6_UNCHECKED(s)  DOT_FNV1A_STEP(DOT_FNV1A_5_UNCHECKED(s), s[5])
#define DOT_FNV1A_7_UNCHECKED(s)  DOT_FNV1A_STEP(DOT_FNV1A_6_UNCHECKED(s), s[6])
#define DOT_FNV1A_8_UNCHECKED(s)  DOT_FNV1A_STEP(DOT_FNV1A_7_UNCHECKED(s), s[7])

#if DOT_COMPILER_MSVC
#define DOT_FNV1A_1(s)  DOT_FNV1A_1_UNCHECKED(s)
#define DOT_FNV1A_2(s)  DOT_FNV1A_2_UNCHECKED(s)
#define DOT_FNV1A_3(s)  DOT_FNV1A_3_UNCHECKED(s)
#define DOT_FNV1A_4(s)  DOT_FNV1A_4_UNCHECKED(s)
#define DOT_FNV1A_5(s)  DOT_FNV1A_5_UNCHECKED(s)
#define DOT_FNV1A_6(s)  DOT_FNV1A_6_UNCHECKED(s)
#define DOT_FNV1A_7(s)  DOT_FNV1A_7_UNCHECKED(s)
#define DOT_FNV1A_8(s)  DOT_FNV1A_8_UNCHECKED(s)
#else
#define DOT_FNV1A_1(s)  (DOT_STATIC_ASSERT_LITERAL_LEN(s,1), DOT_FNV1A_1_UNCHECKED(s))
#define DOT_FNV1A_2(s)  (DOT_STATIC_ASSERT_LITERAL_LEN(s,2), DOT_FNV1A_2_UNCHECKED(s))
#define DOT_FNV1A_3(s)  (DOT_STATIC_ASSERT_LITERAL_LEN(s,3), DOT_FNV1A_3_UNCHECKED(s))
#define DOT_FNV1A_4(s)  (DOT_STATIC_ASSERT_LITERAL_LEN(s,4), DOT_FNV1A_4_UNCHECKED(s))
#define DOT_FNV1A_5(s)  (DOT_STATIC_ASSERT_LITERAL_LEN(s,5), DOT_FNV1A_5_UNCHECKED(s))
#define DOT_FNV1A_6(s)  (DOT_STATIC_ASSERT_LITERAL_LEN(s,6), DOT_FNV1A_6_UNCHECKED(s))
#define DOT_FNV1A_7(s)  (DOT_STATIC_ASSERT_LITERAL_LEN(s,7), DOT_FNV1A_7_UNCHECKED(s))
#define DOT_FNV1A_8(s)  (DOT_STATIC_ASSERT_LITERAL_LEN(s,8), DOT_FNV1A_8_UNCHECKED(s))
#endif

static inline u32 fnv1a_runtime(String8 s);

#endif // !DOT_H
