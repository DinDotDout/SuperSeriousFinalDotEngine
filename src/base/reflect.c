// Recursive descent grammar for the stringized Reflect/ReflectEnum bodies:
//
//   enum_body   = '{' [ enum_value { ',' enum_value } [ ',' ] ] '}'
//   enum_value  = { tag } IDENT [ '=' expr ]
//
//   struct_body = '{' { decl } '}'
//   decl        = { tag } [ "const" ] [ "struct" | "union" ] IDENT declarator { ',' declarator } ';'
//   declarator  = { '*' } IDENT [ '[' expr ']' ]
//
//   expr        = and   { '|' and }
//   and         = shift { '&' shift }
//   shift       = add   { ( '<<' | '>>' ) add }
//   add         = mul   { ( '+' | '-' ) mul }
//   mul         = unary { ( '*' | '/' | '%' ) unary }
//   unary       = '-' unary | '~' unary | '(' expr ')' | INT | IDENT
//
//   tag         = IDENT starting with "rfl_"
//   INT         = decimal or 0x hex, with any u/l suffixes ignored
//   IDENT       = an enum value or ReflectConst name when used in expr
//
// Nested struct/union bodies are rejected; a decl's tag applies to every declarator in it.

global RFL_Registry rfl_g_registry;

ReflectPrimitive(i8,    RFL_TypeKind_Int);
ReflectPrimitive(i16,   RFL_TypeKind_Int);
ReflectPrimitive(i32,   RFL_TypeKind_Int);
ReflectPrimitive(i64,   RFL_TypeKind_Int);
ReflectPrimitive(int,   RFL_TypeKind_Int);
ReflectPrimitive(iptr,  RFL_TypeKind_Int);

ReflectPrimitive(u8,    RFL_TypeKind_Uint);
ReflectPrimitive(u16,   RFL_TypeKind_Uint);
ReflectPrimitive(u32,   RFL_TypeKind_Uint);
ReflectPrimitive(u64,   RFL_TypeKind_Uint);
ReflectPrimitive(usize, RFL_TypeKind_Uint);
ReflectPrimitive(uptr,  RFL_TypeKind_Uint);

ReflectPrimitive(b8,    RFL_TypeKind_Bool);
ReflectPrimitive(b16,   RFL_TypeKind_Bool);
ReflectPrimitive(b32,   RFL_TypeKind_Bool);

ReflectPrimitive(f32,   RFL_TypeKind_Float);
ReflectPrimitive(f64,   RFL_TypeKind_Float);

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

// A punctuator is its character constant: '*' or '<<'. Single chars are just the
// char. Two-char constants are packed in whatever order the compiler uses; the
// check below detects it, and rfl_punct_pack builds lexed tokens the same way.
// Clang and GCC warn on multi-char constants by default: silenced here up to the body parsers.
typedef u16 RFL_Punct;

DOT_DIAGNOSTIC_PUSH
#if DOT_COMPILER_CLANG || DOT_COMPILER_GCC
    DOT_DIAGNOSTIC_IGNORE("-Wmultichar")
#endif

#if 'ab' == (('a' << 8) | 'b')
    #define RFL_PUNCT_FIRST_CHAR_HIGH 1 // MSVC, Clang, GCC
#elif 'ab' == (('b' << 8) | 'a')
    #define RFL_PUNCT_FIRST_CHAR_HIGH 0
#else
    #error "Unknown multi-character constant layout"
#endif

// Packs two lexed chars the way the compiler packs 'ab'.
internal RFL_Punct
rfl_punct_pack(u8 first, u8 second)
{
#if RFL_PUNCT_FIRST_CHAR_HIGH
    return cast(RFL_Punct)((first << 8) | second);
#else
    return cast(RFL_Punct)(first | (second << 8));
#endif
}

typedef struct RFL_Token
{
    RFL_TokenKind kind;
    String8       text;
    i64           value;
    RFL_Punct     punct;
}RFL_Token;

typedef struct RFL_Parser
{
    RFL_Type  *type;
    u64        pos;
    RFL_Token  tok;
    b32        evaluate;
}RFL_Parser;

internal void rfl_resolve_type(RFL_Type *type);

////////////////////////////////
// Lexer: turns the stringized body into tokens, one at a time.

