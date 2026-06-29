#ifndef THREAD_CTX_H
#define THREAD_CTX_H

typedef struct ArenaSlice{
  u32 count;
  Arena **data;
}ArenaSlice;

#define SLICE_ARENA_LIT(...) (SliceArena)SLICE_LIT(Arena*, __VA_ARGS__)

internal thread_local struct ThreadCtx{
    u32 thread_id;
    ArenaSlice temp_arenas;
}t_thread_ctx = {0};

internal void   threadctx_init(Arena *arena, u32 arena_count, u32 arena_size, u32 thread_id);
internal void   threadctx_destroy();
internal u32    threadctx_id();

//////////////////////////////////////////////////////////
// This is supposed to be restored with threadctx_temp_end before function exit.
// avoid is a list of arenas we want to compare the thread arenas to in case
// our function recieves a temp as parameter.
//
// SLICE_ARENA_LIT has been added to just be able to do something like:
// threadctx_temp_begin(SLICE_ARENA_LIT(a, a2, a3));
// for the base cases of 0 we can just pass in NULL or 0 threadctx_temp_begin(0)
internal TempArena  threadctx_temp_begin(ArenaSlice *avoid_arenas);
internal void       threadctx_temp_end(TempArena temp);

// TODO: Implement actual threading constructs
typedef struct ThreadCtxMutex{
    u64 h[1];
}ThreadCtxMutex;

#define MUTEX_SCOPE(mutex) DEFER_LOOP(mutex_Lock(mutex), mutex_unlock(mutex))
#endif // !THREAD_CTX_H
