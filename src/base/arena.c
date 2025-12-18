#include "base/dot.h"
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
    DOT_ASSERT_FL(base != NULL, params->reserve_file, params->reserve_line, "Invalid memory provided");
    Arena a = {.base = base, .used = 0, .reserved = params->reserve_size, .name = params->name};
    return a;
}

internal Arena* Arena_Alloc_(ArenaInitParams *params){
    DOT_ASSERT_FL(params->reserve_size > 0, params->reserve_file, params->reserve_line, "No reserve_size provided");
    DOT_PRINT_FL(params->reserve_file, params->reserve_line, "Arena: requested %M", params->reserve_size);

    u64 page_size = params->large_pages ? PLATFORM_LARGE_PAGE_SIZE : PLATFORM_REGULAR_PAGE_SIZE;

    u64 reserved           = AlignPow2(params->reserve_size, page_size);
    u64 initial_commit     = AlignPow2(Max(params->commit_size, page_size), page_size);
    u64 commit_expand_size = AlignPow2(Max(params->commit_expand_size, page_size), page_size);
    initial_commit = Min(initial_commit, reserved);

    void *memory = OS_Reserve(reserved);
    if(params->large_pages){
        OS_CommitLarge(memory, initial_commit);
    }else{
        OS_Commit(memory, initial_commit);
    }

    DOT_ASSERT_FL(memory, params->reserve_file, params->reserve_line, "Could not allocate");

    Arena* arena = cast(Arena*)memory;
    arena->base               = memory;
    arena->used               = sizeof(Arena);
    arena->reserved           = reserved;
    arena->committed          = initial_commit;
    arena->commit_expand_size = commit_expand_size;
    arena->large_pages        = params->large_pages;

    AsanPoison(arena->base+arena->used, initial_commit-arena->used);
    return arena;
}

internal void Arena_Reset(Arena *arena){
    AsanPoison(arena->base, arena->committed);
    arena->used = 0;
}

internal void Arena_Free(Arena *arena){
    arena->used = 0;
    arena->reserved = 0;
    arena->commit_expand_size = 0;
    OS_Release(arena->base, arena->reserved);
}

internal void* Arena_Push(Arena *arena, usize alloc_size, usize alignment, char* file, u32 line){
    DOT_ASSERT_FL(alloc_size > 0, file, line);
    uptr arena_base = cast(uptr)arena->base;
    uptr current_address =  arena_base + arena->used;
    uptr aligned_address = AlignPow2(current_address, alignment);
    u8 *mem_offset = cast(u8*) aligned_address;
    DOT_ASSERT_FL((aligned_address % alignment) == 0, file, line, "Unaligned address");
    usize required_padded = aligned_address - current_address + alloc_size;
    arena->used += required_padded;
    if(DOT_Unlikely(arena->used > arena->reserved)){
        DOT_ERROR_FL(file, line,
                     "Arena out of memory! "
                     ": requested = %M total used =  reserved = %M",
                     (required_padded), (arena->reserved));
        return NULL;
    }

    // NOTE: We only commit / touch the memory on the first run through
    if(arena->used >= arena->committed){
        usize leftover_size = arena->reserved - arena->committed;
        usize commit_requirement = Min(leftover_size, arena->commit_expand_size);;
        uptr commit_pos = arena_base + arena->committed;
        if(arena->large_pages){
            OS_CommitLarge(cast(void*)commit_pos, commit_requirement);
        }else{
            OS_Commit(cast(void*)commit_pos, commit_requirement);
        }
        arena->committed += commit_requirement;
    }
    AsanUnpoison(mem_offset, required_padded);
    return mem_offset;
}
