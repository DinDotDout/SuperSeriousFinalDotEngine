internal inline const char*
print_log_level_kind(DOT_LogLevelKind debug_kind){
    DOT_ASSERT(debug_kind < DOT_LogLevelKind_Count);
    const char *ret = print_debug_str[debug_kind];
    DOT_ASSERT(ret);
    return ret;
}

void
dot_print_debug_(const DOT_PrintDebugParams* params, const char *fmt, ...){
    char buf[DOT_MAX_LOG_LEVEL_LENGTH];
    FILE* out = params->print_debug_kind == DOT_LogLevelKind_Debug ? stdout : stderr;

    va_list args;
    va_start(args, fmt);
    dot_vsnprintf(buf, sizeof(buf), fmt, args); // TODO: Swap for stb_vsntprintf
    va_end(args);

    const char *fmt_str = params->print_debug_kind == DOT_LogLevelKind_Debug ? "%s%s:%d -> %s\n" : "%s: %s:%d -> %s\n";
    fprintf(out, fmt_str,
        print_log_level_kind(params->print_debug_kind),
        params->file,
        params->line,
        buf);
}
