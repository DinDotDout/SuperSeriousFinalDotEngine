// Recursive descent grammar for the stringized Reflect/ReflectEnum bodies.
//
//   enum_body   = '{' [ enum_value { ',' { tag } enum_value } [ ',' { tag } ] ] '}'   rfl_parse_enum
//   enum_value  = IDENT [ '=' expr ]                                         (inlined in rfl_parse_enum)
//
//   struct_body = '{' { decl } '}'                                           rfl_parse_struct
//   decl        = [ "const" ] [ "struct" | "union" ]
//                 IDENT declarator { ',' { tag } declarator } ';' { tag }    (inlined in rfl_parse_struct)
//   declarator  = { '*' } IDENT [ '[' expr ']' ]                             (inlined in rfl_parse_struct)
//
//   expr        = and   { '|' and }                                          rfl_parse_expr
//   and         = shift { '&' shift }                                        rfl_parse_expr_and
//   shift       = add   { ( '<<' | '>>' ) add }                              rfl_parse_expr_shift
//   add         = mul   { ( '+' | '-' ) mul }                                rfl_parse_expr_add
//   mul         = unary { ( '*' | '/' | '%' ) unary }                        rfl_parse_expr_mul
//                 (the unary after '/' or '%' goes through                   rfl_parse_expr_divisor)
//   unary       = '-' unary | '~' unary | '(' expr ')' | i32 | IDENT         rfl_parse_expr_unary
//
//   tag         = IDENT starting with "rfl_"                                 rfl_parse_tags
//   i32         = decimal or 0x hex, with any u/l suffixes ignored           rfl_lex_int
//   IDENT       = name token                                                 rfl_lex_next_token, rfl_expect_identifier
//                 in expr: an enum value or ReflectConst name                rfl_lookup_constant_value
//
// Token helpers used throughout: rfl_token_is_punct (peek), rfl_accept_punct ('[ x ]', '{ x }'),
// rfl_expect_punct (required token), rfl_expect_body_end (nothing after the closing '}').
//
// Nested struct/union bodies are rejected. Tags trail what they describe: tags after a ','
// belong to the declarator before it, tags after the ';' apply to every declarator in that
// decl that has none of its own. Only the last tag in a run is kept.

global RFL_Registry rfl_g_registry;

ReflectPrimitive(i8,    RFL_TypeKind_I32);
ReflectPrimitive(i16,   RFL_TypeKind_I32);
ReflectPrimitive(i32,   RFL_TypeKind_I32);
ReflectPrimitive(i64,   RFL_TypeKind_I32);
ReflectPrimitive(int,   RFL_TypeKind_I32);
ReflectPrimitive(iptr,  RFL_TypeKind_I32);

ReflectPrimitive(u8,    RFL_TypeKind_U32);
ReflectPrimitive(u16,   RFL_TypeKind_U32);
ReflectPrimitive(u32,   RFL_TypeKind_U32);
ReflectPrimitive(u64,   RFL_TypeKind_U32);
ReflectPrimitive(usize, RFL_TypeKind_U32);
ReflectPrimitive(uptr,  RFL_TypeKind_U32);

ReflectPrimitive(b8,    RFL_TypeKind_Bool);
ReflectPrimitive(b16,   RFL_TypeKind_Bool);
ReflectPrimitive(b32,   RFL_TypeKind_Bool);

ReflectPrimitive(f32,   RFL_TypeKind_F32);
ReflectPrimitive(f64,   RFL_TypeKind_F32);

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
    RFL_TokenKind_I32,
    RFL_TokenKind_Punct,
};

// A punctuator is its character constant: '*' or '<<'. Two-char constants are packed
// first char high on MSVC, Clang and GCC; the lexer builds '<<' / '>>' the same way.
typedef u16 RFL_Punct;

typedef struct RFL_Token
{
    RFL_TokenKind kind;
    String8       text;
    union{
        i64           i32_value;
        RFL_Punct     punct;
    };
}RFL_Token;