// Reads the next token from the body into parser->tok.
internal void
rfl_lex_next_token(RFL_Parser *parser)
{
    String8 body = parser->type->body;
    while(parser->pos < body.size && char_is_whitespace(body.str[parser->pos])){
        parser->pos += 1;
    }

    RFL_Token token = {0};
    u64 token_start = parser->pos;
    if(parser->pos >= body.size){
        token.kind = RFL_TokenKind_End;
    }else if(char_is_digit(body.str[parser->pos])){
        u64 number = 0;
        if(body.str[parser->pos] == '0' && parser->pos + 1 < body.size && (body.str[parser->pos + 1] == 'x' || body.str[parser->pos + 1] == 'X')){
            parser->pos += 2;
            while(parser->pos < body.size && char_is_hex_digit(body.str[parser->pos])){
                number = number * 16 + char_hex_value(body.str[parser->pos]);
                parser->pos += 1;
            }
        }else{
            while(parser->pos < body.size && char_is_digit(body.str[parser->pos])){
                number = number * 10 + cast(u64)(body.str[parser->pos] - '0');
                parser->pos += 1;
            }
        }
        // Skip integer suffixes (u, l, ul, ull...).
        while(parser->pos < body.size && (body.str[parser->pos] == 'u' || body.str[parser->pos] == 'U' || body.str[parser->pos] == 'l' || body.str[parser->pos] == 'L')){
            parser->pos += 1;
        }
        token.kind  = RFL_TokenKind_Int;
        token.value = cast(i64)number;
    }else if(char_is_alpha(body.str[parser->pos]) || body.str[parser->pos] == '_'){
        // Identifier: a type name, field name, keyword or constant.
        while(parser->pos < body.size && (char_is_alnum(body.str[parser->pos]) || body.str[parser->pos] == '_')){
            parser->pos += 1;
        }
        token.kind = RFL_TokenKind_Ident;
    }else{
        u8 symbol = body.str[parser->pos];
        parser->pos += 1;
        token.punct = symbol;
        if((symbol == '<' || symbol == '>') && parser->pos < body.size && body.str[parser->pos] == symbol){
            parser->pos += 1;
            token.punct = rfl_punct_pack(symbol, symbol); // matches '<<' / '>>'
        }
        token.kind = RFL_TokenKind_Punct;
    }
    token.text  = string8(body.str + token_start, parser->pos - token_start);
    parser->tok = token;
}

// evaluate = false only counts entries (constants read as 0);
// evaluate = true computes real values.
internal RFL_Parser
rfl_parser_begin(RFL_Type *type, b32 evaluate)
{
    RFL_Parser parser = {.type = type, .evaluate = evaluate};
    rfl_lex_next_token(&parser);
    return parser;
}

////////////////////////////////
// Token helpers.
//   is_punct:  peek, don't consume.
//   accept:    consume if it matches, report whether it did.
//   expect:    consume, or error if it doesn't match.

// Non-punct tokens have punct == 0 (token starts zeroed), so no kind check is needed.
internal b32
rfl_token_is_punct(RFL_Parser *parser, RFL_Punct punct)
{
    return parser->tok.punct == punct;
}

internal b32
rfl_accept_punct(RFL_Parser *parser, RFL_Punct punct)
{
    b32 matched = rfl_token_is_punct(parser, punct);
    if(matched){
        rfl_lex_next_token(parser);
    }
    return matched;
}

internal void
rfl_expect_punct(RFL_Parser *parser, RFL_Punct punct)
{
    if(!rfl_accept_punct(parser, punct)){
        char text[2] = {cast(char)punct, 0};
        int  length  = 1;
        if(punct > 0xFF){ // two chars, unpacked in the compiler's order
            text[0] = cast(char)(RFL_PUNCT_FIRST_CHAR_HIGH ? punct >> 8 : punct & 0xFF);
            text[1] = cast(char)(RFL_PUNCT_FIRST_CHAR_HIGH ? punct & 0xFF : punct >> 8);
            length  = 2;
        }
        DOT_ERROR_FL(parser->type->file, cast(int)parser->type->line, "Reflect %S: expected '%.*s', got '%S'", parser->type->name, length, text, parser->tok.text);
    }
}

