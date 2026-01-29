internal TempArena
temp_arena_get(Arena *arena){
    TempArena sa;
    sa.arena = arena;
    sa.prevOffset = arena->used;
    return sa;
}

internal void
temp_arena_restore(TempArena *temp){
    temp->arena->used = temp->prevOffset;
}

force_inline internal Arena*
arena_alloc_(ArenaInitParams* params){
    Arena* arena;
    if(params->buffer){
        arena = arena_alloc_from_memory(params);
    }else if(params->parent){
        arena = arena_alloc_from_arena(params);
    }else{
        arena = arena_alloc_from_os(params);
    }
    return arena;
}


// WARN: Since for now we only get memory from arenas, we will always have
// subarenas with prefaulted memory so we set committed == reserved
internal Arena*
arena_alloc_from_memory(ArenaInitParams* params){
    DOT_ASSERT_FL(params->buffer != NULL, params->reserve_file, params->reserve_line, "Invalid memory provided");
    DOT_PRINT_FL(params->reserve_file, params->reserve_line, "Allocating arena from buffer");
    Arena* arena              = cast(Arena*) params->buffer;
    arena->base               = params->buffer+sizeof(Arena);
    arena->used               = 0;
    arena->reserved           = params->reserve_size;
    arena->committed          = params->reserve_size; // We make the assumption that it is prefaulted for now
    arena->commit_expand_size = params->commit_expand_size;
    arena->large_pages        = params->large_pages;
    arena->name               = params->name;
    arena->parent             = params->parent;
    return arena;
}

internal Arena*
arena_alloc_from_arena(ArenaInitParams* params){
    DOT_ASSERT_FL(params->parent != NULL, params->reserve_file, params->reserve_line, "Invalid memory provided");
    DOT_PRINT_FL(params->reserve_file, params->reserve_line, "Allocating arena from parent");
    u8* mem = PUSH_SIZE_NO_ZERO(params->parent, params->reserve_size);
    params->buffer = mem;
    Arena* new_arena = arena_alloc_from_memory(params);
    return new_arena;
}

internal Arena*
arena_alloc_from_os(ArenaInitParams *params){
    DOT_PRINT_FL(params->reserve_file, params->reserve_line, "Allocating arena from os");
    DOT_ASSERT_FL(params->reserve_size > 0, params->reserve_file, params->reserve_line, "No reserve_size provided");
    DOT_PRINT_FL(params->reserve_file, params->reserve_line, "Arena \"%s\": requested %M", params->name, params->reserve_size);

    u64 page_size = params->large_pages ? PLATFORM_LARGE_PAGE_SIZE : PLATFORM_REGULAR_PAGE_SIZE;

    u64 reserved           = ALIGN_POW2(params->reserve_size, page_size);
    u64 initial_commit     = ALIGN_POW2(MAX(params->commit_size, page_size), page_size);
    u64 commit_expand_size = ALIGN_POW2(MAX(params->commit_expand_size, page_size), page_size);
    initial_commit         = MIN(initial_commit, reserved);

    void *memory = os_reserve(reserved);
    if(params->large_pages){
        os_commit_large(memory, initial_commit);
    }else{
        os_commit(memory, initial_commit);
    }
    DOT_ASSERT_FL(memory, params->reserve_file, params->reserve_line, "Could not allocate");

    Arena* arena = cast(Arena*)memory;
    arena->base               = memory;
    arena->used               = sizeof(Arena);
    arena->reserved           = reserved;
    arena->committed          = initial_commit;
    arena->commit_expand_size = commit_expand_size;
    arena->large_pages        = params->large_pages;
    arena->name               = params->name;

    ASAN_POISON(arena->base+arena->used, arena->reserved-arena->used);
    return arena;
}

internal void
arena_reset(Arena *arena){
    ASAN_POISON(arena->base, arena->committed);
    arena->used = sizeof(Arena);
}

internal void
arena_free(Arena *arena){
    DOT_ASSERT(arena->parent == NULL, "Trying to free sub-arena '%s' with parent '%s'!\nThis arena does not own the memory!", arena->name, arena->parent->name);
    u64 reserved = arena->reserved;
    void* base = arena->base;
    DOT_PRINT("Arena \"%s\": freed %M", arena->name, arena->reserved);
    arena->used = 0;
    arena->reserved = 0;
    arena->commit_expand_size = 0;
    os_release(cast(void*)base, reserved);
}

internal void*
arena_push(Arena *arena, usize alloc_size, usize alignment, b8 zero, char* file, u32 line){
    DOT_ASSERT_FL(alloc_size > 0, file, line);
    uptr arena_base = cast(uptr)arena->base;
    uptr current_address =  arena_base + arena->used;
    uptr aligned_address = ALIGN_POW2(current_address, alignment);
    void *mem_offset = cast(void*) aligned_address;
    DOT_ASSERT_FL((aligned_address % alignment) == 0, file, line, "Unaligned address");
    usize required_padded = aligned_address - current_address + alloc_size;
    arena->used += required_padded;

    i64 page_overhang = arena->used - arena->committed;
    if(page_overhang > 0){
        u64 page_size = arena->large_pages ? PLATFORM_LARGE_PAGE_SIZE
                                        : PLATFORM_REGULAR_PAGE_SIZE;

        usize need_aligned = ALIGN_POW2(page_overhang, page_size);
        usize leftover     = arena->reserved - arena->committed;
        usize commit_size  = MAX(arena->commit_expand_size, need_aligned);
        if(commit_size > leftover) commit_size = need_aligned;
        if(DOT_UNLIKELY(commit_size > leftover)){
            DOT_ERROR(file, line,
                "Can't commit more memory! needed = %M; leftover = %M", commit_size, leftover);
            return NULL;
        }

        uptr commit_pos = arena_base + arena->committed;
        if(arena->large_pages){
            os_commit_large((void*)commit_pos, commit_size);
        }else{
            os_commit((void*)commit_pos, commit_size);
        }
        arena->committed += commit_size;
    }
    ASAN_UNPOISON(mem_offset, required_padded);
    if(zero){
        MEMORY_ZERO(mem_offset, required_padded);
    }
    return mem_offset;
}

internal void
arena_print_debug(Arena* arena){
    if (!arena){
        DOT_PRINT("Arena: <null>\n");
        return;
    }

    const u64 page_size = arena->large_pages
        ? PLATFORM_LARGE_PAGE_SIZE
        : PLATFORM_REGULAR_PAGE_SIZE;

    const u64 committed_pages = arena->committed / page_size;
    const u64 reserved_pages  = arena->reserved / page_size;

    DOT_PRINT("Arena '%s'", arena->name ? arena->name : "<unnamed>");
    if(arena->parent){
        DOT_PRINT("Arena parent: '%s'", arena->parent ? arena->parent->name : "no parent");
    }
    DOT_PRINT("  Base:              %p", arena->base);
    DOT_PRINT("  Used:              %M", arena->used - sizeof(Arena));
    DOT_PRINT("  Committed:         %M (%llu pages)", arena->committed, committed_pages);
    DOT_PRINT("  Reserved:          %M (%llu pages)", arena->reserved,  reserved_pages);
    DOT_PRINT("  Page size:         %M (%s)", page_size, arena->large_pages ? "large pages" : "regular pages");
    DOT_PRINT("  Commit expand:     %M\n", arena->commit_expand_size);
}
