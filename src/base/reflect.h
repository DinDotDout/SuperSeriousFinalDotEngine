#ifndef REFLECT_H
#define REFLECT_H

typedef struct RFL_Type RFL_Type;

typedef struct alignas(16) RFL_TagDef
{
    String8   name;
    RFL_Type *args_type;
    char     *file;
    u32       line;
}RFL_TagDef;

typedef struct RFL_Tag
{
    String8     name;
    String8     args;
    RFL_TagDef *def;
    void       *data;
}RFL_Tag;

typedef struct RFL_Field
{
    String8   name;
    String8   type_name;
    String8   type_arg;
    RFL_Tag  *tags;
    u32       tag_count;
    RFL_Type *type;
    u32       offset;
    u32       array_count;
    u32       pointer_depth;
}RFL_Field;

typedef struct RFL_EnumValue
{
    String8  name;
    i64      value;
    RFL_Tag *tags;
    u32      tag_count;
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
    RFL_TypeKind_Int,
    RFL_TypeKind_Uint,
    RFL_TypeKind_Float,
    RFL_TypeKind_Bool,
    RFL_TypeKind_Enum,
    RFL_TypeKind_Struct,
    RFL_TypeKind_Alias,
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
    RFL_Type         *base;

    u32               tag_count;
    RFL_Tag          *tags;

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

typedef struct alignas(16) RFL_ConstDef
{
    String8  name;
    i64      value;
    char    *file;
    u32      line;
}RFL_ConstDef;

typedef struct RFL_Const
{
    RFL_Type     *owner;
    RFL_ConstDef *def;
    i64           value;
    b32           known;
}RFL_Const;

typedef struct RFL_Registry
{
    Arena *arena;
    HashMap(String8, RFL_Type*) types;
    HashMap(String8, RFL_Const) consts;
    HashMap(String8, RFL_TagDef*) tag_defs;
}RFL_Registry;

SECTION_DECLARE(RFL_Type, ReflectSection);
SECTION_DECLARE(RFL_ConstDef, ReflectConstSection);
SECTION_DECLARE(RFL_TagDef, ReflectTagSection);

////////////////////////////////
// Internals of the declaration macros below. Not meant to be used directly.

#define RFL_TYPE_MARKER(T) enum { T##_type_is_not_reflected_use_Reflect = 0 }
#define RFL_ENUM_MARKER(T) enum { T##_enum_is_not_reflected_use_Reflect = 0 }

#define RFL_DEFINE_TYPE(T, k, body_text, base_type) \
    SECTION_ITEM(ReflectSection) static RFL_Type rfl_g_type_##T = { \
        .kind  = (k), \
        .name  = str_lit(#T), \
        .body  = str_lit(body_text), \
        .file  = __FILE__, \
        .line  = __LINE__, \
        .size  = sizeof(T), \
        .align = DOT_ALIGNOF(T), \
        .base  = (base_type), \
    }

////////////////////////////////
// Declaring: reflected types, constants, tags and their argument types.

#define Reflect(T, ...) \
    typedef struct T __VA_ARGS__ T; \
    RFL_TYPE_MARKER(T); \
    RFL_DEFINE_TYPE(T, RFL_TypeKind_Struct, #__VA_ARGS__, 0)

#define ReflectEnum(size_type, T, ...) \
    typedef size_type T; \
    enum __VA_ARGS__; \
    RFL_ENUM_MARKER(T); \
    RFL_DEFINE_TYPE(T, RFL_TypeKind_Enum, #__VA_ARGS__, rfl_type_def(size_type))

#define ReflectAlias(T, base_T) \
    RFL_TYPE_MARKER(T); \
    RFL_DEFINE_TYPE(T, RFL_TypeKind_Alias, "", rfl_type_def(base_T))

#define ReflectExtern(T, ...) \
    RFL_TYPE_MARKER(T); \
    RFL_DEFINE_TYPE(T, RFL_TypeKind_Struct, #__VA_ARGS__, 0)

#define ReflectConst(c) \
    SECTION_ITEM(ReflectConstSection) static RFL_ConstDef rfl_g_const_##c = { \
        .name  = str_lit(#c), \
        .value = cast(i64)(c), \
        .file  = __FILE__, \
        .line  = __LINE__, \
    }

#define ReflectTag(tag_name, ArgsT) \
    SECTION_ITEM(ReflectTagSection) static RFL_TagDef rfl_g_tag_##tag_name = { \
        .name      = str_lit(#tag_name), \
        .args_type = rfl_type_def(ArgsT), \
        .file      = __FILE__, \
        .line      = __LINE__, \
    }

#define rfl(name, ...)      rfl_##name(__VA_ARGS__)
#define rfl_range(min, max) DOT_STATIC_ASSERT((min) < (max), "rfl_range(" #min ", " #max "): min must be less than max");

////////////////////////////////
// Using reflection from code that names the type at compile time.
// rfl_type_def is the one usable in static initializers, for registries keyed by type (settings, UI views).

#define rfl_type_def(T)                 (&rfl_g_type_##T)
#define reflect_type(T)                 ((void)T##_type_is_not_reflected_use_Reflect, rfl_g_type_##T.canonical)
#define reflect_enum(T)                 ((void)T##_enum_is_not_reflected_use_Reflect, rfl_g_type_##T.canonical)
#define rfl_any(T, ptr)                 ((RFL_Any){ .type = ((void)DOT_STATIC_ASSERT_TYPE(*(ptr), T, "rfl_any: pointer is not a " #T "*"), reflect_type(T)), .data = (ptr) })
#define rfl_string8_from_enum(T, value) rfl_string8_from_enum_(reflect_enum(T), cast(i64)(value))

////////////////////////////////
// Built-in tags.

Reflect(RFL_TagRange, {
    f64 min;
    f64 max;
});
ReflectTag(range, RFL_TagRange);

////////////////////////////////
// Generic reflection: walking values whose type is only known at runtime.
// For tools built on top of reflection (UIs, settings, serialization).

internal RFL_Type *rfl_type_lookup(String8 name);
internal RFL_Any   rfl_any_from_type(RFL_Type *type, void *data);
internal RFL_Any   rfl_any_field(RFL_Any v, RFL_Field *field, u32 index);
internal RFL_Tag  *rfl_tag_find(RFL_Tag *tags, u32 tag_count, String8 name);
internal String8   rfl_string8_from_enum_(RFL_Type *type, i64 value);
internal String8   rfl_string8_from_any(Arena *arena, RFL_Any value);
internal b32       rfl_any_set_from_string8(RFL_Any value, String8 text);
internal i64       rfl_int_read(void *data, u32 size, b32 is_signed);
internal void      rfl_int_write(void *data, u32 size, i64 value);

////////////////////////////////
// Setup: run once at startup, before anything above is used at runtime.

internal void      rfl_init(Arena *arena);

#endif // !REFLECT_H
