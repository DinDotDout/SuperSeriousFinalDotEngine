#ifndef THREAD_CTX
#define THREAD_CTX
typedef struct ThreadCtxOpts{
    usize memory_size;
    u8 thread_id;
}ThreadCtxOpts;

typedef struct ThreadCtx{
    Arena* arenas[2];
    u8 thread_id;
}ThreadCtx;

internal thread_local ThreadCtx thread_ctx;
internal thread_local const TempArena TempArena_Nil = {0};

DOT_STATIC_ASSERT(ARRAY_COUNT(thread_ctx.arenas) > 1);

internal void threadctx_init(const ThreadCtxOpts*);
internal void threadctx_destroy();

/* This is supposed to be freed with temp_arena_restore before scope / function exit
alloc_arena should be the arena we want to return allocations from if any to avoid
selection one of the thread_ctx arenas */
internal TempArena thread_ctx_get_temp(Arena *alloc_arena);

// TODO: Implement threading
#define MUTEX_SCOPE(mutex) DEFER_LOOP(Mutex_Lock(mutex), Mutex_Unlock(mutex))
#endif