internal String8
rfl_expect_identifier(RFL_Parser *parser)
{
    if(parser->tok.kind != RFL_TokenKind_Ident){
        DOT_ERROR_FL(parser->type->file, cast(int)parser->type->line, "Reflect %S: expected a name, got '%S'", parser->type->name, parser->tok.text);
    }
    String8 identifier = parser->tok.text;
    rfl_lex_next_token(parser);
    return identifier;
}

internal void
rfl_expect_body_end(RFL_Parser *parser)
{
    if(parser->tok.kind != RFL_TokenKind_End){
        DOT_ERROR_FL(parser->type->file, cast(int)parser->type->line, "Reflect %S: unexpected '%S' after the body", parser->type->name, parser->tok.text);
    }
}

// Skips rfl_* tag identifiers before a field or enum value; returns the last one.
internal String8
rfl_parse_tags(RFL_Parser *parser)
{
    String8 tag = {0};
    while(parser->tok.kind == RFL_TokenKind_Ident && string8_starts_with(parser->tok.text, str_comp("rfl_"))){
        tag = parser->tok.text;
        rfl_lex_next_token(parser);
    }
    return tag;
}

////////////////////////////////
// Constant expressions (enum values, array sizes).
// One function per precedence level, loosest first:
//   |   &   << >>   + -   * / %   unary - ~ ( )

// Value of a named constant (enum value or ReflectConst); resolves its enum if needed.
internal i64
rfl_lookup_constant_value(RFL_Parser *parser, String8 name)
{
    RFL_Const *constant = hm_get(&rfl_g_registry.consts, name);
    if(!constant){
        DOT_ERROR_FL(parser->type->file, cast(int)parser->type->line, "Reflect %S: unknown constant '%S'; reflect its enum with ReflectEnum or add ReflectConst(%S)", parser->type->name, name, name);
    }
    if(!constant->known){
        if(constant->owner->resolve_state == RFL_ResolveState_InProgress){
            DOT_ERROR_FL(parser->type->file, cast(int)parser->type->line, "Reflect %S: constant '%S' used before its value is defined", parser->type->name, name);
        }
        rfl_resolve_type(constant->owner);
    }
    return constant->value;
}

internal i64 rfl_parse_expr(RFL_Parser *parser);

internal i64
rfl_parse_expr_unary(RFL_Parser *parser)
{
    if(rfl_accept_punct(parser, '-')){ return -rfl_parse_expr_unary(parser); }
    if(rfl_accept_punct(parser, '~')){ return ~rfl_parse_expr_unary(parser); }
    if(rfl_accept_punct(parser, '(')){
        i64 inner = rfl_parse_expr(parser);
        rfl_expect_punct(parser, ')');
        return inner;
    }
    if(parser->tok.kind == RFL_TokenKind_Int){
        i64 number = parser->tok.value;
        rfl_lex_next_token(parser);
        return number;
    }
    String8 constant_name = rfl_expect_identifier(parser);
    return parser->evaluate ? rfl_lookup_constant_value(parser, constant_name) : 0;
}

// Right-hand side of / or %; errors on zero.
internal i64
rfl_parse_expr_divisor(RFL_Parser *parser)
{
    i64 divisor = rfl_parse_expr_unary(parser);
    if(parser->evaluate && divisor == 0){
        DOT_ERROR_FL(parser->type->file, cast(int)parser->type->line, "Reflect %S: division by zero", parser->type->name);
    }
    return divisor ? divisor : 1;
}

internal i64
rfl_parse_expr_mul(RFL_Parser *parser)
{
    i64 result = rfl_parse_expr_unary(parser);
    for(;;){
        if(rfl_accept_punct(parser, '*')){      result *= rfl_parse_expr_unary(parser); }
        else if(rfl_accept_punct(parser, '/')){ result /= rfl_parse_expr_divisor(parser); }
        else if(rfl_accept_punct(parser, '%')){ result %= rfl_parse_expr_divisor(parser); }
        else{ return result; }
    }
}

internal i64
rfl_parse_expr_add(RFL_Parser *parser)
{
    i64 result = rfl_parse_expr_mul(parser);
    for(;;){
        if(rfl_accept_punct(parser, '+')){      result += rfl_parse_expr_mul(parser); }
        else if(rfl_accept_punct(parser, '-')){ result -= rfl_parse_expr_mul(parser); }
        else{ return result; }
    }
}

