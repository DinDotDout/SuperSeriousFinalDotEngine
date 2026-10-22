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
#include <float.h>

#include "base/base_types.h"

#if DOT_COMPILER_CLANG || (DOT_COMPILER_GCC && __GNUC__ >= 7)
#   define DOT_FALLTHROUGH __attribute__((fallthrough))
#else
#   define DOT_FALLTHROUGH /* fallthrough */
#endif

#if DOT_COMPILER_CLANG
#   define DOT_ALLOW_PARTIAL_SWITCH \
        _Pragma("clang diagnostic push") \
        _Pragma("clang diagnostic ignored \"-Wswitch-enum\"") \
        _Pragma("clang diagnostic ignored \"-Wswitch\"")

#   define DOT_RESTORE_PARTIAL_SWITCH \
        _Pragma("clang diagnostic pop")

#elif DOT_COMPILER_GCC
#   define DOT_ALLOW_PARTIAL_SWITCH \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Wswitch-enum\"")
        // _Pragma("GCC diagnostic ignored \"-Wswitch\"")

#   define DOT_RESTORE_PARTIAL_SWITCH _Pragma("GCC diagnostic pop")
#elif DOT_COMPILER_MSVC
// 4061: enumerator not handled by a case label (switch has default)
// 4062: enumerator not handled by a case label (switch has no default)
// 4063: case label is not a valid value of the switched enum
// 4065: switch has a default but no case labels
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
#   define force_inline __forceinline
#   define COMPILER_RESET(ptr) __assume(ptr)
#elif DOT_COMPILER_GCC || DOT_COMPILER_CLANG
#   define force_inline inline __attribute__((always_inline))
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
#define DOT_X_ENUM_STR(prefix, v)           str_lit(#v),
#define DOT_X_ENUM_VAL_FROM_STR(prefix, v)  if(string8_equal(string8_from_##prefix[prefix##_##v], str)) { return prefix##_##v; }
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

// Implemented per-platform in os_linux.c / os_windows.c (included later in this
// translation unit via base_include.h); forward declared here so DOT_ASSERT can
// share the same fast stacktrace path as the fatal signal handler.
internal void os_print_stacktrace(void);

////////////////////////////////////////////////////////////////
//
// String concat

#define DOT_STR_HELPER(x) #x
#define DOT_STR(x) DOT_STR_HELPER(x)

#define DOT_CONCAT_2(x, y) x ## y
#define DOT_CONCAT(x, y) DOT_CONCAT_2(x, y)

////////////////////////////////////////////////////////////////
//
// Safe #if without #ifdef
// defined or 1 evaluates to true, undefined or 0 evaluates to false
// Can also be used in regular if statements

#define DOT_ENABLED_PLACEHOLDER_ 0,
#define DOT_ENABLED_PLACEHOLDER_1 0,
#define DOT_ENABLED_SECOND(ignored, value, ...) value
#define DOT_ENABLED_PICK(arg_or_junk) DOT_ENABLED_SECOND(arg_or_junk 1, 0, 0)
#define DOT_ENABLED_EXPAND(value) DOT_ENABLED_PICK(DOT_ENABLED_PLACEHOLDER_##value)
#define DOT_ENABLED(macro) DOT_ENABLED_EXPAND(macro)

#define DOT_FILE_LINE (__FILE__ ": " DOT_STR(__LINE__))

////////////////////////////////////////////////////////////////
//
// Source location helper

#define DEBUG_LOC_ARG __FILE__, __LINE__
#define DEBUG_LOC_FMT "%s:%d"

////////////////////////////////////////////////////////////////
//
// Static Debug

