global DOT_LogLevelKind g_log_level = DOT_LogLevelKind_Debug;

internal void
dot_set_log_level(DOT_LogLevelKind new_log_level)
{
    g_log_level = new_log_level;
}

internal inline const char*
print_log_level_kind(DOT_LogLevelKind debug_kind){
    DOT_ASSERT(debug_kind < DOT_LogLevelKind_Count);
    const char *ret = dot_log_level_kind_str[debug_kind];
    return ret;
}

internal void
dot_print_debug_(const DOT_PrintDebugParams* params, const char *fmt, ...){
    if(params->print_debug_kind < g_log_level) return;
    thread_local static char buf[DOT_MAX_LOG_LEVEL_LENGTH];
    FILE* out = params->print_debug_kind == DOT_LogLevelKind_Debug ? stdout : stderr;

    va_list args;
    va_start(args, fmt);
    dot_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    const char *fmt_str = params->print_debug_kind == DOT_LogLevelKind_Debug ? "%s%s:%d -> %s\n" : "%s: %s:%d -> %s\n";
    fprintf(out, fmt_str,
        print_log_level_kind(params->print_debug_kind),
        params->file,
        params->line,
        buf);
}

internal void
dot_debug_name_set(u32 capacity, char *ptr, String8 src)
{
    u32 src_size = src.size;
    if(src_size > capacity) src_size = capacity - 1;
    MEMORY_COPY(ptr, src.str, src_size);
    ptr[src_size] = 0;
}


internal
u32 fnv1a_runtime(String8 s)
{
    u32 h = FNV1A_OFFSET;
    u8 *p = s.str;
    u8 *end = s.str + s.size;

    for (; p < end; p++) {
        h ^= *p;
        h *= FNV1A_PRIME;
    }
    return(h);
}