internal i64
rfl_parse_expr_shift(RFL_Parser *parser)
{
    i64 result = rfl_parse_expr_add(parser);
    for(;;){
        if(rfl_accept_punct(parser, '<<')){      result = cast(i64)(cast(u64)result << rfl_parse_expr_add(parser)); }
        else if(rfl_accept_punct(parser, '>>')){ result = result >> rfl_parse_expr_add(parser); }
        else{ return result; }
    }
}

internal i64
rfl_parse_expr_and(RFL_Parser *parser)
{
    i64 result = rfl_parse_expr_shift(parser);
    while(rfl_accept_punct(parser, '&')){
        result &= rfl_parse_expr_shift(parser);
    }
    return result;
}

// Entry point for a whole expression (lowest precedence: |).
internal i64
rfl_parse_expr(RFL_Parser *parser)
{
    i64 result = rfl_parse_expr_and(parser);
    while(rfl_accept_punct(parser, '|')){
        result |= rfl_parse_expr_and(parser);
    }
    return result;
}

////////////////////////////////
// Body parsers. Called twice per type:
//   out == 0: count entries (and, for enums, register constant names).
//   out != 0: fill out[] with the real entries.

internal u32
rfl_parse_enum(RFL_Parser *parser, RFL_EnumValue *out_values)
{
    u32 value_count = 0;
    i64 next_value  = 0;
    rfl_expect_punct(parser, '{');
    while(!rfl_token_is_punct(parser, '}')){
        rfl_parse_tags(parser);
        String8 value_name = rfl_expect_identifier(parser);
        i64 value = next_value;
        if(rfl_accept_punct(parser, '=')){
            value = rfl_parse_expr(parser);
        }
        if(out_values){
            RFL_Const *constant = hm_get(&rfl_g_registry.consts, value_name);
            constant->value = value;
            constant->known = true;
            out_values[value_count] = (RFL_EnumValue){.name = value_name, .value = value};
        }else{
            RFL_Const *constant = hm_get_or_create(rfl_g_registry.arena, &rfl_g_registry.consts, value_name);
            if(constant->owner || constant->known){
                DOT_ERROR_FL(parser->type->file, cast(int)parser->type->line, "Reflect %S: constant '%S' is already defined", parser->type->name, value_name);
            }
            constant->owner = parser->type;
        }
        next_value   = value + 1;
        value_count += 1;
        if(!rfl_accept_punct(parser, ',')){
            break;
        }
    }
    rfl_expect_punct(parser, '}');
    rfl_expect_body_end(parser);
    return value_count;
}

internal u32
rfl_parse_struct(RFL_Parser *parser, RFL_Field *out_fields)
{
    u32 field_count = 0;
    rfl_expect_punct(parser, '{');
    while(!rfl_token_is_punct(parser, '}')){
        // One declaration: [tags] [const] [struct|union] Type a, *b, c[N];
        String8 tag       = rfl_parse_tags(parser);
        String8 type_name = rfl_expect_identifier(parser);
        if(string8_equal(type_name, str_comp("const"))){
            type_name = rfl_expect_identifier(parser);
        }
        if(string8_equal(type_name, str_comp("struct")) || string8_equal(type_name, str_comp("union"))){
            if(rfl_token_is_punct(parser, '{')){
                DOT_ERROR_FL(parser->type->file, cast(int)parser->type->line, "Reflect %S: nested struct/union bodies are not supported", parser->type->name);
            }
            type_name = rfl_expect_identifier(parser);
        }
        // Each comma-separated name in the declaration is its own field.
        for(;;){
            u32 pointer_depth = 0;
            while(rfl_accept_punct(parser, '*')){
                pointer_depth += 1;
            }
            String8 field_name = rfl_expect_identifier(parser);
            u32 array_count = 1;
            if(rfl_accept_punct(parser, '[')){
                i64 array_size = rfl_parse_expr(parser);
                rfl_expect_punct(parser, ']');
                if(parser->evaluate && array_size <= 0){
                    DOT_ERROR_FL(parser->type->file, cast(int)parser->type->line, "Reflect %S: array '%S' has size %lld", parser->type->name, field_name, array_size);
                }
                array_count = cast(u32)array_size;
            }
            if(out_fields){
                out_fields[field_count] = (RFL_Field){
                    .name          = field_name,
                    .type_name     = type_name,
                    .tag           = tag,
                    .array_count   = array_count,
                    .pointer_depth = pointer_depth,
                };
            }
            field_count += 1;
            if(!rfl_accept_punct(parser, ',')){
                break;
            }
        }
        rfl_expect_punct(parser, ';');
    }
    rfl_expect_punct(parser, '}');
    rfl_expect_body_end(parser);
    return field_count;
}

