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

#define ARENA_PUSH(arena, size, alignment, zero) arena_push((arena), (size), (alignment), (zero), __FILE__, __LINE__)
#define PUSH_SIZE_NO_ZERO(arena, size)      ARENA_PUSH(arena, (size), ARENA_MAX_ALIGNMENT, false)
#define PUSH_SIZE(arena, size)              ARENA_PUSH(arena, size, ARENA_MAX_ALIGNMENT, true)

#define PUSH_ARRAY_ALIGNED(arena, T, count, alignment)          (T*)ARENA_PUSH(arena, sizeof(T) * (count), (alignment), true)
#define PUSH_ARRAY_NO_ZERO_ALIGNED(arena, T, count, alignment)  (T*)ARENA_PUSH(arena, sizeof(T) * (count), (alignment), false)
#define PUSH_ARRAY(arena, T, count)                             (T*)ARENA_PUSH(arena, sizeof(T) * (count), DOT_MAX(ARENA_MAX_ALIGNMENT, DOT_ALIGNOF(T)), true)
#define PUSH_ARRAY_NO_ZERO(arena, T, count)                     (T*)ARENA_PUSH(arena, sizeof(T) * (count), DOT_MAX(ARENA_MAX_ALIGNMENT, DOT_ALIGNOF(T)), false)

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


internal inline DOT_TestResults
test_basic_allocation_fn(void)
{
    DOT_TestResults test = {0};

    Arena *arena = ARENA_CREATE(.reserve_size = DOT_MB(4), .commit_size = DOT_KB(64));

    DOT_TEST_CHECK(test, "Arena created", arena != NULL);
    DOT_TEST_CHECK(test, "Reserve size", arena->reserved >= DOT_MB(4));
    DOT_TEST_CHECK(test, "Commit size", arena->committed >= DOT_KB(64));
    DOT_TEST_CHECK(test, "Initial used", arena->used == sizeof(Arena));

    ARENA_DESTROY(arena);
    return test;
}

internal inline DOT_TestResults
test_alignment_fn(void)
{
    DOT_TestResults test = {0};

    Arena *arena = ARENA_CREATE(.reserve_size = DOT_MB(2), .commit_size = DOT_KB(64));

    u8 *p1 = PUSH_ARRAY_ALIGNED(arena, u8, 32, 8);
    DOT_TEST_CHECK(test, "Align 8", ((uintptr_t)p1 % 8) == 0);

    u8 *p2 = PUSH_ARRAY_ALIGNED(arena, u8, 64, 64);
    DOT_TEST_CHECK(test, "Align 64", ((uintptr_t)p2 % 64) == 0);

    ARENA_DESTROY(arena);
    return test;
}

internal inline DOT_TestResults
test_out_of_bounds_fn(void)
{
    DOT_TestResults test = {0};

    Arena *arena = ARENA_CREATE(.reserve_size = DOT_MB(1), .commit_size = DOT_KB(64));

    PUSH_ARRAY(arena, u8, DOT_MB(1) - sizeof(Arena) - 128);

    void *p = PUSH_ARRAY_NO_ZERO(arena, u8, DOT_KB(256));
    DOT_TEST_CHECK(test, "Out-of-bounds returns NULL", p == NULL);

    ARENA_DESTROY(arena);
    return test;
}

internal inline DOT_TestResults
test_reset_fn(void)
{
    DOT_TestResults test = {0};

    Arena *arena = ARENA_CREATE(.reserve_size = DOT_MB(2), .commit_size = DOT_KB(64));

    PUSH_ARRAY(arena, u8, 128);
    DOT_TEST_CHECK(test, "Used > sizeof(Arena)", arena->used > sizeof(Arena));

    ARENA_RESET(arena);

    DOT_TEST_CHECK(test, "Used reset", arena->used == 0);
    DOT_TEST_CHECK(test, "Reserve preserved", arena->reserved >= DOT_MB(2));
    DOT_TEST_CHECK(test, "Commit preserved", arena->committed >= DOT_KB(64));

    ARENA_DESTROY(arena);
    return test;
}

