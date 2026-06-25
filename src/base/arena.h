#ifndef ARENA_H
#define ARENA_H

// --- ARENA ----
// Bump style allocator that allows specifying how much memory we want to
// physically back it instead of allowing regular OS lazy page mapping. We can
// specify initial commit size as well as how often subsequent ones will force
// prefaulting again. Optionally, on LINUX we may use huge pages through THP

enum
{
    ARENA_HEADER_SIZE_B = PLATFORM_CACHE_LINE_SIZE,
    ARENA_HEADER_ALIGN  = PLATFORM_CACHE_LINE_SIZE,

    ARENA_MAX_ALIGNMENT = 8,
    ARENA_MIN_CAPACITY  = DOT_KB(16),
};

typedef enum ArenaFlags{
    ArenaFlags_OwnsMemory = DOT_BIT(0),
    ArenaFlags_LargePages = DOT_BIT(1),
    // ArenaFlags_ChainArena = DOT_BIT(2),
}ArenaFlags;

typedef struct Arena Arena;
struct Arena{
    u64 used;
    u8 *base;

    u64 reserved; // Total usable memory
    u64 committed;
    u64 commit_expand_size;

    ArenaFlags flags;

#if DOT_DEBUG
    Arena *parent; // do we really need this?
    char *name;
#endif
};
DOT_STATIC_ASSERT(ARENA_HEADER_SIZE_B <= PLATFORM_REGULAR_PAGE_SIZE, "Arena header doesn't fit min allocation granularity");
DOT_STATIC_ASSERT(sizeof(Arena) <= ARENA_HEADER_SIZE_B, "Arena header should fit in a cache line to avoid false sharing on thread arenas");

typedef struct ArenaInitParams{
    // These 2 will be used to alloc from if not empty, otherwise we will make a new os alloc
    Arena *parent;
    u8 *buffer;

    u64 reserve_size; // Virt reserved memory
    u64 commit_size; // Forced initial populate

    u64  commit_expand_size; // Commitsize after initial commit
    b32 large_pages;

    // Debug
#ifdef DOT_DEBUG
    char *name;
    char *reserve_file;
    int   reserve_line;
#endif
}ArenaInitParams;

typedef struct ArenaOpParams{
    Arena *arena;
#ifdef DOT_DEBUG
    char *file;
    u32 line;
#endif
}ArenaOpParams;

internal Arena  *arena_create_(ArenaInitParams *);
internal void   arena_destroy(ArenaOpParams *);

internal Arena  *arena_create_from_memory(ArenaInitParams *);
internal Arena  *arena_create_from_arena(ArenaInitParams *);
internal Arena  *arena_create_from_os(ArenaInitParams *);

internal u8*    arena_push(ArenaOpParams *, u64 alloc_size, u64 alignment, b32 should_zero);
internal void   arena_reset(ArenaOpParams *);
internal void   arena_pop_to(ArenaOpParams *, u64 pos);
internal void   arena_free(); // (jd) NOTE: Stub to pass to generic allocators
                              //
internal void       arena_print_debug(Arena *arena);
internal String8    arena_to_string(Arena *arena);


#ifdef DOT_DEBUG
#define ARENA_DEFAULT_PARAMS(...) \
    &(ArenaInitParams){ \
        .reserve_size       = ARENA_MIN_CAPACITY,\
        .parent             = NULL, \
        .commit_expand_size = PLATFORM_REGULAR_PAGE_SIZE, \
        .large_pages        = false, \
        .reserve_file       = __FILE__, \
        .reserve_line       = __LINE__, \
        .name               = DOT_FILE_LINE, \
        __VA_ARGS__ \
    }
#define ARENA_OP_PARAMS(...) \
    &(ArenaOpParams){ \
        .file       = __FILE__, \
        .line       = __LINE__, \
        __VA_ARGS__ \
    }
#else
#define ARENA_DEFAULT_PARAMS(...) \
    &(ArenaInitParams){ \
        .reserve_size       = ARENA_MIN_CAPACITY,\
        .parent             = NULL, \
        .commit_expand_size = PLATFORM_REGULAR_PAGE_SIZE, \
        .large_pages        = false, \
        __VA_ARGS__ \
    }
#define ARENA_OP_PARAMS(...) \
    &(ArenaOpParams){ \
        __VA_ARGS__ \
    }
#endif


#define ARENA_CREATE(...)           arena_create_(ARENA_DEFAULT_PARAMS(__VA_ARGS__))
#define ARENA_RESET(a)              arena_reset(ARENA_OP_PARAMS(.arena = (a)));
#define ARENA_DESTROY(a)            arena_destroy(ARENA_OP_PARAMS(.arena = (a)));
#define ARENA_PUSH(a, sz, align, z) arena_push(ARENA_OP_PARAMS(.arena = (a)), (sz), (align), (z))

#define PUSH_SIZE_NO_ZERO(arena, size)      ARENA_PUSH(arena, size, ARENA_MAX_ALIGNMENT, false)
#define PUSH_SIZE(arena, size)              ARENA_PUSH(arena, size, ARENA_MAX_ALIGNMENT, true)

#define PUSH_ARRAY_ALIGNED(arena, T, count, alignment)          (T*)ARENA_PUSH(arena, sizeof(T) * (count), alignment, true)
#define PUSH_ARRAY_NO_ZERO_ALIGNED(arena, T, count, alignment)  (T*)ARENA_PUSH(arena, sizeof(T) * (count), alignment, false)
#define PUSH_ARRAY(arena, T, count)                             (T*)ARENA_PUSH(arena, sizeof(T) * (count), DOT_MAX(ARENA_MAX_ALIGNMENT, DOT_ALIGNOF(T)), true)
#define PUSH_ARRAY_NO_ZERO(arena, T, count)                     (T*)ARENA_PUSH(arena, sizeof(T) * (count), DOT_MAX(ARENA_MAX_ALIGNMENT, DOT_ALIGNOF(T)), false)

// (jd) NOTE: This is used to bypass type checking in certain cases we are sure the types will match
#define PUSH_ARRAY_UNTYPED(arena, elem_0_t, count)              (void*) ARENA_PUSH(arena, sizeof(elem_0_t) * (count), DOT_MAX(ARENA_MAX_ALIGNMENT, DOT_ALIGNOF(elem_0_t)), true)

#define PUSH_STRUCT(arena, T) (T*)PUSH_ARRAY(arena, T, 1)

//////////////////////////////////////////////////////////
/// TempArena
/// 

typedef struct TempArena{
    u64    prev_offset;
    Arena *arena;
}TempArena;

internal TempArena temp_arena_start(Arena *arena);
internal void      temp_arena_restore(TempArena temp);

#endif // !ARENA_H
