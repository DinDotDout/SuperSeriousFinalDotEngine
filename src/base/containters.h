internal u8 *raw_buffer_get(u8 raw_buffer[], i32 idx, u32 elem_size);

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
//     PoolHandle h = POOL_GET_H(&tp);
//     int i = POOL_ACCESS_H(&tp, h);
//     printf("pool int %d", i);
//     POOL_FREE_H(&tp, h);
// }

typedef u32 PoolHandle;
typedef struct Pool{
    u8  *raw_buffer; // elem_size*elem_count
    u32 *idx_buffer; // elem_count
    u32 elem_count;
    u32 elem_current;
}Pool;

// (jd) The ref deref crazy thing is just there to preserve the syntax of how you would call the functions
#define POOL(T) struct{ Pool pool; T *last_accessed; }
#define POOL_INIT(arena, tp, elem_count) pool_init((arena), &(tp)->pool, (elem_count), sizeof(*(tp)->last_accessed), ALIGNOF(*(tp)->last_accessed))
#define POOL_GET_H(tp) pool_get_handle(&(tp)->pool, sizeof(*(tp)->last_accessed))
#define POOL_ACCESS_H(tp, handle) ((tp)->last_accessed = pool_obtain(&(tp)->pool, (handle), sizeof(*(tp)->last_accessed)))
#define POOL_FREE_H(tp, handle) pool_free_handle(&(tp)->pool, (handle))

internal void       pool_init(Arena *arena, Pool *p, u32 elem_count, u32 elem_size, u32 alignment);
// internal void       pool_destroy(Pool *p, u32 elem_count, u32 elem_size);
internal PoolHandle pool_get_handle(Pool *p, u32 elem_size);
internal void       pool_free_handle(Pool *p, PoolHandle h);
internal void      *pool_obtain(Pool *p, PoolHandle h, u32 elem_size);
