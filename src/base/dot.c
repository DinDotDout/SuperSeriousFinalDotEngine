// TODO: Maybe move this back to dot.h and make is single header properly with define impl
#define STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_STATIC
#define STB_SPRINTF_NOUNALIGNED
#include "third_party/stb_sprintf.h"

internal inline const char* PrintDebugKind_GetString(LogLevelKind debug_kind){
    DOT_ASSERT(debug_kind < LOG_LEVEL_COUNT);
    const char* ret = print_debug_str[debug_kind];
    DOT_ASSERT(ret);
    return ret;
}

internal void PrintDebug(const PrintDebugParams* params, const char* fmt, ...){
    char buf[DOT_MAX_LOG_LEVEL_LENGTH];
    FILE* out = params->print_debug_kind == LOG_LEVEL_DEBUG ? stdout : stderr;

    va_list args;
    va_start(args, fmt);
    dot_vsnprintf(buf, sizeof(buf), fmt, args); // TODO: Swap for stb_vsntprintf
    va_end(args);

    const char* fmt_str = params->print_debug_kind == LOG_LEVEL_DEBUG ? "%s%s:%d -> %s\n" : "%s: %s:%d -> %s\n";
    fprintf(out, fmt_str,
        PrintDebugKind_GetString(params->print_debug_kind),
        params->file,
        params->line,
        buf);
}
