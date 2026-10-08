internal void
threadctx_init(Arena *arena, u32 arena_count, u32 arena_size, u32 thread_id)
{
    DOT_ASSERT(t_thread_ctx.thread_id == 0, "Thread %u ctx was already initialized", thread_id);
    u64 per_arena_memory = arena_size;
    t_thread_ctx.thread_id = thread_id;
    t_thread_ctx.temp_arena_count = arena_count;
    t_thread_ctx.temp_arenas = PUSH_ARRAY(arena, Arena*, arena_count);
    for(u8 i = 0; i < t_thread_ctx.temp_arena_count; ++i){
        t_thread_ctx.temp_arenas[i] = ARENA_CREATE(
            .parent       = arena,
            .reserve_size = per_arena_memory,
            .name         = cstr_format(arena, "Thread SubArena %u", i),
        );
    }
}

internal void
threadctx_shutdown()
{
    for(u8 i = 0; i < t_thread_ctx.temp_arena_count; ++i){
        arena_print_debug(t_thread_ctx.temp_arenas[i]);
    }
}

// (jd) NOTE: Using 0 as unitialized state internally.
// Externally it is still a valid idx
internal u32
threadctx_id()
{
    return t_thread_ctx.thread_id - 1;
}

internal Scratch
scratch_begin(u32 avoid_count, Arena *avoid_arenas[])
{
    DOT_ASSERT(t_thread_ctx.temp_arenas && t_thread_ctx.temp_arena_count > 0, "Thread context not initialized");

    // (jd) NOTE: Spread memory load on temp arenas
    local_persist thread_local u32 next = 0;
    for(u8 tries = 0; tries < t_thread_ctx.temp_arena_count; ++tries){
        Arena *candidate_temp = t_thread_ctx.temp_arenas[next];
        next = (next + 1) % t_thread_ctx.temp_arena_count;

        b32 collision = false;
        for(u32 i = 0; i < avoid_count; ++i){
            Arena *arena = avoid_arenas[i];
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
scratch_end(TempArena temp)
{
    temp_arena_restore(temp);
}
