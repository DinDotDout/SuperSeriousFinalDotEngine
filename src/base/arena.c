internal Arena*
arena_create_(ArenaInitParams *params)
{
    DOT_ASSERT_FL(params->reserve_size > ARENA_HEADER_SIZE_B, params->reserve_file, params->reserve_line, "Arena metadata cannot fit in reserved space");
    Arena *arena;
    if(params->buffer){
        arena = arena_create_from_memory(params);
    }else if(params->parent){
        arena = arena_create_from_arena(params);
    }else{
        arena = arena_create_from_os(params);
    }
    return arena;
}

internal Arena*
arena_create_from_memory(ArenaInitParams *params)
{
    DOT_PRINT_FL(params->reserve_file, params->reserve_line, "Allocating arena from buffer with %M", params->reserve_size);
    uptr base = cast(uptr)params->buffer;
    uptr align = ALIGN_POW2(base, ARENA_HEADER_ALIGN);
    iptr diff = align - base;
    u64 usable_reserved = params->reserve_size - diff;
    if(params->sparse){
        DOT_WARNING("Trying to create sparse non owning arena, this flag only applies to OS arenas");
    }

    // If the buffer came from a sparse parent, it is raw reserved-but-uncommitted
    // VA space (mprotect(PROT_NONE)/PAGE_NOACCESS): we must physically commit our
    // own initial slice and track real committed state, same as an OS-backed
    // arena would, instead of assuming the memory is already backed.
    b32 sparse_backing = params->parent && DOT_BITS_MATCH(params->parent->flags, ArenaFlags_Sparse);
    u64 committed = usable_reserved; // NOTE: default assumes caller-owned memory is already backed
    if(sparse_backing){
        u64 page_size = params->large_pages ? PLATFORM_LARGE_PAGE_SIZE : PLATFORM_REGULAR_PAGE_SIZE;
        u64 initial_commit = ALIGN_POW2(DOT_MAX(params->commit_size, page_size), page_size);
        initial_commit = DOT_MIN(initial_commit, usable_reserved);
        if(params->large_pages){
            os_commit_large(cast(void*)align, initial_commit);
        }else{
            os_commit(cast(void*)align, initial_commit);
        }
        committed = initial_commit;
    }

    Arena *arena =              cast(Arena*)align;
    arena->base               = cast(u8*)align,
    arena->reserved           = usable_reserved,
    arena->committed          = committed,
    arena->commit_expand_size = params->commit_expand_size,
    arena->flags              = params->large_pages ? ArenaFlags_LargePages : 0,
    arena->name               = params->name,
    arena->parent             = params->parent,

    ARENA_RESET(arena);
    return arena;
}

internal Arena*
arena_create_from_arena(ArenaInitParams *params)
{
    DOT_PRINT_FL(params->reserve_file, params->reserve_line, "Allocating arena from parent:");
    arena_print_debug(params->parent);
    if(DOT_BITS_MATCH(params->parent->flags, ArenaFlags_Sparse)){
        // Sub-arenas carved from a sparse parent commit their own memory
        // independently via os_commit/mprotect, which only operates at page
        // granularity: both the start and size of each slice must land on a
        // page boundary so siblings can never share (and fight over) a page.
        u64 page_size = params->large_pages ? PLATFORM_LARGE_PAGE_SIZE : PLATFORM_REGULAR_PAGE_SIZE;
        params->reserve_size = ALIGN_POW2(params->reserve_size, page_size);
        params->buffer = ARENA_PUSH(params->parent, params->reserve_size, page_size, false);
    }else{
        params->buffer = PUSH_SIZE_NO_ZERO(params->parent, params->reserve_size);
    }
    Arena *arena = arena_create_from_memory(params);
    return arena;
}

