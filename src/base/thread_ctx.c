internal void threadctx_init(const ThreadCtxOpts* thread_ctx_opts){
    // TODO: Update thread id from thread_ctx_opts
    thread_ctx.arenas[0] = ARENA_ALLOC(.reserve_size = thread_ctx_opts->memory_size, .name = "Th0 arena0");
    thread_ctx.arenas[1] = ARENA_ALLOC(.reserve_size = thread_ctx_opts->memory_size, .name = "Th0 arena1");
}

internal void threadctx_destroy(){
    // Arena_PrintDebug(thread_ctx.arenas[0]);
    // Arena_PrintDebug(thread_ctx.arenas[1]);
    arena_free(thread_ctx.arenas[0]);
    arena_free(thread_ctx.arenas[1]);
}

internal TempArena thread_ctx_get_temp(Arena *alloc_arena){
    for EACH_ELEMENT(i, thread_ctx.arenas){
        Arena* candidate_temp = thread_ctx.arenas[i];
        DOT_ASSERT(candidate_temp);
        if(candidate_temp != alloc_arena)
            return temp_arena_get(candidate_temp);
    }
    DOT_ERROR("Could not find a valid temporary arena");
    return TempArena_Nil;
}