typedef struct RFL_EnumValueArray
{
    RFL_EnumValue *v;
    u32            count;
}RFL_EnumValueArray;

typedef struct RFL_FieldArray
{
    RFL_Field *v;
    u32        count;
}RFL_FieldArray;

typedef u32 RFL_ParserMode;
enum
{
    RFL_ParserMode_Scan,
    RFL_ParserMode_Evaluate,
};

typedef struct RFL_Parser
{
    RFL_Type       *type;
    u64             pos;
    RFL_Token       tok;
    RFL_ParserMode  mode;
}RFL_Parser;

internal void rfl_resolve_type(RFL_Type *type);

// Clang and GCC warn on multi-char constants by default: silenced here up to the body parsers.
DOT_DIAGNOSTIC_PUSH
#if DOT_COMPILER_CLANG || DOT_COMPILER_GCC
    DOT_DIAGNOSTIC_IGNORE("-Wmultichar")
#endif

#if 'ab' != (('a' << 8) | 'b')
    #error "Expected multi-char constants packed first char high (MSVC, Clang, GCC)"
#endif

////////////////////////////////
// Character classes: one flag byte per character, filled once by rfl_char_flags_init.
// Index 0 has no flags, so a loop over a class stops at the end of the body (rfl_peek returns 0).

typedef u8 RFL_CharFlags;
enum
{
    RFL_Char_Space      = 1 << 0,
    RFL_Char_Digit      = 1 << 1,
    RFL_Char_HexDigit   = 1 << 2,
    RFL_Char_IdentStart = 1 << 3, // a-z A-Z _
    RFL_Char_Ident      = 1 << 4, // a-z A-Z 0-9 _
    RFL_Char_I32Suffix  = 1 << 5, // u U l L
};

global RFL_CharFlags rfl_g_char_flags[256];

internal void
rfl_char_flags_init(void)
{
    RFL_CharFlags *flags = rfl_g_char_flags;
    for(u32 c = 'a'; c <= 'z'; ++c){ flags[c] |= RFL_Char_IdentStart | RFL_Char_Ident; }
    for(u32 c = 'A'; c <= 'Z'; ++c){ flags[c] |= RFL_Char_IdentStart | RFL_Char_Ident; }
    for(u32 c = '0'; c <= '9'; ++c){ flags[c] |= RFL_Char_Digit | RFL_Char_HexDigit | RFL_Char_Ident; }
    for(u32 c = 'a'; c <= 'f'; ++c){ flags[c] |= RFL_Char_HexDigit; }
    for(u32 c = 'A'; c <= 'F'; ++c){ flags[c] |= RFL_Char_HexDigit; }
    flags['_'] |= RFL_Char_IdentStart | RFL_Char_Ident;
    flags[' '] = flags['\t'] = flags['\n'] = flags['\r'] = flags['\v'] = flags['\f'] = RFL_Char_Space;
    flags['u'] |= RFL_Char_I32Suffix; flags['U'] |= RFL_Char_I32Suffix; flags['l'] |= RFL_Char_I32Suffix; flags['L'] |= RFL_Char_I32Suffix;
}

////////////////////////////////
// Lexer: turns the stringized body into tokens, one at a time.

// The char `offset` past the cursor, or 0 past the end of the body.
internal u8
rfl_peek_at(RFL_Parser *parser, u64 offset)
{
    String8 body = parser->type->body;
    u64 at = parser->pos + offset;
    return(at < body.size ? body.str[at] : 0);
}

internal u8
rfl_peek(RFL_Parser *parser)
{
    return(rfl_peek_at(parser, 0));
}

internal b32
rfl_peek_is(RFL_Parser *parser, RFL_CharFlags mask)
{
    return((rfl_g_char_flags[rfl_peek(parser)] & mask) != 0);
}

internal void
rfl_skip_while(RFL_Parser *parser, RFL_CharFlags mask)
{
    while(rfl_peek_is(parser, mask)){
        parser->pos += 1;
    }
}