internal Arena*
arena_create_from_os(ArenaInitParams *params)
{
    DOT_ASSERT_FL(params->reserve_size > 0, params->reserve_file, params->reserve_line, "No reserve_size provided");
    DOT_PRINT_FL(params->reserve_file, params->reserve_line, "Allocating arena from os");
    DOT_PRINT_FL(params->reserve_file, params->reserve_line, "Arena \"%s\": requested %M", params->name, params->reserve_size);

    u64 page_size = params->large_pages ? PLATFORM_LARGE_PAGE_SIZE : PLATFORM_REGULAR_PAGE_SIZE;

    u64 reserved           = ALIGN_POW2(params->reserve_size, page_size);
    u64 commit_expand_size = ALIGN_POW2(DOT_MAX(params->commit_expand_size, page_size), page_size);

    void *memory = os_reserve(reserved);

    u64 initial_commit;
    if(params->sparse){
        initial_commit = ALIGN_POW2(ARENA_HEADER_SIZE_B, page_size);
    }else{
        initial_commit = ALIGN_POW2(DOT_MAX(params->commit_size, page_size), page_size);
        initial_commit = DOT_MIN(initial_commit, reserved);
    }

    if(params->large_pages){
        os_commit_large(memory, initial_commit);
    }else{
        os_commit(memory, initial_commit);
    }
    Arena* arena = cast(Arena*) memory;
    *arena = (Arena){
        .base               = memory,
        .reserved           = reserved,
        .committed          = params->sparse ? reserved : initial_commit, // Force max commited to prevent this allocating space
        .commit_expand_size = commit_expand_size,
        .flags              = ArenaFlags_OwnsMemory
                             | (params->large_pages ? ArenaFlags_LargePages : 0)
                             | (params->sparse ? ArenaFlags_Sparse : 0),
        .name               = params->name,
        .parent             = params->parent,
    };
    ARENA_RESET(arena);
    return arena;
}

internal void
arena_pop_to(ArenaOpParams *op, u64 pos)
{
    u64 new_used = DOT_CLAMP_BOT(ARENA_HEADER_SIZE_B, pos);
    ASAN_POISON(op->arena->base + new_used, op->arena->reserved - new_used);
    op->arena->used = new_used;
}

internal void
arena_reset(ArenaOpParams *op)
{
    arena_pop_to(op, 0);
}

internal void *
arena_realloc(Arena *arena, void *ptr, u64 old_size, u64 new_size)
{
    void *result = PUSH_SIZE_NO_ZERO(arena, new_size);
    if(ptr && result){ MEMORY_COPY(result, ptr, DOT_MIN(old_size, new_size)); }
    return(result);
}

internal void
arena_destroy(ArenaOpParams *op)
{
    Arena *arena    = op->arena;
    u64 reserved    = arena->reserved;
    void *base      = arena->base;
    arena->used                 = 0;
    arena->reserved             = 0;
    arena->commit_expand_size   = 0;
    if(!DOT_BITS_MATCH(arena->flags, ArenaFlags_OwnsMemory)){
        DOT_WARNING_FL(op->file, op->line,
            "Trying to free sub-arena '%s' with parent '%s'!\nThis "
            "arena does not own the memory!",
            arena->name,
            arena->parent ? arena->parent->name : "no parent");
        return;
    }
    DOT_PRINT("Arena \"%s\": freed %M", arena->name, arena->reserved);
    os_release(base, reserved);
}

