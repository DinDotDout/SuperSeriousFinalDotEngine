#ifndef DOT
#define DOT
////////////////////////////////////////////////////////////////
//
// Needed headers
//
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



////////////////////////////////////////////////////////////////
//
// Compiler
//
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
//

#if defined(_WIN32)
#define DOT_OS_WIN32
#else
#define DOT_OS_POSIX
#endif

////////////////////////////////////////////////////////////////
//
// Nice qualifiers
//
#define internal static
#define local_persist static
#define global static

////////////////////////////////////////////////////////////////
//
// Sane type renames
//
// size
#include <stddef.h>
// precise int
#include <stdint.h>
typedef uintptr_t uptr;
typedef size_t usize;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef u32 b32;
typedef u8 b8;
typedef float f32;
typedef double f64;

////////////////////////////////////////////////////////////////
//
// Array
//
#define ArrayCount(arr) sizeof(arr) / sizeof(arr[0])
#define array(T) T*

////////////////////////////////////////////////////////////////
//
// Debug
//
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
//
#define DOT_STR_HELPER(x) #x
#define DOT_STR(x) DOT_STR_HELPER(x)

#define DOT_CONCAT2(x, y) x ## y
#define DOT_CONCAT(x, y) DOT_CONCAT2(x, y)

////////////////////////////////////////////////////////////////
//
// Source location helper
//
#define DEBUG_LOC_ARG __FILE__, __LINE__
#define DEBUG_LOC_FMT "%s:%d"


////////////////////////////////////////////////////////////////
//
// Static Debug
//
#define DOT_STATIC_ASSERT(x) \
typedef int DOT_CONCAT(DOT_STATIC_ASSERT_, __COUNTER__) [(x) ? 1 : -1]

////////////////////////////////////////////////////////////////
//
// Debug utils
//

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

typedef enum LogLevelKind{
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ASSERT,
    LOG_LEVEL_COUNT,
}LogLevelKind;

global const char* print_debug_str[] = {
    [LOG_LEVEL_DEBUG]     = "",
    [LOG_LEVEL_ASSERT]    = "Assertion failed",
    [LOG_LEVEL_ERROR]     = "Error",
    [LOG_LEVEL_WARNING]   = "Warning",
};

DOT_STATIC_ASSERT(LOG_LEVEL_COUNT == ArrayCount(print_debug_str));

typedef struct PrintDebugParams{
    LogLevelKind print_debug_kind;
    const char* file;
    u32 line;
}PrintDebugParams;


internal inline void PrintDebug(const PrintDebugParams* params, PRINTF_STRING const char* fmt, ...) PRINTF_LIKE(2, 3);

#define PrintDebugParamsDefault(...) \
&(PrintDebugParams) { \
    .print_debug_kind = LOG_LEVEL_DEBUG, \
    .file = __FILE__, \
    .line = __LINE__, \
    __VA_ARGS__}

// --- Error Macros ---
#define DOT_ERROR_IMPL(params, ...) \
do { \
    PrintDebug(params, __VA_ARGS__); \
    DEBUG_BREAK; \
    abort(); \
} while(0)

#define DOT_ERROR(...) DOT_ERROR_IMPL(PrintDebugParamsDefault(.print_debug_kind = LOG_LEVEL_ERROR), __VA_ARGS__)
#define DOT_ERROR_FL(f, l, ...) DOT_ERROR_IMPL(PrintDebugParamsDefault(.print_debug_kind = LOG_LEVEL_ERROR, .file = (f), .line = (l)), __VA_ARGS__)
#define TODO(msg) \
do { \
    PrintDebug(PrintDebugParamsDefault(.print_debug_kind = LOG_LEVEL_ERROR), "TODO: %s", msg); \
    abort(); \
} while(0)

