internal u64
array_bounds_check(u64 elem_idx, u64 capacity, char *file, int line)
{
    if(elem_idx >= capacity){
        DOT_ERROR_FL(file, line, "Array idx %llu out of max capacity %llu\n", elem_idx, capacity);
    }
    return elem_idx;
}

internal u8*
raw_buffer_get(u8 raw_buffer[], i32 elem_idx, u32 elem_size)
{
    u8 *res = &raw_buffer[elem_idx*elem_size];
    return(res);
}

////////////////////////////////////////////////////////////////
///
/// Pool

internal u64
pool_handle_pack(PoolHandle h)
{
    // u64 res = (cast(u64)h.gen << 32) | (cast(u64)h.idx);
    u64 res = cast(u64)h.idx;
    return(res);
}

internal b32
pool_handle_is_null(PoolHandle h)
{
    b32 res = h.idx == POOL_NULL_HANDLE.idx;
    return res;
}

internal PoolHandle
pool_handle_unpack(u64 pack)
{
    PoolHandle h = {
        .idx = cast(u32)pack,
        // .gen = cast(u32)(pack >> 32),
    };
    return(h);
}

internal u32
pool_handle_to_pool_idx(Pool *p, PoolHandle h)
{
    if(h.idx >= p->capacity){
        DOT_WARNING("Pool idx out of bounds. Returning 0 handle");
        return 0;
    }
    return h.idx;
}

internal PoolHandle
pool_null_handle_get(Pool *p, u32 elem_size, u8 *data)
{
    DOT_ASSERT(p->capacity > 0, "Uninitialized pool");
    PoolHandle h = POOL_NULL_HANDLE;
    u32 idx = pool_handle_to_pool_idx(p, h);
    void *elem = raw_buffer_get(data, idx, elem_size);
    MEMORY_ZERO(elem, elem_size);
    return(h);
}

internal PoolHandle
pool_alloc(Pool *p, u32 elem_size, void *data)
{
    if (DOT_UNLIKELY(p->count >= p->capacity)){
        DOT_WARNING("Pool capacity exceeded, returning null handle");
        return (PoolHandle){ .idx = 0 };
    }

    PoolHandle h = { .idx = p->idx_buffer[p->count] };
    p->count++;

    void *elem = raw_buffer_get(data, h.idx, elem_size);
    MEMORY_ZERO(elem, elem_size);
    return h;
}

internal void
pool_free(Pool *p, PoolHandle h)
{
    DOT_ASSERT(p->capacity > 0, "Uninitialized pool");
    if(h.idx == 0 || p->count == 1){
        return;
    }
    p->count -= 1;
    p->idx_buffer[p->count] = h.idx;
}

internal void*
pool_init(Arena *arena, Pool *p, u32 capacity, u32 elem_size, u32 alignment)
{
    MEMORY_ZERO_STRUCT(p);
    p->capacity   = capacity;
    p->count      = 1;
    p->idx_buffer = PUSH_ARRAY_NO_ZERO(arena, u32, capacity);
    for(u32 i = 0; i < capacity; i++){
        p->idx_buffer[i] = i;
    }
    void *data = PUSH_ARRAY_NO_ZERO_ALIGNED(arena, u8, capacity * elem_size, alignment);
    MEMORY_ZERO(data, elem_size); // Zero first element
    return(data);
}

////////////////////////////////////////////////////////////////
///
/// Tree Pool

internal TreeHeader*
tree_header_(TreePool *tree_pool, PoolHandle h, u32 elem_size, u8 *data)
{
    Pool *pool = &tree_pool->pool;
    u32 idx = pool_handle_to_pool_idx(pool, h);
    void *elem = raw_buffer_get(data, idx, elem_size);
    // u8 *elem = data + pool_handle_to_pool_idx(pool, h);
    TreeHeader *header = cast(TreeHeader*)((cast(u8*)elem) + tree_pool->offsetoff_header);
    return(header);
}

internal void*
tree_init(Arena *arena, TreePool *tree_pool, u32 capacity, u32 elem_size, u32 alignment, u32 offsetoff_header)
{
    void *data = pool_init(arena, &tree_pool->pool, capacity, elem_size, alignment);
    tree_pool->offsetoff_header = offsetoff_header;
    PoolHandle root = pool_alloc(&tree_pool->pool, elem_size, data);
    tree_pool->tree_root = root;
    return(data);
}

