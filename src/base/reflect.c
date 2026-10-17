global RFL_Registry rfl_g_registry;

ReflectPrimitive(i8,    RFL_Kind_Int);
ReflectPrimitive(i16,   RFL_Kind_Int);
ReflectPrimitive(i32,   RFL_Kind_Int);
ReflectPrimitive(i64,   RFL_Kind_Int);
ReflectPrimitive(int,   RFL_Kind_Int);
ReflectPrimitive(iptr,  RFL_Kind_Int);

ReflectPrimitive(u8,    RFL_Kind_Uint);
ReflectPrimitive(u16,   RFL_Kind_Uint);
ReflectPrimitive(u32,   RFL_Kind_Uint);
ReflectPrimitive(u64,   RFL_Kind_Uint);
ReflectPrimitive(usize, RFL_Kind_Uint);
ReflectPrimitive(uptr,  RFL_Kind_Uint);

ReflectPrimitive(b8,    RFL_Kind_Bool);
ReflectPrimitive(b16,   RFL_Kind_Bool);
ReflectPrimitive(b32,   RFL_Kind_Bool);

ReflectPrimitive(f32,   RFL_Kind_Float);
ReflectPrimitive(f64,   RFL_Kind_Float);

ReflectExtern(f32x2,   { f32 x, y; });
ReflectExtern(f32x3,   { f32 x, y, z; });
ReflectExtern(f32x4,   { f32 x, y, z, w; });
ReflectExtern(f32x4x4, { f32x4 cols[4]; });
ReflectExtern(u32x2,   { u32 x, y; });
ReflectExtern(u32x3,   { u32 x, y, z; });
ReflectExtern(i32x2,   { i32 x, y; });
ReflectExtern(i32x3,   { i32 x, y, z; });

typedef u32 RFL_TokenKind;
enum
{
    RFL_TokenKind_End,
    RFL_TokenKind_Ident,
    RFL_TokenKind_Int,
    RFL_TokenKind_Punct,
};

typedef struct RFL_Token
{
    RFL_TokenKind kind;
    String8       text;
    i64           value;
}RFL_Token;

typedef struct RFL_Parser
{
    RFL_Type  *type;
    u64        pos;
    RFL_Token  tok;
    b32        evaluate;
}RFL_Parser;

internal void rfl_resolve(RFL_Type *type);

internal void
rfl_next(RFL_Parser *p)
{
    String8 src = p->type->body;
    while(p->pos < src.size && char_is_whitespace(src.str[p->pos])){
        p->pos += 1;
    }

    RFL_Token tok = {0};
    u64 start = p->pos;
    if(p->pos >= src.size){
        tok.kind = RFL_TokenKind_End;
    }else if(char_is_digit(src.str[p->pos])){
        u64 value = 0;
        if(src.str[p->pos] == '0' && p->pos + 1 < src.size && (src.str[p->pos + 1] == 'x' || src.str[p->pos + 1] == 'X')){
            p->pos += 2;
            while(p->pos < src.size && char_is_hex_digit(src.str[p->pos])){
                value = value * 16 + char_hex_value(src.str[p->pos]);
                p->pos += 1;
            }
        }else{
            while(p->pos < src.size && char_is_digit(src.str[p->pos])){
                value = value * 10 + cast(u64)(src.str[p->pos] - '0');
                p->pos += 1;
            }
        }
        while(p->pos < src.size && (src.str[p->pos] == 'u' || src.str[p->pos] == 'U' || src.str[p->pos] == 'l' || src.str[p->pos] == 'L')){
            p->pos += 1;
        }
        tok.kind  = RFL_TokenKind_Int;
        tok.value = cast(i64)value;
    }else if(char_is_alpha(src.str[p->pos]) || src.str[p->pos] == '_'){
        while(p->pos < src.size && (char_is_alnum(src.str[p->pos]) || src.str[p->pos] == '_')){
            p->pos += 1;
        }
        tok.kind = RFL_TokenKind_Ident;
    }else{
        u8 c = src.str[p->pos];
        p->pos += 1;
        if((c == '<' || c == '>') && p->pos < src.size && src.str[p->pos] == c){
            p->pos += 1;
        }
        tok.kind = RFL_TokenKind_Punct;
    }
    tok.text = string8(src.str + start, p->pos - start);
    p->tok = tok;
}

