#ifndef THREAD_CTX
#define THREAD_CTX
typedef struct ThreadCtxOpts{
    usize memory_size;
    u8 thread_id;
}ThreadCtxOpts;

typedef struct ThreadCtx{
    Arena arenas[2];
    u8 thread_id;
}ThreadCtx;

internal thread_local ThreadCtx thread_ctx;
internal thread_local const TempArena TempArena_Nil = {0};

DOT_STATIC_ASSERT(ArrayCount(thread_ctx.arenas) > 1);

internal void ThreadCtx_Init(const ThreadCtxOpts*);
internal TempArena Memory_GetScratch(Arena *alloc_arena);


// TODO: Implement threading
#define MutexScope(mutex) DeferLoop(Mutex_Lock(mutex), Mutex_Unlock(mutex))
#endif