internal inline DOT_TestResults
test_large_pages_fn(void)
{
    DOT_TestResults test = {0};

    Arena *arena = ARENA_CREATE(
        .reserve_size = DOT_MB(32),
        .commit_size = DOT_MB(2),
        .large_pages = true
    );

    DOT_TEST_CHECK(test, "Large pages flag", DOT_BITS_MATCH(arena->flags, ArenaFlags_LargePages));
    DOT_TEST_CHECK(test, "Commit LP aligned", arena->committed % PLATFORM_LARGE_PAGE_SIZE == 0);
    DOT_TEST_CHECK(test, "Reserve LP aligned", arena->reserved % PLATFORM_LARGE_PAGE_SIZE == 0);

    ARENA_DESTROY(arena);
    return test;
}

internal inline DOT_TestResults
test_padding_behavior_fn(void)
{
    DOT_TestResults test = {0};

    Arena *arena = ARENA_CREATE(.reserve_size = DOT_MB(1), .commit_size = DOT_KB(64));

    u64 before = arena->used;
    u8 *p = PUSH_ARRAY_ALIGNED(arena, u8, 16, 64);

    DOT_TEST_CHECK(test, "Ptr not NULL", p != NULL);
    DOT_TEST_CHECK(test, "Alignment 64", ((uintptr_t)p % 64) == 0);
    DOT_TEST_CHECK(test, "Used increased", arena->used >= before + 16);

    ARENA_DESTROY(arena);
    return test;
}

#include <sys/resource.h>
NO_ASAN internal inline void
test_page_fault_progression()
{
    Arena *arena = ARENA_CREATE(
        .reserve_size = DOT_MB(300),
        .commit_size = DOT_MB(200),
        // .commit_expand_size = DOT_MB(100),
        // .large_pages = true,
    );

    struct rusage before, after;

    u64 pages = (arena->reserved / PLATFORM_REGULAR_PAGE_SIZE) - 1;

    u64 nofault_start = 0;
    u64 nofault_count = 0;

    for (u64 i = 0; i < pages; ++i) {
        getrusage(RUSAGE_SELF, &before);

        u8 *mem = PUSH_ARRAY_NO_ZERO(arena, u8, PLATFORM_REGULAR_PAGE_SIZE);
        if (mem) mem[0] = 1;

        getrusage(RUSAGE_SELF, &after);

        long delta = after.ru_minflt - before.ru_minflt;

        if (delta == 0) {
            // extend no‑fault run
            if (nofault_count == 0)
                nofault_start = i;
            nofault_count++;
        } else {
            // flush no‑fault run before printing the fault
            if (nofault_count > 0) {
                DOT_PRINT(
                    "No‑fault run: %lu DOT_KB (%lu pages) from page %lu to %lu",
                    nofault_count * (PLATFORM_REGULAR_PAGE_SIZE / 1024),
                    nofault_count,
                    nofault_start,
                    nofault_start + nofault_count - 1
                );
                nofault_count = 0;
            }

            DOT_PRINT(
                "FAULT: %ld minor faults at page %lu (offset %M)\n",
                delta,
                i,
                i * PLATFORM_REGULAR_PAGE_SIZE
            );
        }
    }

    // flush trailing no‑fault run
    if (nofault_count > 0) {
        DOT_PRINT(
            "No‑fault run: %lu DOT_KB (%lu pages) from page %lu to %lu\n",
            nofault_count * (PLATFORM_REGULAR_PAGE_SIZE / 1024),
            nofault_count,
            nofault_start,
            nofault_start + nofault_count - 1
        );
    }
    // print_hugepage_info(arena->base);
    printf("PID = %d — press ENTER to continue...\n", getpid());
    getchar();
    ARENA_DESTROY(arena);
}

DOT_TEST_SUITE(arena_tests)
{
    DOT_TestResults test = {0};

    DOT_MERGE_TEST_RESULTS(test, test_basic_allocation_fn());
    DOT_MERGE_TEST_RESULTS(test, test_alignment_fn());
    DOT_MERGE_TEST_RESULTS(test, test_out_of_bounds_fn());
    DOT_MERGE_TEST_RESULTS(test, test_reset_fn());
    DOT_MERGE_TEST_RESULTS(test, test_large_pages_fn());
    DOT_MERGE_TEST_RESULTS(test, test_padding_behavior_fn());
    // test_page_fault_progression(); // This one is just informative
    return test;
}

#endif // !ARENA_H
