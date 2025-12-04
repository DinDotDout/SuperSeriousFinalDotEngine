#ifndef DOT
#define DOT

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
// Needed headers
//
#include <stdbool.h>
// fprintf, FILE
#include <stdio.h>
// abort
#include <stdlib.h>
// memset
#include <string.h>
// va_start, va_end
#include <stdarg.h>

////////////////////////////////////////////////////////////////
//
// Nice qualifiers
//
#define internal static
#define local_persist static
#define global static

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

#define DOT_CONCAT(x, y) x ## y
#define DOT_CONCAT_EXPAND(x, y) DOT_CONCAT(x, y)

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
typedef int DOT_CONCAT_EXPAND(DOT_STATIC_ASSERT_, __COUNTER__) [(x) ? 1 : -1]

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

typedef enum PrintDebugKind{
    PRINT_DEBUG_DEBUG,
    PRINT_DEBUG_ASSERT,
    PRINT_DEBUG_ERROR,
    PRINT_DEBUG_WARNING,
    PRINT_DEBUG_COUNT,
}PrintDebugKind;

global const char* print_debug_str[] = {
    [PRINT_DEBUG_DEBUG]     = "",
    [PRINT_DEBUG_ASSERT]    = "Assertion failed",
    [PRINT_DEBUG_ERROR]     = "Error",
    [PRINT_DEBUG_WARNING]   = "Warning",
};
DOT_STATIC_ASSERT(PRINT_DEBUG_COUNT == ArrayCount(print_debug_str));

typedef struct PrintDebugParams{
    PrintDebugKind print_debug_kind;
    const char* file;
    u32 line;
}PrintDebugParams;


internal inline void PrintDebug(const PrintDebugParams* params, PRINTF_STRING const char* fmt, ...) PRINTF_LIKE(2, 3);