internal u8*
arena_push(ArenaOpParams *op, u64 alloc_size, u64 alignment, b32 should_zero)
{
    Arena *arena = op->arena;
    // NOTE: zero size is valid so we can construct objects / arrays without
    // spreading a bunch of if statements on user code if make a 0 size array
    // (by error), then another array and try to write through first we will
    // corrupt the memory
    if(alloc_size <= 0){
        return NULL;
    }
    uptr arena_base = cast(uptr)arena->base;
    uptr current_address =  arena_base + arena->used;
    uptr aligned_address = ALIGN_POW2(current_address, alignment);
    u8 *mem_offset = cast(u8*) aligned_address;
    DOT_ASSERT_FL((aligned_address % alignment) == 0, op->file, op->line, "Unaligned address");
    usize required_padded = aligned_address - current_address + alloc_size;
    arena->used += required_padded;

    // NOTE: sparse arenas force committed == reserved so the grow-on-demand
    // path below never runs for them; without this check they'd have no
    // guard at all against handing out a pointer past the reserved region.
    if(DOT_UNLIKELY(arena->used > arena->reserved)){
        arena_print_debug(arena);
        arena->used -= required_padded;
        DOT_ERROR_FL(op->file, op->line, "Arena '%s' exceeded reserved capacity (used=%M, reserved=%M)", arena->name, arena->used, arena->reserved);
    }

    b32 need_more_pages = arena->used > arena->committed;
    b32 is_sparse = DOT_BITS_MATCH(arena->flags, ArenaFlags_Sparse);
    if(need_more_pages && !is_sparse){
        i64 needed_memory = arena->used - arena->committed;

        b32 large_pages = DOT_BITS_MATCH(arena->flags, ArenaFlags_LargePages);
        u64 page_size = large_pages ? PLATFORM_LARGE_PAGE_SIZE : PLATFORM_REGULAR_PAGE_SIZE;
        usize need_aligned = ALIGN_POW2(needed_memory, page_size);
        usize commit_size  = DOT_MAX(arena->commit_expand_size, need_aligned);
        usize arena_leftover_memory = arena->reserved - arena->committed;
        if(commit_size > arena_leftover_memory) commit_size = need_aligned;
        if(DOT_UNLIKELY(commit_size > arena_leftover_memory)){
            arena_print_debug(arena);
            DOT_ERROR_FL(op->file, op->line, "Can't commit more memory total = %M, needed = %M; leftover = %M", arena->reserved, commit_size, arena_leftover_memory);
        }
        uptr commit_pos = arena_base + arena->committed;
        if(large_pages){
            os_commit_large((void*)commit_pos, commit_size);
        }else{
            os_commit((void*)commit_pos, commit_size);
        }
        arena->committed += commit_size;
    }
    ASAN_UNPOISON(mem_offset, required_padded);

    // New pages are zeroed out + we could incurr writting into non mapped regions
    if(should_zero && !is_sparse){
        MEMORY_ZERO(mem_offset, required_padded);
    }
    return mem_offset;
}

internal u8 *
arena_push_copy(ArenaOpParams *op, void const *data, u64 size, u64 alignment)
{
    u8 *result = arena_push(op, size, alignment, false);
    MEMORY_COPY_NO_ALIAS(result, data, size);
    return result;
}

internal void
arena_free()
{
}

internal String8
arena_to_string(Arena *arena)
{
    String8 str = str_comp("");
    if(!arena){
        str = string8_format(arena, "Arena: <null>\n");
        return str;
    }

    b32 large_pages = DOT_BITS_MATCH(arena->flags, ArenaFlags_LargePages);
    const u64 page_size = large_pages ? PLATFORM_LARGE_PAGE_SIZE : PLATFORM_REGULAR_PAGE_SIZE;

    b32 sparse = DOT_BITS_MATCH(arena->flags, ArenaFlags_Sparse);
    const u64 committed_pages = arena->committed / page_size;
    const u64 reserved_pages  = arena->reserved / page_size;
    str = string8_format(arena,
        "Arena '%s'\n "
        "  Arena parent: '%s'\n"
        "  Base:              %p\n"
        "  Used:              %M\n"
        "  Committed:         %M (%llu pages)%s\n"
        "  Reserved:          %M (%llu pages)\n"
        "  Page size:         %M (%s)\n"
        "  Commit expand:     %M\n",
        arena->name ? arena->name : "<unnamed>",
        arena->parent ? arena->parent->name : "no parent",
        arena->base,
        arena->used,
        arena->committed, committed_pages, sparse ? " [sparse: not actually backed]" : "",
        reserved_pages, arena->reserved,
        page_size, large_pages ? "large pages" : "regular pages",
        arena->commit_expand_size
    );
    return str;
}

