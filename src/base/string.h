#ifndef DOT_STRING_H
#define DOT_STRING_H
////////////////////////////////////////////////////////////////
//
// String
// Immutable and null terminated for char* compatibility

typedef struct String8{
    union{
        u8 *str;
        char *cstr;
    };
    u64 size;
}String8;

typedef struct String8Node{
    LLNode node;
    String8 str;
}String8Node;

typedef struct String8List{
    LLHead head;
    u64 count;
    u64 total_size;
}String8List;

#define string8_lit(s)  (String8){.str = (u8*)(s), .size = sizeof(s) - 1}
#define str_fmt "%.*s"
#define str_arg(sv) (int)(sv).size, (sv).str

#define MEMORY_COPY_STRING8(dst, s) MEM_COPY(dst, (s).str, (s).size)

#define CHAR_WHITESPACES "\t\n\v\f\r "
internal read_only String8 g_string_whitespaces = string8_lit(CHAR_WHITESPACES);
DOT_STATIC_ASSERT('\r' - '\t' == 4);
DOT_STATIC_ASSERT('\r' - '\n' == 3);
DOT_STATIC_ASSERT('\r' - '\v' == 2);
DOT_STATIC_ASSERT('\r' - '\f' == 1);

internal b32 char_is_slash(u8 c);
internal b32 char_is_whitespace(u8 c);

internal String8 string8(u8 *s, u64 size);
internal String8 string8_copy(Arena *arena, String8 string);
internal String8 string8_append_string8(Arena *arena, String8 a, String8 b);
internal String8 string8_from_cstring(char *c);
internal String8 string8_format_va(Arena *arena, char *fmt, va_list args);
internal String8 string8_format(Arena *arena, char *fmt, ...);

internal b32 string8_equal(String8 a, String8 b);
internal b32 string8_array_contains(u64 size, String8 *arr, String8 b);
internal b32 string8_contains(String8 haystack, String8 needle);


internal char*          cstr_format_va(Arena *arena, char *fmt, va_list args);
internal char*          cstr_format(Arena *arena, char *fmt, ...);
internal const char**   cstr_array_from_string8_array(Arena* arena, u64 size, const String8 src[]);

internal String8 string8_chop_last_slash(String8 file_path);
internal String8 string8_skip_last_slash(String8 string);
internal b32 string8_starts_with(String8 string, String8 tok);

internal String8 string8_cstring_capped(void *cstr, void *cap);



internal String8Node    *string8_node(LLNode *node);
internal String8Node    *string8_node_next(String8Node *node);

internal void           string8_list_push_node(Arena *arena, String8List *list, String8 string);
internal String8List    string8_split(Arena *arena, String8 string, u64 split_char_count, u8 *split_chars);
internal String8        string8_list_join(Arena *arena, String8List *list);

internal i64 string8_compare(String8 a, String8 b);

internal u64 u64_hash_from_string8(String8 string, u64 seed);
#endif // !DOT_STRING_H