DOT_DIAGNOSTIC_POP

////////////////////////////////
// Resolving: turning a registered type into a usable one.
//   enum:   compute every value.
//   struct: build the field list, compute offsets, check against the compiler.

internal RFL_Type *
rfl_lookup_type(String8 name)
{
    RFL_Type **type_slot = hm_get(&rfl_g_registry.types, name);
    return type_slot ? *type_slot : 0;
}

internal void
rfl_resolve_enum(RFL_Type *type)
{
    type->values = PUSH_ARRAY(rfl_g_registry.arena, RFL_EnumValue, type->value_count);
    RFL_Parser parser = rfl_parser_begin(type, true);
    rfl_parse_enum(&parser, type->values);
}

internal void
rfl_resolve_struct(RFL_Type *type)
{
    RFL_Parser count_parser = rfl_parser_begin(type, false);
    type->field_count = rfl_parse_struct(&count_parser, 0);
    type->fields = PUSH_ARRAY(rfl_g_registry.arena, RFL_Field, type->field_count);
    RFL_Parser fill_parser = rfl_parser_begin(type, true);
    rfl_parse_struct(&fill_parser, type->fields);

    // Lay the fields out the way a C compiler does.
    u32 offset       = 0;
    u32 struct_align = 1;
    for(u32 field_index = 0; field_index < type->field_count; ++field_index){
        RFL_Field *field = &type->fields[field_index];
        field->type = rfl_lookup_type(field->type_name);
        u32 field_size  = cast(u32)sizeof(void*);
        u32 field_align = cast(u32)DOT_ALIGNOF(void*);
        if(field->pointer_depth == 0){
            if(!field->type){
                DOT_ERROR_FL(type->file, cast(int)type->line, "Reflect %S: field '%S' has unknown type '%S'; reflect it with Reflect, ReflectEnum, ReflectPrimitive or ReflectExtern", type->name, field->name, field->type_name);
            }
            rfl_resolve_type(field->type);
            field_size  = field->type->size;
            field_align = field->type->align;
        }
        offset        = ALIGN_POW2(offset, field_align);
        field->offset = offset;
        offset       += field_size * field->array_count;
        struct_align  = DOT_MAX(struct_align, field_align);
    }
    u32 struct_size = ALIGN_POW2(offset, struct_align);
    if(struct_size != type->size || struct_align != type->align){
        DOT_ERROR_FL(type->file, cast(int)type->line, "Reflect %S: parsed layout is size %u align %u, the compiler says size %u align %u", type->name, struct_size, struct_align, type->size, type->align);
    }
}

// Resolves a type once; recursion into field types is how dependencies get ordered.
internal void
rfl_resolve_type(RFL_Type *type)
{
    if(type->resolve_state == RFL_ResolveState_Done){
        return;
    }
    if(type->resolve_state == RFL_ResolveState_InProgress){
        DOT_ERROR_FL(type->file, cast(int)type->line, "Reflect %S: type contains itself", type->name);
    }
    type->resolve_state = RFL_ResolveState_InProgress;
    switch(type->kind){
    case RFL_TypeKind_Struct: rfl_resolve_struct(type); break;
    case RFL_TypeKind_Enum:   rfl_resolve_enum(type);   break;
    default: break;
    }
    type->resolve_state = RFL_ResolveState_Done;
}

////////////////////////////////
// Init: three passes over every Reflect* entry in the linker section.
//   1. Register type names and ReflectConst values.
//   2. Count each enum's values and register their names.
//   3. Resolve every type.

