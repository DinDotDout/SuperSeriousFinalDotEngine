#ifndef HM_T
#   error "HM_T must be specified when including this file"
#   define HM_T u64 // This is so that we silence rouge LSPs
#endif

#ifndef HM_K
#define HM_K String8
#endif

#ifndef HM_FUNC
#   error "HM_FUNC must be specified when including this file"
#   define HM_FUNC u64_hash_from_string8
#endif

#ifndef HM_NAME
#   define HM_NAME DOT_CONCAT(HashMap_, HM_T)
#endif

#define HM_NODE DOT_CONCAT(HM_NAME, Node)

typedef struct HM_NODE HM_NODE;
struct HM_NODE{
    HM_T *elem;
    u64 key;
    HM_NODE *next;
};

typedef struct HM_NAME{
    HM_NODE **node_buckets;
    u32 bucket_count;
    u32 total_nodes;
}HM_NAME;

internal void   hashmap_init(Arena *arena, HM_NAME *hashmap, u64 bucket_count);
internal void   hashmap_end(HM_NAME *hashmap);
internal HM_T   *hashmap_push_new(Arena *arena, HM_NAME *hashmap, u64 hash);

internal u64    hashmap_bucket_get(HM_NAME *hashmap, String8 str);

internal HM_T   *hashmap_get_or_create(Arena *arena, HM_NAME *hashmap, String8 str);

// #ifdef HASHMAP_IMPL

internal void
hashmap_init(Arena *arena, HM_NAME *hashmap, u64 bucket_count)
{
    hashmap->bucket_count = bucket_count;
    hashmap->node_buckets = PUSH_ARRAY(arena, HM_NODE*, hashmap->bucket_count);
}

internal void
hashmap_end(HM_NAME *hashmap)
{
    DOT_UNUSED(hashmap);
}


internal HashIdx
hashmap_bucket_from_hash(HM_NAME *hashmap, u64 hash)
{
    HashIdx shader_hash = MOD_POW2(hash, hashmap->bucket_count);
    return shader_hash;
}

internal HM_T
hashmap_push_new(Arena *arena, HM_NAME *hashmap, u64 hash)
{
    HM_NODE *new_node = PUSH_STRUCT(arena, HM_NODE);
    new_node->key = hash;

    HM_NODE **hash_buckets = hashmap->node_buckets;
    u64 shader_bucket_idx = hashmap_bucket_get(hashmap, hash);
    HM_NODE *node = hash_buckets[shader_bucket_idx];
    if(node){
        node->next = new_node;
    }
    hash_buckets[shader_bucket_idx] = new_node;
    hashmap->total_nodes += 1;

    return &new_node->elem;
}

internal HM_T*
hashmap_get_or_create(Arena *arena, HM_NAME *hashmap, HM_K key)
{
    u64 hash = HM_FUNC(key);
    u64 bucket_idx = hashmap_bucket_from_hash(hashmap, hash);
    HM_NODE *node = hashmap->node_buckets[bucket_idx];
    HM_T *elem = NULL;
    for EACH_NODE(it, HM_NODE, node){
        if(string8_equal(key, node->key)){ // NOTE: This still breaks on different key so also need compare func?
            elem = node->elem;
            break;
        }
    }
    if(!elem){
        elem = hashmap_push_new(arena, hashmap, hash);
    }
    return elem;
}
// #endif

// Clean those up for the user
#undef HM_T
#undef HM_FUNC