internal void
arena_print_debug(Arena* arena)
{
    if(!arena){
        DOT_PRINT("Arena: <null>\n");
    }

    b32 large_pages = DOT_BITS_MATCH(arena->flags, ArenaFlags_LargePages);
    const u64 page_size = large_pages ? PLATFORM_LARGE_PAGE_SIZE : PLATFORM_REGULAR_PAGE_SIZE;

    b32 sparse = DOT_BITS_MATCH(arena->flags, ArenaFlags_Sparse);
    const u64 committed_pages = arena->committed / page_size;
    const u64 reserved_pages  = arena->reserved / page_size;
    DOT_PRINT("Arena '%s'", arena->name ? arena->name : "<unnamed>");
    DOT_PRINT("  Arena parent: '%s'", arena->parent ? arena->parent->name : "no parent");
    DOT_PRINT("  Base:              %p", arena->base);
    DOT_PRINT("  Used:              %M", arena->used);
    DOT_PRINT("  Committed:         %M (%" PRIu64 " pages)%s", arena->committed, committed_pages, sparse ? " [sparse: not actually backed]" : "");
    DOT_PRINT("  Reserved:          %M (%" PRIu64 " pages)", arena->reserved,  reserved_pages);
    DOT_PRINT("  Page size:         %M (%s)", page_size, large_pages ? "large pages" : "regular pages");
    DOT_PRINT("  Commit expand:     %M\n", arena->commit_expand_size);
}

internal TempArena
temp_arena_start(Arena *arena)
{
    TempArena sa;
    sa.arena = arena;
    sa.prev_offset = arena->used;
    return sa;
}

