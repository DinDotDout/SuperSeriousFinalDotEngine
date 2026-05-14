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
pool_handle_access(Pool *p, PoolHandle h)
{
    DOT_ASSERT(h < p->capacity, "Invalid pool handle");
    void *elem = raw_buffer_get(p->raw_buffer, h, p->elem_size);
    if(h == 0){
        MEMORY_ZERO(elem, p->elem_size);
    }
    return(elem);
}

internal PoolHandle
pool_handle_get_null(Pool *p)
{
    PoolHandle h = p->idx_buffer[0];
    void *elem = raw_buffer_get(p->raw_buffer, h, p->elem_size);
    MEMORY_ZERO(elem, p->elem_size);
    return(h);
}

internal PoolHandle
pool_handle_get(Pool *p)
{
    if(!(p->count < p->capacity)){
       DOT_WARNING("Pool capacity, exceeded, returning zero handle"); 
       return(0);
    }
    PoolHandle h = p->idx_buffer[p->count];
    p->count += 1;
    void *elem = raw_buffer_get(p->raw_buffer, h, p->elem_size);
    MEMORY_ZERO(elem, p->elem_size);
    return(h);
}

internal void
pool_handle_free(Pool *p, PoolHandle h)
{
    if(h == 0 || p->count == 1){
        return;
    }
    // If we end up making generational counters we can maybe swap with some other idx
    // to spread generations
    p->count -= 1;
    p->idx_buffer[p->count] = h;
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
/// Pool Tree

internal TreeHeader*
tree_header_get_(TreePool *tree_pool, PoolHandle idx)
{
    Pool *pool = &tree_pool->pool;
    u8 *elem = pool_handle_access(pool, idx);
    TreeHeader *header = cast(TreeHeader*)(cast(u8*)elem + tree_pool->offsetoff_header);
    return(header);
}

internal PoolHandle
tree_init(Arena *arena, TreePool *tp, u32 capacity, u32 elem_size, u32 alignment, u32 offsetoff_header)
{
    pool_init(arena, &tp->pool, capacity, elem_size, alignment);
    tp->offsetoff_header = offsetoff_header;
    PoolHandle root = pool_handle_get(&tp->pool);
    tp->tree_root = root;
    return(root);
}


internal PoolHandle
tree_get_next_sibling(TreePool *tp, PoolHandle node_h)
{
    TreeHeader *parent_header = tree_header_get_(tp, node_h);
    PoolHandle sibling_h = parent_header->sibling;
    return(sibling_h);
}

internal PoolHandle
tree_get_last_child(TreePool *tp, PoolHandle parent_h)
{
    TreeHeader *parent_header = tree_header_get_(tp, parent_h);
    PoolHandle last_child_h = parent_header->last_child;
    return(last_child_h);
}

internal PoolHandle
tree_get_first_child(TreePool *tp, PoolHandle parent_h)
{
    PoolHandle last_child_h = tree_get_last_child(tp, parent_h);
    PoolHandle first_child_h = tree_get_next_sibling(tp, last_child_h);
    return(first_child_h);
}

// internal PoolHandle
// tree_push_sibling_new(TreePool *tp, PoolHandle prev_h)
// {
//     PoolHandle new_elem_h = pool_handle_get(&tp->pool);
//     TreeHeader *prev_header = tree_header_get_(tp, prev_h);
//     TreeHeader *new_header = tree_header_get_(tp, new_elem_h);
//     new_header->sibling = prev_header->sibling;
//     prev_header->sibling = new_elem_h;
//     return new_elem_h;
// }

internal void
tree_push_sibling(TreePool *tp, PoolHandle prev_h, PoolHandle new_h)
{
    TreeHeader *prev_header = tree_header_get_(tp, prev_h);
    TreeHeader *new_header = tree_header_get_(tp, new_h);
    new_header->sibling = prev_header->sibling;
    prev_header->sibling = new_h;
}


internal void
tree_push_front(TreePool *tp, PoolHandle parent_h, PoolHandle new_h)
{
    if(parent_h != 0){
        TreeHeader *parent_header = tree_header_get_(tp, parent_h);
        tree_push_sibling(tp, parent_header->last_child, new_h);
    }
}

internal PoolHandle
tree_push_front_new(TreePool *tp, PoolHandle parent_h)
{
    PoolHandle new_elem_h = pool_handle_get(&tp->pool);
    tree_push_front(tp, parent_h, new_elem_h);
    return(new_elem_h);
}

internal PoolHandle
tree_pop_sibling(TreePool *tp, PoolHandle node_h)
{
    TreeHeader *node_header = tree_header_get_(tp, node_h);
    PoolHandle old_sibling_h = node_header->sibling;
    TreeHeader *old_header = tree_header_get_(tp, old_sibling_h);
    node_header->sibling = old_header->sibling;
    return(old_sibling_h);
}

internal void*
tree_handle_access(TreePool *tp, PoolHandle h)
{
    void *ret = pool_handle_access(&tp->pool, h);
    return(ret);
}

internal void
tree_pop_front(TreePool *tp, PoolHandle parent_h)
{
    if(parent_h != 0){
        TreeHeader *p = tree_header_get_(tp, parent_h);
        PoolHandle old_first_h = tree_pop_sibling(tp, p->last_child);
        pool_handle_free(&tp->pool, old_first_h);
    }
}