#define PrintDebugParamsDefault(...) \
&(PrintDebugParams) { \
    .print_debug_kind = PRINT_DEBUG_DEBUG, \
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

#define DOT_ERROR(...) DOT_ERROR_IMPL(PrintDebugParamsDefault(.print_debug_kind = PRINT_DEBUG_ERROR), __VA_ARGS__)
#define DOT_ERROR_FL(f, l, ...) DOT_ERROR_IMPL(PrintDebugParamsDefault(.print_debug_kind = PRINT_DEBUG_ERROR, .file = (f), .line = (l)), __VA_ARGS__)
#define TODO(msg) \
do { \
    PrintDebug(PrintDebugParamsDefault(.print_debug_kind = PRINT_DEBUG_ERROR), "TODO: %s", msg); \
    abort(); \
} while(0)

#ifndef NDEBUG
// --- Printing Macros ---
#define DOT_PRINT(...) PrintDebug(PrintDebugParamsDefault(), __VA_ARGS__)
#define DOT_PRINT_FL(f, l, ...) PrintDebug(PrintDebugParamsDefault(.file = (f), .line = (l)), __VA_ARGS__)
#define DOT_WARNING(...) PrintDebug(PrintDebugParamsDefault(.print_debug_kind = PRINT_DEBUG_WARNING), __VA_ARGS__)
#define DOT_WARNING_FL(f, l, ...) PrintDebug(PrintDebugParamsDefault(.file = (f), .line = (l), .print_debug_kind = PRINT_DEBUG_WARNING), __VA_ARGS__)
// --- Assertion Macros ---
// WARN: "##" allows us to not have and fmt but uses an extension
#define DOT_ASSERT_IMPL(cond, params, fmt, ...) \
do { \
    if (!(cond)) { \
        PrintDebug(params, "%s " fmt, #cond, ##__VA_ARGS__); \
        DEBUG_BREAK; \
    } \
} while(0)
#define DOT_ASSERT(cond, ...) DOT_ASSERT_IMPL((cond), PrintDebugParamsDefault(.print_debug_kind = PRINT_DEBUG_ASSERT), __VA_ARGS__)
#define DOT_ASSERT_FL(cond, f, l, ...) DOT_ASSERT_IMPL((cond), PrintDebugParamsDefault(.print_debug_kind = PRINT_DEBUG_ASSERT, .file = f, .line = l), __VA_ARGS__)
#else
#define DOT_PRINT(...) ((void)0)
#define DOT_PRINT_FL(f, l, ...) ((void)0)
#define DOT_WARNING(...) ((void)0)
#define DOT_WARNING_FL(f, l, ...) ((void)0)
#define DOT_ASSERT(...) ((void)0)
#define DOT_ASSERT_FL(...) ((void)0)
#endif


internal inline const char* PrintDebugKind_GetString(PrintDebugKind debug_kind){
    DOT_ASSERT(debug_kind < PRINT_DEBUG_COUNT);
    const char * ret = print_debug_str[debug_kind];
    DOT_ASSERT(ret);
    return ret;
}

#define DOT_MAX_PRINT_DEBUG_LENGTH 128
internal inline void PrintDebug(const PrintDebugParams* params, const char* fmt, ...){
    char buf[DOT_MAX_PRINT_DEBUG_LENGTH];
    FILE* out = params->print_debug_kind == PRINT_DEBUG_DEBUG ? stdout : stderr;

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args); // TODO: Swap for stb_vsntprintf
    va_end(args);

    fprintf(out, "%s %s:%d -> %s\n",
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

////////////////////////////////////////////////////////////////
//
// Sane type renames
//
#include <stddef.h>
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
#define alignof(T) __alignof(T)
#elif DOT_COMPILER_GCC
#define alignof(T) __alignof__(T)
#else
#error alignof not defined for this compiler.
#endif

#define MemoryZero(s, z) memset((s), 0, (z))
#define MemoryZeroStruct(s) MemoryZero((s), sizeof(*(s)))
#define MemoryZeroArray(a) MemoryZero((a), sizeof(a))
#define MemoryZeroTyped(m, c) MemoryZero((m), sizeof(*(m)) * (c))

#ifdef NO_TRACK_MEMORY
#define PRINT_ALLOC(message, ...)
#else
#define PRINT_ALLOC(message, ...) printf(message "\n", ##__VA_ARGS__);
#endif

#define ARENA_MIN_ALIGNMENT 8
#define ARENA_MIN_CAPACITY KB(16)

typedef struct Arena {
    u64 used;
    u64 capacity;
    u8 *base;
    char *name;
} Arena;

typedef struct ArenaInitParams {
    u64 capacity;
    char *reserve_location;
    int reserve_line;
    char *name;
} ArenaInitParams;

typedef struct MemoryArenaPushParams {
    u64 size;
    char* file;
    char* line;
    u8 alignment;
} MemoryArenaPushParams;

typedef struct TempArena {
    u64 prevOffset;
    Arena *arena;
} TempArena;

internal inline TempArena TempArena_Get(Arena *arena) {
    TempArena sa;
    sa.arena = arena;
    sa.prevOffset = arena->used;
    return sa;
}

internal inline void TempArena_Restore(TempArena *sa) {
    sa->arena->used = sa->prevOffset;
}

internal inline Arena Arena_CreateFromMemory_(u8* base, ArenaInitParams* params) {
    DOT_ASSERT_FL(base != NULL, params->reserve_location, params->reserve_line, "Invalid memory provided");
    Arena a = {.base = base, .used = 0, .capacity = params->capacity, .name = params->name};
    return a;
}

// TODO (joan): should expand to use mmap
internal inline Arena Arena_Alloc_(ArenaInitParams *params) {
    DOT_PRINT_FL(params->reserve_location, params->reserve_line, "Arena: requested %zuKB", params->capacity / 1024);
    Arena arena = {.capacity = params->capacity, .name = params->name};
    u8 *memory = (u8 *)malloc(params->capacity); // Malloc guarantes 8B aligned at least
    DOT_ASSERT_FL(memory, params->reserve_location, params->reserve_line, "Could not allocate");
    arena.base = memory;
    return arena;
}

internal inline void Arena_Reset(Arena *arena) { arena->used = 0; }

// Not needed as will last program duration probably.
internal inline void Arena_Free(Arena *arena) {
    free(arena->base);
    arena->used = 0;
    arena->capacity = 0;
}

#define AlignPow2(x, b) (((x) + (b) - 1) & (~((b) - 1)))

internal inline u8 *Arena_Push(Arena *arena, usize size, usize alignment, char* file, u32 line) {
    DOT_ASSERT_FL(size > 0, file, line, "test");
    usize current_address = cast(usize)arena->base + arena->used;
    usize aligned_address = AlignPow2(current_address, alignment);
    u8 *mem_offset = cast(u8*) aligned_address;

    usize required = (aligned_address - cast(usize)arena->base) + size;

    DOT_ASSERT_FL((cast(usize)mem_offset % alignment) == 0, file, line);
    if (DOT_Unlikely(required > arena->capacity)) {
        DOT_ERROR_FL(file, line,
                     "Arena out of bounds: requested %zuKB (used=%zu), capacity=%zu",
                     (required / 1024), (arena->used / 1024), (arena->capacity / 1024));
        return NULL;
    }
    arena->used = required;
    return mem_offset;
}

internal inline u8 *Arena_Push_(Arena *arena, usize size, usize alignment, char* file, u32 line) {
    DOT_ASSERT_FL(size > 0, file, line);
    usize current_address = cast(usize)(arena->base + arena->used);
    usize pad = (alignment - (current_address & (alignment - 1))) & (alignment - 1);
    u8 *mem_offset = arena->base + arena->used + pad;
    DOT_ASSERT_FL((cast(usize)mem_offset % alignment) == 0, file, line, "Not aligned");
    usize required = arena->used + pad + size;
    if (DOT_Unlikely(required > arena->capacity)) {
        DOT_ERROR_FL(file, line,
                     "Arena out of bounds: requested %zuKB (used=%zu), capacity=%zu",
                     (required / 1024), (arena->used / 1024), (arena->capacity / 1024));
        return NULL;
    }
    arena->used += size + pad;
    return mem_offset;
}

#define Arena_Alloc(...)                       \
Arena_Alloc_(&(ArenaInitParams){ \
    .capacity = ARENA_MIN_CAPACITY,        \
    .reserve_location = __FILE__,          \
    .reserve_line = __LINE__,          \
    .name = "Default",                              \
    __VA_ARGS__})

#define Arena_AllocFromMemory(memory, ...)                                \
Arena_CreateFromMemory_((u8*) (memory), &(ArenaInitParams){ \
    .capacity = ARENA_MIN_CAPACITY,                           \
    .reserve_location = __FILE__,                             \
    .reserve_line = __LINE__,          \
    .name = "Default",                              \
    __VA_ARGS__})

#define PushSize(arena, size)                                                 \
MemoryZero(Arena_Push(arena, size, ARENA_MIN_ALIGNMENT, __FILE__, __LINE__), size)

#define PushArrayAligned(arena, type, count, alignment)                      \
(type *)MemoryZero(Arena_Push(arena, sizeof(type) * (count), alignment, __FILE__, __LINE__), sizeof(type) * (count))

#define PushArray(arena, type, count)                                         \
PushArrayAligned(arena, type, count, Max(ARENA_MIN_ALIGNMENT, alignof(type)))

#define PushStruct(arena, type) PushArray(arena, type, 1)

#define MakeArray(arena, type, count)                                         \
((type##_array){.data = PushArray(arena, type, (count)), .size = (count)})

////////////////////////////////////////////////////////////////
//
// String
//
// TODO: Must implement!!!
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

////////////////////////////////////////////////////////////////
//
// whatever
//
#define Unused(something) (void)something

////////////////////////////////////////////////////////////////
//
// Useful it from raddebugger
//
#define EachIndex(it, count) (u64 it = 0; it < (count); it += 1)
#define EachElement(it, array) (u64 it = 0; it < ArrayCount(array); it += 1)
#define EachEnumVal(type, it)                                                  \
(type it = (type)0; it < type##_COUNT; it = (type)(it + 1))
#define EachNonZeroEnumVal(type, it)                                           \
(type it = (type)1; it < type##_COUNT; it = (type)(it + 1))
#define EachInRange(it, range) (u64 it = (range).min; it < (range).max; it += 1)
#define EachNode(it, T, first) (T *it = first; it != 0; it = it->next)

#define DeferLoop(begin, end)                                                  \
for (int _i_ = ((begin), 0); !_i_; _i_ += 1, (end))
#define DeferLoopChecked(begin, end)                                           \
for (int _i_ = 2 * !(begin); (_i_ == 2 ? ((end), 0) : !_i_); _i_ += 1, (end))



////////////////////////////////////////////////////////////////
//
// Threading
//
#if defined(DOT_COMPILER_MSVC)
#define thread_local __declspec(thread)
#elif defined(DOT_COMPILER_GCC) || defined(DOT_COMPILER_CLANG)
#define thread_local __thread
#else
#error "No thread-local storage keyword available for this compiler in C99 mode"
#endif

#endif // !DOT
