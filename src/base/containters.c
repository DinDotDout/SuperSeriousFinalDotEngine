internal u8*
raw_buffer_get(u8 raw_buffer[], i32 idx, u32 elem_size)
{
    u8 *res = &raw_buffer[idx*elem_size];
    return res;
}

// Reserve 0 for default and zero every time we hand it?
internal u8*
pool_obtain(Pool *p, PoolHandle h, u32 elem_size)
{
    u32 idx = h.handle[0];
    DOT_ASSERT(idx < p->elem_count, "Pool exceeded its capacity");
    u32 elem_idx = p->idx_buffer[p->elem_current];
    u8 *res = raw_buffer_get(p->raw_buffer, elem_idx, elem_size);
    return res;
}

internal PoolHandle
pool_get_handle(Pool *p)
{
    DOT_ASSERT(p->elem_current+1 < p->elem_count, "Pool exceeded its capacity");
    u32 next_idx = p->elem_current+1;
    PoolHandle ret = { .handle[0] = next_idx};
    ++p->elem_current;
    return ret;
}

internal void
pool_free_handle(Pool *p, PoolHandle h)
{
    u32 idx = h.handle[0];
    if(idx == 0){
        return;
    }

    u32 *idx_buffer = p->idx_buffer; 
    DOT_SWAP(u32, idx_buffer[p->elem_current+1], idx_buffer[idx]);
}

internal void
pool_init(Arena *arena, Pool *p, u32 elem_count, u32 elem_size)
{
    p->elem_count = elem_count;
    p->raw_buffer = PUSH_ARRAY_NO_ZERO(arena, u8, elem_count*elem_size);
    p->idx_buffer = PUSH_ARRAY_NO_ZERO(arena, u32, elem_count);
    p->elem_current = 0;
    for EACH_INDEX(i, elem_count){
       p->idx_buffer[i] = i;
    }
}