internal PoolHandle
tree_get_next_sibling(TreePool *tree_pool, PoolHandle node, u32 elem_size, u8 *data)
{
    TreeHeader *node_header = tree_header_(tree_pool, node, elem_size, data);
    PoolHandle sibling = node_header->sibling;
    return(sibling);
}

internal PoolHandle
tree_get_last_child(TreePool *tree_pool, PoolHandle parent, u32 elem_size, u8 *data)
{
    TreeHeader *parent_header = tree_header_(tree_pool, parent, elem_size ,data);
    PoolHandle last_child = parent_header->last_child;
    return(last_child);
}

internal PoolHandle
tree_get_first_child(TreePool *tree_pool, PoolHandle parent, u32 elem_size, u8 *data)
{
    PoolHandle last_child = tree_get_last_child(tree_pool, parent, elem_size, data);
    PoolHandle first_child = tree_get_next_sibling(tree_pool, last_child, elem_size, data);
    return(first_child);
}

internal void
tree_push_sibling(TreePool *tree_pool, PoolHandle prev_h, PoolHandle new, u32 elem_size, u8 *data)
{
    TreeHeader *prev_header = tree_header_(tree_pool, prev_h, elem_size, data);
    TreeHeader *new_header = tree_header_(tree_pool, new, elem_size, data);
    new_header->sibling = prev_header->sibling;
    prev_header->sibling = new;
}

internal void
tree_push_front(TreePool *tree_pool, PoolHandle parent, PoolHandle new, u32 elem_size, u8 *data)
{
    if(parent.idx != 0){
        TreeHeader *parent_header = tree_header_(tree_pool, parent, elem_size, data);
        TreeHeader *new_header = tree_header_(tree_pool, new, elem_size ,data);
        if(pool_handle_is_null(parent_header->last_child)){
            parent_header->last_child = new;
            new_header->sibling = new;
        }else{
            tree_push_sibling(tree_pool, parent_header->last_child, new, elem_size, data);
        }
    }
}

internal PoolHandle
tree_push_front_new(TreePool *tree_pool, PoolHandle parent, u32 elem_size, u8 *data)
{
    PoolHandle new_elem = pool_alloc(&tree_pool->pool, elem_size, data);
    tree_push_front(tree_pool, parent, new_elem, elem_size, data);
    return(new_elem);
}

internal PoolHandle
tree_pop_sibling(TreePool *tree_pool, PoolHandle node, u32 elem_size, u8 *data)
{
    TreeHeader *node_header = tree_header_(tree_pool, node, elem_size, data);
    PoolHandle old_sibling = node_header->sibling;
    TreeHeader *old_header = tree_header_(tree_pool, old_sibling, elem_size, data);
    node_header->sibling = old_header->sibling;
    return(old_sibling);
}

internal void
tree_pop_front(TreePool *tree_pool, PoolHandle parent, u32 elem_size, u8 *data)
{
    if(parent.idx != 0){
        TreeHeader *p = tree_header_(tree_pool, parent, elem_size, data);
        PoolHandle old_first_h = tree_pop_sibling(tree_pool, p->last_child, elem_size, data);
        pool_free(&tree_pool->pool, old_first_h);
    }
}

// (jd) NOTE: Should we always assume worst case of root containing all children
// or pass in a capacity?
internal TreeIterator
tree_iter_begin(Arena *arena, TreePool *tree_pool, u32 elem_size, u8 *data, PoolHandle iter_root)
{
    u32 stack_capacity = tree_pool->pool.count - 1; // Zero is reserved, so we skip it
    TreeIterator it = {
        .tree_pool = tree_pool,
        .elem_size = elem_size,
        .data = data,
        .stack_capacity = stack_capacity,
        .stack = PUSH_ARRAY_NO_ZERO(arena, PoolHandle, stack_capacity),
        .stack_idx = 0,
    };
    it.stack[it.stack_idx++] = iter_root;
    return it;
}

internal PoolHandle
tree_iter_next(TreeIterator *it)
{
    if(it->stack_idx == 0){
        return(POOL_IT_END);
    }
    PoolHandle curr = it->stack[--it->stack_idx];
    PoolHandle first = tree_get_first_child(it->tree_pool, curr, it->elem_size, it->data);
    if(first.idx == 0){
        return(curr);
    }
    it->stack[it->stack_idx++] = first;
    PoolHandle next = tree_get_next_sibling(it->tree_pool, first, it->elem_size, it->data);
    while (next.idx != first.idx) {
        if (it->stack_idx >= it->stack_capacity) {
          DOT_ERROR("Tree iterator capacity exceeded, either the iterator is "
                    "too small or a node has been inserted more than once");
          return(POOL_IT_END);
        }
        it->stack[it->stack_idx++] = next;
        next = tree_get_next_sibling(it->tree_pool, next, it->elem_size, it->data);
    }
    return(curr);
}

