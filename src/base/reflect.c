// Recursive descent grammar for the stringized Reflect/ReflectEnum bodies.
//
//   enum_body   = '{' [ enum_value { ',' { tag } enum_value } [ ',' { tag } ] ] '}'   rfl_parse_enum
//   enum_value  = IDENT [ '=' expr ]                                         (inlined in rfl_parse_enum)
//
//   struct_body = '{' { decl } '}'                                           rfl_parse_struct
//   decl        = [ "const" ] [ "struct" | "union" ]
//                 IDENT [ '(' IDENT ')' ]
//                 declarator { ',' { tag } declarator } ';' { tag }          (inlined in rfl_parse_struct)
//   declarator  = { '*' } IDENT [ '[' expr ']' ]                             (inlined in rfl_parse_struct)
//
//   expr        = and   { '|' and }                                          rfl_parse_expr
//   and         = shift { '&' shift }                                        rfl_parse_expr_and
//   shift       = add   { ( '<<' | '>>' ) add }                              rfl_parse_expr_shift
//   add         = mul   { ( '+' | '-' ) mul }                                rfl_parse_expr_add
//   mul         = unary { ( '*' | '/' | '%' ) unary }                        rfl_parse_expr_mul
//                 (the unary after '/' or '%' goes through                   rfl_parse_expr_divisor)
//   unary       = '-' unary | '~' unary | '(' expr ')' | INT | IDENT         rfl_parse_expr_unary
//
//   tag         = IDENT starting with "rfl_" | "rfl" '(' IDENT [ ',' args ] ')' rfl_parse_tags
//   args        = raw text up to the matching ')', strings skipped           rfl_lex_tag_args
//   INT         = decimal or 0x hex, with any u/l suffixes ignored           rfl_lex_int
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

#define RFL_PRIMITIVE(T, k) \
    RFL_TYPE_MARKER(T); \
    RFL_DEFINE_TYPE(T, k, "", 0)

RFL_PRIMITIVE(i8,    RFL_TypeKind_Int);
RFL_PRIMITIVE(i16,   RFL_TypeKind_Int);
RFL_PRIMITIVE(i32,   RFL_TypeKind_Int);
RFL_PRIMITIVE(i64,   RFL_TypeKind_Int);
RFL_PRIMITIVE(int,   RFL_TypeKind_Int);
RFL_PRIMITIVE(iptr,  RFL_TypeKind_Int);

RFL_PRIMITIVE(u8,    RFL_TypeKind_Uint);
RFL_PRIMITIVE(u16,   RFL_TypeKind_Uint);
RFL_PRIMITIVE(u32,   RFL_TypeKind_Uint);
RFL_PRIMITIVE(u64,   RFL_TypeKind_Uint);
RFL_PRIMITIVE(usize, RFL_TypeKind_Uint);
RFL_PRIMITIVE(uptr,  RFL_TypeKind_Uint);

RFL_PRIMITIVE(b8,    RFL_TypeKind_Bool);
RFL_PRIMITIVE(b16,   RFL_TypeKind_Bool);
RFL_PRIMITIVE(b32,   RFL_TypeKind_Bool);

RFL_PRIMITIVE(f32,   RFL_TypeKind_Float);
RFL_PRIMITIVE(f64,   RFL_TypeKind_Float);

#undef RFL_PRIMITIVE

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

// A punctuator is its character constant: '*' or '<<'. Two-char constants are packed
// first char high on MSVC, Clang and GCC; the lexer builds '<<' / '>>' the same way.
typedef u16 RFL_Punct;

typedef struct RFL_Token
{
    RFL_TokenKind kind;
    String8       text;
    union{
        i64           int_value;
        RFL_Punct     punct;
    };
}RFL_Token;

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
    u32             entry_count;
    u32             tag_count;
}RFL_Parser;

internal void rfl_resolve_type(RFL_Type *type);
internal i64  rfl_int_normalize(i64 value, RFL_Type *int_type);

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
    RFL_Char_IntSuffix  = 1 << 5, // u U l L
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
    flags['u'] |= RFL_Char_IntSuffix; flags['U'] |= RFL_Char_IntSuffix; flags['l'] |= RFL_Char_IntSuffix; flags['L'] |= RFL_Char_IntSuffix;
}

////////////////////////////////
// Lexer: turns the stringized body into tokens, one at a time.