internal u64
rfl_lex_digits(RFL_Parser *parser, RFL_CharFlags digit_mask, u64 base)
{
    u64 number = 0;
    while(rfl_peek_is(parser, digit_mask)){
        number = number * base + char_hex_value(rfl_peek(parser));
        parser->pos += 1;
    }
    return(number);
}

internal RFL_Token
rfl_lex_int(RFL_Parser *parser)
{
    RFL_Token token = {.kind = RFL_TokenKind_I32};
    if(rfl_peek(parser) == '0' && char_to_lower(rfl_peek_at(parser, 1)) == 'x'){
        parser->pos += 2;
        token.i32_value = cast(i64)rfl_lex_digits(parser, RFL_Char_HexDigit, 16);
    }else{
        token.i32_value = cast(i64)rfl_lex_digits(parser, RFL_Char_Digit, 10);
    }
    rfl_skip_while(parser, RFL_Char_I32Suffix);
    return(token);
}

internal RFL_Token
rfl_lex_punct(RFL_Parser *parser)
{
    RFL_Token token = {.kind = RFL_TokenKind_Punct};
    u8 symbol = rfl_peek(parser);
    parser->pos += 1;
    token.punct = symbol;
    if((symbol == '<' || symbol == '>') && rfl_peek(parser) == symbol){
        parser->pos += 1;
        token.punct = cast(RFL_Punct)((symbol << 8) | symbol); // same as '<<' / '>>'
    }
    return(token);
}

// Reads the next token from the body into parser->tok.
internal void
rfl_lex_next_token(RFL_Parser *parser)
{
    rfl_skip_while(parser, RFL_Char_Space);
    u64 token_start = parser->pos;

    RFL_Token token = {0};
    if(!rfl_peek(parser))                            { token.kind = RFL_TokenKind_End; }
    else if(rfl_peek_is(parser, RFL_Char_Digit))     { token = rfl_lex_int(parser); }
    else if(rfl_peek_is(parser, RFL_Char_IdentStart)){ token.kind = RFL_TokenKind_Ident; rfl_skip_while(parser, RFL_Char_Ident); }
    else                                             { token = rfl_lex_punct(parser); }

    token.text  = string8(parser->type->body.str + token_start, parser->pos - token_start);
    parser->tok = token;
}

// Evaluate computes real values. Scan only walks the body (constants read as 0);
// rfl_register_enum_constants uses it before any value can be computed.
internal RFL_Parser
rfl_parser_begin(RFL_Type *type, RFL_ParserMode mode)
{
    RFL_Parser parser = {.type = type, .mode = mode};
    rfl_lex_next_token(&parser);
    return(parser);
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
    return(parser->tok.kind == RFL_TokenKind_Punct && parser->tok.punct == punct);
}

internal b32
rfl_accept_punct(RFL_Parser *parser, RFL_Punct punct)
{
    b32 matched = rfl_token_is_punct(parser, punct);
    if(matched){
        rfl_lex_next_token(parser);
    }
    return(matched);
}

internal void
rfl_expect_punct(RFL_Parser *parser, RFL_Punct punct)
{
    if(!rfl_accept_punct(parser, punct)){
        char text[2] = {cast(char)(punct >> 8), cast(char)punct}; // reading order
        i32  skip    = punct <= 0xFF;                             // single char: empty high byte
        DOT_ERROR_FL(parser->type->file, cast(i32)parser->type->line, "Reflect %S: expected '%.*s', got '%S'", parser->type->name, 2 - skip, text + skip, parser->tok.text);
    }
}

internal String8
rfl_expect_identifier(RFL_Parser *parser)
{
    if(parser->tok.kind != RFL_TokenKind_Ident){
        DOT_ERROR_FL(parser->type->file, cast(i32)parser->type->line, "Reflect %S: expected a name, got '%S'", parser->type->name, parser->tok.text);
    }
    String8 identifier = parser->tok.text;
    rfl_lex_next_token(parser);
    return(identifier);
}

