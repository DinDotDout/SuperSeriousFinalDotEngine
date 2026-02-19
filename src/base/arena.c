internal TempArena
temp_arena_get(Arena *arena){
    TempArena sa;
    sa.arena = arena;
    sa.prev_offset = arena->used;
    return sa;
}

internal void
temp_arena_restore(TempArena temp){
    temp.arena->used = temp.prev_offset;
}

force_inline internal Arena*
arena_alloc_(ArenaInitParams *params){
    Arena* arena;
    if(params->buffer){
        // NOTE: Keeping this use case just in case, if we ever end up using it
        // we will have to add some check to not try to free the handed buffer as
        // the arena will not really own it
        // DOT_ERROR("Not sure we will ever use this directly?");
        arena = arena_alloc_from_memory(params);
    }else if(params->parent){
        arena = arena_alloc_from_arena(params);
    }else{
        arena = arena_alloc_from_os(params);
    }
    return arena;
}

internal Arena*
arena_alloc_from_memory(ArenaInitParams *params){
    DOT_ASSERT_FL(params->buffer != NULL, params->reserve_file, params->reserve_line, "Invalid memory provided");
    DOT_PRINT_FL(params->reserve_file, params->reserve_line, "Allocating arena from buffer");
    // NOTE: Since we don't own the memory it must be backed and we assume it is committed (reserve == commit)
    Arena* arena              = cast(Arena*) params->buffer;
    arena->base               = params->buffer;
    arena->reserved           = params->reserve_size;
    arena->committed          = params->reserve_size;
    arena->commit_expand_size = params->commit_expand_size;
    arena->large_pages        = params->large_pages;
    arena->name               = params->name;
    arena->parent             = params->parent;
    arena_reset(arena);
    return arena;
}

internal Arena*
arena_alloc_from_arena(ArenaInitParams *params){
    DOT_ASSERT_FL(params->parent != NULL, params->reserve_file, params->reserve_line, "Invalid memory provided");
    DOT_PRINT_FL(params->reserve_file, params->reserve_line, "Allocating arena from parent");
    params->buffer = PUSH_SIZE_NO_ZERO(params->parent, params->reserve_size);
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

    Arena* arena = cast(Arena*) memory;
    arena->base               = memory;
    arena->reserved           = reserved;
    arena->committed          = initial_commit;
    arena->commit_expand_size = commit_expand_size;
    arena->large_pages        = params->large_pages;
    arena->name               = params->name;
    arena->parent             = params->parent;
    arena_reset(arena);
    return arena;
}

internal void
arena_reset(Arena *arena){
    u64 arena_size = sizeof(Arena);
    ASAN_POISON(arena->base+arena_size, arena->reserved-arena_size);
    arena->used = arena_size;
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
arena_push(Arena *arena, usize alloc_size, usize alignment, b8 zero, char *file, u32 line){
    DOT_ASSERT_FL(alloc_size > 0, file, line);
    uptr arena_base = cast(uptr)arena->base;
    uptr current_address =  arena_base + arena->used;
    uptr aligned_address = ALIGN_POW2(current_address, alignment);
    void *mem_offset = cast(void*) aligned_address;
    DOT_ASSERT_FL((aligned_address % alignment) == 0, file, line, "Unaligned address");
    usize required_padded = aligned_address - current_address + alloc_size;
    arena->used += required_padded;

    // NOTE: Up to here we have calculated how much memory do we need exactly. We will now check
    // if we need to further commit pages to back up our needs
    u64 need_zero = required_padded;
    // NOTE: Fresh OS pages are zeroed, so we only need to zero the portion we already owned
    b8 need_more_pages = arena->used > arena->committed;
    if(need_more_pages){
        i64 needed_memory = arena->used - arena->committed;
        need_zero -= needed_memory;

        u64 page_size = arena->large_pages ? PLATFORM_LARGE_PAGE_SIZE
                                        : PLATFORM_REGULAR_PAGE_SIZE;
        usize need_aligned = ALIGN_POW2(needed_memory, page_size);
        usize commit_size  = MAX(arena->commit_expand_size, need_aligned);
        usize arena_leftover_memory = arena->reserved - arena->committed;
        if(commit_size > arena_leftover_memory) commit_size = need_aligned;
        if(DOT_UNLIKELY(commit_size > arena_leftover_memory)){
            DOT_ERROR_FL(file, line,
                "Can't commit more memory! needed = %M; leftover = %M", commit_size, arena_leftover_memory);
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
    if(zero && need_zero > 0){
        MEMORY_ZERO(mem_offset, need_zero);
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
    DOT_PRINT("  Used:              %M", arena->used);
    DOT_PRINT("  Committed:         %M (%llu pages)", arena->committed, committed_pages);
    DOT_PRINT("  Reserved:          %M (%llu pages)", arena->reserved,  reserved_pages);
    DOT_PRINT("  Page size:         %M (%s)", page_size, arena->large_pages ? "large pages" : "regular pages");
    DOT_PRINT("  Commit expand:     %M\n", arena->commit_expand_size);
}
