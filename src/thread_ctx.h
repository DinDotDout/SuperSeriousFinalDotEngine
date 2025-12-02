 typedef struct ThreadCtxOpts{
        usize memory_size;
        u8 thread_id;
        // u8* memory;
}ThreadCtxOpts;

typedef struct ThreadCtx{
        Arena arenas[2];
}ThreadCtx;

thread_local ThreadCtx thread_ctx;
DOT_STATIC_ASSERT(ArrayCount(thread_ctx.arenas) > 1);



internal void ThreadCtx_Init(const ThreadCtxOpts*);
internal TempArena Memory_GetScratch(Arena *alloc_arena);