#ifndef NDEBUG
// --- Printing Macros ---
#define DOT_PRINT(...) PrintDebug(PrintDebugParamsDefault(), __VA_ARGS__)
#define DOT_PRINT_FL(f, l, ...) PrintDebug(PrintDebugParamsDefault(.file = (f), .line = (l)), __VA_ARGS__)
#define DOT_WARNING(...) PrintDebug(PrintDebugParamsDefault(.print_debug_kind = LOG_LEVEL_WARNING), __VA_ARGS__)
#define DOT_WARNING_FL(f, l, ...) PrintDebug(PrintDebugParamsDefault(.file = (f), .line = (l), .print_debug_kind = LOG_LEVEL_WARNING), __VA_ARGS__)
// --- Assertion Macros ---
// WARN: "##" allows us to not have and fmt but uses an extension
#define DOT_ASSERT_IMPL(cond, params, fmt, ...) \
do { \
    if(!(cond)){ \
        PrintDebug(params, "%s " fmt, #cond, ##__VA_ARGS__); \
        DEBUG_BREAK; \
    } \
} while(0)
#define DOT_ASSERT(cond, ...) DOT_ASSERT_IMPL((cond), PrintDebugParamsDefault(.print_debug_kind = LOG_LEVEL_ASSERT), __VA_ARGS__)
#define DOT_ASSERT_FL(cond, f, l, ...) DOT_ASSERT_IMPL((cond), PrintDebugParamsDefault(.print_debug_kind = LOG_LEVEL_ASSERT, .file = f, .line = l), __VA_ARGS__)
#else
#define DOT_PRINT(...) ((void)0)
#define DOT_PRINT_FL(f, l, ...) ((void)0)
#define DOT_WARNING(...) ((void)0)
#define DOT_WARNING_FL(f, l, ...) ((void)0)
#define DOT_ASSERT(...) ((void)0)
#define DOT_ASSERT_FL(...) ((void)0)
#endif

internal inline const char* PrintDebugKind_GetString(LogLevelKind debug_kind){
    DOT_ASSERT(debug_kind < LOG_LEVEL_COUNT);
    const char * ret = print_debug_str[debug_kind];
    DOT_ASSERT(ret);
    return ret;
}

#define DOT_MAX_LOG_LEVEL_LENGTH 128
internal inline void PrintDebug(const PrintDebugParams* params, const char* fmt, ...){
    char buf[DOT_MAX_LOG_LEVEL_LENGTH];
    FILE* out = params->print_debug_kind == LOG_LEVEL_DEBUG ? stdout : stderr;

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args); // TODO: Swap for stb_vsntprintf
    va_end(args);

    const char* fmt_str = params->print_debug_kind == LOG_LEVEL_DEBUG ? "%s%s:%d -> %s\n" : "%s: %s:%d -> %s\n";
    fprintf(out, fmt_str,
        PrintDebugKind_GetString(params->print_debug_kind),
        params->file,
        params->line,
        buf);
}

////////////////////////////////////////////////////////////////
//
// Cast for easy cast grep
//
#define cast(t) (t)

////////////////////////////////////////////////////////////////
//
// B size utils
//
#define KB(x) ((x) * (u64)1024)
#define MB(x) ((KB(x)) * (u64)1024)
#define GB(x) ((MB(x)) * (u64)1024)


#define Max(a, b) ((a) > (b) ? (a) : (b))
#define Min(a, b) ((a) < (b) ? (a) : (b))

////////////////////////////////////////////////////////////////
//
// Processor hints
//
#if defined(__GNUC__) || defined(__clang__)
#define DOT_Likely(x) __builtin_expect(!!(x), 1)
#define DOT_Unlikely(x) __builtin_expect(!!(x), 0)
#else
#define Dot_Likely(x) (x)
#define Dot_Unlikely(x) (x)
#endif


////////////////////////////////////////////////////////////////
//
// Memory
//
//
#if defined(DOT_COMPILER_MSVC)
#define alignof(T) __alignof(T)
#elif defined(DOT_COMPILER_CLANG)
#define alignof(T) __alignof__(T)
#elif DOT_COMPILER_GCC
#define alignof(T) __alignof__(T)
#else
#error alignof not defined for this compiler.
#endif

#define AlignPow2(x, b) (((x) + (b) - 1) & (~((b) - 1)))

