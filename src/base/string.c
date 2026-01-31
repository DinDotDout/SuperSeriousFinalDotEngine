internal b8
string8_equal(String8 a, String8 b){
    if(a.size == b.size){
        b8 result = MEMORY_EQUAL(a.str, b.str, a.size);
        return result;
    }
    return false;
}

internal b8
string8_array_has(String8* arr, usize size, String8 b){
    b8 found = false;
    for(u64 i = 0; i < size; ++i){
        found = string8_equal(arr[i], b);
        if(found){
            break;
        }
    }
    return found;
}

internal String8
string8_cstring(const char *c){
    String8 result = {cast(u8*)c, strlen(c)};
    return result;
}

internal const char**
string8_array_to_str_array(Arena* arena, usize size, const String8* src){
    const char **dst = PUSH_ARRAY(arena, const char*, size);
    for(u64 i = 0; i < size; ++i){
        dst[i] = cast(char*) src[i].str;
    }
    return dst;
}

internal String8
string8_format_va(Arena *arena, char *fmt, va_list args){
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
string8_format(Arena *arena, char *fmt, ...){
    va_list args;
    va_start(args, fmt);
    String8 result = string8_format_va(arena, fmt, args);
    va_end(args);
    return result;
}

internal char*
cstr_format_va(Arena *arena, char *fmt, va_list args){
    va_list args2;
    va_copy(args2, args);

    u32 needed_bytes = dot_vsnprintf(0, 0, fmt, args) + 1;

    char *result = PUSH_ARRAY_NO_ZERO(arena, char, needed_bytes);
    dot_vsnprintf(result, needed_bytes, fmt, args2);

    va_end(args2);
    return result;
}

internal char*
cstr_format(Arena *arena, char *fmt, ...){
    va_list args;
    va_start(args, fmt);

    char *result = cstr_format_va(arena, fmt, args);

    va_end(args);
    return result;
}

// This is good enough for now
internal u64 HashFromString8(String8 string, u64 seed){
    u64 result = seed; // raddebugger uses 5381
    for(u64 i = 0; i < string.size; ++i){
        result = ((result << 5) + result) + string.str[i];
    }
    return result;
}