internal RFL_Parser
rfl_parser_begin(RFL_Type *type, b32 evaluate)
{
    RFL_Parser p = {.type = type, .evaluate = evaluate};
    rfl_next(&p);
    return p;
}

internal b32
rfl_is(RFL_Parser *p, char *punct)
{
    return p->tok.kind == RFL_TokenKind_Punct && string8_equal(p->tok.text, string8_from_cstring(punct));
}

internal b32
rfl_accept(RFL_Parser *p, char *punct)
{
    b32 result = rfl_is(p, punct);
    if(result){
        rfl_next(p);
    }
    return result;
}

internal void
rfl_expect(RFL_Parser *p, char *punct)
{
    if(!rfl_accept(p, punct)){
        DOT_ERROR_FL(p->type->file, cast(int)p->type->line, "Reflect %S: expected '%s', got '%S'", p->type->name, punct, p->tok.text);
    }
}

internal String8
rfl_expect_ident(RFL_Parser *p)
{
    if(p->tok.kind != RFL_TokenKind_Ident){
        DOT_ERROR_FL(p->type->file, cast(int)p->type->line, "Reflect %S: expected a name, got '%S'", p->type->name, p->tok.text);
    }
    String8 result = p->tok.text;
    rfl_next(p);
    return result;
}

internal void
rfl_expect_end(RFL_Parser *p)
{
    if(p->tok.kind != RFL_TokenKind_End){
        DOT_ERROR_FL(p->type->file, cast(int)p->type->line, "Reflect %S: unexpected '%S' after the body", p->type->name, p->tok.text);
    }
}

internal String8
rfl_parse_tags(RFL_Parser *p)
{
    String8 tag = {0};
    while(p->tok.kind == RFL_TokenKind_Ident && string8_starts_with(p->tok.text, str_comp("rfl_"))){
        tag = p->tok.text;
        rfl_next(p);
    }
    return tag;
}

internal i64
rfl_const_value(RFL_Parser *p, String8 name)
{
    RFL_Const *c = hm_get(&rfl_g_registry.consts, name);
    if(!c){
        DOT_ERROR_FL(p->type->file, cast(int)p->type->line, "Reflect %S: unknown constant '%S'; reflect its enum with ReflectEnum or add ReflectConst(%S)", p->type->name, name, name);
    }
    if(!c->known){
        if(c->owner->resolve_state == RFL_ResolveState_InProgress){
            DOT_ERROR_FL(p->type->file, cast(int)p->type->line, "Reflect %S: constant '%S' used before its value is defined", p->type->name, name);
        }
        rfl_resolve(c->owner);
    }
    return c->value;
}

internal i64 rfl_expr(RFL_Parser *p);

internal i64
rfl_expr_unary(RFL_Parser *p)
{
    if(rfl_accept(p, "-")){ return -rfl_expr_unary(p); }
    if(rfl_accept(p, "~")){ return ~rfl_expr_unary(p); }
    if(rfl_accept(p, "(")){
        i64 value = rfl_expr(p);
        rfl_expect(p, ")");
        return value;
    }
    if(p->tok.kind == RFL_TokenKind_Int){
        i64 value = p->tok.value;
        rfl_next(p);
        return value;
    }
    String8 name = rfl_expect_ident(p);
    return p->evaluate ? rfl_const_value(p, name) : 0;
}

