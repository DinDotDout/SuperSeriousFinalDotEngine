#ifndef CONTAINERS_H
#define CONTAINERS_H

internal u8* raw_buffer_get(u8 raw_buffer[], u32 elem_idx, u32 elem_size);
internal u64 array_bounds_check(u64 elem_idx, u64 capacity, char *file, int line);

////////////////////////////////////////////////////////////////
///
/// Linked lists
///
/// Use _NEXT versions direction if "next" field has some other name

#define SLL_STACK_PUSH(top, node) SLL_STACK_PUSH_NEXT(top, node, next)
#define SLL_STACK_PUSH_NEXT(top, node, next) \
do{ \
    (node)->next = (top); \
    (top) = (node); \
}while(0)

#define SLL_STACK_POP(top) SLL_STACK_POP_NEXT(top, next)
#define SLL_STACK_POP_NEXT(top, next) \
do{ \
    (top) = (top)->next; \
}while(0)

#define SLL_QUEUE_PUSH(first, last, node) SLL_QUEUE_PUSH_NEXT(first, last, node, next)
#define SLL_QUEUE_PUSH_NEXT(first, last, node, next) \
do{ \
    (node)->next = 0; \
    if(last){ (last)->next = (node); }else{ (first) = (node); } \
    (last) = (node); \
}while(0)

////////////////////////////////////////////////////////////////
///
/// SLICE

#define SLICE(T) \
struct{ \
    u32 count; \
    T *data; \
}

#define SLICE_LIT(T, ...) \
{ \
    .data = (T[]){__VA_ARGS__}, \
    .count = (sizeof((T[]){__VA_ARGS__}) / sizeof(T)) \
}

#define SLICE_INIT(arena, slc, c)\
do{ \
    (slc)->count = c; \
    (slc)->data = PUSH_ARRAY_UNTYPED((arena), (slc)->data[0], (c)); \
}while(0)

#define SLICE_CREATE(arena, T, c) \
{ \
    .count = c, \
    .data = PUSH_ARRAY(arena, T, c), \
}

#define SLICE_GET(arr, idx) ((arr).data[array_bounds_check((idx), (arr).count, __FILE__, __LINE__)])
#define SLICE_LAST(arr)     ((arr).data[(arr).count])

////////////////////////////////////////////////////////////////
///
/// Array

#define ARRAY(T, capacity) \
struct{ \
    u64 count; \
    T data[capacity]; \
}

#define ARRAY_INIT(arr, T, ...) \
do{ \
    (arr)->data = sizeof((T[]){__VA_ARGS__}) / sizeof(T) \
    (arr)->count = {__VA_ARGS__}; \
}while(0)

#define ARRAY_LIT(T, ...) \
{ \
    .data = {__VA_ARGS__}, \
    .count= sizeof((T[]){__VA_ARGS__}) / sizeof(T) \
}

#define ARRAY_GET(arr, idx)     ((arr).data[array_bounds_check((idx), DOT_ARRAY_COUNT((arr).data), __FILE__, __LINE__)])
#define ARRAY_PUSH(arr, elem)   ((arr).data[array_bounds_check((arr).count++, DOT_ARRAY_COUNT((arr).data), __FILE__, __LINE__)] = (elem))
#define ARRAY_LAST(arr)         ((arr).data[(arr).count])

////////////////////////////////////////////////////////////////
///
/// Binary search
/// Define a compare for your element type and call through the macros:
///   internal i32 heap_range_compare(void *element, void *key){ ... return < 0, 0, > 0; }
///   i64 idx = BINARY_SEARCH_LAST_LE(ranges, count, &address, heap_range_compare);

// (jd) Compares the element against the searched key, element side first.
typedef i32 BinarySearchCompare(void *element, void *key);