internal void
rfl_init(Arena *arena)
{
    rfl_g_registry.arena = arena;
    hm_init_string8(arena, &rfl_g_registry.types, RFL_TYPE_BUCKETS_MAX);
    hm_init_string8(arena, &rfl_g_registry.consts, RFL_CONST_BUCKETS_MAX);

    for EACH_IN_SECTION(ReflectSection, RFL_Type, type){
        if(type->kind == RFL_TypeKind_Const){
            RFL_Const *constant = hm_get_or_create(arena, &rfl_g_registry.consts, type->name);
            if(constant->owner || constant->known){
                DOT_ERROR_FL(type->file, cast(int)type->line, "ReflectConst %S: constant is already defined", type->name);
            }
            constant->value = type->const_value;
            constant->known = true;
            continue;
        }
        RFL_Type **type_slot = hm_get_or_create(arena, &rfl_g_registry.types, type->name);
        if(*type_slot){
            DOT_ERROR_FL(type->file, cast(int)type->line, "Reflect %S: already reflected at %s:%u", type->name, (*type_slot)->file, (*type_slot)->line);
        }
        *type_slot = type;
    }

    for EACH_IN_SECTION(ReflectSection, RFL_Type, type){
        if(type->kind == RFL_TypeKind_Enum){
            RFL_Parser count_parser = rfl_parser_begin(type, false);
            type->value_count = rfl_parse_enum(&count_parser, 0);
        }
    }

    for EACH_IN_SECTION(ReflectSection, RFL_Type, type){
        if(type->kind != RFL_TypeKind_Const){
            rfl_resolve_type(type);
        }
    }
}

// A view of one field (one array element) inside a reflected value.
internal RFL_Any
rfl_any_field(RFL_Any value, RFL_Field *field, u32 element_index)
{
    DOT_ASSERT(field->pointer_depth == 0, "Field %S is a pointer", field->name);
    DOT_ASSERT(element_index < field->array_count);
    RFL_Any result = {
        .type = field->type,
        .data = cast(u8*)value.data + field->offset + element_index * field->type->size,
    };
    return result;
}

/////////////////////////////////////////////// Test suite

DOT_TEST_SUITE(rfl_tests)
{
    DOT_TestResults test = {0};

    #define RFL_TEST_SCALE 2
    #define rfl_test_tag
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
        rfl_test_tag f32x3 color;
        u32 slots[RFL_TestKind_Count * RFL_TEST_SCALE];
        struct RFL_TestStruct *next, **list;
        u64 big;
        RFL_TestKind kind;
        f32x4x4 m;
        });

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
    DOT_TEST_CHECK(test, "tag",              string8_equal(s->fields[3].tag, str_comp("rfl_test_tag")));
    DOT_TEST_CHECK(test, "no tag",           s->fields[2].tag.size == 0);
    DOT_TEST_CHECK(test, "array count",      s->fields[4].array_count == RFL_TestKind_Count * RFL_TEST_SCALE);
    DOT_TEST_CHECK(test, "pointer depth",    s->fields[5].pointer_depth == 1 && s->fields[6].pointer_depth == 2);
    DOT_TEST_CHECK(test, "self pointer",     s->fields[5].type == s);
    DOT_TEST_CHECK(test, "enum field",       s->fields[8].type == kind);
    DOT_TEST_CHECK(test, "extern fields",    s->fields[3].type->field_count == 3 && string8_equal(s->fields[3].type->fields[2].name, str_comp("z")));
    DOT_TEST_CHECK(test, "extern offsets",   s->fields[3].type->fields[2].offset == offsetof(f32x3, z));
    DOT_TEST_CHECK(test, "extern nested",    s->fields[9].type->fields[0].type == reflect_type(f32x4) && s->fields[9].type->fields[0].array_count == 4);
    DOT_TEST_CHECK(test, "lookup",           rfl_lookup_type(str_comp("RFL_TestStruct")) == s);
    DOT_TEST_CHECK(test, "lookup missing",   rfl_lookup_type(str_comp("RFL_Missing")) == 0);

    RFL_TestStruct value = {.slots[3] = 42};
    RFL_Any any  = rfl_any(RFL_TestStruct, &value);
    RFL_Any slot = rfl_any_field(any, &s->fields[4], 3);
    DOT_TEST_CHECK(test, "any field",        slot.type == reflect_type(u32) && *cast(u32*)slot.data == 42);

    return test;
}
