internal u64
hash_fnv1a_u64(void *data, u64 size)
{
    u8 *p = cast(u8*) data;
    u64 h = FNV1A_OFFSET;
    for(u64 i = 0; i < size; ++i){
        h ^= p[i];
        h *= FNV1A_PRIME;
    }
    return(h);
}

internal u64
hash_djb2_u64(void *data, u64 size)
{
    u8 *p = cast(u8*) data;
    u64 h = 5381;
    for(u64 i = 0; i < size; ++i){
        h = ((h << 5) + h) + p[i];
    }
    return(h);
}

internal u64
hash_mix_u64(u64 x)
{
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return(x ^ (x >> 31));
}

internal u64
hm_hasheq_bytes(HashMapOpKind op, void *a, void *b, u64 key_size)
{
    switch(op){
    case HashMapOpKind_Hash:  return(HASH_MAP_DEFAULT_HASH(a, key_size));
    case HashMapOpKind_Equal: return(memcmp(a, b, key_size) == 0);
    }
    return(0);
}

internal u64
hm_hasheq_string8(HashMapOpKind op, void *a, void *b, u64 key_size)
{
    DOT_UNUSED(key_size);
    String8 *sa = cast(String8*) a;
    String8 *sb = cast(String8*) b;
    switch(op){
    case HashMapOpKind_Hash:  return(HASH_MAP_DEFAULT_HASH(sa->str, sa->size));
    case HashMapOpKind_Equal: return(string8_equal(*sa, *sb));
    }
    return(0);
}

internal u64
hm_hasheq_u64(HashMapOpKind op, void *a, void *b, u64 key_size)
{
    DOT_ASSERT(key_size == sizeof(u64), "hm_hasheq_u64 needs 8-byte keys, got %" PRIu64, key_size);
    u64 ka = *cast(u64*) a;
    switch(op){
    case HashMapOpKind_Hash:  return(hash_mix_u64(ka));
    case HashMapOpKind_Equal: return(ka == *cast(u64*) b);
    }
    return(0);
}

internal void
hm_init_(Arena *arena, HashMapCtx *ctx, u32 bucket_count, u64 key_size, u64 value_size, HashMapHashEq *hasheq)
{
    DOT_ASSERT(IS_POW2(bucket_count) && bucket_count > 0, "Bucket count must be a power of 2!");
    MEMORY_ZERO_STRUCT(ctx);
    ctx->buckets      = PUSH_ARRAY(arena, HashMapNode*, bucket_count);
    ctx->bucket_count = bucket_count;
    ctx->key_size     = cast(u32) key_size;
    ctx->value_size   = cast(u32) value_size;
    ctx->key_offset   = cast(u32) ALIGN_POW2(sizeof(HashMapNode), HASH_MAP_ALIGN);
    ctx->value_offset = cast(u32) ALIGN_POW2(ctx->key_offset + ctx->key_size, HASH_MAP_ALIGN);
    ctx->hasheq       = hasheq ? hasheq : HASH_MAP_DEFAULT_HASHEQ;
}

internal u64
hm_hash_(HashMapCtx *ctx, void *key)
{
    DOT_ASSERT(ctx->hasheq, "Hashmap may not have been initialized");
    return(ctx->hasheq(HashMapOpKind_Hash, key, NULL, ctx->key_size));
}

internal HashMapNode *
hm_find_(HashMapCtx *ctx, void *key, u64 hash)
{
    DOT_ASSERT(ctx->bucket_count > 0, "Hashmap may not have been initialized");
    HashMapNode *first = ctx->buckets[MOD_POW2(hash, ctx->bucket_count)];
    for EACH_NODE(it, HashMapNode, first){
        if(it->hash == hash && ctx->hasheq(HashMapOpKind_Equal, key, cast(u8*) it + ctx->key_offset, ctx->key_size)){
            return(it);
        }
    }
    return(NULL);
}

internal void *
hm_get_(HashMapCtx *ctx, void *key)
{
    HashMapNode *node = hm_find_(ctx, key, hm_hash_(ctx, key));
    return(node ? cast(u8*) node + ctx->value_offset : NULL);
}

internal void *
hm_get_or_create_(Arena *arena, HashMapCtx *ctx, void *key, void *default_value)
{
    u64 hash = hm_hash_(ctx, key);
    HashMapNode *node = hm_find_(ctx, key, hash);
    if(!node){
        node = cast(HashMapNode*) ARENA_PUSH(arena, ctx->value_offset + ctx->value_size, HASH_MAP_ALIGN, true);
        node->hash = hash;
        MEMORY_COPY(cast(u8*) node + ctx->key_offset, key, ctx->key_size);
        MEMORY_COPY(cast(u8*) node + ctx->value_offset, default_value, ctx->value_size);

        HashMapNode **bucket = &ctx->buckets[MOD_POW2(hash, ctx->bucket_count)];
        SLL_STACK_PUSH(*bucket, node);
        ctx->count += 1;
    }
    return(cast(u8*) node + ctx->value_offset);
}

internal void *
hm_iter_next_(HashMapCtx *ctx, HashMapIter *it)
{
    if(it->node && it->node->next){
        it->node = it->node->next;
        return(cast(u8*) it->node + ctx->value_offset);
    }
    for(u32 i = it->bucket_idx; i < ctx->bucket_count; ++i){
        if(ctx->buckets[i]){
            it->bucket_idx = i + 1;
            it->node       = ctx->buckets[i];
            return(cast(u8*) it->node + ctx->value_offset);
        }
    }
    return(NULL);
}

DOT_TEST_SUITE(hm_tests)
{
    DOT_TestResults test = {0};
    Arena *arena = ARENA_CREATE();

    HashMap(String8, u32) names = {0};
    names.default_value = 7;
    hm_init_string8(arena, &names, 4);
    *hm_get_or_create(arena, &names, str_comp("alpha")) = 1;
    *hm_get_or_create(arena, &names, str_comp("alpha")) = 1;
    // *hm_get_or_create(arena, &names, str_comp("alpha")) = 1;
    *hm_get_or_create(arena, &names, str_comp("beta"))  = 2;
    String8 alpha_copy = string8_copy(arena, str_comp("alpha"));
    DOT_TEST_CHECK(test, "String8 keys compare by content", *hm_get(&names, alpha_copy) == 1);
    DOT_TEST_CHECK(test, "Missing key returns NULL", hm_get(&names, str_comp("gamma")) == NULL);
    DOT_TEST_CHECK(test, "New values start as default_value", *hm_get_or_create(arena, &names, str_comp("gamma")) == 7);
    DOT_TEST_CHECK(test, "Count", hm_count(&names) == 3);
    DOT_TEST_CHECK(test, "Two lookups in one expression", *hm_get(&names, str_comp("alpha")) + *hm_get(&names, str_comp("beta")) == 3);

    HashMap(u64, f32) by_id = {0};
    hm_init(arena, &by_id, 2, NULL);
    for(u64 i = 0; i < 64; ++i){
        *hm_get_or_create(arena, &by_id, i) = cast(f32) i;
    }
    DOT_TEST_CHECK(test, "Default hasheq with chained buckets", *hm_get(&by_id, 41ull) == 41.0f);

    f32 sum = 0.0f;
    u32 visited = 0;
    HashMapIter it = {0};
    for(f32 *v = hm_iter_next(&by_id, &it); v; v = hm_iter_next(&by_id, &it)){
        sum += *v;
        visited += 1;
    }
    DOT_TEST_CHECK(test, "Iteration visits every entry", visited == 64 && sum == 2016.0f);

    ARENA_DESTROY(arena);
    return(test);
}