// (jd) DOT_STATIC_ASSERT_EXPR is used as a compile time assert expression. Can be composed with other expressions
# if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#   define DOT_STATIC_ASSERT(cond, ...)  _Static_assert(cond, "" __VA_ARGS__)
#   define DOT_STATIC_ASSERT_EXPR(cond, ...) (0 * sizeof(struct { DOT_STATIC_ASSERT(cond, __VA_ARGS__); char _dummy; }))
# else
#   define DOT_STATIC_ASSERT(cond, ...) typedef int DOT_CONCAT(DOT_STATIC_ASSERT, __COUNTER__) [(cond) ? 1 : -1]
#   define DOT_STATIC_ASSERT_EXPR(cond, ...) (0 * sizeof(char[(cond) ? 1 : -1]))
# endif
#define DOT_STATIC_ASSERT_LITERAL_LEN(s, n) DOT_STATIC_ASSERT_EXPR(sizeof(s) - 1 == (n), "string literal length is not " #n)
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#   define DOT_STATIC_ASSERT_TYPE(x, T, ...) DOT_STATIC_ASSERT_EXPR(_Generic((x), T: 1, default: 0), __VA_ARGS__)
#elif DOT_COMPILER_CLANG || DOT_COMPILER_GCC
#   define DOT_STATIC_ASSERT_TYPE(x, T, ...) DOT_STATIC_ASSERT_EXPR(__builtin_types_compatible_p(TYPEOF(x), T), __VA_ARGS__)
#else
#   define DOT_STATIC_ASSERT_TYPE(x, T, ...) 0
#endif

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
&(DOT_PrintDebugParams){ \
    .print_debug_kind = DOT_LogLevelKind_Debug, \
    .file = __FILE__, \
    .line = __LINE__, \
    __VA_ARGS__ \
}

// --- Error Macros ---
#define DOT_ERROR_IMPL(params, fmt, ...) \
do{ \
    dot_print_debug_(params, fmt, ##__VA_ARGS__); \
    DEBUG_BREAK; \
    abort(); \
}while(0)

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
        os_print_stacktrace(); \
        DEBUG_BREAK; \
    } \
} while(0)
#define DOT_ASSERT(cond, ...)           DOT_ASSERT_IMPL((cond), DOT_PRINT_DEBUG_PARAMS_DEFAULT(.print_debug_kind = DOT_LogLevelKind_Error), "" __VA_ARGS__)
#define DOT_ASSERT_FL(cond, f, l, ...)  DOT_ASSERT_IMPL((cond), DOT_PRINT_DEBUG_PARAMS_DEFAULT(.print_debug_kind = DOT_LogLevelKind_Error, .file = (f), .line = (l)), "" __VA_ARGS__)
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
#   if _MSC_VER >= 1939
    // MSVC 19.39+ (VS 2022 17.9+) accepts __typeof__ in every C mode
#   define TYPEOF(x) __typeof__(x)
#   else
#   error "typeof is not supported on this compiler. Upgrade MSVC to 17.9+ or use GCC/Clang."
#   endif
#else
#   error "typeof is not supported on this compiler. Upgrade MSVC to 17.9+ or use GCC/Clang."
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
#define MEMORY_COPY_STRUCT(dst,src)             MEMORY_COPY_NO_ALIAS((dst),(src),sizeof(*(dst)))
#define MEMORY_COPY_ARRAY(dst,src)              MEMORY_COPY((dst),(src),sizeof(dst))
// #define MemoryCopyTyped(dst,src,c) MEMORY_COPY((dst),(ssrc),sizeof(*(dst))*(c))

////////////////////////////////////////////////////////////////
//
// Removalble debug field names

internal void dot_debug_name_set(u64 capacity, char *ptr, String8 src);

typedef struct DOT_DebugName16{
    char buff[16];
}DOT_DebugName16;

typedef struct DOT_DebugName8{
    char buff[8];
}DOT_DebugName;

#if DOT_DEBUG
#   define DOT_DebugName32(field) DOT_DebugName32 field;
#   define DOT_DebugName16(field) DOT_DebugName16 field;
#   define DOT_DebugName8(field) DOT_DebugName8 field;
#   define DOT_DebugNameSet(field, str8) dot_debug_name_set(sizeof(field.buff), field.buff, str8)
#   define DOT_DebugNameGet(field) string8_from_cstring((field).buff)
#else
#   define DOT_DebugName32(field)
#   define DOT_DebugName16(field)
#   define DOT_DebugName8(field)
#   define DOT_DebugNameSet(buff, str8) (void) str8
#   define DOT_DebugNameGet(field) ((String8){0})
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

