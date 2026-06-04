internal u32
array_bounds_check(u32 elem_idx, u32 capacity, char *file, int line)
{
    if(elem_idx >= capacity){
        DOT_ERROR_FL(file, line, "Array idx out of bounds");
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
    // DOT_ASSERT(h.idx < p->capacity, "Invalid pool handle");
    if(h.idx >= p->capacity){
        DOT_WARNING("Pool capacity exceeded. Returning 0 handle");
        return 0;
    }
    return h.idx;
}

internal PoolHandle
pool_null_handle_get(Pool *p, u32 elem_size, u8 *data)
{
    DOT_ASSERT(p->capacity > 0, "Uninitialized pool");
    PoolHandle h = POOL_NULL_HANDLE;
    void *elem = raw_buffer_get(data, h.idx, elem_size);
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
    // void *elem = cast(u8*)data + h.idx * elem_size;
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
    return(data);
}


////////////////////////////////////////////////////////////////
///
/// Tree Pool

internal TreeHeader*
tree_header_(TreePool *tree_pool, PoolHandle h, u8 *data)
{
    Pool *pool = &tree_pool->pool;
    u8 *elem = data + pool_handle_to_pool_idx(pool, h);
    TreeHeader *header = cast(TreeHeader*)(cast(u8*)elem + tree_pool->offsetoff_header);
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
tree_get_next_sibling(TreePool *tree_pool, PoolHandle node, u8 *data)
{
    TreeHeader *node_header = tree_header_(tree_pool, node, data);
    PoolHandle sibling = node_header->sibling;
    return(sibling);
}

internal PoolHandle
tree_get_last_child(TreePool *tree_pool, PoolHandle parent, u8 *data)
{
    TreeHeader *parent_header = tree_header_(tree_pool, parent, data);
    PoolHandle last_child = parent_header->last_child;
    return(last_child);
}

internal PoolHandle
tree_get_first_child(TreePool *tree_pool, PoolHandle parent, u8 *data)
{
    PoolHandle last_child = tree_get_last_child(tree_pool, parent, data);
    PoolHandle first_child = tree_get_next_sibling(tree_pool, last_child, data);
    return(first_child);
}

internal void
tree_push_sibling(TreePool *tree_pool, PoolHandle prev_h, PoolHandle new, u8 *data)
{
    TreeHeader *prev_header = tree_header_(tree_pool, prev_h, data);
    TreeHeader *new_header = tree_header_(tree_pool, new, data);
    new_header->sibling = prev_header->sibling;
    prev_header->sibling = new;
}

internal void
tree_push_front(TreePool *tree_pool, PoolHandle parent, PoolHandle new, u8 *data)
{
    if(parent.idx != 0){
        TreeHeader *parent_header = tree_header_(tree_pool, parent, data);
        TreeHeader *new_header = tree_header_(tree_pool, new, data);
        if(pool_handle_is_null(parent_header->last_child)){
            parent_header->last_child = new;
            new_header->sibling = new;
        }else{
            tree_push_sibling(tree_pool, parent_header->last_child, new, data);
        }
    }
}

internal PoolHandle
tree_push_front_new(TreePool *tree_pool, PoolHandle parent, u32 elem_size, u8 *data)
{
    PoolHandle new_elem = pool_alloc(&tree_pool->pool, elem_size, data);
    tree_push_front(tree_pool, parent, new_elem, data);
    return(new_elem);
}

internal PoolHandle
tree_pop_sibling(TreePool *tree_pool, PoolHandle node, u8 *data)
{
    TreeHeader *node_header = tree_header_(tree_pool, node, data);
    PoolHandle old_sibling = node_header->sibling;
    TreeHeader *old_header = tree_header_(tree_pool, old_sibling, data);
    node_header->sibling = old_header->sibling;
    return(old_sibling);
}

internal void
tree_pop_front(TreePool *tree_pool, PoolHandle parent, u8 *data)
{
    if(parent.idx != 0){
        TreeHeader *p = tree_header_(tree_pool, parent, data);
        PoolHandle old_first_h = tree_pop_sibling(tree_pool, p->last_child, data);
        pool_free(&tree_pool->pool, old_first_h);
    }
}

// (jd) NOTE: Should we always assume worst case of root containing all children
// or pass in a capacity?
internal TreeIterator
tree_iter_begin(Arena *arena, TreePool *tree_pool, u8 *data, PoolHandle iter_root)
{
    u32 stack_capacity = tree_pool->pool.count;
    TreeIterator it = {
        .tree_pool = tree_pool,
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
    PoolHandle first = tree_get_first_child(it->tree_pool, curr, it->data);
    if(first.idx == 0){
        return(curr);
    }
    it->stack[it->stack_idx++] = first;
    PoolHandle next = tree_get_next_sibling(it->tree_pool, first, it->data);
    while (next.idx != first.idx) {
        if (it->stack_idx >= it->stack_capacity) {
          DOT_ERROR("Tree iterator capacity exceeded, either the iterator is "
                    "too small or a node has been inserted more than once");
          return(POOL_IT_END);
        }
        it->stack[it->stack_idx++] = next;
        next = tree_get_next_sibling(it->tree_pool, next, it->data);
    }
    return(curr);
}