internal i64
rfl_expr_divisor(RFL_Parser *p)
{
    i64 value = rfl_expr_unary(p);
    if(p->evaluate && value == 0){
        DOT_ERROR_FL(p->type->file, cast(int)p->type->line, "Reflect %S: division by zero", p->type->name);
    }
    return value ? value : 1;
}

internal i64
rfl_expr_mul(RFL_Parser *p)
{
    i64 value = rfl_expr_unary(p);
    for(;;){
        if(rfl_accept(p, "*")){      value *= rfl_expr_unary(p); }
        else if(rfl_accept(p, "/")){ value /= rfl_expr_divisor(p); }
        else if(rfl_accept(p, "%")){ value %= rfl_expr_divisor(p); }
        else{ return value; }
    }
}

internal i64
rfl_expr_add(RFL_Parser *p)
{
    i64 value = rfl_expr_mul(p);
    for(;;){
        if(rfl_accept(p, "+")){      value += rfl_expr_mul(p); }
        else if(rfl_accept(p, "-")){ value -= rfl_expr_mul(p); }
        else{ return value; }
    }
}

internal i64
rfl_expr_shift(RFL_Parser *p)
{
    i64 value = rfl_expr_add(p);
    for(;;){
        if(rfl_accept(p, "<<")){      value = cast(i64)(cast(u64)value << rfl_expr_add(p)); }
        else if(rfl_accept(p, ">>")){ value = value >> rfl_expr_add(p); }
        else{ return value; }
    }
}

internal i64
rfl_expr_and(RFL_Parser *p)
{
    i64 value = rfl_expr_shift(p);
    while(rfl_accept(p, "&")){
        value &= rfl_expr_shift(p);
    }
    return value;
}

internal i64
rfl_expr(RFL_Parser *p)
{
    i64 value = rfl_expr_and(p);
    while(rfl_accept(p, "|")){
        value |= rfl_expr_and(p);
    }
    return value;
}

internal u32
rfl_parse_enum(RFL_Parser *p, RFL_EnumValue *out)
{
    u32 count = 0;
    i64 next  = 0;
    rfl_expect(p, "{");
    while(!rfl_is(p, "}")){
        rfl_parse_tags(p);
        String8 name = rfl_expect_ident(p);
        i64 value = next;
        if(rfl_accept(p, "=")){
            value = rfl_expr(p);
        }
        if(out){
            RFL_Const *c = hm_get(&rfl_g_registry.consts, name);
            c->value = value;
            c->known = true;
            out[count] = (RFL_EnumValue){.name = name, .value = value};
        }else{
            RFL_Const *c = hm_get_or_create(rfl_g_registry.arena, &rfl_g_registry.consts, name);
            if(c->owner || c->known){
                DOT_ERROR_FL(p->type->file, cast(int)p->type->line, "Reflect %S: constant '%S' is already defined", p->type->name, name);
            }
            c->owner = p->type;
        }
        next   = value + 1;
        count += 1;
        if(!rfl_accept(p, ",")){
            break;
        }
    }
    rfl_expect(p, "}");
    rfl_expect_end(p);
    return count;
}

