internal u8* raw_buffer_get(u8 raw_buffer[], i32 idx, u32 elem_size);

typedef struct PoolHandle{
    u32 handle[1];
}PoolHandle;

typedef struct Pool{
    u8  *raw_buffer; // elem_size*elem_count
    u32 *idx_buffer; // elem_count
    u32 elem_count;
    u32 elem_current;
}Pool;

typedef struct TexturePool{
    Pool pool;
    int *last_accessed;
}TexturePool;

#define POOL(T) struct{Pool pool; T *last_accessed;}
#define POOL_GET(tp) pool_get_handle(&(tp).pool)
#define POOL_ACCESS(tp, handle) ((tp).last_accessed = pool_obtain(&(tp).pool, (handle), sizeof(*(tp).last_accessed)))
#define POOL_FREE_H(tp, handle) pool_free_handle(&(tp).pool, (h))

#define POOL_INIT(arena, tp, elem_count) pool_init((arena), (elem_count), sizeof(*(tp).last_accessed))

internal Pool pool_create(Arena *arena, u32 elem_count, u32 elem_size);
// internal void pool_destroy(Pool *p, u32 elem_count, u32 elem_size);

internal PoolHandle pool_get_handle(Pool *p);
internal void       pool_free_handle(Pool *p, PoolHandle h);
internal u8* pool_obtain(Pool *p, PoolHandle h, u32 elem_size);

typedef POOL(int) PoolInt;
void test2(PoolInt t){
}

void test(){
    Arena *arena = ARENA_ALLOC();

    POOL(int) *tp_ptr;
    POOL_INIT(arena, *tp_ptr, 32);
    PoolHandle h = POOL_GET(*tp_ptr);
    int i = POOL_ACCESS(*tp_ptr, h);
    POOL_FREE_H(*tp_ptr, h);

    POOL(int) tp;
    POOL_INIT(arena, tp, 32);
    PoolHandle h2 = POOL_GET(tp);
    int i2 = POOL_ACCESS(tp, h2);
    POOL_FREE_H(tp, h2);


    PoolInt tp4;
    POOL_INIT(arena, tp4, 32);
}
