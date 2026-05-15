#ifndef ARENA_H
#define ARENA_H

// --- ARENA ----
// Bump style allocator that allows specifying how much memory we want to
// physically back it instead of allowing regular OS lazy page mapping. We can
// specify initial commit size as well as how often subsequent ones will force
// prefaulting again. Optionally, on LINUX we may use huge pages through THP

enum
{
    // (jd) NOTE: May help in avoid false sharing
    ARENA_HEADER_SIZE_B = PLATFORM_CACHE_LINE_SIZE,

    ARENA_MAX_ALIGNMENT = 8,
    ARENA_MIN_CAPACITY  = KB(16),
};

typedef enum ArenaKind{
    ArenaKind_Null,
    ArenaKind_FromOS,
    ArenaKind_FromParent,
    ArenaKind_FromBuffer,
    ArenaKind_Count,
}ArenaKind;


typedef struct Arena Arena;
struct Arena{
    Arena *parent; // Can be null, possible memory owner

    u64 used;
    u8 *base;

    u64 reserved; // Total usable memory
    u64 committed;
    u64 commit_expand_size;

    b32 large_pages;
    ArenaKind kind;

    // Debug
    char *name;
};

DOT_STATIC_ASSERT(sizeof(Arena) <= ARENA_HEADER_SIZE_B, "Keep cache line sized");

typedef struct ArenaInitParams{
    // These 2 will be used to alloc from if not empty, otherwise we will make a new os alloc
    Arena *parent;
    u8 *buffer;

    u64 reserve_size; // Virt reserved memory
    u64 commit_size; // Forced initial populate

    u64  commit_expand_size; // Commitsize after initial commit
    b32 large_pages;

    // Debug
    char *name;
    char *reserve_file;
    int   reserve_line;
}ArenaInitParams;

internal Arena *arena_create_(ArenaInitParams *params);
internal Arena *arena_create_from_memory(ArenaInitParams *params);
internal Arena *arena_create_from_arena(ArenaInitParams *params);
internal Arena *arena_create_from_os(ArenaInitParams *params);

internal u8*    arena_push(Arena *arena, usize size, usize alignment, b32 zero, char *file, u32 line);
internal void   arena_reset(Arena *arena, char *file, u32 line);
internal void   arena_destroy(Arena *arena, char *file, u32 line);
internal void   arena_print_debug(Arena *arena);

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

#define ARENA_CREATE(...)       arena_create_(ARENA_DEFAULT_PARAMS(__VA_ARGS__))
#define ARENA_RESET(arena)      arena_reset(arena, __FILE__, __LINE__)
#define ARENA_DESTROY(arena)    arena_destroy(arena, __FILE__, __LINE__)

#define ARENA_PUSH(arena, size, alignment, zero) \
    arena_push((arena), (size), (alignment), (zero), __FILE__, __LINE__)

#define PUSH_SIZE_NO_ZERO(arena, size) \
    ARENA_PUSH(arena, (size), ARENA_MAX_ALIGNMENT, false)

#define PUSH_SIZE(arena, size) \
    ARENA_PUSH(arena, size, ARENA_MAX_ALIGNMENT, true)

#define PUSH_ARRAY_ALIGNED(arena, T, count, alignment) \
    (T*)ARENA_PUSH(arena, sizeof(T) * (count), (alignment), true)

#define PUSH_ARRAY_NO_ZERO_ALIGNED(arena, T, count, alignment) \
    (T*)ARENA_PUSH(arena, sizeof(T) * (count), (alignment), false)

#define PUSH_ARRAY(arena, T, count) \
    (T*)ARENA_PUSH(arena, sizeof(T) * (count), MAX(ARENA_MAX_ALIGNMENT, ALIGNOF(T)), true)

#define PUSH_ARRAY_NO_ZERO(arena, T, count) \
   (T*)ARENA_PUSH(arena, sizeof(T) * (count), MAX(ARENA_MAX_ALIGNMENT, ALIGNOF(T)), false)

#define PUSH_STRUCT(arena, T) (T*)PUSH_ARRAY(arena, T, 1)

// (jd) NOTE: Experiments. Clang seems to optimize the copy pretty well
#define PUSH_COPY(arena, T, src)        MEMORY_COPY_STRUCT(PUSH_ARRAY_NO_ZERO(arena, T, 1), (src))
#define PUSH_COPY_LIT(arena, T, src)    MEMORY_COPY_STRUCT(PUSH_ARRAY_NO_ZERO(arena, T, 1), &(T)src)

#define PUSH_SLICE(arena, T, c) {.data = PUSH_ARRAY(arena, T, c), .count = (c)}
#define PUSH_SLICE_LIT(arena, slice_type, T, c) (slice_type){.data = PUSH_ARRAY(arena, T, c), .count= (c)}

//////////////////////////////////////////////////////////
/// TempArena
/// 

typedef struct TempArena{
    u64    prev_offset;
    Arena *arena;
}TempArena;

internal TempArena temp_arena_get(Arena *arena);
internal void      temp_arena_restore(TempArena temp);


#endif // !ARENA_H