#define MemoryZero(s, z) memset((s), 0, (z))
#define MemoryZeroStruct(s) MemoryZero((s), sizeof(*(s)))
#define MemoryZeroArray(a) MemoryZero((a), sizeof(a))
#define MemoryZeroTyped(m, c) MemoryZero((m), sizeof(*(m)) * (c))

// Keep this around for when using sorting functions
#if defined(DOT_COMPILER_MSVC)
#define force_inline __forceinline __declspec(safebuffers)
#define CompilerReset(ptr) __assume(ptr)
#elif defined(DOT_COMPILER_GCC) || defined(DOT_DOMPILER_CLANG)
#define force_inline __attribute__((always_inline))
#define CompilerReset(ptr)
#endif

////////////////////////////////////////////////////////////////
//
// String
//
// TODO: Must implement!!!

// This will be used when String8 size matches on string8 compare
#define MemoryCompare(a, b, size) memcmp((a), (b), (size))


#define StrFmt "%.*s"
#define StrArg(sv) (int)(sv).len, (sv).buff

#define MIN_STRING8_SIZE

#define Str8Lit(s)  str8((u8*)(s), sizeof(s) - 1)
#define Str8Comp(s)  (String8){(u8*)(s), sizeof(s) - 1}

typedef struct String8 {
    u8 *str;
    u64 size;
} String8;

typedef struct String8Node String8Node;
struct String8Node {
    String8Node *next;
    String8 str;
};

// Impl should be a collapsable string list into a regular String8
typedef struct StringBuilder {
    String8Node *head;
    String8Node *tail;
    u32 *count;
} StringBuilder;

internal inline String8
Str8(u8 *str, u64 size){
    String8 result = {str, size};
    return(result);
}

// This is good enough for now
internal inline u64 HashFromString8(String8 string, u64 seed){
    u64 result = seed; // raddebugger uses 5381
    for(u64 i = 0; i < string.size; ++i){
        result = ((result << 5) + result) + string.str[i];
    }
    return result;
}

////////////////////////////////////////////////////////////////
//
// whatever
//
#define Unused(something) (void)something

////////////////////////////////////////////////////////////////
//
// Useful it from raddebugger
//
#define EachIndex(it, count) (u64 it = 0; it < (count); ++it)
#define EachElement(it, array) (u64 it = 0; it < ArrayCount(array); ++it)
#define EachEnumVal(type, it) \
(type it = (type)0; it < type##_COUNT; it = (type)(it + 1))
#define EachNonZeroEnumVal(type, it) \
(type it = (type)1; it < type##_COUNT; it = (type)(it + 1))
#define EachInRange(it, range) (u64 it = (range).min; it < (range).max; it += 1)
#define EachNode(it, T, first) (T *it = first; it != 0; it = it->next)


////////////////////////////////////////////////////////////////
//
// Useful Mem copy from raddebugger
//
#define MemoryCopy(dst, src, size)    memmove((dst), (src), (size))
#define MemoryCopyStruct(d,s)  MemoryCopy((d),(s),sizeof(*(d)))
#define MemoryCopyArray(d,s)   MemoryCopy((d),(s),sizeof(d))
#define MemoryCopyTyped(d,s,c) MemoryCopy((d),(s),sizeof(*(d))*(c))
#define MemoryCopyStr8(dst, s) MemoryCopy(dst, (s).str, (s).size)


////////////////////////////////////////////////////////////////
//
// Defer for profile blocks, lock/unlock...
// This accepts expressions that will run before and after a scope block once
#define DeferLoop(before, after) \
        for(int _once_defer_ = 0; _once_defer_ == 0;) \
                for(before; _once_defer_++ == 0; after)


#define DeferLoopCond(before, cond, after) \
        for(int _once_cond_defer_ = 0; _once_cond_defer_ == 0;) \
                for(before; _once_cond_defer_++ == 0 && (cond); after)

////////////////////////////////////////////////////////////////
//
// Threading
//
#if defined(DOT_COMPILER_MSVC)
#define thread_local __declspec(thread)
#elif defined(DOT_COMPILER_GCC) || defined(DOT_COMPILER_CLANG)
#define thread_local __thread
#else
#error "No thread-local storage keyword available for this compiler"
#endif

#endif // !DOT

