#ifndef THREAD_CTX_H
#define THREAD_CTX_H

typedef struct ThreadCtxOptions{
    usize per_thread_temp_arena_size;
    u8    per_thread_temp_arena_count;
}ThreadCtxOptions;

typedef SLICE(Arena*) SliceArena;
#define SLICE_ARENA_LIT(...) (SliceArena)SLICE_LIT(Arena*, __VA_ARGS__)

internal thread_local struct ThreadCtx{
    u8      thread_id;
    SliceArena temp_arenas;
}t_thread_ctx = {0};

internal void threadctx_init(Arena *arena, const ThreadCtxOptions *thread_ctx_opts, u8 thread_id);
internal void threadctx_destroy();

//////////////////////////////////////////////////////////
// This is supposed to be restored with temp_arena_restore before function exit.
// avoid is a list of arenas we want to compare the thread arenas to in case
// our function recieves a temp as parameter.
//
// SLICE_ARENA_LIT has been added to just be able to do something like:
// threadctx_get_temp(SLICE_ARENA_LIT(a, a2, a3));
// for the base cases of 0 we can just pass in NULL or 0 threadctx_get_temp(0)
internal TempArena threadctx_get_temp(SliceArena *avoid_arenas);

// TODO: Implement actual threading constructs
typedef struct ThreadCtxMutex{
    u64 h[1];
}ThreadCtxMutex;

#define MUTEX_SCOPE(mutex) DEFER_LOOP(mutex_Lock(mutex), mutex_unlock(mutex))
#endif // !THREAD_CTX_H
