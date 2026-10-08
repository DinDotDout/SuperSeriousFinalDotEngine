#ifndef THREAD_CTX_H
#define THREAD_CTX_H

internal thread_local struct ThreadCtx{
    u32 thread_id;

    u32 temp_arena_count;
    Arena **temp_arenas;
}t_thread_ctx = {0};

internal void   threadctx_init(Arena *arena, u32 arena_count, u32 arena_size, u32 thread_id);
internal void   threadctx_destroy();
internal u32    threadctx_id();

//////////////////////////////////////////////////////////
// This is supposed to be restored with scratch_end before function exit.
// avoid is a list of arenas we want to compare the thread arenas to in case
// our function recieves a temp as parameter.

typedef TempArena Scratch;
internal Scratch    scratch_begin(u32 count, Arena *avoid_arenas[]);
internal void       scratch_end(TempArena temp);

// Opaque storage for pthread_mutex_t / SRWLOCK, implemented in os_linux.c / os_windows.c.
typedef struct Mutex{
    u64 h[8];
}Mutex;

internal void mutex_init(Mutex *mutex);
internal void mutex_release(Mutex *mutex);
internal void mutex_lock(Mutex *mutex);
internal void mutex_unlock(Mutex *mutex);

#define MUTEX_SCOPE(mutex) DEFER_LOOP(mutex_lock(mutex), mutex_unlock(mutex))
#endif // !THREAD_CTX_H
