#ifndef CONTAINERS_H
#define CONTAINERS_H
internal u8* raw_buffer_get(u8 raw_buffer[], i32 elem_idx, u32 elem_size);
internal u64 array_bounds_check(u64 elem_idx, u64 capacity, char *file, int line);

////////////////////////////////////////////////////////////////
///
///NODE

typedef struct LLNode LLNode;
struct LLNode{
    LLNode *next;
};

typedef struct LLHead{
    LLNode *first;
    LLNode *last;
}LLHead;

read_only global LLNode g_llnode_sentinel = {
    cast(LLNode*)&g_llnode_sentinel
};

internal u8 *arena_push_llnode(Arena *arena, u32 size, u32 offset, u32 align);

#define llhead_node(...)    {.next = cast(LLNode*)&g_llnode_sentinel, __VA_ARGS__};
#define llhead_lit(...)     {.head.first = cast(LLNode*)&g_llnode_sentinel, .head.last = cast(LLNode*)&g_llnode_sentinel, __VA_ARGS__}

#define PushLLNode(a, T)    (T*)arena_push_llnode((a), sizeof(T), DOT_OFFSETOF(T, node), DOT_ALIGNOF(T))

#define LLNodeIsNil(p)       ((p) == &g_llnode_sentinel || (p) == 0)
#define LLHeadFirst(T, h)   DOT_CONTAINER_OF((h)->head.first, T, node)
#define LLHeadLast(T, h)    DOT_CONTAINER_OF((h)->head.last, T, node)

#define LLNodeGet(T, n)     DOT_CONTAINER_OF(n, T, node)
#define LLNodeNext(T, n)    DOT_CONTAINER_OF(n->node.next, T, node)

////////////////////////////////////////////////////////////////
///
/// Free List

internal u8 * free_list_get_or_create(Arena *arena, LLHead *head, u32 size, u32 offset, u32 align);
internal void free_list_free(LLHead *head, LLNode *node);

#define FreeListGetOrCreate(a, h, T)    (T*)free_list_get_or_create((a), &(h)->head, sizeof(T), DOT_OFFSETOF(T, node), DOT_ALIGNOF(T))
#define FreeListFree(h, n)              free_list_free(&(h)->head, &(n)->node)

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
#define POOL_INIT(arena, tp, cap)   ((tp)->data = pool_init((arena), &(tp)->pool, (cap), sizeof(POOL_ELEM_(tp)), DOT_ALIGNOF(POOL_ELEM_(tp))))
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
    MemoryPoolBlock *next_block;
#ifdef DOT_DEBUG
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

#define TREE_INIT(arena, T, ttp, capacity)  ((ttp)->data = tree_init((arena), &TREE_BASE_(ttp), (capacity), sizeof(POOL_ELEM_(ttp)), DOT_ALIGNOF(POOL_ELEM_(ttp)), DOT_OFFSETOF(T, node)))
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
