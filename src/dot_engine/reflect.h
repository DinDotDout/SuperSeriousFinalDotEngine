#ifndef REFLECT_H
#define REFLECT_H

// NOTE: Leaving this here for now and revisiting in the future. I just wanted to take this out of
// my brain
#if DOT_COMPILER_MSVC
#   pragma section(".reflect", read)
#   pragma comment(linker, "/merge:.reflect=.data")
#endif

#define RFL_REFLECT_SECTION SECTION_ITEM(reflect, 0)

//(jd)  NOTE: Maybe just enum / BitField is enough do know how to represent stuff instead of reflect kind
// (jd) NOTE: can we infer display kind from fields? ie vec3 from its components
// typedef u8 RFL_ReflectKind;
// enum{
//     RFL_ReflectKind_Primitive,
//     RFL_ReflectKind_Struct,
//     RFL_ReflectKind_Enum,
// };

// Parsing annotatins
// These can be used to add tags in the parsed fields that will only survive stringification
// so we can use it to specify the parser certain things
#define rfl_none
#define rfl_number
#define rfl_decimal
#define rfl_checkbox
#define rfl_color
#define rfl_hook
#define rfl_skip // skip field display

typedef u8 RFL_ReflectKind;
enum{
    // Primitives
    RFL_ReflectKind_Int,
    RFL_ReflectKind_Uint,
    RFL_ReflectKind_Float,
    RFL_ReflectKind_Checkbox,

    RFL_ReflectKind_BitField, // Should we display mask names somehow too?

    RFL_ReflectKind_Enum,

    // Compound
    RFL_ReflectKind_Compound,
    // Special Coumpound types
    RFL_ReflectKind_CompoundColor, // Special case for compound type?
};

#define RFL_EDIT_HOOK(name) void name(u32 data_count_b, u8 *dst, u8 *src)
typedef RFL_EDIT_HOOK(RFL_EditHook);

#define RFL_EDIT_HOOK_PTR(name) RFL_EDIT_HOOK((*name))

RFL_EDIT_HOOK(rfl_edit_hook_default);

RFL_EDIT_HOOK(rfl_edit_hook_default)
{
    (void)data_count_b, (void)src; (void)dst;
}

typedef struct RFL_ReflectMeta RFL_ReflectMeta;
struct RFL_ReflectMeta{
    RFL_ReflectKind kind;
    String8 name;
    u32 size;
    u32 align;


    RFL_ReflectMeta *fields;
    String8         *field_names;
    RFL_EditHook    *hook;
    u32 field_count;

    String8 parse_data; // Used to fill in other data for compound types
};

#define HM_T RFL_ReflectMeta*
#define HM_PREFIX RFL_ReflectMetaPtr
#include "base/templates/hashmap.h"

DECLARE_SECTION(RFL_ReflectMeta, reflect);

