#ifndef HASHMAP_H
#define HASHMAP_H

////////////////////////////////////////////////////////////////
/// Static hashing

#define FNV1A_OFFSET 0xcbf29ce484222325ull
#define FNV1A_PRIME  0x100000001b3ull

#define DOT_FNV1A_STEP(h, c) ((h ^ (unsigned char)(c)) * FNV1A_PRIME)

#define DOT_FNV1A_1_UNCHECKED(s)  DOT_FNV1A_STEP(FNV1A_OFFSET, s[0])
#define DOT_FNV1A_2_UNCHECKED(s)  DOT_FNV1A_STEP(DOT_FNV1A_1_UNCHECKED(s), s[1])
#define DOT_FNV1A_3_UNCHECKED(s)  DOT_FNV1A_STEP(DOT_FNV1A_2_UNCHECKED(s), s[2])
#define DOT_FNV1A_4_UNCHECKED(s)  DOT_FNV1A_STEP(DOT_FNV1A_3_UNCHECKED(s), s[3])
#define DOT_FNV1A_5_UNCHECKED(s)  DOT_FNV1A_STEP(DOT_FNV1A_4_UNCHECKED(s), s[4])
#define DOT_FNV1A_6_UNCHECKED(s)  DOT_FNV1A_STEP(DOT_FNV1A_5_UNCHECKED(s), s[5])
#define DOT_FNV1A_7_UNCHECKED(s)  DOT_FNV1A_STEP(DOT_FNV1A_6_UNCHECKED(s), s[6])
#define DOT_FNV1A_8_UNCHECKED(s)  DOT_FNV1A_STEP(DOT_FNV1A_7_UNCHECKED(s), s[7])

#define DOT_FNV1A_1(s)  (DOT_STATIC_ASSERT_LITERAL_LEN(s,1), DOT_FNV1A_1_UNCHECKED(s))
#define DOT_FNV1A_2(s)  (DOT_STATIC_ASSERT_LITERAL_LEN(s,2), DOT_FNV1A_2_UNCHECKED(s))
#define DOT_FNV1A_3(s)  (DOT_STATIC_ASSERT_LITERAL_LEN(s,3), DOT_FNV1A_3_UNCHECKED(s))
#define DOT_FNV1A_4(s)  (DOT_STATIC_ASSERT_LITERAL_LEN(s,4), DOT_FNV1A_4_UNCHECKED(s))
#define DOT_FNV1A_5(s)  (DOT_STATIC_ASSERT_LITERAL_LEN(s,5), DOT_FNV1A_5_UNCHECKED(s))
#define DOT_FNV1A_6(s)  (DOT_STATIC_ASSERT_LITERAL_LEN(s,6), DOT_FNV1A_6_UNCHECKED(s))
#define DOT_FNV1A_7(s)  (DOT_STATIC_ASSERT_LITERAL_LEN(s,7), DOT_FNV1A_7_UNCHECKED(s))
#define DOT_FNV1A_8(s)  (DOT_STATIC_ASSERT_LITERAL_LEN(s,8), DOT_FNV1A_8_UNCHECKED(s))

////////////////////////////////////////////////////////////////
/// Dynamic hashing

typedef enum HashMapOpKind
{
    HashMapOpKind_Hash,
    HashMapOpKind_Equal,
}HashMapOpKind;

typedef u64 HashMapHashEq(HashMapOpKind op, void *a, void *b, u64 key_size);


internal u64 hash_fnv1a_u64(void *data, u64 size);
internal u64 hash_djb2_u64(void *data, u64 size);
internal u64 hash_mix_u64(u64 x);

#ifndef HASH_MAP_DEFAULT_HASH
#define HASH_MAP_DEFAULT_HASH hash_fnv1a_u64
#endif

#ifndef HASH_MAP_DEFAULT_HASHEQ
#define HASH_MAP_DEFAULT_HASHEQ hm_hasheq_bytes
#endif

enum { HASH_MAP_ALIGN = 16 };

internal u64 hm_hasheq_bytes(HashMapOpKind op, void *a, void *b, u64 key_size);
internal u64 hm_hasheq_string8(HashMapOpKind op, void *a, void *b, u64 key_size);
internal u64 hm_hasheq_u64(HashMapOpKind op, void *a, void *b, u64 key_size);

typedef struct HashMapNode HashMapNode;
struct HashMapNode
{
    HashMapNode *next;
    u64          hash;
};

typedef struct HashMapCtx
{
    HashMapNode  **buckets;
    u32            bucket_count;
    u32            count;
    u32            key_size;
    u32            value_size;
    u32            key_offset;
    u32            value_offset;
    HashMapHashEq *hasheq;
}HashMapCtx;

typedef struct HashMapIter
{
    u32          bucket_idx;
    HashMapNode *node;
}HashMapIter;

internal void  hm_init_(Arena *arena, HashMapCtx *ctx, u32 bucket_count, u64 key_size, u64 value_size, HashMapHashEq *hasheq);
internal u64   hm_hash_(HashMapCtx *ctx, void *key);
internal void *hm_get_(HashMapCtx *ctx, void *key);
internal void *hm_get_or_create_(Arena *arena, HashMapCtx *ctx, void *key, void *default_value);
internal void *hm_iter_next_(HashMapCtx *ctx, HashMapIter *it);

#define HashMap(K, V) \
struct \
{ \
    HashMapCtx ctx; \
    K         *key_type; \
    V          default_value; \
}

#define HASH_MAP_KEY_(hm, k)   ((TYPEOF(*(hm)->key_type)[1]){(k)})
#define HASH_MAP_CAST_(hm, p) ((TYPEOF(&(hm)->default_value))(p)) // Type check and allos temporaries see `hm_tests`

#define hm_init(arena, hm, bucket_count, hasheq)  hm_init_((arena), &(hm)->ctx, (bucket_count), sizeof(*(hm)->key_type), sizeof((hm)->default_value), (hasheq))
#define hm_init_string8(arena, hm, bucket_count)  (DOT_STATIC_ASSERT_TYPE(*(hm)->key_type, String8), hm_init((arena), (hm), (bucket_count), hm_hasheq_string8))
#define hm_init_u64(arena, hm, bucket_count)      (DOT_STATIC_ASSERT_TYPE(*(hm)->key_type, u64), hm_init((arena), (hm), (bucket_count), hm_hasheq_u64))
#define hm_hash(hm, k)                            hm_hash_(&(hm)->ctx, HASH_MAP_KEY_(hm, k))
#define hm_get(hm, k)                             HASH_MAP_CAST_(hm, hm_get_(&(hm)->ctx, HASH_MAP_KEY_(hm, k)))
#define hm_get_or_create(arena, hm, k)            HASH_MAP_CAST_(hm, hm_get_or_create_((arena), &(hm)->ctx, HASH_MAP_KEY_(hm, k), &(hm)->default_value))
#define hm_iter_next(hm, it)                      HASH_MAP_CAST_(hm, hm_iter_next_(&(hm)->ctx, (it)))
#define hm_count(hm) ((hm)->ctx.count)

#endif // !HASHMAP_H