internal void
temp_arena_restore(TempArena temp)
{
    temp.arena->used = temp.prev_offset;
}

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

    DOT_TEST_CHECK(test, "Used reset", arena->used == ARENA_HEADER_SIZE_B);
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
test_sparse_fn(void)
{
    DOT_TestResults test = {0};

    Arena *root = ARENA_CREATE(.reserve_size = DOT_MB(64), .sparse = true);

    DOT_TEST_CHECK(test, "Sparse root created", root != NULL);
    DOT_TEST_CHECK(test, "Sparse flag set", DOT_BITS_MATCH(root->flags, ArenaFlags_Sparse));
    DOT_TEST_CHECK(test, "Sparse committed forced to reserved", root->committed == root->reserved);

    Arena *child_a = ARENA_CREATE(.parent = root, .reserve_size = DOT_MB(16), .commit_size = DOT_KB(64));
    Arena *child_b = ARENA_CREATE(.parent = root, .reserve_size = DOT_MB(16), .commit_size = DOT_KB(64));

    DOT_TEST_CHECK(test, "Child A created", child_a != NULL);
    DOT_TEST_CHECK(test, "Child B created", child_b != NULL);
    DOT_TEST_CHECK(test, "Child A not itself sparse", !DOT_BITS_MATCH(child_a->flags, ArenaFlags_Sparse));
    DOT_TEST_CHECK(test, "Child A committed tracks real memory", child_a->committed < child_a->reserved);
    DOT_TEST_CHECK(test, "Child A committed at least commit_size", child_a->committed >= DOT_KB(64));

    u8 *p = PUSH_ARRAY(child_a, u8, DOT_MB(1));
    DOT_TEST_CHECK(test, "Child A push succeeded", p != NULL);
    if(p){
        p[0] = 0xAB;
        p[DOT_MB(1) - 1] = 0xCD;
        DOT_TEST_CHECK(test, "Child A memory is actually writable", p[0] == 0xAB && p[DOT_MB(1) - 1] == 0xCD);
    }
    DOT_TEST_CHECK(test, "Child A grew its own commit", child_a->committed >= child_a->used);

    u8 *q = PUSH_ARRAY(child_b, u8, 64);
    DOT_TEST_CHECK(test, "Child B push independent of A", q != NULL);
    if(q){
        q[0] = 0xEF;
        DOT_TEST_CHECK(test, "Child B memory is actually writable", q[0] == 0xEF);
    }
    DOT_TEST_CHECK(test, "Siblings didn't overlap", (void*)p != (void*)q);

    ARENA_DESTROY(root);
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

// #include <sys/resource.h>
// NO_ASAN internal inline void
// test_page_fault_progression()
// {
//     Arena *arena = ARENA_CREATE(
//         .reserve_size = DOT_MB(300),
//         .commit_size = DOT_MB(200),
//         // .commit_expand_size = DOT_MB(100),
//         // .large_pages = true,
//     );
//
//     struct rusage before, after;
//
//     u64 pages = (arena->reserved / PLATFORM_REGULAR_PAGE_SIZE) - 1;
//
//     u64 nofault_start = 0;
//     u64 nofault_count = 0;
//
//     for (u64 i = 0; i < pages; ++i) {
//         getrusage(RUSAGE_SELF, &before);
//
//         u8 *mem = PUSH_ARRAY_NO_ZERO(arena, u8, PLATFORM_REGULAR_PAGE_SIZE);
//         if (mem) mem[0] = 1;
//
//         getrusage(RUSAGE_SELF, &after);
//
//         long delta = after.ru_minflt - before.ru_minflt;
//
//         if (delta == 0) {
//             // extend no‑fault run
//             if (nofault_count == 0)
//                 nofault_start = i;
//             nofault_count++;
//         } else {
//             // flush no‑fault run before printing the fault
//             if (nofault_count > 0) {
//                 DOT_PRINT(
//                     "No‑fault run: %lu DOT_KB (%lu pages) from page %lu to %lu",
//                     nofault_count * (PLATFORM_REGULAR_PAGE_SIZE / 1024),
//                     nofault_count,
//                     nofault_start,
//                     nofault_start + nofault_count - 1
//                 );
//                 nofault_count = 0;
//             }
//
//             DOT_PRINT(
//                 "FAULT: %ld minor faults at page %lu (offset %M)\n",
//                 delta,
//                 i,
//                 i * PLATFORM_REGULAR_PAGE_SIZE
//             );
//         }
//     }
//
//     // flush trailing no‑fault run
//     if (nofault_count > 0) {
//         DOT_PRINT(
//             "No‑fault run: %lu DOT_KB (%lu pages) from page %lu to %lu\n",
//             nofault_count * (PLATFORM_REGULAR_PAGE_SIZE / 1024),
//             nofault_count,
//             nofault_start,
//             nofault_start + nofault_count - 1
//         );
//     }
//     // print_hugepage_info(arena->base);
//     printf("PID = %d — press ENTER to continue...\n", getpid());
//     getchar();
//     ARENA_DESTROY(arena);
// }

DOT_TEST_SUITE(arena_tests)
{
    DOT_TestResults test = {0};
    DOT_MERGE_TEST_RESULTS(test, test_basic_allocation_fn());
    DOT_MERGE_TEST_RESULTS(test, test_alignment_fn());
    // jd: This causes an out of memory and thus the test cannot continue tests. Must think of a way of addressing this
    // DOT_MERGE_TEST_RESULTS(test, test_out_of_bounds_fn());
    DOT_MERGE_TEST_RESULTS(test, test_reset_fn());
    DOT_MERGE_TEST_RESULTS(test, test_large_pages_fn());
    DOT_MERGE_TEST_RESULTS(test, test_sparse_fn());
    DOT_MERGE_TEST_RESULTS(test, test_padding_behavior_fn());
    // test_page_fault_progression(); // This one is just informative
    return test;
}