internal void
rfl_expect_body_end(RFL_Parser *parser)
{
    if(parser->tok.kind != RFL_TokenKind_End){
        DOT_ERROR_FL(parser->type->file, cast(i32)parser->type->line, "Reflect %S: unexpected '%S' after the body", parser->type->name, parser->tok.text);
    }
}

// Skips rfl_* tag identifiers after a field's ',' / ';' or an enum value's ','; returns the last one.
internal String8
rfl_parse_tags(RFL_Parser *parser)
{
    String8 tag = {0};
    while(parser->tok.kind == RFL_TokenKind_Ident && string8_starts_with(parser->tok.text, str_comp("rfl_"))){
        tag = parser->tok.text;
        rfl_lex_next_token(parser);
    }
    return(tag);
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
        DOT_ERROR_FL(parser->type->file, cast(i32)parser->type->line, "Reflect %S: unknown constant '%S'; reflect its enum with ReflectEnum or add ReflectConst(%S)", parser->type->name, name, name);
    }
    if(!constant->known){
        if(constant->owner->resolve_state == RFL_ResolveState_InProgress){
            DOT_ERROR_FL(parser->type->file, cast(i32)parser->type->line, "Reflect %S: constant '%S' used before its value is defined", parser->type->name, name);
        }
        rfl_resolve_type(constant->owner);
    }
    return(constant->value);
}

internal i64 rfl_parse_expr(RFL_Parser *parser);

internal i64
rfl_parse_expr_unary(RFL_Parser *parser)
{
    if(rfl_accept_punct(parser, '-')){ return(-rfl_parse_expr_unary(parser)); }
    if(rfl_accept_punct(parser, '~')){ return(~rfl_parse_expr_unary(parser)); }
    if(rfl_accept_punct(parser, '(')){
        i64 inner = rfl_parse_expr(parser);
        rfl_expect_punct(parser, ')');
        return(inner);
    }
    if(parser->tok.kind == RFL_TokenKind_I32){
        i64 number = parser->tok.i32_value;
        rfl_lex_next_token(parser);
        return(number);
    }
    String8 constant_name = rfl_expect_identifier(parser);
    return(parser->mode == RFL_ParserMode_Evaluate ? rfl_lookup_constant_value(parser, constant_name) : 0);
}

// Right-hand side of / or %; errors on zero.
internal i64
rfl_parse_expr_divisor(RFL_Parser *parser)
{
    i64 divisor = rfl_parse_expr_unary(parser);
    if(parser->mode == RFL_ParserMode_Evaluate && divisor == 0){
        DOT_ERROR_FL(parser->type->file, cast(i32)parser->type->line, "Reflect %S: division by zero", parser->type->name);
    }
    return(divisor ? divisor : 1);
}

internal i64
rfl_parse_expr_mul(RFL_Parser *parser)
{
    i64 result = rfl_parse_expr_unary(parser);
    for(;;){
        if(rfl_accept_punct(parser, '*')){      result *= rfl_parse_expr_unary(parser); }
        else if(rfl_accept_punct(parser, '/')){ result /= rfl_parse_expr_divisor(parser); }
        else if(rfl_accept_punct(parser, '%')){ result %= rfl_parse_expr_divisor(parser); }
        else{ return(result); }
    }
}

internal i64
rfl_parse_expr_add(RFL_Parser *parser)
{
    i64 result = rfl_parse_expr_mul(parser);
    for(;;){
        if(rfl_accept_punct(parser, '+')){      result += rfl_parse_expr_mul(parser); }
        else if(rfl_accept_punct(parser, '-')){ result -= rfl_parse_expr_mul(parser); }
        else{ return(result); }
    }
}

internal i64
rfl_parse_expr_shift(RFL_Parser *parser)
{
    i64 result = rfl_parse_expr_add(parser);
    for(;;){
        if(rfl_accept_punct(parser, '<<')){      result = cast(i64)(cast(u64)result << rfl_parse_expr_add(parser)); }
        else if(rfl_accept_punct(parser, '>>')){ result = result >> rfl_parse_expr_add(parser); }
        else{ return(result); }
    }
}

