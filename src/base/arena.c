internal TempArena
temp_arena_get(Arena *arena)
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

internal Arena*
arena_create_(ArenaInitParams *params)
{
    Arena *arena;
    if(params->buffer){
        // NOTE: Keeping this use case just in case. If we ever end up using it
        // we will have to add some check to not try to free the handed buffer as
        // the arena will not really own it
        // DOT_ERROR("Not sure we will ever use this directly?");
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
    DOT_ASSERT_FL(params->buffer != NULL, params->reserve_file, params->reserve_line, "Invalid memory provided");
    DOT_PRINT_FL(params->reserve_file, params->reserve_line, "Allocating arena from buffer with %M", params->reserve_size);
    // NOTE: Since we don't own the memory it must be backed and we assume it is committed (reserve == commit)

    DOT_ASSERT_FL(params->buffer != NULL, params->reserve_file, params->reserve_line, "Invalid memory provided");
    uptr base = cast(uptr)params->buffer;
    uptr align = ALIGN_POW2(base, DOT_ALIGNOF(Arena));
    iptr diff = align - base;
    Arena *arena = cast(Arena*)align;
    *arena = (Arena){
        .base               = cast(u8*)align,
        .reserved           = params->reserve_size - diff,
        .committed          = params->reserve_size - diff,
        .commit_expand_size = params->commit_expand_size,
        .flags              = params->large_pages ? ArenaFlags_LargePages : 0,
#if DOT_DEBUG
        .name               = params->name,
        .parent             = params->parent,
#endif
    };
    ARENA_RESET(arena);
    return arena;
}

internal Arena*
arena_create_from_arena(ArenaInitParams *params)
{
    DOT_ASSERT_FL(params->parent != NULL, params->reserve_file, params->reserve_line, "Invalid memory provided");
    DOT_PRINT_FL(params->reserve_file, params->reserve_line, "Allocating arena from parent:");
    arena_print_debug(params->parent);
    params->buffer = PUSH_SIZE_NO_ZERO(params->parent, params->reserve_size);
    Arena *arena = arena_create_from_memory(params);
    printf("\n");
    printf("\n");
    return arena;
}

internal Arena*
arena_create_from_os(ArenaInitParams *params)
{
    DOT_PRINT_FL(params->reserve_file, params->reserve_line, "Allocating arena from os");
    DOT_ASSERT_FL(params->reserve_size > 0, params->reserve_file, params->reserve_line, "No reserve_size provided");
    DOT_PRINT_FL(params->reserve_file, params->reserve_line, "Arena \"%s\": requested %M", params->name, params->reserve_size);

    u64 page_size = params->large_pages ? PLATFORM_LARGE_PAGE_SIZE : PLATFORM_REGULAR_PAGE_SIZE;

    u64 reserved           = ALIGN_POW2(params->reserve_size, page_size);
    u64 initial_commit     = ALIGN_POW2(DOT_MAX(params->commit_size, page_size), page_size);
    u64 commit_expand_size = ALIGN_POW2(DOT_MAX(params->commit_expand_size, page_size), page_size);
    initial_commit         = DOT_MIN(initial_commit, reserved);

    void *memory = os_reserve(reserved);
    if(params->large_pages){
        os_commit_large(memory, initial_commit);
    }else{
        os_commit(memory, initial_commit);
    }
    DOT_ASSERT_FL(memory, params->reserve_file, params->reserve_line, "Could not allocate");
    Arena* arena = cast(Arena*) memory;
    *arena = (Arena){
        .base               = memory,
        .reserved           = reserved,
        .committed          = initial_commit,
        .commit_expand_size = commit_expand_size,
        .flags              = ArenaFlags_OwnsMemory |
            (params->large_pages ? ArenaFlags_LargePages : 0),
#if DOT_DEBUG
        .name               = params->name,
        .parent             = params->parent,
#endif
    };
    ARENA_RESET(arena);
    return arena;
}

internal void
arena_reset(ArenaOpParams *op)
{
    DOT_ASSERT_FL(op->arena, op->file, op->line);
    ASAN_POISON(arena->base + ARENA_HEADER_SIZE_B, arena->reserved - ARENA_HEADER_SIZE_B);
    op->arena->used = ARENA_HEADER_SIZE_B;
}

internal void
arena_destroy(ArenaOpParams *op)
{
    Arena *arena = op->arena;
    u64 reserved = arena->reserved;
    void* base = arena->base;
    arena->used = 0;
    arena->reserved = 0;
    arena->commit_expand_size = 0;
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
arena_push(ArenaOpParams *op)
{
    Arena *arena = op->arena;
    if(op->size < 0){
        // DOT_ASSERT_FL(op->size > 0, op->file, op->line, "Allocation should be bigger than 0");
        return NULL;
    }
    uptr arena_base = cast(uptr)arena->base;
    uptr current_address =  arena_base + arena->used;
    uptr aligned_address = ALIGN_POW2(current_address, op->alignment);
    u8 *mem_offset = cast(u8*) aligned_address;
    DOT_ASSERT_FL((aligned_address % op->alignment) == 0, op->file, op->line, "Unaligned address");
    usize required_padded = aligned_address - current_address + op->size;
    arena->used += required_padded;

    // NOTE: Fresh OS pages are zeroed, so we only need to zero the portion we already owned
    // We could maybe make a non reset permanent owning arena type and have it never zero
    // u64 need_zero = required_padded;
 
    // NOTE: Up to here we have calculated how much memory do we need exactly. We will now check
    // if we need to further commit pages to back up our needs
    b32 need_more_pages = arena->used > arena->committed;
    if(need_more_pages){
        i64 needed_memory = arena->used - arena->committed;
        // need_zero -= needed_memory;

        b32 large_pages = DOT_BITS_MATCH(arena->flags, ArenaFlags_LargePages);
        u64 page_size = large_pages ? PLATFORM_LARGE_PAGE_SIZE : PLATFORM_REGULAR_PAGE_SIZE;
        usize need_aligned = ALIGN_POW2(needed_memory, page_size);
        usize commit_size  = DOT_MAX(arena->commit_expand_size, need_aligned);
        usize arena_leftover_memory = arena->reserved - arena->committed;
        if(commit_size > arena_leftover_memory) commit_size = need_aligned;
        if(DOT_UNLIKELY(commit_size > arena_leftover_memory)){
            arena_print_debug(arena);
            DOT_ERROR_FL(op->file, op->line, "Can't commit more memory total = %M, needed = %M; leftover = %M", arena->reserved, commit_size, arena_leftover_memory);
            return NULL;
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
    // if(zero && need_zero > 0){
        // MEMORY_ZERO(mem_offset, need_zero);
    if(op->zero){
        MEMORY_ZERO(mem_offset, required_padded);
    }
    return mem_offset;
}

internal void
arena_free()
{
}

internal String8
arena_to_string(Arena *arena)
{
    String8 str = String8Lit("");
    if(!arena){
        str = string8_format(arena, "Arena: <null>\n");
        return str;
    }

    b32 large_pages = DOT_BITS_MATCH(arena->flags, ArenaFlags_LargePages);
    const u64 page_size = large_pages ? PLATFORM_LARGE_PAGE_SIZE : PLATFORM_REGULAR_PAGE_SIZE;

    const u64 committed_pages = arena->committed / page_size;
    const u64 reserved_pages  = arena->reserved / page_size;
    str = string8_format(arena, 
        "Arena '%s'\n "
        "  Arena parent: '%s'\n"
        "  Base:              %p\n"
        "  Used:              %M\n"
        "  Committed:         %M (%llu pages)\n"
        "  Reserved:          %M (%llu pages)\n"
        "  Page size:         %M (%s)\n"
        "  Commit expand:     %M\n",
        arena->name ? arena->name : "<unnamed>",
        arena->parent ? arena->parent->name : "no parent",
        arena->base,
        arena->used,
        arena->committed, committed_pages,
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

    const u64 committed_pages = arena->committed / page_size;
    const u64 reserved_pages  = arena->reserved / page_size;
    DOT_PRINT("Arena '%s'", arena->name ? arena->name : "<unnamed>");
    DOT_PRINT("  Arena parent: '%s'", arena->parent ? arena->parent->name : "no parent");
    DOT_PRINT("  Base:              %p", arena->base);
    DOT_PRINT("  Used:              %M", arena->used);
    DOT_PRINT("  Committed:         %M (%llu pages)", arena->committed, committed_pages);
    DOT_PRINT("  Reserved:          %M (%llu pages)", arena->reserved,  reserved_pages);
    DOT_PRINT("  Page size:         %M (%s)", page_size, large_pages ? "large pages" : "regular pages");
    DOT_PRINT("  Commit expand:     %M\n", arena->commit_expand_size);
}