////////////////////////////////////////////////////////////////
//
// Custom section

#if DOT_OS_WINDOWS
// For some reason #name "$a" doesn't work with __declspec(allocate)
#   define SECTION_NAME_JOIN(a, b) DOT_STR(a##b)
#   define SECTION_DECLARE(T, name) \
        DOT_STATIC_ASSERT(sizeof(T) % 16 == 0, "T must be 16-byte sized for section iteration to stay aligned, add alignas(16) to T"); \
        __pragma(data_seg(#name "$a")) \
        __pragma(data_seg(#name "$m")) \
        __pragma(data_seg(#name "$z")) \
        __pragma(data_seg()) \
        __pragma(comment(linker, "/merge:" #name "=.data")) \
        __declspec(allocate(SECTION_NAME_JOIN(name, $a))) static T DOT_CONCAT(__dot_section_begin_, name) = {0}; \
        __declspec(allocate(SECTION_NAME_JOIN(name, $z))) static T DOT_CONCAT(__dot_section_end_, name) = {0}; \
        static T *__start_##name = &DOT_CONCAT(__dot_section_begin_, name) + 1; \
        static T *__stop_##name  = &DOT_CONCAT(__dot_section_end_, name)

#   if DOT_COMPILER_MSVC
#       define SECTION_ITEM(name) \
            __declspec(allocate(SECTION_NAME_JOIN(name, $m)))
#   else
#       define SECTION_ITEM(name) \
            __declspec(allocate(SECTION_NAME_JOIN(name, $m))) __attribute__((no_sanitize("address"), used))
#   endif
#else
#   define SECTION_DECLARE(T, name) \
        DOT_STATIC_ASSERT(sizeof(T) % 16 == 0, "T must be 16-byte sized for section iteration to stay aligned, add alignas(16) to T"); \
        extern T __start_##name[] __attribute__((weak)), __stop_##name[] __attribute__((weak))

#   define SECTION_ITEM(name) \
        __attribute__((no_sanitize("address"), used, retain, section(#name)))
#endif

#define REGISTER_IN_SECTION(name, type, symbol, ...) \
    SECTION_ITEM(name) static const type symbol = (type){ __VA_ARGS__ }

#define EACH_IN_SECTION(section_name, T, it) \
    (T *it = __start_##section_name; (it) < __stop_##section_name; (it)++)

#define SECTION_COUNT(section_name) cast(u64)(__stop_##section_name - __start_##section_name)

// NOTE: revisit atomics, also grabbed this from raddbg for the stracktrace thing
#if DOT_COMPILER_MSVC
#   include <intrin.h>
#   if DOT_ARCH_X64
#       define ins_atomic_u32_eval_cond_assign(x,k,c)  _InterlockedCompareExchange((long *)(x), (k), (c))
#       define ins_atomic_u64_add_eval(x,c)            ((u64)_InterlockedExchangeAdd64((__int64 *)(x), (__int64)(c)) + (c))
#       define ins_atomic_i64_add_eval(x,c)            ((i64)_InterlockedExchangeAdd64((__int64 *)(x), (__int64)(c)) + (c))
#   else
#       error "Atomic intrinsics not defined for this compiler / architecture combination."
#   endif
#elif DOT_COMPILER_CLANG || DOT_COMPILER_GCC
#   include <immintrin.h>
#   include <emmintrin.h>
#   define ins_atomic_u32_eval_cond_assign(x,k,c)  ({ u32 _new = (c); __atomic_compare_exchange_n((u32 *)(x),&_new,(k),0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST); _new; })
#   define ins_atomic_u64_add_eval(x,c)            __atomic_add_fetch((u64 *)(x), (c), __ATOMIC_SEQ_CST)
#   define ins_atomic_i64_add_eval(x,c)            __atomic_add_fetch((i64 *)(x), (c), __ATOMIC_SEQ_CST)
#else
#    error "Atomic intrinsics not defined for this compiler / architecture."
#endif

#endif // !DOT_H