internal i64
rfl_parse_expr_and(RFL_Parser *parser)
{
    i64 result = rfl_parse_expr_shift(parser);
    while(rfl_accept_punct(parser, '&')){
        result &= rfl_parse_expr_shift(parser);
    }
    return(result);
}

// Entry point for a whole expression (lowest precedence: |).
internal i64
rfl_parse_expr(RFL_Parser *parser)
{
    i64 result = rfl_parse_expr_and(parser);
    while(rfl_accept_punct(parser, '|')){
        result |= rfl_parse_expr_and(parser);
    }
    return(result);
}

////////////////////////////////
// Body parsers. One pass each: entries are pushed into `scratch` as they're parsed, and the
// caller copies the result to the permanent arena. A nested resolve (an expression naming
// another enum's constant) begins and ends its own scratch on the same scratch arena before
// this parse pushes again, so the entries stay contiguous.

// When evaluating, each value is published to its constant right away, so later values
// in the same enum can refer to earlier ones.
internal RFL_EnumValueArray
rfl_parse_enum(Arena *arena, RFL_Parser *parser)
{
    RFL_EnumValueArray values = {0};
    i64 next_value = 0;
    rfl_expect_punct(parser, '{');
    while(!rfl_token_is_punct(parser, '}')){
        String8 value_name = rfl_expect_identifier(parser);
        i64 value = next_value;
        if(rfl_accept_punct(parser, '=')){
            value = rfl_parse_expr(parser);
        }
        if(parser->mode == RFL_ParserMode_Evaluate){
            RFL_Const *constant = hm_get(&rfl_g_registry.consts, value_name);
            constant->value = value;
            constant->known = true;
        }
        RFL_EnumValue *entry = PUSH_COPY(arena, &(RFL_EnumValue){.name = value_name, .value = value});
        if(values.count == 0){
            values.v = entry;
        }
        values.count += 1;
        next_value    = value + 1;
        if(!rfl_accept_punct(parser, ',')){
            break;
        }
        rfl_parse_tags(parser);
    }
    rfl_expect_punct(parser, '}');
    rfl_expect_body_end(parser);
    return(values);
}

internal RFL_FieldArray
rfl_parse_struct(Arena *arena, RFL_Parser *parser)
{
    RFL_FieldArray fields = {0};
    rfl_expect_punct(parser, '{');
    while(!rfl_token_is_punct(parser, '}')){
        // One declaration: [const] [struct|union] Type a, [tags] *b, [tags] c[N]; [tags]
        String8 type_name = rfl_expect_identifier(parser);
        if(string8_equal(type_name, str_comp("const"))){
            type_name = rfl_expect_identifier(parser);
        }
        if(string8_equal(type_name, str_comp("struct")) || string8_equal(type_name, str_comp("union"))){
            if(rfl_token_is_punct(parser, '{')){
                DOT_ERROR_FL(parser->type->file, cast(i32)parser->type->line, "Reflect %S: nested struct/union bodies are not supported", parser->type->name);
            }
            type_name = rfl_expect_identifier(parser);
        }
        // Each comma-separated name in the declaration is its own field.
        u64 decl_first_field = fields.count;
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
                if(parser->mode == RFL_ParserMode_Evaluate && array_size <= 0){
                    DOT_ERROR_FL(parser->type->file, cast(i32)parser->type->line, "Reflect %S: array '%S' has size %lld", parser->type->name, field_name, array_size);
                }
                array_count = cast(u32)array_size;
            }
            RFL_Field *field = PUSH_COPY(arena, &(RFL_Field){
                .name          = field_name,
                .type_name     = type_name,
                .array_count   = array_count,
                .pointer_depth = pointer_depth,
            });
            if(fields.count == 0){
                fields.v = field;
            }
            fields.count += 1;
            if(!rfl_accept_punct(parser, ',')){
                break;
            }
            field->tag = rfl_parse_tags(parser);
        }
        rfl_expect_punct(parser, ';');
        String8 decl_tag = rfl_parse_tags(parser);
        for(u64 i = decl_first_field; i < fields.count; i++){
            if(fields.v[i].tag.size == 0){
                fields.v[i].tag = decl_tag;
            }
        }
    }
    rfl_expect_punct(parser, '}');
    rfl_expect_body_end(parser);
    return(fields);
}
DOT_DIAGNOSTIC_POP

