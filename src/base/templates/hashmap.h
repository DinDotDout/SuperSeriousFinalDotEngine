#ifndef HM_T
#   error "HM_T must be specified when including this file"
#endif

#ifdef HM_K
#   ifndef HM_HASHER
#       error "if you provide you own HM_K, you need to provide a custom hashing 'HM_HASHER' too"
#   endif
#   ifndef HM_COMPARE
#       error "if you provide you own key, you need to provide a custom compare 'HM_COMPARE' too"
#   endif
#else
#   define HM_K String8
#   define HM_HASHER(k) u64_hash_from_string8(k, U64_MAX)
#   define HM_COMPARE string8_equal
#endif

// (jd) NOTE: You can provide your own custom name if desired

#ifndef HM_PREFIX
#define HM_PREFIX HM_T
#endif

#define HM_NAME DOT_CONCAT(HashMap_, HM_PREFIX)

#define HM_NODE DOT_CONCAT(HM_NAME, _Node)
#define HM_FN(name) DOT_CONCAT(hash_map_, DOT_CONCAT(HM_PREFIX, _##name))

// (jd) NOTE: if you get:
// 'error: pasting formed '*_Node', an invalid preprocessing token'
// just define your own HM_PREFIX :)
typedef struct HM_NODE HM_NODE;
struct HM_NODE{
    HM_T    *elem;
    HM_K    key;
    u64     hash;
    HM_NODE *next;
};

typedef struct HM_NAME{
    HM_NODE  **node_buckets;
    u32     bucket_count;
    u32     elem_count;
}HM_NAME;

internal void   HM_FN(init)(Arena *arena, HM_NAME *hashmap, u32 bucket_count);
internal void   HM_FN(end)(HM_NAME *hashmap);
internal HM_T  *HM_FN(get_or_create)(Arena *arena, HM_NAME *hashmap, HM_K key);
internal u64    HM_FN(hash)(HM_K key);
internal HM_T  *HM_FN(bucket_push_new)(Arena *arena, HM_NAME *hashmap, HM_K key, u64 hash);
internal u64    HM_FN(bucket_from_hash)(HM_NAME *hashmap, u64 hash);

#ifndef HM_ITER
#define HM_ITER
typedef struct HashMap_Iter{
    u32 bucket_idx;
    void *node;     // opaque HM_NODE*
}HashMap_Iter;

internal HashMap_Iter
hash_map_iter_init(void)
{
    HashMap_Iter ret = {0};
    return  ret;
}
#endif // HM_ITER

internal HM_T *HM_FN(iter_next)(HM_NAME *hm, HashMap_Iter *it);

#ifdef HM_IMPL

internal void
HM_FN(init)(Arena *arena, HM_NAME *hashmap, u32 bucket_count)
{
    DOT_ASSERT(IS_POW2(bucket_count), "Bucket count must be a power of 2!");
    hashmap->bucket_count = bucket_count;
    hashmap->node_buckets = PUSH_ARRAY(arena, HM_NODE*, hashmap->bucket_count);
}

internal void
HM_FN(end)(HM_NAME *hashmap)
{
    DOT_UNUSED(hashmap);
}

internal u64
HM_FN(bucket_from_hash)(HM_NAME *hashmap, u64 hash)
{
    u64 shader_hash = MOD_POW2(hash, hashmap->bucket_count);
    return(shader_hash);
}

internal HM_T *
HM_FN(bucket_push_new)(Arena *arena, HM_NAME *hashmap, HM_K key, u64 hash)
{
    HM_NODE *new_node = PUSH_STRUCT(arena, HM_NODE);
    new_node->elem  = PUSH_STRUCT(arena, HM_T);
    new_node->hash  = hash;
    new_node->key   = key;

    u64 shader_bucket_idx   = HM_FN(bucket_from_hash)(hashmap, hash);
    HM_NODE **bucket        = &hashmap->node_buckets[shader_bucket_idx];

    new_node->next  = *bucket;
    *bucket         = new_node;

    hashmap->elem_count += 1;
    return(new_node->elem);
}

internal u64
HM_FN(hash)(HM_K key)
{
    u64 hash = HM_HASHER(key);
    return(hash);
}

internal HM_T*
HM_FN(get_or_create)(Arena *arena, HM_NAME *hashmap, HM_K key)
{
    u64 hash = HM_HASHER(key);
    u64 bucket_idx = HM_FN(bucket_from_hash)(hashmap, hash);
    HM_NODE *node = hashmap->node_buckets[bucket_idx];
    HM_T *elem = NULL;
    for EACH_NODE(it, HM_NODE, node){
        if(hash == it->hash && HM_COMPARE(key, it->key)){
            elem = it->elem;
            break;
        }
    }
    if(!elem){
        elem = HM_FN(bucket_push_new)(arena, hashmap, key, hash);
    }
    return(elem);
}

internal HM_T *
HM_FN(iter_next)(HM_NAME *hm, HashMap_Iter *it) {
    HM_NODE *node = (HM_NODE *)it->node;

    // Continue within current bucket
    if(node && node->next){
        node = node->next;
        it->node = node;
        HM_T * elem = node->elem;
        return(elem);
    }

    // Move to next bucket
    for(u32 i = it->bucket_idx; i < hm->bucket_count; i++){
        node = (HM_NODE *)hm->node_buckets[i];
        if(node){
            it->bucket_idx = i + 1;
            it->node = node;
            HM_T *elem = node->elem;
            return(elem);
        }
    }

    return(0);
}

#endif // !HM_IMPL

// (jd) NOTE: Clean those up for the user
#undef HM_T
#undef HM_K
#undef HM_HASHER
#undef HM_COMPARE
#undef HM_NAME
#undef HM_NODE
#undef HM_FN
#undef HM_PREFIX
