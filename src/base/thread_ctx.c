internal void
threadctx_init_arenas_(Arena *arena, u32 arena_count, u64 arena_size)
{
    DOT_ASSERT(t_thread_ctx.temp_arenas == NULL && t_thread_ctx.temp_arenas == 0, "Thread %u ctx was already initialized", t_thread_ctx.thread_id);
    u64 per_arena_memory = arena_size;
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
threadctx_init(Arena *arena, u32 arena_count, u64 arena_size)
{
    threadctx_init_arenas_(arena, arena_count, arena_size);
    t_thread_ctx.thread_id = ins_atomic_u32_add_eval(&g_thread_id_counter, 1);
}

internal void
threadctx_shutdown()
{
    for(u8 i = 0; i < t_thread_ctx.temp_arena_count; ++i){
        arena_print_debug(t_thread_ctx.temp_arenas[i]);
    }
    if(g_backing_fallback){
        ARENA_DESTROY(g_backing_fallback);
    }
}

internal u32
threadctx_id()
{
    DOT_ASSERT(t_thread_ctx.thread_id != 0, "Thread ctx not initialized, call threadctx_init on this thread first");
    return t_thread_ctx.thread_id - 1;
}

internal u32
threadctx_count()
{
    DOT_ASSERT(t_thread_ctx.thread_id != 0, "Thread ctx not initialized, call threadctx_init on this thread first");
    return g_thread_id_counter;
}

internal Scratch
scratch_begin(u32 avoid_count, Arena *avoid_arenas[])
{
    if(!t_thread_ctx.temp_arenas || t_thread_ctx.temp_arena_count == 0){
        g_backing_fallback = ARENA_CREATE();
        threadctx_init_arenas_(g_backing_fallback, 2, g_backing_fallback->reserved);
        DOT_ASSERT(t_thread_ctx.temp_arenas && t_thread_ctx.temp_arena_count > 0, "Thread context not initialized");
    }

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