////////////////////////////////
// Resolving: turning a registered type into a usable one.
//   enum:   compute every value.
//   struct: build the field list, compute offsets, check against the compiler.

internal RFL_Type *
rfl_type_lookup(String8 name)
{
    RFL_Type **type_slot = hm_get(&rfl_g_registry.types, name);
    return(type_slot ? *type_slot : 0);
}

// Claims every constant name in an enum before any value is computed, so an enum can
// refer to another enum's constants regardless of resolve order.
internal void
rfl_register_enum_constants(RFL_Type *type)
{
    Scratch scratch = scratch_begin(0,0);
    RFL_Parser parser = rfl_parser_begin(type, RFL_ParserMode_Scan);

    RFL_EnumValueArray values = rfl_parse_enum(scratch.arena, &parser);

    for(u32 value_index = 0; value_index < values.count; ++value_index){
        String8 name = values.v[value_index].name;
        RFL_Const *constant = hm_get_or_create(rfl_g_registry.arena, &rfl_g_registry.consts, name);
        if(constant->owner || constant->known){
            DOT_ERROR_FL(type->file, cast(i32)type->line, "Reflect %S: constant '%S' is already defined", type->name, name);
        }
        constant->owner = type;
    }
    scratch_end(scratch);
}

internal void
rfl_resolve_enum(RFL_Type *type)
{
    Scratch scratch = scratch_begin(0,0);

    RFL_Parser parser           = rfl_parser_begin(type, RFL_ParserMode_Evaluate);
    RFL_EnumValueArray values   = rfl_parse_enum(scratch.arena, &parser);

    type->value_count = values.count;
    type->values      = PUSH_ARRAY(rfl_g_registry.arena, RFL_EnumValue, values.count);
    MEMORY_COPY(type->values, values.v, values.count * sizeof(RFL_EnumValue));

    scratch_end(scratch);
}

