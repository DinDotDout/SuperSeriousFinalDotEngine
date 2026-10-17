#ifndef REFLECT_H
#define REFLECT_H

#include "base/reflect_shared.h"

typedef u32 RFL_Kind;
enum
{
    RFL_Kind_Int,
    RFL_Kind_Uint,
    RFL_Kind_Float,
    RFL_Kind_Bool,
    RFL_Kind_Enum,
    RFL_Kind_Struct,
    RFL_Kind_Const,
};

typedef u32 RFL_ResolveState;
enum
{
    RFL_ResolveState_None,
    RFL_ResolveState_InProgress,
    RFL_ResolveState_Done,
};

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

struct alignas(16) RFL_Type
{
    RFL_Kind          kind;
    u32               size;
    u32               align;
    u32               line;
    String8           name;
    String8           body;
    char             *file;
    RFL_Field        *fields;
    RFL_EnumValue    *values;
    u32               field_count;
    u32               value_count;
    RFL_ResolveState  resolve_state;
    i64               const_value;
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

#define RFL_SECTION_ITEM SECTION_ITEM(ReflectSection, 0)
#define RFL_MARKER(T) enum { T##_is_not_reflected_use_Reflect = 0 }

#define Reflect(T, ...) \
    typedef struct T __VA_ARGS__ T; \
    RFL_MARKER(T); \
    RFL_SECTION_ITEM static RFL_Type rfl_g_type_##T = { \
        .kind  = RFL_Kind_Struct, \
        .name  = str_lit(#T), \
        .body  = str_lit(#__VA_ARGS__), \
        .file  = __FILE__, \
        .line  = __LINE__, \
        .size  = sizeof(T), \
        .align = DOT_ALIGNOF(T), \
    }

#define ReflectEnum(B, T, ...) \
    typedef B T; \
    enum __VA_ARGS__; \
    RFL_MARKER(T); \
    RFL_SECTION_ITEM static RFL_Type rfl_g_type_##T = { \
        .kind  = RFL_Kind_Enum, \
        .name  = str_lit(#T), \
        .body  = str_lit(#__VA_ARGS__), \
        .file  = __FILE__, \
        .line  = __LINE__, \
        .size  = sizeof(T), \
        .align = DOT_ALIGNOF(T), \
    }

#define ReflectPrimitive(T, k) \
    RFL_MARKER(T); \
    RFL_SECTION_ITEM static RFL_Type rfl_g_type_##T = { \
        .kind  = (k), \
        .name  = str_lit(#T), \
        .file  = __FILE__, \
        .line  = __LINE__, \
        .size  = sizeof(T), \
        .align = DOT_ALIGNOF(T), \
    }

#define ReflectExtern(T, ...) \
    RFL_MARKER(T); \
    RFL_SECTION_ITEM static RFL_Type rfl_g_type_##T = { \
        .kind  = RFL_Kind_Struct, \
        .name  = str_lit(#T), \
        .body  = str_lit(#__VA_ARGS__), \
        .file  = __FILE__, \
        .line  = __LINE__, \
        .size  = sizeof(T), \
        .align = DOT_ALIGNOF(T), \
    }

#define ReflectConst(c) \
    RFL_SECTION_ITEM static RFL_Type rfl_g_const_##c = { \
        .kind        = RFL_Kind_Const, \
        .name        = str_lit(#c), \
        .file        = __FILE__, \
        .line        = __LINE__, \
        .const_value = cast(i64)(c), \
    }

#define reflect_type(T) ((void)T##_is_not_reflected_use_Reflect, &rfl_g_type_##T)
#define rfl_any(T, ptr) ((RFL_Any){ .type = ((void)DOT_STATIC_ASSERT_TYPE(*(ptr), T, "rfl_any: pointer is not a " #T "*"), reflect_type(T)), .data = (ptr) })

internal void      rfl_init(Arena *arena);
internal RFL_Type *rfl_lookup(String8 name);
internal RFL_Any   rfl_any_field(RFL_Any v, RFL_Field *field, u32 index);

#endif // !REFLECT_H