internal u32
rfl_parse_struct(RFL_Parser *p, RFL_Field *out)
{
    u32 count = 0;
    rfl_expect(p, "{");
    while(!rfl_is(p, "}")){
        String8 tag = rfl_parse_tags(p);
        String8 type_name = rfl_expect_ident(p);
        if(string8_equal(type_name, str_comp("const"))){
            type_name = rfl_expect_ident(p);
        }
        if(string8_equal(type_name, str_comp("struct")) || string8_equal(type_name, str_comp("union"))){
            if(rfl_is(p, "{")){
                DOT_ERROR_FL(p->type->file, cast(int)p->type->line, "Reflect %S: nested struct/union bodies are not supported", p->type->name);
            }
            type_name = rfl_expect_ident(p);
        }
        for(;;){
            u32 pointer_depth = 0;
            while(rfl_accept(p, "*")){
                pointer_depth += 1;
            }
            String8 name = rfl_expect_ident(p);
            u32 array_count = 1;
            if(rfl_accept(p, "[")){
                i64 n = rfl_expr(p);
                rfl_expect(p, "]");
                if(p->evaluate && n <= 0){
                    DOT_ERROR_FL(p->type->file, cast(int)p->type->line, "Reflect %S: array '%S' has size %lld", p->type->name, name, n);
                }
                array_count = cast(u32)n;
            }
            if(out){
                out[count] = (RFL_Field){
                    .name          = name,
                    .type_name     = type_name,
                    .tag           = tag,
                    .array_count   = array_count,
                    .pointer_depth = pointer_depth,
                };
            }
            count += 1;
            if(!rfl_accept(p, ",")){
                break;
            }
        }
        rfl_expect(p, ";");
    }
    rfl_expect(p, "}");
    rfl_expect_end(p);
    return count;
}

internal RFL_Type *
rfl_lookup(String8 name)
{
    RFL_Type **slot = hm_get(&rfl_g_registry.types, name);
    return slot ? *slot : 0;
}

internal void
rfl_resolve_enum(RFL_Type *type)
{
    type->values = PUSH_ARRAY(rfl_g_registry.arena, RFL_EnumValue, type->value_count);
    RFL_Parser p = rfl_parser_begin(type, true);
    rfl_parse_enum(&p, type->values);
}

internal void
rfl_resolve_struct(RFL_Type *type)
{
    RFL_Parser count_parser = rfl_parser_begin(type, false);
    type->field_count = rfl_parse_struct(&count_parser, 0);
    type->fields = PUSH_ARRAY(rfl_g_registry.arena, RFL_Field, type->field_count);
    RFL_Parser p = rfl_parser_begin(type, true);
    rfl_parse_struct(&p, type->fields);

    u32 offset = 0;
    u32 align  = 1;
    for(u32 i = 0; i < type->field_count; ++i){
        RFL_Field *field = &type->fields[i];
        field->type = rfl_lookup(field->type_name);
        u32 field_size  = cast(u32)sizeof(void*);
        u32 field_align = cast(u32)DOT_ALIGNOF(void*);
        if(field->pointer_depth == 0){
            if(!field->type){
                DOT_ERROR_FL(type->file, cast(int)type->line, "Reflect %S: field '%S' has unknown type '%S'; reflect it with Reflect, ReflectEnum, ReflectPrimitive or ReflectExtern", type->name, field->name, field->type_name);
            }
            rfl_resolve(field->type);
            field_size  = field->type->size;
            field_align = field->type->align;
        }
        offset        = ALIGN_POW2(offset, field_align);
        field->offset = offset;
        offset       += field_size * field->array_count;
        align         = DOT_MAX(align, field_align);
    }
    u32 size = ALIGN_POW2(offset, align);
    if(size != type->size || align != type->align){
        DOT_ERROR_FL(type->file, cast(int)type->line, "Reflect %S: parsed layout is size %u align %u, the compiler says size %u align %u", type->name, size, align, type->size, type->align);
    }
}

internal void
rfl_resolve(RFL_Type *type)
{
    if(type->resolve_state == RFL_ResolveState_Done){
        return;
    }
    if(type->resolve_state == RFL_ResolveState_InProgress){
        DOT_ERROR_FL(type->file, cast(int)type->line, "Reflect %S: type contains itself", type->name);
    }
    type->resolve_state = RFL_ResolveState_InProgress;
    switch(type->kind){
    case RFL_Kind_Struct: rfl_resolve_struct(type); break;
    case RFL_Kind_Enum:   rfl_resolve_enum(type);   break;
    default: break;
    }
    type->resolve_state = RFL_ResolveState_Done;
}

