#ifndef THREAD_CTX
#define THREAD_CTX
#include <threads.h>

typedef struct ThreadCtxOptions{
    usize per_thread_temp_arena_size;
    u8    per_thread_temp_arena_count;
}ThreadCtxOptions;

internal thread_local struct ThreadCtx{
    u8      thread_id;
    u8      temp_arena_count;
    Arena** temp_arenas;
}thread_ctx = {0};

internal void threadctx_init(const ThreadCtxOptions* thread_ctx_opts, u8 thread_id);
internal void threadctx_destroy();

#define AVOID_LIST(...) ARRAY_PARAM(Arena*, __VA_ARGS__)
///
//////////////////////////////////////////////////////////
// This is supposed to be restored with temp_arena_restore before function exit.
// avoid is a list of arenas we want to compare the thread arenas to in case
// our function recieves a temp as parameter.
//
// AVOID_LIST has been added to just be able to do something like:
// threadctx_get_temp(AVOID_LIST({a, a2, a3}));
// for the base cases of 0 and 1 we can just:
// threadctx_get_temp(0,0) or threadctx_get_temp(&a,1)
internal TempArena threadctx_get_temp(Arena *avoid[], u32 avoid_count);

// TODO: Implement threading
#define MUTEX_SCOPE(mutex) DEFER_LOOP(mutex_Lock(mutex), mutex_unlock(mutex))
#endif // !THREAD_CTX
