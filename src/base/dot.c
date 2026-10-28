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

// --- Logging ---
// Maybe the logging system should just be tied to a frame arena / thread arena
enum { DOT_MAX_LOG_LEVEL_LENGTH = 2048 };

internal void
dot_print_debug_(DOT_LogLevelKind log_level, const char *file, u32 line, const char *fmt, ...)
{
    if(log_level < g_log_level) return;
    local_persist thread_local char buf[DOT_MAX_LOG_LEVEL_LENGTH];
    FILE* out = log_level == DOT_LogLevelKind_Debug ? stdout : stderr;

    va_list args;
    va_start(args, fmt);
    dot_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    const char *fmt_str = log_level == DOT_LogLevelKind_Debug ? "%s%s:%d -> %s\n" : "%s: %s:%d -> %s\n";
    fprintf(out, fmt_str, print_log_level_kind(log_level), file, line, buf);
}

internal void
dot_debug_name_set(i64 capacity, char *ptr, String8 src)
{
    if(src.size >= capacity){
        src.size = capacity - 1;
        DOT_WARNING("Truncating name: %S", src);
    }
    if(src.size > 0){
        MEMORY_COPY(ptr, src.str, src.size);
    }
    ptr[src.size] = 0;
}