// (jd) First index whose element is >= key, count if there's none. Also the insertion point
internal force_inline i64
binary_search_lower_bound_(void *base, i64 count, u64 elem_size, void *key, BinarySearchCompare *compare)
{
    i64 lo = 0;
    i64 hi = count;
    while(lo < hi){
        i64 mid = lo + (hi - lo) / 2;
        if(compare(cast(u8*) base + cast(u64) mid * elem_size, key) < 0){ lo = mid + 1; }else{ hi = mid; }
    }
    return(lo);
}

// (jd) First index whose element is > key, count if there's none
internal force_inline i64
binary_search_upper_bound_(void *base, i64 count, u64 elem_size, void *key, BinarySearchCompare *compare)
{
    i64 lo = 0;
    i64 hi = count;
    while(lo < hi){
        i64 mid = lo + (hi - lo) / 2;
        if(compare(cast(u8*) base + cast(u64) mid * elem_size, key) <= 0){ lo = mid + 1; }else{ hi = mid; }
    }
    return(lo);
}

// (jd) Last index whose element is <= key, -1 if there's none. What range lookups want
internal force_inline i64
binary_search_last_le_(void *base, i64 count, u64 elem_size, void *key, BinarySearchCompare *compare)
{
    return(binary_search_upper_bound_(base, count, elem_size, key, compare) - 1);
}

// (jd) Index of an element matching key, -1 if there's none.
internal force_inline i64
binary_search_find_(void *base, i64 count, u64 elem_size, void *key, BinarySearchCompare *compare)
{
    i64 idx = binary_search_lower_bound_(base, count, elem_size, key, compare);
    if(idx < count && compare(cast(u8*) base + cast(u64) idx * elem_size, key) == 0){
        return(idx);
    }
    return(-1);
}

// (jd) Inserts keeping the array sorted, shifting the tail up. Returns the index written to.
internal force_inline i64
binary_search_insert_(void *base, i64 *count, i64 capacity, u64 elem_size, void *element, void *key, BinarySearchCompare *compare)
{
    DOT_ASSERT(*count < capacity, "Sorted array is full (%" PRId64 " elements)", capacity);
    i64 idx = binary_search_upper_bound_(base, *count, elem_size, key, compare);
    u8 *slot = cast(u8*) base + cast(u64) idx * elem_size;

    MEMORY_COPY(slot + elem_size, slot, cast(u64)(*count - idx) * elem_size);
    MEMORY_COPY(slot, element, elem_size);
    *count += 1;
    return(idx);
}

// (jd) Removes a known index, shifting the tail down.
internal inline void
binary_search_remove_at_(void *base, i64 *count, u64 elem_size, i64 idx)
{
    DOT_ASSERT(idx >= 0 && idx < *count, "Sorted array remove index %" PRId64 " out of range", idx);
    u8 *slot = cast(u8*) base + cast(u64) idx * elem_size;

    *count -= 1;
    MEMORY_COPY(slot, slot + elem_size, cast(u64)(*count - idx) * elem_size);
}

#define BINARY_SEARCH_LOWER_BOUND(arr, count, key, compare) binary_search_lower_bound_((arr), (count), sizeof(*(arr)), (key), (compare))
#define BINARY_SEARCH_UPPER_BOUND(arr, count, key, compare) binary_search_upper_bound_((arr), (count), sizeof(*(arr)), (key), (compare))
#define BINARY_SEARCH_LAST_LE(arr, count, key, compare)     binary_search_last_le_((arr), (count), sizeof(*(arr)), (key), (compare))
#define BINARY_SEARCH_FIND(arr, count, key, compare)        binary_search_find_((arr), (count), sizeof(*(arr)), (key), (compare))

// (jd) count is taken by pointer, these update it
#define BINARY_SEARCH_INSERT(arr, count, capacity, element, key, compare) binary_search_insert_((arr), (count), (capacity), sizeof(*(arr)), (element), (key), (compare))
#define BINARY_SEARCH_REMOVE_AT(arr, count, idx)                          binary_search_remove_at_((arr), (count), sizeof(*(arr)), (idx))

////////////////////////////////////////////////////////////////
///
/// Pool
/// Go to definition for example usage: pool_tests()