DOT_TEST_SUITE(array_tests)
{
    typedef ARRAY(int, 7) intArray7;
    intArray7 arr = ARRAY_LIT(int, 3, 4, 5, 9);
    (void)arr;

    DOT_TestResults test = {0};
    ARRAY(int, 7) array = ARRAY_LIT(int, 3, 4, 5, 9);
    int v = ARRAY_GET(array, 6);
    DOT_TEST_CHECK(test, "Array zero init", v == 0);
    ARRAY_GET(array, 6) = 5;
    DOT_TEST_CHECK(test, "Array assign", v == 5);
    ARRAY_PUSH(array, 7);
    DOT_TEST_CHECK(test, "Array assign", v == 5);
    return test;
}

DOT_TEST_SUITE(slice_test)
{
    typedef SLICE(int) IntSlice;

    DOT_TestResults test = {0};
    SLICE(int) slice = SLICE_LIT(int, 3, 4, 5, 9);

    int v = SLICE_GET(slice, 6);
    bool valid = v == 0;
    SLICE_GET(slice, 6) = 5;

    SLICE_GET(slice, 2) = 7;
    valid = v == 5;
    (void)valid;

    IntSlice slice2 = {0};
    int v2 = SLICE_GET(slice2, 6);
    bool valid2 = v2 == 0;
    SLICE_GET(slice2, 6) = 5;
    (void) valid2;
    return test;
}

DOT_TEST_SUITE(pool_tests)
{
    DOT_TestResults test = {0};
    typedef struct Particle {
        float x, y, z;
        float vx, vy, vz;
        int   alive;
    } Particle;

    Arena *arena = ARENA_CREATE();

    POOL(Particle) pp;
    POOL_INIT(arena, &pp, 16);

    PoolHandle h1 = POOL_ALLOC(&pp);
    PoolHandle h2 = POOL_ALLOC(&pp);
    PoolHandle h3 = POOL_ALLOC(&pp);

    DOT_TEST_CHECK(test, "POOL_ALLOC", h1.idx != 0);
    DOT_TEST_CHECK(test, "POOL_ALLOC", h2.idx != 0);
    DOT_TEST_CHECK(test, "POOL_ALLOC", h3.idx != 0);

    Particle p1_init = { .x=1, .y=2, .z=3, .vx=0.1f, .vy=0.2f, .vz=0.3f, .alive=1 };
    *POOL_GET(&pp, h1) = p1_init;
    *POOL_GET(&pp, h3) = (Particle){ .x=4, .y=5, .z=6, .vx=0.4f, .vy=0.5f, .vz=0.6f, .alive=1 };

    DOT_TEST_CHECK(test, "POOL_GET", POOL_GET(&pp, h1)->vz == 0.3f);
    DOT_TEST_CHECK(test, "POOL_GET", POOL_GET(&pp, h2)->vx == 0.4f);
    DOT_TEST_CHECK(test, "POOL_GET", POOL_GET(&pp, h3)->alive == 1);

    u64 packed = pool_handle_pack(h2);
    PoolHandle unpacked = pool_handle_unpack(packed);
    DOT_TEST_CHECK(test, "Handle packing", unpacked.idx == h2.idx);

    // (jd) NOTE: If this starts failing, it might be because I added generational counters
    // and started distributing the selection to spread gen tag reuse
    POOL_FREE(&pp, h2);
    PoolHandle h4 = POOL_ALLOC(&pp);
    DOT_TEST_CHECK(test, "Handle reuse", h4.idx == h2.idx);

    *POOL_GET(&pp, h4) = (Particle){42,0,0,0,0,0,1};
    DOT_TEST_CHECK(test, "Value assign", POOL_GET(&pp, h4)->x == 42);

    // Out-of-capacity behavior
    for(int i = 0; i < 12; i++){
        PoolHandle h = POOL_ALLOC(&pp);
        DOT_TEST_CHECK(test, "Stress test", h.idx != 0);
    }
    PoolHandle h = POOL_ALLOC(&pp);
    DOT_TEST_CHECK(test, "Alloc fail test", h.idx == 0);
    DOT_TEST_CHECK(test, "Reached total count", pp.pool.count == 16);

    return test;
}

