internal u8* raw_buffer_get(u8 raw_buffer[], i32 elem_idx, u32 elem_size);
internal u32 array_bounds_check(u32 elem_idx, u32 capacity, char *file, int line);

////////////////////////////////////////////////////////////////
///
/// SLICE

#define SLICE(T) \
struct{ \
    u32 count; \
    T *data; \
}

#define SLICE_INIT(T, ...) \
{ \
    .data = (T[]){__VA_ARGS__}, \
    .count = (sizeof((T[]){__VA_ARGS__}) / sizeof(T)) \
}

#define SLICE_GET(arr, idx)     ((arr)->data[array_bounds_check((idx), (arr)->count, __FILE__, __LINE__)])
#define SLICE_LAST(arr)    ((arr)->data[(arr)->count])

DOT_TEST_SUITE(array_slice)
{
    typedef SLICE(int) IntSlice;

    DOT_TestResults test = {0};
    SLICE(int) slice = SLICE_INIT(int, 3, 4, 5, 9);

    int v = SLICE_GET(&slice, 6);
    bool valid = v == 0;
    SLICE_GET(&slice, 6) = 5;

    SLICE_GET(&slice, 2) = 7;
    valid = v == 5;
    (void)valid;

    IntSlice slice2 = {0};
    int v2 = SLICE_GET(&slice2, 6);
    bool valid2 = v2 == 0;
    SLICE_GET(&slice2, 6) = 5;
    (void) valid2;
    return test;
}

////////////////////////////////////////////////////////////////
///
/// Array

#define ARRAY(T, capacity) \
struct{ \
    u32 count; \
    T data[capacity]; \
}

#define ARRAY_INIT(T, ...) \
{ \
    .data = {__VA_ARGS__}, \
    .count= sizeof((T[]){__VA_ARGS__}) / sizeof(T) \
}

#define ARRAY_GET(arr, idx)     ((arr)->data[array_bounds_check((idx), DOT_ARRAY_COUNT((arr)->data), __FILE__, __LINE__)])
#define ARRAY_PUSH(arr, elem)   ((arr)->data[array_bounds_check((arr)->count++, DOT_ARRAY_COUNT((arr)->data), __FILE__, __LINE__)] = (elem))
#define ARRAY_LAST(arr)         ((arr)->data[(arr)->count])

DOT_TEST_SUITE(array_tests)
{
    typedef ARRAY(int, 7) intArray7;
    intArray7 arr = ARRAY_INIT(int, 3, 4, 5, 9);
    (void)arr;

    DOT_TestResults test = {0};
    ARRAY(int, 7) array = ARRAY_INIT(int, 3, 4, 5, 9);
    int v = ARRAY_GET(&array, 6);
    DOT_TEST_CHECK(test, "Array zero init", v == 0);
    ARRAY_GET(&array, 6) = 5;
    DOT_TEST_CHECK(test, "Array assign", v == 5);
    ARRAY_PUSH(&array, 7);
    DOT_TEST_CHECK(test, "Array assign", v == 5);
    return test;
}

////////////////////////////////////////////////////////////////
///
/// Pool
/// Go to definition for example usage: pool_tests()

typedef struct PoolHandle{
    u32 idx;
    // u32 gen;
}PoolHandle;

#define POOL_NULL_HANDLE (PoolHandle){0}
internal u64 pool_handle_pack(PoolHandle h);
internal b32 pool_handle_is_null(PoolHandle h);
internal PoolHandle pool_handle_unpack(u64 pack);

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
internal PoolHandle pool_null_handle_get(Pool *p, u32 elem_size, u8 *data);

#define POOL(T) struct{ \
    Pool pool; \
    T *data; \
}

#define POOL_ELEM_(tp) (*(tp)->data)

// (jd) The ref deref crazyness is just there to preserve the syntax of how you would call the functions
#define POOL_INIT(arena, tp, cap)   ((tp)->data = pool_init((arena), &(tp)->pool, (cap), sizeof(POOL_ELEM_(tp)), DOT_ALIGNOF(POOL_ELEM_(tp))))
#define POOL_ALLOC(tp)              pool_alloc(&(tp)->pool, sizeof(POOL_ELEM_(tp)), (tp)->data)
#define POOL_GET(tp, h)             ((tp)->data + pool_handle_to_pool_idx(&(tp)->pool, (h)))
#define POOL_NULL_HANDLE_GET(tp)    pool_null_handle_get(&(tp)->pool, sizeof(POOL_ELEM_(tp)), (u8*)(tp)->data)
#define POOL_REF_COPY(tp, handle)   pool_ref_copy(&(tp)->pool, (handle))
#define POOL_FREE(tp, handle)       pool_free(&(tp)->pool, (handle))
#define POOL_IT_END                 POOL_NULL_HANDLE

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
    TreePool    *tree_pool;
    PoolHandle  *stack;
    u8          *data;
    u32          stack_idx;
    u32          stack_capacity;
}TreeIterator;

// internal u8 *tree_push_front(Pool *p, TreeHeader *header, u32 elem_size, u32 header_offset);
internal void*          tree_init(Arena *arena, TreePool *tp, u32 capacity, u32 elem_size, u32 alignment, u32 offsetoff_header);
internal PoolHandle     tree_push_front_new(TreePool *tree_pool, PoolHandle parent, u32 elem_size, u8 *data);
internal void           tree_push_front(TreePool *tree_pool, PoolHandle parent, PoolHandle new, u8 *data);
internal void           tree_pop_front(TreePool *tree_pool, PoolHandle parent, u8 *data);

// (jd) NOTE: We could define a iteration methods if needed and specify in on iter begin
// Right to left, depth first, preorder
internal TreeIterator   tree_iter_begin(Arena *arena, TreePool *tree_pool, u8 *data, PoolHandle iter_root);
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
#define TREE_POP_FRONT(ttp, parent)         tree_pop_front(&TREE_BASE_(ttp), (parent), (u8*)(ttp)->data)
#define TREE_GET(ttp, h)                    ((ttp)->data + pool_handle_to_pool_idx(&TREE_BASE_(ttp).pool, h))
#define TREE_GET_ROOT_H(ttp)                (TREE_BASE_(ttp).tree_root)
#define TREE_GET_ROOT(ttp)                  TREE_GET(ttp, TREE_GET_ROOT_H(ttp))

#define TREE_ITER_BEGIN(a, ttp, node)   tree_iter_begin(a, &TREE_BASE_(ttp), (u8*)&(ttp)->data, node)
#define EACH_TREE_NODE(h, it)           (PoolHandle h; (h = tree_iter_next(it), h.idx);) 

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
    TempArena temp = threadctx_get_temp(0,0);
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

