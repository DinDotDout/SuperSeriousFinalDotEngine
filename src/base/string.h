#ifndef DOT_STRING_H
#define DOT_STRING_H
////////////////////////////////////////////////////////////////
//
// String
// Immutable and null terminated for char* compat/

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

internal inline String8 string8(u8 *str, u64 size);
internal String8 str8_cstring(char *c);
internal inline bool string8_equal(String8 a, String8 b);
internal inline bool string8_array_has(String8* arr, usize size, String8 b);
internal const char** string8_array_to_str_array(Arena* arena, usize size, const String8* src);
#endif // DOT_STRING_H