DOT_TEST_SUITE(tree_tests)
{
    TempArena temp = threadctx_get_temp(0);
    DOT_TestResults test = {0};
    typedef struct SceneNode{
        const char *name;
        float       local_xform[16];
        int         mesh_id;
        TreeHeader  node;
    }SceneNode;

    Arena *arena = ARENA_CREATE();

    TREE_POOL(SceneNode) tp;
    TREE_INIT(arena, SceneNode, &tp, 32);
    PoolHandle root = TREE_GET_ROOT_H(&tp);
    SceneNode *root_n = TREE_GET(&tp, root);
    SceneNode *root_n1 = TREE_GET_ROOT(&tp);

    DOT_TEST_CHECK(test, "compare root", root_n == root_n1);

    root_n->name = "Root";
    root_n->mesh_id = -1;
    // PoolHandle new = TREE_ALLOC(&tp, root);
    // TREE_PUSH_FRONT(&tp, root, new);

    PoolHandle cam  = TREE_PUSH_FRONT_NEW(&tp, root);
    PoolHandle car  = TREE_PUSH_FRONT_NEW(&tp, root);
    PoolHandle lamp = TREE_PUSH_FRONT_NEW(&tp, root);

    SceneNode *cam_n = TREE_GET(&tp, cam);
    cam_n->name = "Camera";

    SceneNode *car_n = TREE_GET(&tp, car);
    car_n->name = "Car";
    car_n->mesh_id = 12;

    SceneNode *lamp_n = TREE_GET(&tp, lamp);
    lamp_n->name = "Lamp";
    lamp_n->mesh_id = 3;


    PoolHandle wheel_fl = TREE_PUSH_FRONT_NEW(&tp, car);
    SceneNode *wheel_fl_n = TREE_GET(&tp, wheel_fl);
    wheel_fl_n->name = "Wheel_fl";
    wheel_fl_n->mesh_id = 100;

    PoolHandle wheel_fr = TREE_PUSH_FRONT_NEW(&tp, car);
    SceneNode *wheel_fr_n = TREE_GET(&tp, wheel_fr);
    wheel_fr_n->name = "Wheel_fr";
    wheel_fr_n->mesh_id = 101;

    TreeIterator it = TREE_ITER_BEGIN(temp.arena, &tp, root);
    const char *seen[16] = {0};
    int count = 0;

    for EACH_TREE_NODE(h, &it){
        seen[count++] = TREE_GET(&tp, h)->name;
    }
    temp_arena_restore(temp);

    // Expected preorder visit
    //
    // Root
    //   Camera
    //   Car
    //     Wheel_fl
    //     Wheel_fr
    //   Lamp

    DOT_TEST_CHECK(test, "Test push count", count == 6);
    DOT_TEST_CHECK(test, "Test tree layout", strcmp(seen[0], "Root")        == 0);
    DOT_TEST_CHECK(test, "Test tree layout", strcmp(seen[1], "Camera")      == 0);
    DOT_TEST_CHECK(test, "Test tree layout", strcmp(seen[2], "Car")         == 0);
    DOT_TEST_CHECK(test, "Test tree layout", strcmp(seen[3], "Wheel_fl")    == 0);
    DOT_TEST_CHECK(test, "Test tree layout", strcmp(seen[4], "Wheel_fr")    == 0);
    DOT_TEST_CHECK(test, "Test tree layout", strcmp(seen[5], "Lamp")        == 0);

    TREE_POP_FRONT(&tp, car); // pop wheel fl
    it = TREE_ITER_BEGIN(temp.arena, &tp, root);
    count = 0;
    MEMORY_ZERO_ARRAY(seen);
    for EACH_TREE_NODE(h, &it){
        seen[count++] = TREE_GET(&tp, h)->name;
    }
    temp_arena_restore(temp);
    DOT_TEST_CHECK(test, "Test tree layout", strcmp(seen[4], "wheel_fr") != 0);

    TREE_POP_FRONT(&tp, root); // pop lamp
    it = TREE_ITER_BEGIN(temp.arena, &tp, root);
    count = 0;
    MEMORY_ZERO_ARRAY(seen);
    for EACH_TREE_NODE(h, &it){
        seen[count++] = TREE_GET(&tp, h)->name;
    }
    temp_arena_restore(temp);
    DOT_TEST_CHECK(test, "Test tree layout", strcmp(seen[3], "Lamp") != 0);

    return test;
}
