internal u8*
raw_buffer_get(u8 raw_buffer[], i32 elem_idx, u32 elem_size)
{
    u8 *res = &raw_buffer[elem_idx*elem_size];
    return res;
}

////////////////////////////////////////////////////////////////
///
/// Pool

// Reserve 0 for default and zero every time we hand it?
internal void*
pool_handle_access(Pool *p, PoolHandle h, u32 elem_size)
{
    DOT_ASSERT(h < p->capacity, "Invalid pool handle");
    void *elem = raw_buffer_get(p->raw_buffer, h, elem_size);
    if(h == 0){
        MEMORY_ZERO(elem, elem_size);
    }
    return elem;
}

internal PoolHandle
pool_handle_get_null(Pool *p, u32 elem_size)
{
    PoolHandle h = p->idx_buffer[0];
    void *elem = raw_buffer_get(p->raw_buffer, h, elem_size);
    MEMORY_ZERO(elem, elem_size);
    return h;
}

internal PoolHandle
pool_handle_get(Pool *p, u32 elem_size)
{
    if(!(p->count < p->capacity)){
       DOT_WARNING("Pool capacity, exceeded, returning zero handle"); 
       return 0;
    }
    PoolHandle h = p->idx_buffer[p->count];
    p->count += 1;
    void *elem = raw_buffer_get(p->raw_buffer, h, elem_size);
    MEMORY_ZERO(elem, elem_size);
    return h;
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
    p->raw_buffer = PUSH_ARRAY_NO_ZERO_ALIGNED(arena, u8, capacity * elem_size, alignment);
    p->idx_buffer = PUSH_ARRAY_NO_ZERO(arena, u32, capacity);
    p->count = 1;
    for EACH_INDEX(i, capacity){
       p->idx_buffer[i] = i;
    }
}
