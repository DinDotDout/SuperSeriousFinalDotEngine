internal void
threadctx_init(const ThreadCtxOpts* thread_ctx_opts){
    DOT_ASSERT(thread_ctx_opts);
    DOT_ASSERT(thread_ctx_opts->per_thread_temp_arena_count > 1,
        "We need at least two temp_arenas to double buffer through callstack");

    u8 temp_arena_count = thread_ctx_opts->per_thread_temp_arena_count;
    // NOTE: Storing space for the temp_arena array is causing page overhang
    // that we are taking advantage off to then store Thread SubArena names. If
    // this ever blows up we know where to check first
    u64 total_memory = thread_ctx_opts->memory_size + sizeof(Arena*)*temp_arena_count;
    u64 per_arena_memory = thread_ctx_opts->memory_size / temp_arena_count;

    Arena *main_arena = ARENA_ALLOC(.reserve_size = total_memory, .name = "Thread Arena");
    thread_ctx.temp_arena_count = temp_arena_count;
    thread_ctx.temp_arenas      = PUSH_ARRAY(main_arena, Arena*, temp_arena_count);
    for(u8 i = 0; i < temp_arena_count; ++i){
        thread_ctx.temp_arenas[i] = ARENA_ALLOC(
            .parent       = main_arena,
            .reserve_size = per_arena_memory,
            .name         =  cstr_format(main_arena, "Thread SubArena %u", i),
        );
    }
}

internal void
threadctx_destroy(){
    Arena* main_arena = thread_ctx.temp_arenas[0]->parent;
    for(u8 i = 0; i < thread_ctx.temp_arena_count; ++i){
        arena_print_debug(thread_ctx.temp_arenas[i]);
    }
    arena_free(main_arena);
}

internal TempArena
threadctx_get_temp(Arena *alloc_arena){
    for(u8 i = 0; i < thread_ctx.temp_arena_count; ++i){
        Arena* candidate_temp = thread_ctx.temp_arenas[i];
        DOT_ASSERT(candidate_temp);
        if(candidate_temp != alloc_arena)
            return temp_arena_get(candidate_temp);
    }
    DOT_ERROR("Could not find a valid temporary arena");
    return TempArena_Nil;
}
