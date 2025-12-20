#include "base/dot.h"
internal TempArena TempArena_Get(Arena *arena){
    TempArena sa;
    sa.arena = arena;
    sa.prevOffset = arena->used;
    return sa;
}

internal void TempArena_Restore(TempArena *temp){
    temp->arena->used = temp->prevOffset;
}

// WARN: Since for now we only get memory from arenas, we will always have
// subarenas with prefaulted memory so we set committed == reserved
internal Arena* Arena_AllocFromMemory_(u8* memory, ArenaInitParams* params){
    DOT_ASSERT_FL(memory != NULL, params->reserve_file, params->reserve_line, "Invalid memory provided");
    Arena* arena = cast(Arena*)memory;
    arena->base               = memory+sizeof(Arena);
    arena->used               = 0;
    arena->committed          = params->reserve_size;
    arena->reserved           = params->reserve_size;
    arena->commit_expand_size = params->commit_expand_size;
    arena->large_pages        = params->large_pages;
    arena->name               = params->name;
    return arena;
}

internal Arena* Arena_Alloc_(ArenaInitParams *params){
    DOT_ASSERT_FL(params->reserve_size > 0, params->reserve_file, params->reserve_line, "No reserve_size provided");
    DOT_PRINT_FL(params->reserve_file, params->reserve_line, "Arena \"%s\": requested %M", params->name, params->reserve_size);

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
    arena->name               = params->name;

    AsanPoison(arena->base+arena->used, arena->reserved-arena->used);
    return arena;
}

internal void Arena_Reset(Arena *arena){
    AsanPoison(arena->base, arena->committed);
    arena->used = sizeof(Arena); // Keep arena data valid :)
}

internal void Arena_Free(Arena *arena){
    u64 reserved = arena->reserved;
    void* base = arena->base;
    DOT_PRINT("Arena \"%s\": freed %M", arena->name, arena->reserved);
    arena->used = 0;
    arena->reserved = 0;
    arena->commit_expand_size = 0;
    OS_Release(cast(void*)base, reserved);
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

    i64 page_overhang = arena->used - arena->committed;
    if(page_overhang > 0){
        u64 page_size = arena->large_pages ? PLATFORM_LARGE_PAGE_SIZE
                                        : PLATFORM_REGULAR_PAGE_SIZE;

        usize need_aligned = AlignPow2(page_overhang, page_size);
        usize leftover     = arena->reserved - arena->committed;
        usize commit_size  = Max(arena->commit_expand_size, need_aligned);
        if(commit_size > leftover) commit_size = need_aligned;
        if(DOT_Unlikely(commit_size > leftover)){
            DOT_ERROR_FL(file, line,
                "Can't commit more memory! needed = %M; leftover = %M", commit_size, leftover);
            return NULL;
        }

        uptr commit_pos = arena_base + arena->committed;
        if(arena->large_pages){
            OS_CommitLarge((void*)commit_pos, commit_size);
        }else{
            OS_Commit((void*)commit_pos, commit_size);
        }
        arena->committed += commit_size;
    }
    AsanUnpoison(mem_offset, required_padded);
    return mem_offset;
}
