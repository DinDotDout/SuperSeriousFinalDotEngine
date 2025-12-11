internal TempArena TempArena_Get(Arena *arena){
    TempArena sa;
    sa.arena = arena;
    sa.prevOffset = arena->used;
    return sa;
}

internal void TempArena_Restore(TempArena *sa){
    sa->arena->used = sa->prevOffset;
}

internal Arena Arena_CreateFromMemory_(u8* base, ArenaInitParams* params){
    DOT_ASSERT_FL(base != NULL, params->reserve_location, params->reserve_line, "Invalid memory provided");
    Arena a = {.base = base, .used = 0, .reserved = params->reserve_size, .name = params->name};
    return a;
}

// TODO (joan): should expand to use Platform_Reserve / Platform_Commit
internal Arena Arena_Alloc_(ArenaInitParams *params){
    DOT_PRINT_FL(params->reserve_location, params->reserve_line, "Arena: requested %zuKB", params->reserve_size / 1024);
    Arena arena = {.reserved = params->reserve_size, .name = params->name};
    u8 *memory = (u8 *)malloc(params->reserve_size); // Malloc guarantes 8B aligned at least

    // u64 reserved = params->reserve_size;
    // u64 initial_commit = params->commit_size;
    // if (params->large_pages){
    // reserved = AlignPow2(reserved, 
    // u8 *memory = (u8 *)OS_Reserve(params->reserve_size); // align to page size
    // }else{
    // u64 reserved = params
    // }
    DOT_ASSERT_FL(memory, params->reserve_location, params->reserve_line, "Could not allocate");
    arena.base = memory;
    return arena;
}

internal void Arena_Reset(Arena *arena){ arena->used = 0; }

// Not needed as will last program duration probably.
internal void Arena_Free(Arena *arena){
    free(arena->base);
    arena->used = 0;
    arena->reserved = 0;
}

// TODO (joan): should expand to use Platform_Reserve / Platform_Commit
internal u8 *Arena_Push(Arena *arena, usize size, usize alignment, char* file, u32 line){
    DOT_ASSERT_FL(size > 0, file, line);
    usize current_address = cast(usize)arena->base + arena->used;
    usize aligned_address = AlignPow2(current_address, alignment);
    u8 *mem_offset = cast(u8*) aligned_address;

    usize required = (aligned_address - cast(usize)arena->base) + size;

    DOT_ASSERT_FL((cast(usize)mem_offset % alignment) == 0, file, line);
    if(DOT_Unlikely(required > arena->reserved)){
        DOT_ERROR_FL(file, line,
                     "Arena out of bounds: requested %zuKB (used=%zu), reserve_size=%zu",
                     (required / 1024), (arena->used / 1024), (arena->reserved / 1024));
        return NULL;
    }
    arena->used = required;
    return mem_offset;
}
