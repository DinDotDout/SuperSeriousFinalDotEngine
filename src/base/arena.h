#ifndef ARENA_H
#define ARENA_H

#ifdef NO_TRACK_MEMORY
#define PRINT_ALLOC(message, ...)
#else
#define PRINT_ALLOC(message, ...) printf(message "\n", ##__VA_ARGS__);
#endif

#define ARENA_MAX_ALIGNMENT 8 // only some 16b or SSE types will be bigger
#define ARENA_MIN_CAPACITY KB(16)

typedef struct Arena {
    u64 used;
    u8 *base;

    u64 reserved; // Total usable memory
    u64 committed;
    bool large_pages;
    u64 commit_expand_size;

    char *name;
} Arena;

typedef struct ArenaInitParams {
    u64 reserve_size; // Virt reserved memory
    u64 commit_size; // Forced initial populate

    u64 commit_expand_size; // Commitsize after initial commit
    bool large_pages;

    // Debug
    char *reserve_file;
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

internal TempArena TempArena_Get(Arena *arena);
internal void TempArena_Restore(TempArena *sa);

internal Arena* Arena_CreateFromMemory_(u8* base, ArenaInitParams* params);
internal Arena* Arena_Alloc_(ArenaInitParams *params);
internal void Arena_Reset(Arena *arena);
internal void Arena_Free(Arena *arena);
internal void* Arena_Push(Arena *arena, usize size, usize alignment, char* file, u32 line);

#define ArenaDefaultParams(...) \
&(ArenaInitParams){ \
    .reserve_size = ARENA_MIN_CAPACITY,\
    .large_pages = false, \
    .reserve_file = __FILE__, \
    .reserve_line = __LINE__, \
    .name = "Default", \
    __VA_ARGS__}

#define Arena_Alloc(...) Arena_Alloc_(ArenaDefaultParams(__VA_ARGS__))

#define Arena_AllocFromMemory(memory, ...) Arena_CreateFromMemory_((u8*) (memory), ArenaDefaultParams(__VA_ARGS__))

#define PushSize(arena, size) \
    MemoryZero(Arena_Push(arena, size, ARENA_MAX_ALIGNMENT, __FILE__, __LINE__), size)

#define PushArrayAligned(arena, type, count, alignment) \
    (type *)MemoryZero(Arena_Push(arena, sizeof(type) * (count), alignment, __FILE__, __LINE__), sizeof(type) * (count))

#define PushArray(arena, type, count) \
    PushArrayAligned(arena, type, count, Max(ARENA_MAX_ALIGNMENT, Alignof(type)))

#define PushArrayNoZero(arena, type, count) \
    (type *)Arena_Push(arena, sizeof(type) * (count), \
                       Max(ARENA_MAX_ALIGNMENT, Alignof(type)), \
                       __FILE__, __LINE__)

#define PushStruct(arena, type) PushArray(arena, type, 1)

#define MakeArray(arena, type, count) \
    ((type##_array){.data = PushArray(arena, type, (count)), .size = (count)})

#endif
