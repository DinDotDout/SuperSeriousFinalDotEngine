internal String8
string8(u8 *s, u64 size)
{
    String8 str = {.str = s, .size = size};
    return str;
}

internal b32
char_is_whitespace(u8 c)
{
    return c == ' ' || c - '\t' < 5;
}

internal b32
char_is_slash(u8 c)
{
  return (c == '/' || c == '\\');
}

internal String8
string8_append_string8(Arena *arena, String8 a, String8 b)
{
    String8 new_str = {
        .size = a.size+b.size,
        .str = PUSH_ARRAY_NO_ZERO(arena, u8, new_str.size + 1)
    };
    MEMORY_COPY(new_str.str, a.str, a.size);
    MEMORY_COPY(new_str.str + a.size, b.str, b.size);
    new_str.str[new_str.size] = 0;
    return(new_str);
}

internal b32
string8_starts_with(String8 string, String8 tok)
{
    if(string.size < tok.size)
        return false;
    String8 new = string8(string.str, tok.size);
    return string8_equal(new, tok);
}

internal String8
string8_skip_last_slash(String8 string)
{
    if(string.size == 0){
        return string;
    }
    u8 *ptr = string.str + string.size - 1;
    for(;ptr >= string.str; ptr -= 1){
        if(char_is_slash(*ptr)){
            break;
        }
    }
    if(ptr >= string.str){
        ptr += 1;
        string.size = (u64)(string.str + string.size - ptr);
        string.str = ptr;
    }
    return string;
}

internal String8
string8_chop_last_slash(String8 string)
{
    if(string.size == 0){
        return string;
    }
    u8 *ptr = string.str + string.size - 1;
    for(;ptr >= string.str; --ptr){
        if(char_is_slash(*ptr)){
            break;
        }
    }
    if(ptr >= string.str){
        string.size = cast(u64)(ptr - string.str);
    }else{
        string.size = 0;
    }
    return(string);
}

internal String8
string8_strip_file_extension(String8 string)
{
    if(string.size <= 0){
        return string;
    }
    u8 *ptr = string.str + string.size - 1;
    for(;ptr >= string.str; --ptr){
        if(*ptr == '.'){
            break;
        }
    }
    if(ptr >= string.str){
        string.size = cast(u64)(ptr - string.str);
    }else{
        string.size = 0;
    }
    return(string);
}

internal String8
string8_file_extension(String8 string)
{
    if(string.size <= 0){
        return string;
    }
    u8 *ptr = string.str + string.size - 1;
    for(;ptr >= string.str; --ptr){
        if(*ptr == '.'){
            break;
        }
    }
    if(ptr >= string.str){
        u8 *end = string.str + string.size;
        string.str  = ++ptr;
        string.size = cast(u64)(end - string.str);
    }else{
        string.size = 0;
    }
    return(string);
}

internal String8
string8_copy(Arena *arena, String8 string)
{
  String8 str;
  str.size = string.size;
  str.str = PUSH_ARRAY_NO_ZERO(arena, u8, str.size + 1);
  MEMORY_COPY(str.str, string.str, string.size);
  str.str[str.size] = 0;
  return str;
}

internal b32
string8_equal(String8 a, String8 b)
{
    if(a.size == b.size){
        b32 result = MEMORY_EQUAL(a.str, b.str, a.size);
        return result;
    }
    return false;
}

internal b32
string8_find_substring(String8 haystack, String8 needle)
{
    if(needle.size == 0 || haystack.size < needle.size){
        return false;
    }

    for(u64 i = 0; i <= haystack.size - needle.size; ++i){
        if(haystack.str[i] == needle.str[0]){
            b32 match = true;

            for(u64 j = 1; j < needle.size; ++j){
                if(haystack.str[i + j] != needle.str[j]){
                    match = false;
                    break;
                }
            }

            if(match){
                return true;
            }
        }
    }

    return false;
}

internal b32
string8_contains(String8 haystack, String8 needle)
{
    if(needle.size == 0 || haystack.size < needle.size){
        return false;
    }
    b32 found = false;
    for(u64 i = 0; i <= haystack.size - needle.size; ++i){
        if(haystack.str[i] == needle.str[0]){

            b32 is_word = true;
            for(u64 j = 1; j < needle.size; ++j){
                if(haystack.str[j+i] != needle.str[j]){
                    is_word = false;
                    break;
                }
            }
            if(is_word){
                found = true;
                break;
            }
        }
    }
    return found;
}

internal b32
string8_array_contains(u64 size, String8 *arr, String8 b)
{
    b32 found = false;
    for(u64 i = 0; i < size; ++i){
        found = string8_equal(arr[i], b);
        if(found){
            break;
        }
    }
    return found;
}

internal String8
string8_from_cstring(char *c)
{
    String8 result = {.cstr = c, .size = strlen(c)};
    return result;
}

