internal u8*
raw_buffer_get(u8 raw_buffer[], i32 idx, u32 elem_size)
{
    u8 *res = &raw_buffer[idx*elem_size];
    return res;
}

// Reserve 0 for default and zero every time we hand it?
internal void*
pool_obtain(Pool *p, PoolHandle h, u32 elem_size)
{
    u32 idx = h;
    if(idx == 0){
        u8 *ptr = raw_buffer_get(p->raw_buffer, 0, elem_size);
        MEMORY_ZERO(ptr,  elem_size);
        return ptr;
    }
    DOT_ASSERT(idx < p->elem_count, "Invalid pool handle");
    u32 elem_idx = p->idx_buffer[p->elem_current];
    u8 *res = raw_buffer_get(p->raw_buffer, elem_idx, elem_size);
    return res;
}

internal PoolHandle
pool_get_handle(Pool *p)
{
    DOT_ASSERT(p->elem_current < p->elem_count, "Pool exceeded its capacity");
    PoolHandle h = p->elem_current++;
    return h;
}

internal void
pool_free_handle(Pool *p, PoolHandle h, u32 elem_size)
{
    u32 idx = h;
    if(idx == 0){
        return;
    }

    u8 *ptr = raw_buffer_get(p->raw_buffer, p->idx_buffer[idx], elem_size);
    MEMORY_ZERO(ptr,  elem_size);

    DOT_SWAP(u32, p->idx_buffer[p->elem_current], p->idx_buffer[idx]);
    --p->elem_current;
}

internal void
pool_init(Arena *arena, Pool *p, u32 elem_count, u32 elem_size, u32 alignment)
{
    p->elem_count = elem_count;
    p->raw_buffer = PUSH_ARRAY_ALIGNED(arena, u8, elem_count*elem_size, alignment);
    p->idx_buffer = PUSH_ARRAY_NO_ZERO(arena, u32, elem_count);
    p->elem_current = 0;
    for EACH_INDEX(i, elem_count){
       p->idx_buffer[i] = i;
    }
}
