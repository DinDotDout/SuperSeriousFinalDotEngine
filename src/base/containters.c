internal u8*
raw_buffer_get(u8 raw_buffer[], i32 elem_idx, u32 elem_size)
{
    u8 *res = &raw_buffer[elem_idx*elem_size];
    return(res);
}

////////////////////////////////////////////////////////////////
///
/// Pool

// Reserve 0 for default and zero every time we hand it?
internal void*
pool_get(Pool *p, PoolHandle h)
{
    DOT_ASSERT(p->capacity > 0, "Uninitialized pool");
    DOT_ASSERT(h.idx < p->capacity, "Invalid pool handle");
    void *elem = raw_buffer_get(p->raw_buffer, h.idx, p->elem_size);
    if(h.idx == 0){
        MEMORY_ZERO(elem, p->elem_size);
    }
    return(elem);
}

internal PoolHandle
pool_null_handle_get(Pool *p)
{
    DOT_ASSERT(p->capacity > 0, "Uninitialized pool");
    PoolHandle h = POOL_NULL_HANDLE;
    void *elem = raw_buffer_get(p->raw_buffer, h.idx, p->elem_size);
    MEMORY_ZERO(elem, p->elem_size);
    return(h);
}

internal PoolHandle
pool_alloc(Pool *p)
{
    DOT_ASSERT(p->capacity > 0, "Uninitialized pool");
    if(DOT_UNLIKELY(p->count >= p->capacity)){
       DOT_WARNING("Pool capacity, exceeded, returning zero handle");
       return(pool_null_handle_get(p));
    }
    PoolHandle h = {.idx = p->idx_buffer[p->count]};
    p->count += 1;
    void *elem = raw_buffer_get(p->raw_buffer, h.idx, p->elem_size);
    MEMORY_ZERO(elem, p->elem_size);
    return(h);
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

internal void
pool_init(Arena *arena, Pool *p, u32 capacity, u32 elem_size, u32 alignment)
{
    MEMORY_ZERO_STRUCT(p);
    p->capacity = capacity;
    p->elem_size = elem_size;
    p->raw_buffer = PUSH_ARRAY_NO_ZERO_ALIGNED(arena, u8, capacity * elem_size, alignment);
    p->idx_buffer = PUSH_ARRAY_NO_ZERO(arena, u32, capacity);
    p->count = 1;
    for EACH_INDEX(i, capacity){
       p->idx_buffer[i] = i;
    }
}

////////////////////////////////////////////////////////////////
///
/// Tree Pool

internal TreeHeader*
tree_header_(TreePool *tree_pool, PoolHandle idx)
{
    Pool *pool = &tree_pool->pool;
    u8 *elem = pool_get(pool, idx);
    TreeHeader *header = cast(TreeHeader*)(cast(u8*)elem + tree_pool->offsetoff_header);
    return(header);
}

internal PoolHandle
tree_init(Arena *arena, TreePool *tree_pool, u32 capacity, u32 elem_size, u32 alignment, u32 offsetoff_header)
{
    pool_init(arena, &tree_pool->pool, capacity, elem_size, alignment);
    tree_pool->offsetoff_header = offsetoff_header;
    PoolHandle root = pool_alloc(&tree_pool->pool);
    tree_pool->tree_root = root;
    return(root);
}

internal PoolHandle
tree_get_next_sibling(TreePool *tree_pool, PoolHandle node)
{
    TreeHeader *node_header = tree_header_(tree_pool, node);
    PoolHandle sibling = node_header->sibling;
    return(sibling);
}

internal PoolHandle
tree_get_last_child(TreePool *tree_pool, PoolHandle parent)
{
    TreeHeader *parent_header = tree_header_(tree_pool, parent);
    PoolHandle last_child = parent_header->last_child;
    return(last_child);
}

internal PoolHandle
tree_get_first_child(TreePool *tree_pool, PoolHandle parent)
{
    PoolHandle last_child = tree_get_last_child(tree_pool, parent);
    PoolHandle first_child = tree_get_next_sibling(tree_pool, last_child);
    return(first_child);
}

internal void
tree_push_sibling(TreePool *tree_pool, PoolHandle prev_h, PoolHandle new)
{
    TreeHeader *prev_header = tree_header_(tree_pool, prev_h);
    TreeHeader *new_header = tree_header_(tree_pool, new);
    new_header->sibling = prev_header->sibling;
    prev_header->sibling = new;
}

internal void
tree_push_front(TreePool *tree_pool, PoolHandle parent, PoolHandle new)
{
    if(parent.idx != 0){
        TreeHeader *parent_header = tree_header_(tree_pool, parent);
        TreeHeader *new_header = tree_header_(tree_pool, new);
        if(pool_handle_is_null(parent_header->last_child)){
            parent_header->last_child = new;
            new_header->sibling = new;
        }else{
            tree_push_sibling(tree_pool, parent_header->last_child, new);
        }
    }
}

internal PoolHandle
tree_push_front_new(TreePool *tree_pool, PoolHandle parent)
{
    PoolHandle new_elem = pool_alloc(&tree_pool->pool);
    tree_push_front(tree_pool, parent, new_elem);
    return(new_elem);
}

internal PoolHandle
tree_pop_sibling(TreePool *tree_pool, PoolHandle node)
{
    TreeHeader *node_header = tree_header_(tree_pool, node);
    PoolHandle old_sibling = node_header->sibling;
    TreeHeader *old_header = tree_header_(tree_pool, old_sibling);
    node_header->sibling = old_header->sibling;
    return(old_sibling);
}

internal void
tree_pop_front(TreePool *tree_pool, PoolHandle parent)
{
    if(parent.idx != 0){
        TreeHeader *p = tree_header_(tree_pool, parent);
        PoolHandle old_first_h = tree_pop_sibling(tree_pool, p->last_child);
        pool_free(&tree_pool->pool, old_first_h);
    }
}

// (jd) NOTE: Should we always assume worst case of root containing all children
// or pass in a capacity?
internal TreeIterator
tree_iter_begin(Arena *arena, TreePool *tree_pool, PoolHandle iter_root)
{
    u32 stack_capacity = tree_pool->pool.count;
    TreeIterator it = {
        .tree_pool = tree_pool,
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
    PoolHandle first = tree_get_first_child(it->tree_pool, curr);
    if(first.idx == 0){
        return(curr);
    }
    it->stack[it->stack_idx++] = first;
    PoolHandle next = tree_get_next_sibling(it->tree_pool, first);
    while (next.idx != first.idx) {
        if (it->stack_idx >= it->stack_capacity) {
          DOT_ERROR("Tree iterator capacity exceeded, either the iterator is "
                    "too small or a node has been inserted more than once");
          return(POOL_IT_END);
        }
        it->stack[it->stack_idx++] = next;
        next = tree_get_next_sibling(it->tree_pool, next);
    }
    return(curr);
}
