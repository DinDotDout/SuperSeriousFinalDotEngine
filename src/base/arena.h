#ifndef ARENA_H
#define ARENA_H

// --- ARENA ----
//  Bump style allocator that allows specifying how much memory we want to
// physically back it instead of allowing regular OS lazy page mapping. We can
// specify initial commit size as well as how often subsequent ones will force
// prefaulting again. Optionally, on LINUX we may use huge pages through THP
//

#define ARENA_MAX_ALIGNMENT 8 // only some 16b or SSE types will be bigger
#define ARENA_MIN_CAPACITY KB(16)

typedef struct Arena{
    u64 used;
    u8 *base;

    u64 reserved; // Total usable memory
    u64 committed;
    bool large_pages;
    u64 commit_expand_size;

    char *name;
}Arena;

typedef struct ArenaInitParams{
    u64 reserve_size; // Virt reserved memory
    u64 commit_size; // Forced initial populate

    u64  commit_expand_size; // Commitsize after initial commit
    bool large_pages;

    // Debug
    char *name;
    char *reserve_file;
    int   reserve_line;
}ArenaInitParams;

typedef struct MemoryArenaPushParams{
    u64   size;
    char* file;
    char* line;
    u8    alignment;
}MemoryArenaPushParams;

typedef struct TempArena {
    u64    prevOffset;
    Arena *arena;
} TempArena;

internal TempArena temp_arena_get(Arena *arena);
internal void      temp_arena_restore(TempArena *temp);

internal Arena* arena_alloc_from_memory_(u8* base, ArenaInitParams* params);
internal Arena* arena_alloc_(ArenaInitParams *params);
internal void   arena_reset(Arena *arena);
internal void   arena_free(Arena *arena);
internal void*  arena_push(Arena *arena, usize size, usize alignment, char* file, u32 line);
internal void   arena_print_debug(Arena *arena);

#define ARENA_DEFAULT_PARAMS(...) \
    &(ArenaInitParams){ \
        .reserve_size       = ARENA_MIN_CAPACITY,\
        .commit_expand_size = PLATFORM_REGULAR_PAGE_SIZE, \
        .large_pages        = false, \
        .reserve_file       = __FILE__, \
        .reserve_line       = __LINE__, \
        .name               = "Default", \
        __VA_ARGS__}

#define ARENA_ALLOC(...) arena_alloc_(ARENA_DEFAULT_PARAMS(__VA_ARGS__))
#define ARENA_ALLOC_FROM_MEMORY(memory, ...) \
    arena_alloc_from_memory_((u8*) (memory), ARENA_DEFAULT_PARAMS(__VA_ARGS__))

#define PUSH_SIZE_NO_ZERO(arena, size) \
  arena_push(arena, size, ARENA_MAX_ALIGNMENT, __FILE__, __LINE__)

#define PUSH_SIZE(arena, size) \
  MEMORY_ZERO(arena_push(arena, size, ARENA_MAX_ALIGNMENT, __FILE__, __LINE__), size)

#define PUSH_ARRAY_ALIGNED(arena, type, count, alignment) \
    (type *)MEMORY_ZERO(arena_push(arena, sizeof(type) * (count), alignment, __FILE__, __LINE__), sizeof(type) * (count))

#define PUSH_ARRAY(arena, type, count) \
    PUSH_ARRAY_ALIGNED(arena, type, count, MAX(ARENA_MAX_ALIGNMENT, ALIGNOF(type)))

#define PUSH_ARRAY_NO_ZERO(arena, type, count)                                    \
  (type *)arena_push(arena, sizeof(type) * (count),                            \
                     MAX(ARENA_MAX_ALIGNMENT, ALIGNOF(type)), __FILE__,        \
                     __LINE__)

#define PUSH_STRUCT(arena, type) PUSH_ARRAY(arena, type, 1)

#define MAKE_ARRAY(arena, type, count) \
    ((type##_array){.data = PUSH_ARRAY(arena, type, (count)), .size = (count)})

#endif
