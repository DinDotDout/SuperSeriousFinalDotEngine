////////////////////////////////////////////////////////////////
///
/// Pool

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

typedef u32 PoolHandle;
typedef struct Pool{
    u8  *raw_buffer; // elem_size * capacity
    u32 *idx_buffer; // count; maps handle -> raw_buffer index
    u32  count;
    u32  capacity;
    u32  elem_size;
}Pool;

internal void       pool_init(Arena *arena, Pool *p, u32 capacity, u32 elem_size, u32 alignment);
// internal void       pool_destroy(Pool *p, u32 elem_count, u32 elem_size);
internal PoolHandle pool_handle_get(Pool *p);
internal void       pool_handle_free(Pool *p, PoolHandle h);
internal void      *pool_handle_access(Pool *p, PoolHandle h);

// (jd) The ref deref crazyness is just there to preserve the syntax of how you would call the functions
#define POOL(T) struct{Pool pool; T *last_accessed;}
#define POOL_INIT(arena, tp, capacity)  pool_init((arena), &(tp)->pool, (capacity), sizeof(*(tp)->last_accessed), ALIGNOF(*(tp)->last_accessed))
#define POOL_H_GET(tp)                  pool_handle_get(&(tp)->pool)
#define POOL_H_ACCESS(tp, handle)       ((tp)->last_accessed = pool_handle_access(&(tp)->pool, (handle)))
#define POOL_H_FREE(tp, handle)         pool_handle_free(&(tp)->pool, (handle))
#define POOL_H_GET_NULL(tp)             pool_handle_get_null(&(tp)->pool)

////////////////////////////////////////////////////////////////
///
/// Pool Tree

// (jd) Uses:
//typedef struct TreeNode{
//     int payload0;
//     TreeHeader node;
//     int payload1;
//     int payload2;
// }TreeNode;

// void main(){
//     Arena *arena = ARENA_ALLOC();
//     POOL(TreeNode) tp;
//     POOL_INIT(arena, &tp, 32);
//
// }

typedef struct TreePool{
    Pool         pool;

    // Offset off TreeHeader in node. Caching this to avoid passing T, node each time
    u32         offsetoff_header;
    PoolHandle  tree_root;

#ifdef DEBUG
    u64 *used; // pool->capacity / sizeof(used); bitmap validation to avoid reinsertion
#endif
}TreePool;

#define TREE(T) struct{TreePool tree_pool; T *last_accessed;}
typedef struct TreeHeader{
    PoolHandle sibling;
    PoolHandle last_child; // should we embed the pool? or just pass it in in the methods
}TreeHeader;

typedef struct MyType{
    int payload0;
    TreeHeader node;
    int payload1;
    int payload2;
}MyType;

// internal u8 *tree_push_front(Pool *p, TreeHeader *header, u32 elem_size, u32 header_offset);
internal PoolHandle     tree_init(Arena *arena, TreePool *tp, u32 capacity, u32 elem_size, u32 alignment, u32 offsetoff_header);
internal PoolHandle     tree_push_front_new(TreePool *tp, PoolHandle parent);
internal void           tree_push_front(TreePool *tp, PoolHandle parent_h, PoolHandle new_h);
internal void           tree_pop_front(TreePool *tp, PoolHandle parent_h);
internal void*          tree_handle_access(TreePool *tp, PoolHandle h);

internal TreeHeader*    tree_header_get_(TreePool *tree_pool, PoolHandle idx);


// In case we already have a node and need a different name, we can specify it in TREE_INIT_NAME
// #define TREE_INIT_NAME(arena, ttp, T, field_name, capacity) tree_init((arena), &((ttp)->tree_pool), (capacity), sizeof(*((ttp)->last_accessed)), ALIGNOF(*((ttp)->last_accessed)), DOT_OFFSETOF(T, field_name))
#define TREE(T) struct{TreePool tree_pool; T *last_accessed;}
#define TREE_INIT(arena, ttp, T, capacity)  tree_init((arena), &((ttp)->tree_pool), (capacity), sizeof(*((ttp)->last_accessed)), ALIGNOF(*((ttp)->last_accessed)), DOT_OFFSETOF(T, node))
#define TREE_ACCESS(ttp, h)                 tree_handle_access(&((ttp)->tree_pool), h)
#define TREE_PUSH_FRONT_NEW(ttp, parent)    tree_push_front_new(&((ttp)->tree_pool), parent)
#define TREE_PUSH_FRONT(ttp, parent, new)   tree_push_front(&((ttp)->tree_pool), parent, new)

typedef struct TreeIterator{
    TreePool    *tp;
    PoolHandle  *stack;
    u32          sp;
    u32          capacity;
    // PoolHandle   current;
} TreeIterator;


TreeIterator
tree_iter_begin(TreePool *tp, PoolHandle root,
                             PoolHandle *stack, u32 stack_cap)
{
    DOT_ASSERT(root != 0);
    DOT_ASSERT(stack_cap > 0);
    TreeIterator it = {
        .tp = tp,
        .stack = stack,
        .sp = 0,
        .capacity = stack_cap,
        // .current = 0,
    };
    it.stack[it.sp++] = root;
    return it;
}


PoolHandle
tree_iter_next(TreeIterator *it)
{

    if (it->sp == 0) {
        return 0;
    }

    // Pop next node
    PoolHandle h = it->stack[--it->sp];
    // it->current = h;

    // Get header
    TreeHeader *hdr = tree_header_get_(it->tp, h);

    // Push children (in sibling order)
    PoolHandle child = hdr->last_child;
    while (child) {
        it->stack[it->sp++] = child;
        TreeHeader *ch = tree_header_get_(it->tp, child);
        child = ch->sibling;
    }

    return h;
}

#define EACH_TREE_NODE(h, it) (PoolHandle h = 0; (h = tree_iter_next(&it)); ) 

void main2(){
    Arena *arena = ARENA_ALLOC();
    TREE(MyType) tree_pool;
    PoolHandle root = TREE_INIT(arena, &tree_pool, MyType, 32);

    // DOT_OFFSETOF(tp->last_accessed, Tree
    PoolHandle first_child = TREE_PUSH_FRONT_NEW(&tree_pool, root);
    PoolHandle sibling = TREE_PUSH_FRONT_NEW(&tree_pool, root);
    (void)first_child;(void)sibling;


    TempArena temp = threadctx_get_temp(0,0);
    u32 stack_size = 1024;
    PoolHandle *stack = PUSH_ARRAY(temp.arena, PoolHandle, stack_size);
    TreeIterator it = tree_iter_begin(&tree_pool.tree_pool, root, stack, stack_size);

    for EACH_TREE_NODE(h, it){
        MyType *n = TREE_ACCESS(&tree_pool, h);
        (void)n;
    }

    for (PoolHandle h = 0; (h = tree_iter_next(&it)); ) {
        MyType *n = TREE_ACCESS(&tree_pool, h);
        (void)n;

        // use n
    }
    PoolHandle h;
    while ((h = tree_iter_next(&it))) {
        MyType *n = TREE_ACCESS(&tree_pool, h);
        (void)n;

        // Do something with n
    }

    temp_arena_restore(temp);

}
