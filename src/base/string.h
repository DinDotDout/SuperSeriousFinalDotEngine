#ifndef DOT_STRING_H
#define DOT_STRING_H

////////////////////////////////////////////////////////////////
//
// String
// Immutable and null terminated for char* compatibility

typedef struct String8 {
    u8 *str;
    u64 size;
} String8;

typedef struct String8Node String8Node;
struct String8Node {
    String8Node *next;
    String8 str;
};

// Impl should be a collapsable string list into a regular String8
typedef struct StringBuilder {
    String8Node *head;
    String8Node *tail;
    u32 *count;
} StringBuilder;

#define String8Lit(s)  (String8){(u8*)(s), sizeof(s) - 1}
#define StrFmt "%.*s"
#define StrArg(sv) (int)(sv).len, (sv).buff
#define MEM_COPY_STRING8(dst, s) MEM_COPY(dst, (s).str, (s).size)

internal String8 str8_cstring(char *c);
internal String8 string8_format_va(Arena *arena, char *fmt, va_list args);
internal String8 string8_format(Arena *arena, char *fmt, ...);

internal char* cstr_format_va(Arena *arena, char *fmt, va_list args);
internal char* cstr_format(Arena *arena, char *fmt, ...);

internal b8      string8_equal(String8 a, String8 b);
internal b8      string8_array_has(String8* arr, usize size, String8 b);

internal const char**  string8_array_to_str_array(Arena* arena, usize size, const String8* src);
#endif // DOT_STRING_H
