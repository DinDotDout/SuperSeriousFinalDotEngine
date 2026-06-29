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

typedef struct String8Node String8Node;
struct String8Node{
    String8Node *next;
    String8 str;
};

// Impl should be a collapsable string list into a regular String8
typedef struct String8List{
    String8Node *head;
    String8Node *tail;
    u32 *count;
}StringBuilder;

#define string8_lit(s)  (String8){.str = (u8*)(s), .size = sizeof(s) - 1}
#define str_fmt "%.*s"
#define str_arg(sv) (int)(sv).size, (sv).str

#define MEMORY_COPY_STRING8(dst, s) MEM_COPY(dst, (s).str, (s).size)

internal b32 char_is_slash(u8 c);

internal String8 string8_copy(Arena *arena, String8 string);
internal String8 string8_append_string8(Arena *arena, String8 a, String8 b);
internal String8 string8_from_cstring(char *c);
internal String8 string8_format_va(Arena *arena, char *fmt, va_list args);
internal String8 string8_format(Arena *arena, char *fmt, ...);

internal b32 string8_equal(String8 a, String8 b);
internal b32 string8_array_has(String8 *arr, usize size, String8 b);

internal char* cstr_format_va(Arena *arena, char *fmt, va_list args);
internal char* cstr_format(Arena *arena, char *fmt, ...);

internal String8 string8_chop_last_slash(String8 file_path);
internal String8 string8_skip_last_slash(String8 string);
internal String8 string8_cstring_capped(void *cstr, void *cap);

internal const char** cstr_array_from_string8_array(Arena* arena, usize size, const String8 src[]);

// Sort
internal i64 string8_compare(String8 a, String8 b);

internal u64 u64_hash_from_string8(String8 string, u64 seed);
#endif // !DOT_STRING_H