typedef struct PoolHandle{
    u32 idx;
    u32 gen;
    // u16 kind; // Use in multipool
}PoolHandle;

internal u64        pool_handle_pack(PoolHandle h);
internal PoolHandle pool_handle_unpack(u64 pack);
internal b32        pool_handle_is_default(PoolHandle h);

#define POOL_DEFAULT_HANDLE (PoolHandle){0}

typedef struct Pool{
    u32 *idx_buffer; // count; maps handle -> raw_buffer index
    u32  count;
    // u32  head; // Make it a ring buffer when adding refcount to spread gen usage
    // u32  tail;
    u32  capacity;
}Pool;

internal void*      pool_init(Arena *arena, Pool *p, u32 capacity, u32 elem_size, u32 alignment);
internal PoolHandle pool_alloc(Pool *p, u32 elem_size, void *data);
internal u32        pool_handle_to_pool_idx(Pool *p, PoolHandle h);
internal void       pool_free(Pool *p, PoolHandle h);
internal PoolHandle pool_handle_get_default(Pool *p);
// internal PoolHandle pool_handle_get_default(Pool *p, u32 elem_size, u8 *data);

#define POOL(T) struct{ \
    Pool pool; \
    T *data; \
}

#define POOL_ELEM_(tp) (*(tp)->data)

// (jd) The ref deref crazyness is just there to preserve the syntax of how you would call the functions
#define POOL_INIT(arena, T, tp, cap)((tp)->data = pool_init((arena), &(tp)->pool, (cap), sizeof(POOL_ELEM_(tp)), DOT_ALIGNOF(T)))
#define POOL_ALLOC(tp)              pool_alloc(&(tp)->pool, sizeof(POOL_ELEM_(tp)), (tp)->data)
#define POOL_GET(tp, h)             ((tp)->data + pool_handle_to_pool_idx(&(tp)->pool, (h)))
// #define POOL_HANDLE_GET_DEFAULT(tp) pool_handle_get_default(&(tp)->pool, sizeof(POOL_ELEM_(tp)), (u8*)(tp)->data)
#define POOL_REF_COPY(tp, handle)   pool_ref_copy(&(tp)->pool, (handle))
#define POOL_FREE(tp, handle)       pool_free(&(tp)->pool, (handle))
#define POOL_IT_END                 POOL_DEFAULT_HANDLE


////////////////////////////////////////////////////////////////
///
/// MultiMemoryPool
/// Go to definition for example usage:

typedef struct MemoryPoolBlock MemoryPoolBlock;
struct MemoryPoolBlock{
    MemoryPoolBlock *next;
#if DOT_DEBUG
    u64 tag;
#endif
    u8 memory[];
};

typedef struct MemoryPool{
    u32             block_size_b;
    MemoryPoolBlock *next_free_block;
}MemoryPool;

internal MemoryPool memory_pool_create(u32 block_size);
internal u8        *memory_pool_alloc(Arena *arena, MemoryPool *memory_pool);
internal void       memory_pool_free(MemoryPool *memory_pool, u8 *ptr);

////////////////////////////////////////////////////////////////
///
/// MultiMemoryPool
/// Go to definition for example usage:

typedef struct {
    u32 count;
    u32 *data;
}MultiMemoryPoolBlockSizes;

typedef struct MultiMemoryPool{
    u32 pools_count;
    MemoryPool *memoy_pools;
}MultiMemoryPool;

internal MultiMemoryPool multi_memory_pool_create(Arena *arena, MultiMemoryPoolBlockSizes create_info);
internal u8              *multi_memory_pool_alloc(Arena *arena, MultiMemoryPool *multi_memory_pool, u64 size);

#define MULTI_MEMORY_POOL_CREATE(a, ...) multi_memory_pool_create(a, (MultiMemoryPoolBlockSizes)SLICE_LIT(u32, __VA_ARGS__));

////////////////////////////////////////////////////////////////
///
/// TreePool
/// Go to definition for example usage: tree_tests()

