internal void
threadctx_init(const ThreadCtxOptions* thread_ctx_opts, u8 thread_id)
{
    DOT_ASSERT(thread_ctx_opts);
    if(thread_ctx_opts->per_thread_temp_arena_count < 2){
      DOT_WARNING("We probably need at least two temp_arenas to double buffer "
        "through callstack");
    }

    u8 temp_arena_count = thread_ctx_opts->per_thread_temp_arena_count;

    // NOTE: Storing space for the actual arena backing + extra page for arena metadata
    u64 total_memory = thread_ctx_opts->per_thread_temp_arena_size * temp_arena_count
        + MAX(sizeof(Arena) * temp_arena_count, PLATFORM_REGULAR_PAGE_SIZE);

    u64 per_arena_memory = thread_ctx_opts->per_thread_temp_arena_size;

    Arena *main_arena = ARENA_ALLOC(.reserve_size = total_memory, .name = "Thread Arena");
    thread_ctx.thread_id        = thread_id;
    thread_ctx.temp_arena_count = temp_arena_count;
    thread_ctx.temp_arenas      = PUSH_ARRAY(main_arena, Arena*, temp_arena_count);

    for(u8 i = 0; i < temp_arena_count; ++i){
        thread_ctx.temp_arenas[i] = ARENA_ALLOC(
            .parent       = main_arena,
            .reserve_size = per_arena_memory,
            .name         = cstr_format(main_arena, "Thread SubArena %u", i),
        );
    }
}

internal void
threadctx_shutdown()
{
    Arena* main_arena = thread_ctx.temp_arenas[0]->parent;
    for(u8 i = 0; i < thread_ctx.temp_arena_count; ++i){
        arena_print_debug(thread_ctx.temp_arenas[i]);
    }
    ARENA_FREE(main_arena);
}


internal TempArena
threadctx_get_temp(Arena *avoid[], u32 avoid_count)
{
    // Spread memory usage across arenas
    local_persist thread_local u64 next = 0;
    u32 arena_count = thread_ctx.temp_arena_count;
    for(u8 tries = 0; tries < arena_count; ++tries){
        u8 idx = next;
        next = (next + 1) % arena_count;

        Arena *candidate_temp = thread_ctx.temp_arenas[idx];
        DOT_ASSERT(candidate_temp);

        b32 collision = false;
        for(u32 i = 0; i < avoid_count; ++i){
            if(candidate_temp == avoid[i]){
                collision = true;
                break;
            }
        }

        if(!collision){
            return temp_arena_get(candidate_temp);
        }
    }

    DOT_ERROR("Could not find non-colliding TempArena. Try increasing temp_arena_count.");
}