// The char `offset` past the cursor, or 0 past the end of the body.
internal u8
rfl_peek_at(RFL_Parser *parser, u64 offset)
{
    String8 body = parser->type->body;
    i64 at = parser->pos + offset;
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
    RFL_Token token = {.kind = RFL_TokenKind_Int};
    if(rfl_peek(parser) == '0' && char_to_lower(rfl_peek_at(parser, 1)) == 'x'){
        parser->pos += 2;
        token.int_value = cast(i64)rfl_lex_digits(parser, RFL_Char_HexDigit, 16);
    }else{
        token.int_value = cast(i64)rfl_lex_digits(parser, RFL_Char_Digit, 10);
    }
    rfl_skip_while(parser, RFL_Char_IntSuffix);
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

internal String8
rfl_lex_tag_args(RFL_Parser *parser)
{
    String8 body = parser->type->body;
    rfl_skip_while(parser, RFL_Char_Space);
    u64 start = parser->pos;
    u32 depth = 0;
    for(u8 c = rfl_peek(parser); c && !(c == ')' && depth == 0); c = rfl_peek(parser)){
        if(c == '('){
            depth += 1;
        }else if(c == ')'){
            depth -= 1;
        }else if(c == '"' || c == '\''){
            parser->pos += 1;
            while(rfl_peek(parser) && rfl_peek(parser) != c){
                parser->pos += rfl_peek(parser) == '\\' ? 2 : 1;
            }
        }
        parser->pos += 1;
    }
    u64 end = DOT_MIN(parser->pos, cast(u64)body.size);
    while(end > start && (rfl_g_char_flags[body.str[end - 1]] & RFL_Char_Space)){
        end -= 1;
    }
    rfl_lex_next_token(parser);
    return(string8(body.str + start, end - start));
}

internal String8
rfl_tag_name_from_ident(String8 ident)
{
    String8 prefix = str_comp("rfl_");
    return(string8(ident.str + prefix.size, ident.size - prefix.size));
}

internal String8
rfl_expect_tag_name(RFL_Parser *parser)
{
    String8 name = rfl_expect_identifier(parser);
    if(string8_starts_with(name, str_comp("rfl_"))){
        name = rfl_tag_name_from_ident(name);
    }
    return(name);
}

internal void
rfl_emit_tag(RFL_Parser *parser, RFL_Tag tag)
{
    RFL_Type *type = parser->type;
    if(type->tags){
        DOT_ASSERT(parser->tag_count < type->tag_count, "Reflect %S: more tags than were counted", type->name);
        type->tags[parser->tag_count] = tag;
    }
    parser->tag_count += 1;
}

// Reads the tags after a field's ',' / ';' or an enum value's ','; returns how many and where they start.
internal u32
rfl_parse_tags(RFL_Parser *parser, RFL_Tag **out_tags)
{
    u32 first_tag = parser->tag_count;
    for(;;){
        if(parser->tok.kind == RFL_TokenKind_Ident && string8_starts_with(parser->tok.text, str_comp("rfl_"))){
            rfl_emit_tag(parser, (RFL_Tag){ .name = rfl_tag_name_from_ident(parser->tok.text) });
            rfl_lex_next_token(parser);
        }else if(parser->tok.kind == RFL_TokenKind_Ident && string8_equal(parser->tok.text, str_comp("rfl"))){
            rfl_lex_next_token(parser);
            rfl_expect_punct(parser, '(');
            RFL_Tag tag = { .name = rfl_expect_tag_name(parser) };
            if(rfl_token_is_punct(parser, ',')){
                tag.args = rfl_lex_tag_args(parser);
            }
            rfl_expect_punct(parser, ')');
            rfl_emit_tag(parser, tag);
        }else{
            *out_tags = parser->type->tags ? parser->type->tags + first_tag : 0;
            return(parser->tag_count - first_tag);
        }
    }
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
    if(parser->tok.kind == RFL_TokenKind_Int){
        i64 number = parser->tok.int_value;
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
// Body parsers. Entries go into the parser's own type through rfl_emit_*, which count them
// in parser->entry_count. Before rfl_init reserves storage the type has no array and the
// parse only counts; afterwards each write is checked against the count the type holds.

internal void
rfl_emit_value(RFL_Parser *parser, RFL_EnumValue value)
{
    RFL_Type *type = parser->type;
    if(type->values){
        DOT_ASSERT(parser->entry_count < type->value_count, "Reflect %S: more values than were counted", type->name);
        type->values[parser->entry_count] = value;
    }
    parser->entry_count += 1;
}

internal void
rfl_emit_field(RFL_Parser *parser, RFL_Field field)
{
    RFL_Type *type = parser->type;
    if(type->fields){
        DOT_ASSERT(parser->entry_count < type->field_count, "Reflect %S: more fields than were counted", type->name);
        type->fields[parser->entry_count] = field;
    }
    parser->entry_count += 1;
}

// When evaluating, each value is published to its constant right away, so later values
// in the same enum can refer to earlier ones.
internal void
rfl_parse_enum(RFL_Parser *parser)
{
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
        rfl_emit_value(parser, (RFL_EnumValue){.name = value_name, .value = rfl_int_normalize(value, parser->type->base)});
        next_value = value + 1;
        if(!rfl_accept_punct(parser, ',')){
            break;
        }
        RFL_Tag *tags      = 0;
        u32      tag_count = rfl_parse_tags(parser, &tags);
        if(parser->type->values){
            parser->type->values[parser->entry_count - 1].tags      = tags;
            parser->type->values[parser->entry_count - 1].tag_count = tag_count;
        }
    }
    rfl_expect_punct(parser, '}');
    rfl_expect_body_end(parser);
}

internal void
rfl_parse_struct(RFL_Parser *parser)
{
    RFL_Field *fields = parser->type->fields;
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
        String8 type_arg = {0};
        if(rfl_accept_punct(parser, '(')){
            type_arg = rfl_expect_identifier(parser);
            rfl_expect_punct(parser, ')');
        }
        // Each comma-separated name in the declaration is its own field.
        u32 decl_first_field = parser->entry_count;
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
                    DOT_ERROR_FL(parser->type->file, cast(i32)parser->type->line, "Reflect %S: array '%S' has size %" PRId64, parser->type->name, field_name, array_size);
                }
                array_count = cast(u32)array_size;
            }
            RFL_Field field = {
                .name          = field_name,
                .type_name     = type_name,
                .type_arg      = type_arg,
                .array_count   = array_count,
                .pointer_depth = pointer_depth,
            };
            b32 more_declarators = rfl_accept_punct(parser, ',');
            if(more_declarators){
                field.tag_count = rfl_parse_tags(parser, &field.tags);
            }
            rfl_emit_field(parser, field);
            if(!more_declarators){
                break;
            }
        }
        rfl_expect_punct(parser, ';');
        RFL_Tag *decl_tags      = 0;
        u32      decl_tag_count = rfl_parse_tags(parser, &decl_tags);
        for(u32 i = decl_first_field; fields && i < parser->entry_count; i++){
            if(fields[i].tag_count == 0){
                fields[i].tags      = decl_tags;
                fields[i].tag_count = decl_tag_count;
            }
        }
    }
    rfl_expect_punct(parser, '}');
    rfl_expect_body_end(parser);
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
    RFL_Parser parser = rfl_parser_begin(type, RFL_ParserMode_Scan);
    rfl_parse_enum(&parser);
    DOT_ASSERT(parser.entry_count == type->value_count, "Reflect %S: fewer values than were counted", type->name);
    DOT_ASSERT(parser.tag_count == type->tag_count, "Reflect %S: fewer tags than were counted", type->name);

    for(u32 value_index = 0; value_index < type->value_count; ++value_index){
        String8 name = type->values[value_index].name;
        RFL_Const *constant = hm_get_or_create(rfl_g_registry.arena, &rfl_g_registry.consts, name);
        if(constant->owner || constant->known){
            DOT_ERROR_FL(type->file, cast(i32)type->line, "Reflect %S: constant '%S' is already defined", type->name, name);
        }
        constant->owner = type;
    }
}