#define Reflect(n, hook_fn, ...) \
    typedef struct n __VA_ARGS__ n; \
    RFL_REFLECT_SECTION \
    static RFL_ReflectMeta __rfl_g_reflected_##n= { \
        .kind = RFL_ReflectKind_Compound, \
        .name = string8_lit(#n), \
        .size = sizeof(n), \
        .align = DOT_ALIGNOF(n), \
        .fields = 0, \
        .hook = hook_fn, \
        .parse_data = string8_lit(#__VA_ARGS__), \
    }

#define ReflectPrimitive(n, dk) \
    RFL_REFLECT_SECTION \
    static RFL_ReflectMeta __rfl_g_reflected_##n = { \
        .parse_data = {0}, \
        .name = string8_lit(#n), \
        .size = sizeof(n), \
        .align = DOT_ALIGNOF(n), \
        .fields = 0, \
        .kind = dk, \
        .hook = 0 \
    }

#define ReflectEnum(T, n, ...) \
    typedef T n; \
    enum  __VA_ARGS__ ; \
    RFL_REFLECT_SECTION \
    static RFL_ReflectMeta __rfl_g_reflected_##n = { \
        .kind = RFL_ReflectKind_Enum, \
        .parse_data = string8_lit(#__VA_ARGS__), \
        .name = string8_lit(#n), \
        .size = sizeof(n), \
        .align = DOT_ALIGNOF(n), \
        .fields = 0, \
    };

#define ReflectExtern(name, body) \
    RFL_REFLECT_SECTION \
    static RFL_ReflectMeta __rfl_g_reflected_##name = { #body, sizeof(name), DOT_ALIGNOF(name) };


internal inline u32
reflect_compute_offset(u32 current_offset, u32 field_align)
{
     return (current_offset + field_align - 1) & ~(field_align - 1);
}

ReflectPrimitive(int,  RFL_ReflectKind_Int);
ReflectPrimitive(i8,   RFL_ReflectKind_Int);
ReflectPrimitive(i16,  RFL_ReflectKind_Int);
ReflectPrimitive(i32,  RFL_ReflectKind_Int);
ReflectPrimitive(i64,  RFL_ReflectKind_Int);
ReflectPrimitive(iptr, RFL_ReflectKind_Int);

ReflectPrimitive(uint, RFL_ReflectKind_Uint);
ReflectPrimitive(u8,   RFL_ReflectKind_Uint);
ReflectPrimitive(u16,  RFL_ReflectKind_Uint);
ReflectPrimitive(u32,  RFL_ReflectKind_Uint);
ReflectPrimitive(u64,  RFL_ReflectKind_Uint);
ReflectPrimitive(usize,RFL_ReflectKind_Uint);
ReflectPrimitive(uptr, RFL_ReflectKind_Uint);

ReflectPrimitive(b32,  RFL_ReflectKind_Checkbox);
ReflectPrimitive(b16,  RFL_ReflectKind_Checkbox);
ReflectPrimitive(b8,   RFL_ReflectKind_Checkbox);

ReflectPrimitive(f32,  RFL_ReflectKind_Float);
ReflectPrimitive(f64,  RFL_ReflectKind_Float);

Reflect(Color, rfl_edit_hook_default,
rfl_color{
    f32 x;
    f32 y;
    f32 z;
});

Reflect(
Vec3, rfl_edit_hook_default,{
    f32 x; // Comments get removed before preprocessing step
    f32 y; rfl_none // The tag survives only to stringification
    f32 z;
});

ReflectEnum(u32, Some_enum, {
    Some_enum_test = 100,
    Some_enum_2,
    Some_enum_3
    }
)

internal b32
rfl_is_tag(String8 tok)
{
    return string8_starts_with(tok, string8_lit("rfl_"));
}

typedef struct RFL_ParsedField{
    String8 type_name;
    String8 field_name;
    String8 tag;
}RFL_ParsedField;

// We probably need to scrap this. We need to be aware of line endings and ; to proprely identify
// type vs names and tags
internal void
rfl_parse_struct_fields(Arena *arena, RFL_ReflectMeta *meta)
{
    String8 body = meta->parse_data;
    u8 tokens[] = CHAR_WHITESPACES"\n;,{}";
    String8List toks = string8_split(arena, body, DOT_ARRAY_COUNT(tokens) - 1, tokens);

    RFL_ParsedField tmp_fields[128];
    u32 tmp_count = 0;

    String8 pending_tag = {0};

    String8Node *it = LLHeadFirst(String8Node, &toks);
    while (it) {
        String8 tok = it->str;

        if (tok.size == 0) { it = string8_node_next(it); continue; }
        if (tok.size == 0) { it = LLNodeNext(String8Node, it); continue; }

        if (rfl_is_tag(tok)) {
            pending_tag = tok;
            it = LLNodeNext(String8Node, it);
            continue;
        }

        // Expect: type name
        String8 type = tok;
        it = LLNodeNext(String8Node, it);
        if (!it) break;

        // Expect: field name
        String8 name = it->str;
        it = LLNodeNext(String8Node, it);

        tmp_fields[tmp_count++] = (RFL_ParsedField){
            .type_name  = type,
            .field_name = name,
            .tag        = pending_tag,
        };

        pending_tag = (String8){0};
    }

    // Allocate final arrays
    meta->field_count = tmp_count;
    meta->field_names = malloc(sizeof(String8) * tmp_count);
    meta->hook = 0;
    meta->fields      = malloc(sizeof(RFL_ReflectMeta*) * tmp_count);

    meta->hook = &rfl_edit_hook_default;
    for (u32 i = 0; i < tmp_count; ++i) {
        meta->field_names[i] = tmp_fields[i].field_name;

        // Default hook
        // Tag → hook mapping
        // if (string8_equal(tmp_fields[i].tag, string8_lit("rfl_color"))) {
            // You can add custom hooks here
        // }

        // fields[i] unresolved for now
        // meta->field_names[i] = {0};
    }
}

typedef enum RFL_ParseTokenKind{
    RFL_ParseTokenKind_Comma,
    RFL_ParseTokenKind_HalfStop,
    RFL_ParseTokenKind_Type,
    RFL_ParseTokenKind_Var,
    RFL_ParseTokenKind_Tag,
}RFL_ParseTokenKind;


#endif // !REFLECT_H