internal const char**
cstr_array_from_string8_array(Arena *arena, u64 size, const String8 src[])
{
    const char **dst = PUSH_ARRAY(arena, const char*, size);
    for(u64 i = 0; i < size; ++i){
        dst[i] = src[i].cstr;
    }
    return dst;
}

internal String8
string8_format_va(Arena *arena, char *fmt, va_list args)
{
    va_list args2;
    va_copy(args2, args);
    u32 needed_bytes = dot_vsnprintf(0, 0, fmt, args) + 1;
    String8 result = {0};
    result.str  = PUSH_ARRAY_NO_ZERO(arena, u8, needed_bytes);
    result.size = dot_vsnprintf((char*)result.str, needed_bytes, fmt, args2);
    result.str[result.size] = 0;
    va_end(args2);
    return result;
}

internal String8
string8_format(Arena *arena, char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    String8 result = string8_format_va(arena, fmt, args);
    va_end(args);
    return result;
}

internal char*
cstr_format_va(Arena *arena, char *fmt, va_list args)
{
    va_list args2;
    va_copy(args2, args);

    u32 needed_bytes = dot_vsnprintf(0, 0, fmt, args) + 1;

    char *result = PUSH_ARRAY_NO_ZERO(arena, char, needed_bytes);
    dot_vsnprintf(result, needed_bytes, fmt, args2);

    va_end(args2);
    return result;
}

internal char*
cstr_format(Arena *arena, char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char *result = cstr_format_va(arena, fmt, args);

    va_end(args);
    return result;
}

internal i64
string8_compare(String8 a, String8 b)
{
    for(u32 i = 0; i < DOT_MIN(a.size, b.size); ++i){
        u8 char_a = a.str[i];
        u8 char_b = b.str[i];
        if(char_a < char_b){
            return -1;
        }else if(char_a > char_b){
            return 1;
        }
    }
    return a.size == b.size ? 0 :
        (a.size > b.size ? 1 : -1);
}

internal u64
u64_hash_from_string8(String8 string, u64 seed)
{
    u64 result = seed != U64_MAX ? seed : 5381;
    for(u64 i = 0; i < string.size; ++i){
        result = ((result << 5) + result) + string.str[i];
    }
    return result;
}

internal String8
string8_cstring_capped(void *cstr, void *cap)
{
    u8 *start = cast(u8 *)cstr;
    u8 *end   = cast(u8 *)cap;

    u64 cap_size = cast(u64)(end - start);

    // search for '\0' within the cap
    u8 *nul = cast(u8 *)memchr(start, 0, cap_size);

    u64 size = nul ? cast(u64)(nul - start) : cap_size;
    return (String8){.str = start, .size = size};
}

internal String8Node *
string8_node(LLNode *node)
{
    String8Node *str8node = LLNodeGet(String8Node, node);
    return str8node;
}

internal String8Node *
string8_node_next(String8Node *node)
{
    String8Node *next = LLNodeNext(String8Node, node);
    return(next);
}

// internal void
// string8_node_push(String8Node *str8node, String8Node *str8pushed)
// {
//     str8node->node = str8pushed->node;
// }

internal void
string8_list_push_node(Arena *arena, String8List *list, String8 string)
{
    String8Node *str8node = PushLLNode(arena, String8Node);
    str8node->str.str   = string.str;
    str8node->str.size  = string.size;

    list->head.last = &str8node->node;
    list->count += 1;
    list->total_size += string.size;
}

internal String8List
string8_split(Arena *arena, String8 string, u64 split_char_count, u8 *split_chars)
{
    String8List list = llhead_lit();
    string8_list_join(arena, &list);
    u8 *substr = string.str;

    u8 *head = string.str;
    u8 *tail = (string.str+string.size);
    for(;head < tail; ++head){
        u8 curr = *head;
        b32 is_split = false;
        for(u64 j = 0; j < split_char_count; ++j){
            if(curr == split_chars[j]){
                is_split = true;
                break;
            }
        }
        if(is_split){
            String8 str = string8(substr, head - substr);
            string8_list_push_node(arena, &list, str);
            substr = head + 1;
        }
    }
    if(substr < tail){
        String8 str = string8(substr, head - substr);
        string8_list_push_node(arena, &list, str);
    }
    return list;
}

internal String8
string8_list_join(Arena *arena, String8List *list)
{
    String8 res = {0};
    res.size = list->total_size;
    res.str = PUSH_ARRAY_NO_ZERO(arena, u8, list->total_size);

    u64 pos = 0;
    String8Node *curr_node = LLHeadFirst(String8Node, list);
    for(u64 i = 0; i < list->count; ++i){
        String8 curr_str = curr_node->str;
        MEMORY_COPY(res.str+pos, curr_str.str, curr_str.size);
        pos += curr_str.size;
        curr_node = string8_node_next(curr_node);
    }
    return res;
}