internal void
rfl_init(Arena *arena)
{
    rfl_g_registry.arena = arena;
    hm_init_string8(arena, &rfl_g_registry.types, RFL_TYPE_BUCKETS_MAX);
    hm_init_string8(arena, &rfl_g_registry.consts, RFL_CONST_BUCKETS_MAX);

    for EACH_IN_SECTION(ReflectSection, RFL_Type, type){
        if(type->kind == RFL_Kind_Const){
            RFL_Const *c = hm_get_or_create(arena, &rfl_g_registry.consts, type->name);
            if(c->owner || c->known){
                DOT_ERROR_FL(type->file, cast(int)type->line, "ReflectConst %S: constant is already defined", type->name);
            }
            c->value = type->const_value;
            c->known = true;
            continue;
        }
        RFL_Type **slot = hm_get_or_create(arena, &rfl_g_registry.types, type->name);
        if(*slot){
            DOT_ERROR_FL(type->file, cast(int)type->line, "Reflect %S: already reflected at %s:%u", type->name, (*slot)->file, (*slot)->line);
        }
        *slot = type;
    }

    for EACH_IN_SECTION(ReflectSection, RFL_Type, type){
        if(type->kind == RFL_Kind_Enum){
            RFL_Parser p = rfl_parser_begin(type, false);
            type->value_count = rfl_parse_enum(&p, 0);
        }
    }

    for EACH_IN_SECTION(ReflectSection, RFL_Type, type){
        if(type->kind != RFL_Kind_Const){
            rfl_resolve(type);
        }
    }
}

internal RFL_Any
rfl_any_field(RFL_Any v, RFL_Field *field, u32 index)
{
    DOT_ASSERT(field->pointer_depth == 0, "Field %S is a pointer", field->name);
    DOT_ASSERT(index < field->array_count);
    RFL_Any result = {
        .type = field->type,
        .data = cast(u8*)v.data + field->offset + index * field->type->size,
    };
    return result;
}

/////////////////////////////////////////////// Test suite

#define RFL_TEST_SCALE 2
ReflectConst(RFL_TEST_SCALE);

ReflectEnum(u32, RFL_TestKind, {
    RFL_TestKind_A,
    RFL_TestKind_B = 5,
    RFL_TestKind_C,
    RFL_TestKind_Count,
});

ReflectEnum(i32, RFL_TestFlags, {
    RFL_TestFlagBit_X    = 1 << 0,
    RFL_TestFlagBit_Y    = 1 << 3,
    RFL_TestFlagBit_XY   = RFL_TestFlagBit_X | RFL_TestFlagBit_Y,
    RFL_TestFlagBit_Neg  = -(2 + 1),
    RFL_TestFlagBit_Far  = RFL_TestKind_Count << 1,
    RFL_TestFlagBit_Mask = 0xF0 & ~0x30,
});

Reflect(RFL_TestStruct, {
    u8 a;
    f32 x, y;
    rfl_color f32x3 color;
    u32 slots[RFL_TestKind_Count * RFL_TEST_SCALE];
    struct RFL_TestStruct *next, **list;
    u64 big;
    RFL_TestKind kind;
    f32x4x4 m;
});

