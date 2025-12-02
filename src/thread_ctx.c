internal void ThreadCtx_Init(const ThreadCtxOpts* thread_ctx_opts){
    thread_ctx.arenas[0] = Arena_Alloc(.capacity = thread_ctx_opts->memory_size, .name = "Thread 0 a" DOT_STR(0));
    thread_ctx.arenas[1] = Arena_Alloc(.capacity = thread_ctx_opts->memory_size, .name = "Thread 0 a" DOT_STR(1));
}

// void Memory_InitGlobalScratch(usize size){
    // global_arena = Arena_Alloc(.capacity = size, .name = "Global Scratch");
// }


static TempArena TempArena_Nil = {0};
TempArena Memory_GetScratch(Arena *alloc_arena){
    for EachElement(i, thread_ctx.arenas){
        Arena* candidate_temp = &thread_ctx.arenas[i];
        DOT_ASSERT(candidate_temp);
        if (candidate_temp != alloc_arena)
            return TempArena_Get(candidate_temp);
    }
    DOT_ERROR("Could not find a valid temporary arena");
    return TempArena_Nil;
}
