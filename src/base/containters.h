internal u8 *raw_buffer_get(u8 raw_buffer[], i32 elem_idx, u32 elem_size);

// (jd) Uses:
// typedef POOL(int) IntPool;
//typedef struct IntPool{
//     Pool pool;
//     int *last_accessed;
// }IntPool;

// void main(){
//     Arena *arena = ARENA_ALLOC();
//     POOL(int) tp;
//     POOL_INIT(arena, &tp, 32);
//
//     PoolHandle h = POOL_H_GET(&tp);
//     int i = POOL_H_ACCESS(&tp, h);
//     printf("pool int %d", i);
//     POOL_H_FREE(&tp, h);
// }

typedef struct Pool{
    u8  *raw_buffer; // elem_size * capacity
    u32 *idx_buffer; // count; maps handle -> raw_buffer index
    u32  count;
    u32  capacity;
}Pool;

// (jd) The ref deref crazyness is just there to preserve the syntax of how you would call the functions
#define POOL(T) struct{ Pool pool; T *last_accessed; }
#define POOL_INIT(arena, tp, capacity) pool_init((arena), &(tp)->pool, (capacity), sizeof(*(tp)->last_accessed), ALIGNOF(*(tp)->last_accessed))
#define POOL_H_GET(tp) pool_handle_get(&(tp)->pool, sizeof(*(tp)->last_accessed))
#define POOL_H_ACCES(tp, handle) ((tp)->last_accessed = pool_handle_access(&(tp)->pool, (handle), sizeof(*(tp)->last_accessed)))
#define POOL_H_FREE(tp, handle) pool_handle_free(&(tp)->pool, (handle))

internal void       pool_init(Arena *arena, Pool *p, u32 capacity, u32 elem_size, u32 alignment);
// internal void       pool_destroy(Pool *p, u32 elem_count, u32 elem_size);
internal PoolHandle pool_handle_get(Pool *p, u32 elem_size);
internal void       pool_handle_free(Pool *p, PoolHandle h);
internal void      *pool_handle_access(Pool *p, PoolHandle h, u32 elem_size);