DOT_TEST_SUITE(rfl_tests)
{
    DOT_TestResults test = {0};

    RFL_Type *kind = reflect_type(RFL_TestKind);
    DOT_TEST_CHECK(test, "enum count",       kind->value_count == 4);
    DOT_TEST_CHECK(test, "enum implicit",    kind->values[0].value == RFL_TestKind_A);
    DOT_TEST_CHECK(test, "enum explicit",    kind->values[1].value == RFL_TestKind_B);
    DOT_TEST_CHECK(test, "enum next",        kind->values[2].value == RFL_TestKind_C);
    DOT_TEST_CHECK(test, "enum count value", kind->values[3].value == RFL_TestKind_Count);
    DOT_TEST_CHECK(test, "enum name",        string8_equal(kind->values[2].name, str_comp("RFL_TestKind_C")));

    RFL_Type *flags = reflect_type(RFL_TestFlags);
    DOT_TEST_CHECK(test, "flags shift",      flags->values[1].value == RFL_TestFlagBit_Y);
    DOT_TEST_CHECK(test, "flags or",         flags->values[2].value == RFL_TestFlagBit_XY);
    DOT_TEST_CHECK(test, "flags negate",     flags->values[3].value == RFL_TestFlagBit_Neg);
    DOT_TEST_CHECK(test, "flags other enum", flags->values[4].value == RFL_TestFlagBit_Far);
    DOT_TEST_CHECK(test, "flags and/not",    flags->values[5].value == RFL_TestFlagBit_Mask);

    RFL_Type *s = reflect_type(RFL_TestStruct);
    DOT_TEST_CHECK(test, "struct fields",    s->field_count == 10);
    DOT_TEST_CHECK(test, "offset a",         s->fields[0].offset == offsetof(RFL_TestStruct, a));
    DOT_TEST_CHECK(test, "offset x",         s->fields[1].offset == offsetof(RFL_TestStruct, x));
    DOT_TEST_CHECK(test, "offset y",         s->fields[2].offset == offsetof(RFL_TestStruct, y));
    DOT_TEST_CHECK(test, "offset color",     s->fields[3].offset == offsetof(RFL_TestStruct, color));
    DOT_TEST_CHECK(test, "offset slots",     s->fields[4].offset == offsetof(RFL_TestStruct, slots));
    DOT_TEST_CHECK(test, "offset next",      s->fields[5].offset == offsetof(RFL_TestStruct, next));
    DOT_TEST_CHECK(test, "offset list",      s->fields[6].offset == offsetof(RFL_TestStruct, list));
    DOT_TEST_CHECK(test, "offset big",       s->fields[7].offset == offsetof(RFL_TestStruct, big));
    DOT_TEST_CHECK(test, "offset kind",      s->fields[8].offset == offsetof(RFL_TestStruct, kind));
    DOT_TEST_CHECK(test, "offset m",         s->fields[9].offset == offsetof(RFL_TestStruct, m));
    DOT_TEST_CHECK(test, "tag",              string8_equal(s->fields[3].tag, str_comp("rfl_color")));
    DOT_TEST_CHECK(test, "no tag",           s->fields[2].tag.size == 0);
    DOT_TEST_CHECK(test, "array count",      s->fields[4].array_count == RFL_TestKind_Count * RFL_TEST_SCALE);
    DOT_TEST_CHECK(test, "pointer depth",    s->fields[5].pointer_depth == 1 && s->fields[6].pointer_depth == 2);
    DOT_TEST_CHECK(test, "self pointer",     s->fields[5].type == s);
    DOT_TEST_CHECK(test, "enum field",       s->fields[8].type == kind);
    DOT_TEST_CHECK(test, "extern fields",    s->fields[3].type->field_count == 3 && string8_equal(s->fields[3].type->fields[2].name, str_comp("z")));
    DOT_TEST_CHECK(test, "extern offsets",   s->fields[3].type->fields[2].offset == offsetof(f32x3, z));
    DOT_TEST_CHECK(test, "extern nested",    s->fields[9].type->fields[0].type == reflect_type(f32x4) && s->fields[9].type->fields[0].array_count == 4);
    DOT_TEST_CHECK(test, "lookup",           rfl_lookup(str_comp("RFL_TestStruct")) == s);
    DOT_TEST_CHECK(test, "lookup missing",   rfl_lookup(str_comp("RFL_Missing")) == 0);

    RFL_TestStruct value = {.slots[3] = 42};
    RFL_Any any  = rfl_any(RFL_TestStruct, &value);
    RFL_Any slot = rfl_any_field(any, &s->fields[4], 3);
    DOT_TEST_CHECK(test, "any field",        slot.type == reflect_type(u32) && *cast(u32*)slot.data == 42);

    return test;
}
