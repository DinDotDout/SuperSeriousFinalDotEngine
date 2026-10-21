#ifndef REFLECT_H
#define REFLECT_H

typedef struct RFL_Type RFL_Type;

typedef struct RFL_Field
{
    String8   name;
    String8   type_name;
    String8   tag;
    RFL_Type *type;
    u32       offset;
    u32       array_count;
    u32       pointer_depth;
}RFL_Field;

typedef struct RFL_EnumValue
{
    String8 name;
    i64     value;
}RFL_EnumValue;

typedef u32 RFL_ResolveState;
enum
{
    RFL_ResolveState_None,
    RFL_ResolveState_InProgress,
    RFL_ResolveState_Done,
};

typedef u32 RFL_TypeKind;
enum
{
    RFL_TypeKind_I32,
    RFL_TypeKind_U32,
    RFL_TypeKind_F32,
    RFL_TypeKind_Bool,
    RFL_TypeKind_Enum,
    RFL_TypeKind_Struct,
    RFL_TypeKind_Const,
};

struct alignas(16) RFL_Type
{
    RFL_TypeKind      kind;
    u32               size;
    u32               align;
    String8           name;

    u32               field_count;
    RFL_Field        *fields;

    u32               value_count;
    RFL_EnumValue    *values;

    i64               const_value;

    String8           body;
    u32               line;
    char             *file;
    RFL_ResolveState  resolve_state;
    RFL_Type         *canonical;
};

typedef struct RFL_Any
{
    RFL_Type *type;
    void     *data;
}RFL_Any;

typedef struct RFL_Const
{
    RFL_Type *owner;
    i64       value;
    b32       known;
}RFL_Const;

enum
{
    RFL_TYPE_BUCKETS_MAX  = 256,
    RFL_CONST_BUCKETS_MAX = 512,
};

typedef struct RFL_Registry
{
    Arena *arena;
    HashMap(String8, RFL_Type*) types;
    HashMap(String8, RFL_Const) consts;
}RFL_Registry;

SECTION_DECLARE(RFL_Type, ReflectSection);
#define RFL_SECTION_ITEM SECTION_ITEM(ReflectSection)
#define RFL_MARKER(T) enum { T##_is_not_reflected_use_Reflect = 0 }

#define RFL_DEFINE_TYPE(T, k, body_text) \
    RFL_MARKER(T); \
    RFL_SECTION_ITEM static RFL_Type rfl_g_type_##T = { \
        .kind  = (k), \
        .name  = str_lit(#T), \
        .body  = str_lit(body_text), \
        .file  = __FILE__, \
        .line  = __LINE__, \
        .size  = sizeof(T), \
        .align = DOT_ALIGNOF(T), \
    }

#define Reflect(T, ...) \
    typedef struct T __VA_ARGS__ T; \
    RFL_DEFINE_TYPE(T, RFL_TypeKind_Struct, #__VA_ARGS__)

#define ReflectEnum(size_type, T, ...) \
    typedef size_type T; \
    enum __VA_ARGS__; \
    RFL_DEFINE_TYPE(T, RFL_TypeKind_Enum, #__VA_ARGS__)

#define ReflectPrimitive(T, k) \
    RFL_DEFINE_TYPE(T, k, "")

#define ReflectExtern(T, ...) \
    RFL_DEFINE_TYPE(T, RFL_TypeKind_Struct, #__VA_ARGS__)

#define ReflectConst(c) \
    RFL_SECTION_ITEM static RFL_Type rfl_g_const_##c = { \
        .kind        = RFL_TypeKind_Const, \
        .name        = str_lit(#c), \
        .file        = __FILE__, \
        .line        = __LINE__, \
        .const_value = cast(i64)(c), \
    }

#define reflect_type(T) ((void)T##_is_not_reflected_use_Reflect, rfl_g_type_##T.canonical)
#define rfl_any(T, ptr) ((RFL_Any){ .type = ((void)DOT_STATIC_ASSERT_TYPE(*(ptr), T, "rfl_any: pointer is not a " #T "*"), reflect_type(T)), .data = (ptr) })

internal void      rfl_init(Arena *arena);
internal RFL_Type *rfl_type_lookup(String8 name);
internal RFL_Any   rfl_any_field(RFL_Any v, RFL_Field *field, u32 index);

#endif // !REFLECT_H
