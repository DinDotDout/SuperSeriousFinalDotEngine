internal void
threadctx_init(Arena *arena, u32 arena_count, u32 arena_size, u32 thread_id)
{
    DOT_ASSERT(t_thread_ctx.thread_id == 0, "Thread %u ctx was already initialized", thread_id);
    u64 per_arena_memory = arena_size;
    t_thread_ctx.thread_id = thread_id;
    SLICE_INIT(arena, &t_thread_ctx.temp_arenas, arena_count);
    for(u8 i = 0; i < t_thread_ctx.temp_arenas.count; ++i){
        SLICE_GET(t_thread_ctx.temp_arenas, i) = ARENA_CREATE(
            .parent       = arena,
            .reserve_size = per_arena_memory,
            .name         = cstr_format(arena, "Thread SubArena %u", i),
        );
    }
}

internal void
threadctx_shutdown()
{
    for(u8 i = 0; i < t_thread_ctx.temp_arenas.count; ++i){
        arena_print_debug(SLICE_GET(t_thread_ctx.temp_arenas, i));
    }
}

// (jd) NOTE: Using 0 as unitialized state internally.
// Externally it is still a valid idx
internal u32
threadctx_id()
{
    return t_thread_ctx.thread_id - 1;
}

internal TempArena
threadctx_temp_begin(SliceArena *avoid_arenas)
// threadctx_temp_begin(Arena *avoid[], u32 avoid_count)
{
    u32 avoid_count = !avoid_arenas ? 0 : avoid_arenas->count;
    // (jd) NOTE: Spread memory load
    local_persist thread_local u64 next = 0;
    u32 arena_count = t_thread_ctx.temp_arenas.count;
    for(u8 tries = 0; tries < arena_count; ++tries){
        u8 idx = next;
        next = (next + 1) % arena_count;

        Arena *candidate_temp = SLICE_GET(t_thread_ctx.temp_arenas, idx);
        b32 collision = false;
        for(u32 i = 0; i < avoid_count; ++i){
            Arena *arena = SLICE_GET(*avoid_arenas, i);
            if(candidate_temp == arena){
                collision = true;
                break;
            }
        }

        if(!collision){
            return temp_arena_start(candidate_temp);
        }
    }

    DOT_ERROR("Could not find non-colliding TempArena. Try increasing temp_arena_count to match max number of arena parameters");
}

internal void
threadctx_temp_end(TempArena temp)
{
    temp_arena_restore(temp);
}
