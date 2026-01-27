internal inline String8
string8(u8 *str, u64 size){
    String8 result = {str, size};
    return result;
}

internal inline bool
string8_equal(String8 a, String8 b){
    if(a.size == b.size){
        bool result = MEMORY_EQUAL(a.str, b.str, a.size);
        return result;
    }
    return false;
}

internal bool
string8_array_has(String8* arr, usize size, String8 b){
    bool found = false;
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

// This is good enough for now
internal inline u64 HashFromString8(String8 string, u64 seed){
    u64 result = seed; // raddebugger uses 5381
    for(u64 i = 0; i < string.size; ++i){
        result = ((result << 5) + result) + string.str[i];
    }
    return result;
}