internal void
rfl_resolve_struct(RFL_Type *type)
{
    Scratch scratch = scratch_begin(0,0);

    RFL_Parser parser       = rfl_parser_begin(type, RFL_ParserMode_Evaluate);
    RFL_FieldArray fields   = rfl_parse_struct(scratch.arena, &parser);

    type->field_count = fields.count;
    type->fields      = PUSH_ARRAY(rfl_g_registry.arena, RFL_Field, fields.count);
    MEMORY_COPY(type->fields, fields.v, fields.count * sizeof(RFL_Field));

    scratch_end(scratch);

    // Lay the fields out the way a C compiler does.
    u32 offset       = 0;
    u32 struct_align = 1;
    for(u32 field_index = 0; field_index < type->field_count; ++field_index){
        RFL_Field *field = &type->fields[field_index];
        field->type = rfl_type_lookup(field->type_name);
        u32 field_size  = cast(u32)sizeof(void*);
        u32 field_align = cast(u32)DOT_ALIGNOF(void*);
        if(field->pointer_depth == 0){
            if(!field->type){
                DOT_ERROR_FL(type->file, cast(i32)type->line, "Reflect %S: field '%S' has unknown type '%S'; reflect it with Reflect, ReflectEnum, ReflectPrimitive or ReflectExtern", type->name, field->name, field->type_name);
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
        DOT_ERROR_FL(type->file, cast(i32)type->line, "Reflect %S: parsed layout is size %u align %u, the compiler says size %u align %u", type->name, struct_size, struct_align, type->size, type->align);
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
        DOT_ERROR_FL(type->file, cast(i32)type->line, "Reflect %S: type contains itself", type->name);
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
// Init: build the char table, then three passes over every Reflect* entry in the linker section.
//   1. Register type names and ReflectConst values.
//   2. Register every enum's constant names.
//   3. Resolve every type.

internal b32
rfl_type_same_definition(RFL_Type *a, RFL_Type *b)
{
    return a->kind        == b->kind  &&
           a->size        == b->size  &&
           a->align       == b->align &&
           a->const_value == b->const_value &&
           string8_equal(a->body, b->body);
}

internal void
rfl_init(Arena *arena)
{
    rfl_char_flags_init();

    rfl_g_registry.arena = arena;
    hm_init_string8(arena, &rfl_g_registry.types, RFL_TYPE_BUCKETS_MAX);
    hm_init_string8(arena, &rfl_g_registry.consts, RFL_CONST_BUCKETS_MAX);

    for EACH_IN_SECTION(ReflectSection, RFL_Type, type){
        if(type->kind == RFL_TypeKind_Const){
            RFL_Const *constant = hm_get_or_create(arena, &rfl_g_registry.consts, type->name);
            if(constant->owner && rfl_type_same_definition(constant->owner, type)){
                type->canonical = constant->owner;
                continue;
            }
            if(constant->owner || constant->known){
                DOT_ERROR_FL(type->file, cast(i32)type->line, "ReflectConst %S: constant is already defined", type->name);
            }
            constant->owner = type;
            constant->value = type->const_value;
            constant->known = true;
            type->canonical = type;
            continue;
        }
        RFL_Type **type_slot = hm_get_or_create(arena, &rfl_g_registry.types, type->name);
        if(*type_slot && rfl_type_same_definition(*type_slot, type)){
            type->canonical = *type_slot;
            continue;
        }
        if(*type_slot){
            DOT_ERROR_FL(type->file, cast(i32)type->line, "Reflect %S: already reflected at %s:%u", type->name, (*type_slot)->file, (*type_slot)->line);
        }
        *type_slot      = type;
        type->canonical = type;
    }

    for EACH_IN_SECTION(ReflectSection, RFL_Type, type){
        if(type->kind == RFL_TypeKind_Enum && type->canonical == type){
            rfl_register_enum_constants(type);
        }
    }

    for EACH_IN_SECTION(ReflectSection, RFL_Type, type){
        if(type->kind != RFL_TypeKind_Const && type->canonical == type){
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
    return(result);
}

/////////////////////////////////////////////// Test suite

DOT_TEST_SUITE(rfl_tests)
{
    DOT_TestResults test = {0};

    #define RFL_TEST_SCALE 2
    #define rfl_test_tag
    #define rfl_test_other_tag
    ReflectConst(RFL_TEST_SCALE);

    ReflectEnum(u32, RFL_TestKind, {
        RFL_TestKind_A,
        RFL_TestKind_B = 5,
        RFL_TestKind_C, rfl_test_tag
        RFL_TestKind_Count,
        });

    ReflectEnum(i32, RFL_TestFlags, {
        RFL_TestFlagBit_X    = 1 << 0,
        RFL_TestFlagBit_Y    = 1 << 3,
        RFL_TestFlagBit_XY   = RFL_TestFlagBit_X | RFL_TestFlagBit_Y,
        RFL_TestFlagBit_Neg  = -(2 + 1),
        RFL_TestFlagBit_Far  = RFL_TestKind_Count << 1,
        RFL_TestFlagBit_Mask = 0xF0 & ~0x30,
        RFL_TestFlagBit_Hex  = 0X0A,
        });

    Reflect(RFL_TestStruct, {
        u8 a;
        f32 x, y; rfl_test_tag
        f32x3 color; rfl_test_tag rfl_test_other_tag
        u32 slots[RFL_TestKind_Count * RFL_TEST_SCALE];
        struct RFL_TestStruct *next, **list;
        u64 big;
        RFL_TestKind kind;
        f32x4x4 m;
        });

    Reflect(RFL_TestTagStruct, {
        f32 p, rfl_test_other_tag
            q, r; rfl_test_tag
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
    DOT_TEST_CHECK(test, "flags upper hex",  flags->values[6].value == RFL_TestFlagBit_Hex);

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
    DOT_TEST_CHECK(test, "no tag",           s->fields[0].tag.size == 0);
    DOT_TEST_CHECK(test, "tag",              string8_equal(s->fields[1].tag, str_comp("rfl_test_tag")));
    DOT_TEST_CHECK(test, "tag declarators",  string8_equal(s->fields[2].tag, str_comp("rfl_test_tag")));
    DOT_TEST_CHECK(test, "tag last wins",    string8_equal(s->fields[3].tag, str_comp("rfl_test_other_tag")));
    DOT_TEST_CHECK(test, "tag not leaked",   s->fields[4].tag.size == 0);
    DOT_TEST_CHECK(test, "enum tag skipped", string8_equal(kind->values[2].name, str_comp("RFL_TestKind_C")));
    DOT_TEST_CHECK(test, "enum after tag",   string8_equal(kind->values[3].name, str_comp("RFL_TestKind_Count")));

    RFL_Type *tagged = reflect_type(RFL_TestTagStruct);
    DOT_TEST_CHECK(test, "comma tag",        string8_equal(tagged->fields[0].tag, str_comp("rfl_test_other_tag")));
    DOT_TEST_CHECK(test, "decl tag fills",   string8_equal(tagged->fields[1].tag, str_comp("rfl_test_tag")));
    DOT_TEST_CHECK(test, "decl tag last",    string8_equal(tagged->fields[2].tag, str_comp("rfl_test_tag")));
    DOT_TEST_CHECK(test, "array count",      s->fields[4].array_count == RFL_TestKind_Count * RFL_TEST_SCALE);
    DOT_TEST_CHECK(test, "pointer depth",    s->fields[5].pointer_depth == 1 && s->fields[6].pointer_depth == 2);
    DOT_TEST_CHECK(test, "self pointer",     s->fields[5].type == s);
    DOT_TEST_CHECK(test, "enum field",       s->fields[8].type == kind);
    DOT_TEST_CHECK(test, "extern fields",    s->fields[3].type->field_count == 3 && string8_equal(s->fields[3].type->fields[2].name, str_comp("z")));
    DOT_TEST_CHECK(test, "extern offsets",   s->fields[3].type->fields[2].offset == offsetof(f32x3, z));
    DOT_TEST_CHECK(test, "extern nested",    s->fields[9].type->fields[0].type == reflect_type(f32x4) && s->fields[9].type->fields[0].array_count == 4);
    DOT_TEST_CHECK(test, "lookup",           rfl_type_lookup(str_comp("RFL_TestStruct")) == s);
    DOT_TEST_CHECK(test, "lookup missing",   rfl_type_lookup(str_comp("RFL_Missing")) == 0);

    RFL_TestStruct value = {.slots[3] = 42};
    RFL_Any any  = rfl_any(RFL_TestStruct, &value);
    RFL_Any slot = rfl_any_field(any, &s->fields[4], 3);
    DOT_TEST_CHECK(test, "any field",        slot.type == reflect_type(u32) && *cast(u32*)slot.data == 42);

    RFL_SECTION_ITEM static RFL_Type rfl_test_f32x2_other_tu = {
        .kind  = RFL_TypeKind_Struct,
        .name  = str_lit("f32x2"),
        .body  = str_lit("{ f32 x, y; }"),
        .file  = __FILE__,
        .line  = __LINE__,
        .size  = sizeof(f32x2),
        .align = DOT_ALIGNOF(f32x2),
    };
    DOT_TEST_CHECK(test, "duplicate canonical", rfl_test_f32x2_other_tu.canonical == reflect_type(f32x2));
    DOT_TEST_CHECK(test, "duplicate skipped",   rfl_test_f32x2_other_tu.canonical != &rfl_test_f32x2_other_tu);
    DOT_TEST_CHECK(test, "duplicate lookup",    rfl_type_lookup(str_comp("f32x2")) == reflect_type(f32x2));

    return(test);
}