typedef struct TreePool{
    Pool         pool;
    PoolHandle  tree_root;
 
    // Offset off TreeHeader in node. Caching this to avoid passing T, node each time
    u32         offsetoff_header;
}TreePool;

typedef struct TreeHeader{
    PoolHandle sibling;
    PoolHandle last_child; // should we embed the pool? or just pass it in in the methods
}TreeHeader;

typedef struct TreeIterator{
    TreePool   *tree_pool;
    PoolHandle *stack;
    u32         elem_size;
    u8         *data;
    u32         stack_idx;
    u32         stack_capacity;
}TreeIterator;

// internal u8 *tree_push_front(Pool *p, TreeHeader *header, u32 elem_size, u32 header_offset);
internal void*          tree_init(Arena *arena, TreePool *tp, u32 capacity, u32 elem_size, u32 alignment, u32 offsetoff_header);
internal PoolHandle     tree_push_front_new(TreePool *tree_pool, PoolHandle parent, u32 elem_size, u8 *data);
internal void           tree_push_front(TreePool *tree_pool, PoolHandle parent, PoolHandle new, u32 elem_size, u8 *data);
internal void           tree_pop_front(TreePool *tree_pool, PoolHandle parent, u32 elem_size, u8 *data);
internal TreeHeader*    tree_header_(TreePool *tree_pool, PoolHandle h, u32 elem_size, u8 *data);

// (jd) NOTE: We could define a iteration methods if needed and specify in on iter begin
// Right to left, depth first, preorder
internal TreeIterator   tree_iter_begin(Arena *arena, TreePool *tree_pool, u32 elem_size, u8 *data, PoolHandle iter_root);
internal PoolHandle     tree_iter_next(TreeIterator *it);

#define TREE_POOL(T) \
struct{ \
    TreePool tree_pool; \
    T *data; \
}

#define TREE_BASE_(ttp) (ttp)->tree_pool

// (jd) This is in case our created tree node isn't named node because it clashes with something else
// #define TREE_INIT_NAME(arena, ttp, T, capacity, field_name) tree_init((arena), &TREE_BASE_(ttp), (capacity), sizeof(*((ttp)->last_accessed)), DOT_ALIGNOF(POOL_ELEM_(ttp)), DOT_OFFSETOF(T, field_name))

#define TREE_INIT(arena, T, ttp, capacity)  ((ttp)->data = tree_init((arena), &TREE_BASE_(ttp), (capacity), sizeof(POOL_ELEM_(ttp)), DOT_ALIGNOF(T), DOT_OFFSETOF(T, node)))
#define TREE_ALLOC(ttp, parent)             pool_alloc(&TREE_BASE_(ttp).pool, sizeof(POOL_ELEM_(ttp)), (ttp)->data)
#define TREE_PUSH_FRONT(ttp, parent, new)   tree_push_front(&TREE_BASE_(ttp), (parent), (new))
#define TREE_PUSH_FRONT_NEW(ttp, parent)    tree_push_front_new(&TREE_BASE_(ttp), (parent), sizeof(POOL_ELEM_(ttp)), (u8*)((ttp)->data))
#define TREE_POP_FRONT(ttp, parent)         tree_pop_front(&TREE_BASE_(ttp), (parent), sizeof(POOL_ELEM_(ttp)), (u8*)(ttp)->data)
#define TREE_GET(ttp, h)                    ((ttp)->data + pool_handle_to_pool_idx(&TREE_BASE_(ttp).pool, h))
#define TREE_GET_ROOT_H(ttp)                (TREE_BASE_(ttp).tree_root)
#define TREE_GET_ROOT(ttp)                  TREE_GET(ttp, TREE_GET_ROOT_H(ttp))

#define TREE_ITER_BEGIN(a, ttp, node)   tree_iter_begin(a, &TREE_BASE_(ttp), sizeof(POOL_ELEM_(ttp)), (u8*)(ttp)->data, node)
#define EACH_TREE_NODE(h, it)           (PoolHandle h; (h = tree_iter_next(it), h.idx);) 

#endif // !CONTAINERS_H