internal void
rfl_resolve_enum(RFL_Type *type)
{
    RFL_Parser parser = rfl_parser_begin(type, RFL_ParserMode_Evaluate);
    rfl_parse_enum(&parser);
    DOT_ASSERT(parser.entry_count == type->value_count, "Reflect %S: fewer values than were counted", type->name);
    DOT_ASSERT(parser.tag_count == type->tag_count, "Reflect %S: fewer tags than were counted", type->name);
}

internal void
rfl_resolve_struct(RFL_Type *type)
{
    RFL_Parser parser = rfl_parser_begin(type, RFL_ParserMode_Evaluate);
    rfl_parse_struct(&parser);
    DOT_ASSERT(parser.entry_count == type->field_count, "Reflect %S: fewer fields than were counted", type->name);
    DOT_ASSERT(parser.tag_count == type->tag_count, "Reflect %S: fewer tags than were counted", type->name);

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
                DOT_ERROR_FL(type->file, cast(i32)type->line, "Reflect %S: field '%S' has unknown type '%S'; reflect it with Reflect, ReflectEnum, ReflectAlias or ReflectExtern", type->name, field->name, field->type_name);
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
// Init: build the char table, then passes over the two linker sections.
//   1. Register type names (ReflectSection).
//   2. Count every enum's values and every struct's fields, then reserve all of them at once.
//   3. Register ReflectConst values (ReflectConstSection), then every enum's constant names.
//   4. Resolve every type.
// The section counts and the totals from 2 size both hash maps.

internal b32
rfl_type_same_definition(RFL_Type *a, RFL_Type *b)
{
    return a->kind  == b->kind  &&
           a->size  == b->size  &&
           a->align == b->align &&
           string8_equal(a->body, b->body);
}

// Smallest power of two >= entry_count: the hash map masks with MOD_POW2, so bucket counts
// must be powers of two. Doubling from 1 only ever visits powers of two. A bit scan
// (1 << (32 - clz(entry_count - 1))) would do it without the loop.
// TODO: (jd) move this into dot.h math as a shared u32_next_pow2 so other hash maps can size themselves the same way.
internal u32
rfl_bucket_count(u64 entry_count)
{
    u32 bucket_count = 1;
    while(bucket_count < entry_count){
        bucket_count <<= 1;
    }
    DOT_ASSERT(IS_POW2(bucket_count), "Hash map bucket counts must be powers of two");
    return(bucket_count);
}

internal void
rfl_resolve_alias(RFL_Type *type)
{
    RFL_Type *base = type->base;
    if(base->kind == RFL_TypeKind_Alias){
        rfl_resolve_alias(base);
    }
    b32 base_is_number = base->kind == RFL_TypeKind_Int  || base->kind == RFL_TypeKind_Uint ||
                         base->kind == RFL_TypeKind_Float || base->kind == RFL_TypeKind_Bool;
    if(!base_is_number){
        DOT_ERROR_FL(type->file, cast(i32)type->line, "ReflectAlias %S: base %S must be an integer, float or bool type", type->name, base->name);
    }
    type->kind = base->kind;
}

internal void
rfl_register_tag_defs(Arena *arena)
{
    hm_init_string8(arena, &rfl_g_registry.tag_defs, rfl_bucket_count(SECTION_COUNT(ReflectTagSection)));
    for EACH_SECTION_ITEM(ReflectTagSection, RFL_TagDef, def){
        RFL_TagDef **def_slot = hm_get_or_create(arena, &rfl_g_registry.tag_defs, def->name);
        if(*def_slot && (*def_slot)->args_type->canonical == def->args_type->canonical){
            continue;
        }
        if(*def_slot){
            DOT_ERROR_FL(def->file, cast(i32)def->line, "ReflectTag %S: already defined at %s:%u", def->name, (*def_slot)->file, (*def_slot)->line);
        }
        *def_slot = def;
    }
}

internal void
rfl_parse_tag_args(Arena *arena)
{
    u64 data_size = 0;
    for EACH_SECTION_ITEM(ReflectSection, RFL_Type, type){
        if(type->canonical != type){
            continue;
        }
        for(u32 tag_index = 0; tag_index < type->tag_count; ++tag_index){
            RFL_Tag *tag = &type->tags[tag_index];
            RFL_TagDef **def_slot = hm_get(&rfl_g_registry.tag_defs, tag->name);
            if(def_slot){
                tag->def   = *def_slot;
                data_size += ALIGN_POW2(cast(u64)tag->def->args_type->canonical->size, 16);
            }else if(tag->args.size){
                DOT_ERROR_FL(type->file, cast(i32)type->line, "Reflect %S: tag '%S' has arguments '%S' but no ReflectTag(%S, ArgsType)", type->name, tag->name, tag->args, tag->name);
            }
        }
    }

    u8 *next_data = ARENA_PUSH(arena, data_size, 16, true);
    for EACH_SECTION_ITEM(ReflectSection, RFL_Type, type){
        if(type->canonical != type){
            continue;
        }
        for(u32 tag_index = 0; tag_index < type->tag_count; ++tag_index){
            RFL_Tag *tag = &type->tags[tag_index];
            if(!tag->def){
                continue;
            }
            tag->data  = next_data;
            next_data += ALIGN_POW2(cast(u64)tag->def->args_type->canonical->size, 16);
            if(tag->args.size && !rfl_any_set_from_string8(rfl_any_from_type(tag->def->args_type, tag->data), tag->args)){
                DOT_ERROR_FL(type->file, cast(i32)type->line, "Reflect %S: tag '%S' arguments '%S' don't fit %S", type->name, tag->name, tag->args, tag->def->args_type->name);
            }
        }
    }
}

internal void
rfl_init(Arena *arena)
{
    rfl_char_flags_init();

    rfl_g_registry.arena = arena;
    hm_init_string8(arena, &rfl_g_registry.types, rfl_bucket_count(SECTION_COUNT(ReflectSection)));

    for EACH_SECTION_ITEM(ReflectSection, RFL_Type, type){
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

    for EACH_SECTION_ITEM(ReflectSection, RFL_Type, type){
        if(type->canonical == type && type->base){
            type->base = type->base->canonical;
        }
    }
    for EACH_SECTION_ITEM(ReflectSection, RFL_Type, type){
        if(type->canonical == type && type->kind == RFL_TypeKind_Alias){
            rfl_resolve_alias(type);
        }
    }

    u64 value_total = 0;
    u64 field_total = 0;
    u64 tag_total   = 0;
    for EACH_SECTION_ITEM(ReflectSection, RFL_Type, type){
        if(type->canonical != type){
            continue;
        }
        RFL_Parser parser = rfl_parser_begin(type, RFL_ParserMode_Scan);
        if(type->kind == RFL_TypeKind_Enum){
            rfl_parse_enum(&parser);
            type->value_count = parser.entry_count;
            value_total      += type->value_count;
        }else if(type->kind == RFL_TypeKind_Struct){
            rfl_parse_struct(&parser);
            type->field_count = parser.entry_count;
            field_total      += type->field_count;
        }
        type->tag_count = parser.tag_count;
        tag_total      += type->tag_count;
    }

    RFL_EnumValue *next_values = PUSH_ARRAY(arena, RFL_EnumValue, value_total);
    RFL_Field     *next_fields = PUSH_ARRAY(arena, RFL_Field, field_total);
    RFL_Tag       *next_tags   = PUSH_ARRAY(arena, RFL_Tag, tag_total);
    for EACH_SECTION_ITEM(ReflectSection, RFL_Type, type){
        if(type->canonical != type){
            continue;
        }
        type->values  = next_values;
        type->fields  = next_fields;
        type->tags    = next_tags;
        next_values  += type->value_count;
        next_fields  += type->field_count;
        next_tags    += type->tag_count;
    }

    hm_init_string8(arena, &rfl_g_registry.consts, rfl_bucket_count(SECTION_COUNT(ReflectConstSection) + value_total));

    for EACH_SECTION_ITEM(ReflectConstSection, RFL_ConstDef, def){
        RFL_Const *constant = hm_get_or_create(arena, &rfl_g_registry.consts, def->name);
        if(constant->def && constant->def->value == def->value){
            continue;
        }
        if(constant->known){
            DOT_ERROR_FL(def->file, cast(i32)def->line, "ReflectConst %S: already defined at %s:%u", def->name, constant->def->file, constant->def->line);
        }
        constant->def   = def;
        constant->value = def->value;
        constant->known = true;
    }

    for EACH_SECTION_ITEM(ReflectSection, RFL_Type, type){
        if(type->kind == RFL_TypeKind_Enum && type->canonical == type){
            rfl_register_enum_constants(type);
        }
    }

    for EACH_SECTION_ITEM(ReflectSection, RFL_Type, type){
        if(type->canonical == type){
            rfl_resolve_type(type);
        }
    }

    rfl_register_tag_defs(arena);
    rfl_parse_tag_args(arena);
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

internal RFL_Any
rfl_any_from_type(RFL_Type *type, void *data)
{
    return((RFL_Any){ .type = type->canonical, .data = data });
}

internal RFL_Tag *
rfl_tag_find(RFL_Tag *tags, u32 tag_count, String8 name)
{
    for(u32 tag_index = 0; tag_index < tag_count; ++tag_index){
        if(string8_equal(tags[tag_index].name, name)){
            return(&tags[tag_index]);
        }
    }
    return(0);
}

internal String8
rfl_string8_from_enum_(RFL_Type *type, i64 value)
{
    DOT_ASSERT(type->kind == RFL_TypeKind_Enum, "Reflect %S is not an enum", type->name);
    value = rfl_int_normalize(value, type->base);
    for(u32 value_index = 0; value_index < type->value_count; ++value_index){
        if(type->values[value_index].value == value){
            return(type->values[value_index].name);
        }
    }
    return((String8){0});
}

internal i64
rfl_int_read(void *data, u32 size, b32 is_signed)
{
    switch(size){
    case 1:  return(is_signed ? cast(i64)*cast(i8*)data  : cast(i64)*cast(u8*)data);
    case 2:  return(is_signed ? cast(i64)*cast(i16*)data : cast(i64)*cast(u16*)data);
    case 4:  return(is_signed ? cast(i64)*cast(i32*)data : cast(i64)*cast(u32*)data);
    default: return(*cast(i64*)data);
    }
}

internal void
rfl_int_write(void *data, u32 size, i64 value)
{
    switch(size){
    case 1:  *cast(u8*)data  = cast(u8)value;  break;
    case 2:  *cast(u16*)data = cast(u16)value; break;
    case 4:  *cast(u32*)data = cast(u32)value; break;
    default: *cast(i64*)data = value;          break;
    }
}

internal i64
rfl_int_normalize(i64 value, RFL_Type *int_type)
{
    i64 storage = 0;
    rfl_int_write(&storage, int_type->size, value);
    return(rfl_int_read(&storage, int_type->size, int_type->kind == RFL_TypeKind_Int));
}

internal b32
rfl_enum_name_matches_(String8 name, String8 token)
{
    if(string8_equal(name, token)){
        return(true);
    }
    if(token.size >= name.size){
        return(false);
    }
    String8 suffix = string8(name.str + name.size - token.size, token.size);
    return(name.str[name.size - token.size - 1] == '_' && string8_equal(suffix, token));
}

internal b32
rfl_any_set_from_token_(RFL_Any value, String8 token)
{
    RFL_Type *type = value.type;
    char buffer[64];
    if(token.size == 0 || token.size >= cast(i64)sizeof(buffer)){
        return(false);
    }
    MEMORY_COPY(buffer, token.str, cast(u64)token.size);
    buffer[token.size] = 0;
    char *end = NULL;

    switch(type->kind){
    case RFL_TypeKind_Int:{
        long long parsed = strtoll(buffer, &end, 0);
        if(*end) return(false);
        rfl_int_write(value.data, type->size, cast(i64)parsed);
    }break;
    case RFL_TypeKind_Uint:{
        unsigned long long parsed = strtoull(buffer, &end, 0);
        if(*end) return(false);
        rfl_int_write(value.data, type->size, cast(i64)parsed);
    }break;
    case RFL_TypeKind_Bool:{
        if(string8_equal(token, str_comp("true")) || string8_equal(token, str_comp("1"))){
            rfl_int_write(value.data, type->size, 1);
        }else if(string8_equal(token, str_comp("false")) || string8_equal(token, str_comp("0"))){
            rfl_int_write(value.data, type->size, 0);
        }else{
            return(false);
        }
    }break;
    case RFL_TypeKind_Float:{
        b32 has_float_suffix = buffer[token.size - 1] == 'f' || buffer[token.size - 1] == 'F';
        if(has_float_suffix && token.size > 1){
            buffer[token.size - 1] = 0;
        }
        if(type->size == sizeof(f64)){
            f64 parsed = strtod(buffer, &end);
            if(*end) return(false);
            *cast(f64*)value.data = parsed;
        }else{
            f32 parsed = strtof(buffer, &end);
            if(*end) return(false);
            *cast(f32*)value.data = parsed;
        }
    }break;
    case RFL_TypeKind_Enum:{
        for(u32 value_index = 0; value_index < type->value_count; ++value_index){
            if(rfl_enum_name_matches_(type->values[value_index].name, token)){
                rfl_int_write(value.data, type->size, type->values[value_index].value);
                return(true);
            }
        }
        long long parsed = strtoll(buffer, &end, 0);
        if(*end) return(false);
        rfl_int_write(value.data, type->size, cast(i64)parsed);
    }break;
    default: return(false);
    }
    return(true);
}

internal b32
rfl_any_set_from_tokens_(RFL_Any value, String8 *tokens, u32 token_count, u32 *cursor)
{
    if(value.type->kind != RFL_TypeKind_Struct){
        if(*cursor >= token_count){
            return(false);
        }
        String8 token = tokens[*cursor];
        *cursor += 1;
        return(rfl_any_set_from_token_(value, token));
    }
    for(u32 field_index = 0; field_index < value.type->field_count; ++field_index){
        RFL_Field *field = &value.type->fields[field_index];
        if(field->pointer_depth){
            return(false);
        }
        for(u32 element = 0; element < field->array_count; ++element){
            if(!rfl_any_set_from_tokens_(rfl_any_field(value, field, element), tokens, token_count, cursor)){
                return(false);
            }
        }
    }
    return(true);
}

internal b32
rfl_any_set_from_string8(RFL_Any value, String8 text)
{
    String8 tokens[64];
    u32 token_count = 0;
    i64 start = -1;
    for(i64 c = 0; c <= text.size; ++c){
        u8 ch = c < text.size ? text.str[c] : ' ';
        b32 is_separator = ch == ' ' || ch == '\t' || ch == ',' || ch == '(' || ch == ')' || ch == '{' || ch == '}';
        if(!is_separator && start < 0){
            start = c;
        }else if(is_separator && start >= 0){
            if(token_count == DOT_ARRAY_COUNT(tokens)){
                return(false);
            }
            tokens[token_count++] = string8(text.str + start, cast(u64)(c - start));
            start = -1;
        }
    }

    u8 backup[256];
    b32 can_restore = value.type->size <= sizeof(backup);
    if(can_restore){
        MEMORY_COPY(backup, value.data, value.type->size);
    }
    u32 cursor = 0;
    b32 ok = rfl_any_set_from_tokens_(value, tokens, token_count, &cursor) && cursor == token_count;
    if(!ok && can_restore){
        MEMORY_COPY(value.data, backup, value.type->size);
    }
    return(ok);
}

internal void
rfl_string8_list_push_any(Arena *arena, String8List *list, RFL_Any value)
{
    RFL_Type *type = value.type;
    switch(type->kind){
    case RFL_TypeKind_Int:
        string8_list_push_node(arena, list, string8_format(arena, "%" PRId64, rfl_int_read(value.data, type->size, true)));
        break;
    case RFL_TypeKind_Uint:
        string8_list_push_node(arena, list, string8_format(arena, "%" PRIu64, cast(u64)rfl_int_read(value.data, type->size, false)));
        break;
    case RFL_TypeKind_Bool:
        string8_list_push_node(arena, list, rfl_int_read(value.data, type->size, false) ? str_comp("true") : str_comp("false"));
        break;
    case RFL_TypeKind_Float:
        if(type->size == sizeof(f64)){
            string8_list_push_node(arena, list, string8_format(arena, "%f", *cast(f64*)value.data));
        }else{
            string8_list_push_node(arena, list, string8_format(arena, "%f", *cast(f32*)value.data));
        }
        break;
    case RFL_TypeKind_Enum:{
        i64 number = rfl_int_read(value.data, type->size, type->base->kind == RFL_TypeKind_Int);
        String8 name = rfl_string8_from_enum_(type, number);
        string8_list_push_node(arena, list, name.size ? name : string8_format(arena, "%" PRId64, number));
    }break;
    case RFL_TypeKind_Struct:
        string8_list_push_node(arena, list, str_comp("{"));
        for(u32 field_index = 0; field_index < type->field_count; ++field_index){
            RFL_Field *field = &type->fields[field_index];
            string8_list_push_node(arena, list, string8_format(arena, field_index ? ", %S = " : "%S = ", field->name));
            if(field->pointer_depth){
                string8_list_push_node(arena, list, string8_format(arena, "%p", *cast(void**)(cast(u8*)value.data + field->offset)));
            }else if(field->array_count > 1){
                string8_list_push_node(arena, list, str_comp("["));
                for(u32 element_index = 0; element_index < field->array_count; ++element_index){
                    if(element_index){
                        string8_list_push_node(arena, list, str_comp(", "));
                    }
                    rfl_string8_list_push_any(arena, list, rfl_any_field(value, field, element_index));
                }
                string8_list_push_node(arena, list, str_comp("]"));
            }else{
                rfl_string8_list_push_any(arena, list, rfl_any_field(value, field, 0));
            }
        }
        string8_list_push_node(arena, list, str_comp("}"));
        break;
    default:
        break;
    }
}

internal String8
rfl_string8_from_any(Arena *arena, RFL_Any value)
{
    Scratch s = scratch_begin(1, &arena);
    String8List list = {0};
    rfl_string8_list_push_any(s.arena, &list, value);
    String8 result = string8_list_join(arena, &list);
    scratch_end(s);
    return(result);
}

/////////////////////////////////////////////// Test suite

DOT_TEST_SUITE(rfl_tests)
{
    DOT_TestResults test = {0};

    #define RFL_TEST_SCALE 2
    #define rfl_test_tag
    #define rfl_test_other_tag
    #define rfl_test_args(...)
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

    ReflectEnum(u32, RFL_TestUnsigned, {
        RFL_TestUnsigned_Zero,
        RFL_TestUnsigned_Max = -1,
        });

    ReflectEnum(i8, RFL_TestSmall, {
        RFL_TestSmall_Wrap = 200,
        });

    typedef u16 RFL_TestAlias;
    ReflectAlias(RFL_TestAlias, u16);
    typedef RFL_TestAlias RFL_TestAliasChain;
    ReflectAlias(RFL_TestAliasChain, RFL_TestAlias);

    typedef u64 RFL_TestPtr;
    #define RFL_TestPtr(T) RFL_TestPtr
    ReflectAlias(RFL_TestPtr, u64);
    Reflect(RFL_TestPtrStruct, {
        RFL_TestPtr(f32x3) positions;
        u32                count;
        });

    ReflectEnum(RFL_TestAlias, RFL_TestAliasEnum, {
        RFL_TestAliasEnum_Wrap = -1,
        });

    Reflect(RFL_TestStruct, {
        u8 a;
        f32 x, y;       rfl_test_tag
        f32x3 color;    rfl_test_tag rfl_test_other_tag
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

    Reflect(RFL_TestTagArgs, {
        i32 a;
        i32 b;
        f32 c;
        });
    ReflectTag(test_args, RFL_TestTagArgs);

    Reflect(RFL_TestArgsTagStruct, {
        f32 low;    rfl(range, 0, 1.5f)
        f32 high,   rfl(test_args, (1, 2), 0.5f) wide; rfl(test_args)
        f32 bare;   rfl_test_tag rfl(test_args, 5, -6, 7.25)
        });

    RFL_Type *kind = reflect_enum(RFL_TestKind);
    DOT_TEST_CHECK(test, "enum count",       kind->value_count == 4);
    DOT_TEST_CHECK(test, "enum implicit",    kind->values[0].value == RFL_TestKind_A);
    DOT_TEST_CHECK(test, "enum explicit",    kind->values[1].value == RFL_TestKind_B);
    DOT_TEST_CHECK(test, "enum next",        kind->values[2].value == RFL_TestKind_C);
    DOT_TEST_CHECK(test, "enum count value", kind->values[3].value == RFL_TestKind_Count);
    DOT_TEST_CHECK(test, "enum name",        string8_equal(kind->values[2].name, str_comp("RFL_TestKind_C")));

    RFL_Type *flags = reflect_enum(RFL_TestFlags);
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
    DOT_TEST_CHECK(test, "no tag",           s->fields[0].tag_count == 0);
    DOT_TEST_CHECK(test, "tag",              s->fields[1].tag_count == 1 && string8_equal(s->fields[1].tags[0].name, str_comp("test_tag")));
    DOT_TEST_CHECK(test, "tag declarators",  s->fields[2].tag_count == 1 && string8_equal(s->fields[2].tags[0].name, str_comp("test_tag")));
    DOT_TEST_CHECK(test, "tags all kept",    s->fields[3].tag_count == 2 && string8_equal(s->fields[3].tags[0].name, str_comp("test_tag")) && string8_equal(s->fields[3].tags[1].name, str_comp("test_other_tag")));
    DOT_TEST_CHECK(test, "tag not leaked",   s->fields[4].tag_count == 0);
    DOT_TEST_CHECK(test, "enum tag kept",    kind->values[2].tag_count == 1 && string8_equal(kind->values[2].tags[0].name, str_comp("test_tag")));
    DOT_TEST_CHECK(test, "enum after tag",   string8_equal(kind->values[3].name, str_comp("RFL_TestKind_Count")) && kind->values[3].tag_count == 0);

    RFL_Type *tagged = reflect_type(RFL_TestTagStruct);
    DOT_TEST_CHECK(test, "comma tag",        tagged->fields[0].tag_count == 1 && string8_equal(tagged->fields[0].tags[0].name, str_comp("test_other_tag")));
    DOT_TEST_CHECK(test, "decl tag fills",   tagged->fields[1].tag_count == 1 && string8_equal(tagged->fields[1].tags[0].name, str_comp("test_tag")));
    DOT_TEST_CHECK(test, "decl tag last",    tagged->fields[2].tag_count == 1 && string8_equal(tagged->fields[2].tags[0].name, str_comp("test_tag")));

    RFL_Type *args = reflect_type(RFL_TestArgsTagStruct);
    RFL_Tag *low_range   = rfl_tag_find(args->fields[0].tags, args->fields[0].tag_count, str_comp("range"));
    RFL_Tag *high_args   = rfl_tag_find(args->fields[1].tags, args->fields[1].tag_count, str_comp("test_args"));
    RFL_Tag *wide_args   = rfl_tag_find(args->fields[2].tags, args->fields[2].tag_count, str_comp("test_args"));
    RFL_Tag *bare_args   = rfl_tag_find(args->fields[3].tags, args->fields[3].tag_count, str_comp("test_args"));
    RFL_TagRange    *low_range_data = low_range ? low_range->data : 0;
    RFL_TestTagArgs *high_data      = high_args ? high_args->data : 0;
    RFL_TestTagArgs *wide_data      = wide_args ? wide_args->data : 0;
    RFL_TestTagArgs *bare_data      = bare_args ? bare_args->data : 0;
    DOT_TEST_CHECK(test, "tag args count",   args->field_count == 4);
    DOT_TEST_CHECK(test, "tag range typed",  low_range_data && low_range_data->min == 0.0 && low_range_data->max == 1.5);
    DOT_TEST_CHECK(test, "tag args raw",     low_range && string8_equal(low_range->args, str_comp("0, 1.5f")));
    DOT_TEST_CHECK(test, "tag args comma",   high_data && high_data->a == 1 && high_data->b == 2 && high_data->c == 0.5f);
    DOT_TEST_CHECK(test, "tag args empty",   wide_data && wide_data->a == 0 && wide_data->b == 0 && wide_data->c == 0.0f);
    DOT_TEST_CHECK(test, "tag args mixed",   args->fields[3].tag_count == 2 && rfl_tag_find(args->fields[3].tags, args->fields[3].tag_count, str_comp("test_tag")));
    DOT_TEST_CHECK(test, "tag args after",   bare_data && bare_data->a == 5 && bare_data->b == -6 && bare_data->c == 7.25f);
    DOT_TEST_CHECK(test, "tag args offsets", args->fields[3].offset == offsetof(RFL_TestArgsTagStruct, bare));
    DOT_TEST_CHECK(test, "tag find missing", rfl_tag_find(args->fields[0].tags, args->fields[0].tag_count, str_comp("missing")) == 0);

    DOT_TEST_CHECK(test, "enum to string",   string8_equal(rfl_string8_from_enum(RFL_TestKind, RFL_TestKind_B), str_comp("RFL_TestKind_B")));
    DOT_TEST_CHECK(test, "enum to string 0", string8_equal(rfl_string8_from_enum(RFL_TestKind, RFL_TestKind_A), str_comp("RFL_TestKind_A")));
    DOT_TEST_CHECK(test, "enum negative",    string8_equal(rfl_string8_from_enum(RFL_TestFlags, RFL_TestFlagBit_Neg), str_comp("RFL_TestFlagBit_Neg")));
    DOT_TEST_CHECK(test, "enum missing",     rfl_string8_from_enum(RFL_TestKind, 99).size == 0);

    RFL_TestUnsigned unsigned_max = cast(RFL_TestUnsigned)RFL_TestUnsigned_Max;
    RFL_TestSmall    small_wrap   = cast(RFL_TestSmall)RFL_TestSmall_Wrap;
    DOT_TEST_CHECK(test, "enum unsigned var",   string8_equal(rfl_string8_from_enum(RFL_TestUnsigned, unsigned_max), str_comp("RFL_TestUnsigned_Max")));
    DOT_TEST_CHECK(test, "enum unsigned const", string8_equal(rfl_string8_from_enum(RFL_TestUnsigned, RFL_TestUnsigned_Max), str_comp("RFL_TestUnsigned_Max")));
    DOT_TEST_CHECK(test, "enum small var",      string8_equal(rfl_string8_from_enum(RFL_TestSmall, small_wrap), str_comp("RFL_TestSmall_Wrap")));
    DOT_TEST_CHECK(test, "enum small const",    string8_equal(rfl_string8_from_enum(RFL_TestSmall, RFL_TestSmall_Wrap), str_comp("RFL_TestSmall_Wrap")));
    DOT_TEST_CHECK(test, "enum base",           reflect_enum(RFL_TestUnsigned)->base == reflect_type(u32) && reflect_enum(RFL_TestSmall)->base == reflect_type(i8));

    RFL_Type *alias = reflect_type(RFL_TestAlias);
    RFL_TestAliasChain alias_value = 7;
    DOT_TEST_CHECK(test, "alias kind",          alias->kind == RFL_TypeKind_Uint && alias->size == sizeof(u16));
    DOT_TEST_CHECK(test, "alias base",          alias->base == reflect_type(u16));
    DOT_TEST_CHECK(test, "alias name",          string8_equal(alias->name, str_comp("RFL_TestAlias")));
    DOT_TEST_CHECK(test, "alias chain",         reflect_type(RFL_TestAliasChain)->kind == RFL_TypeKind_Uint && reflect_type(RFL_TestAliasChain)->base == alias);
    Scratch alias_scratch = scratch_begin(0, 0);
    DOT_TEST_CHECK(test, "alias any",           string8_equal(rfl_string8_from_any(alias_scratch.arena, rfl_any(RFL_TestAliasChain, &alias_value)), str_comp("7")));
    scratch_end(alias_scratch);
    DOT_TEST_CHECK(test, "alias enum base",     string8_equal(rfl_string8_from_enum(RFL_TestAliasEnum, cast(RFL_TestAliasEnum)0xFFFF), str_comp("RFL_TestAliasEnum_Wrap")));
    RFL_Type *ptr_struct = reflect_type(RFL_TestPtrStruct);
    DOT_TEST_CHECK(test, "type arg name",       ptr_struct->fields[0].type == reflect_type(RFL_TestPtr) && string8_equal(ptr_struct->fields[0].type_arg, str_comp("f32x3")));
    DOT_TEST_CHECK(test, "type arg absent",     ptr_struct->fields[1].type_arg.size == 0);
    DOT_TEST_CHECK(test, "type arg offsets",    ptr_struct->fields[1].offset == offsetof(RFL_TestPtrStruct, count) && ptr_struct->size == sizeof(RFL_TestPtrStruct));
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

    SECTION_ITEM(ReflectSection) static RFL_Type rfl_test_f32x2_other_tu = {
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

    SECTION_ITEM(ReflectConstSection) static RFL_ConstDef rfl_test_scale_other_tu = {
        .name  = str_lit("RFL_TEST_SCALE"),
        .value = RFL_TEST_SCALE,
        .file  = __FILE__,
        .line  = __LINE__,
    };
    RFL_Const *scale = hm_get(&rfl_g_registry.consts, str_comp("RFL_TEST_SCALE"));
    DOT_TEST_CHECK(test, "duplicate const",     scale && scale->value == RFL_TEST_SCALE && scale->def != &rfl_test_scale_other_tu);
    DOT_TEST_CHECK(test, "values reserved",     flags->values == kind->values + kind->value_count || kind->values == flags->values + flags->value_count);

    return(test);
}
