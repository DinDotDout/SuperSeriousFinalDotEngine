#include <mach-o/dyld.h>

////////////////////////////////////////////////////////////////
//
// Setup
//
internal String8
os_executable_path(char *buffer, u64 capacity)
{
    u32 size = cast(u32)capacity;
    if(_NSGetExecutablePath(buffer, &size) != 0){
        DOT_WARNING("Could not resolve executable path, it needs %u bytes", size);
        return((String8){0});
    }
    return(string8_from_cstring(buffer));
}

////////////////////////////////////////////////////////////////
//
// Memory
//
internal bool
os_commit(void *ptr, u64 size)
{
    if(DOT_UNLIKELY(mprotect(ptr, size, PROT_READ|PROT_WRITE) != 0)){
        DOT_ERROR("Failed to commit memory (errno=%d): %s\n", errno, strerror(errno));
        return(false);
    }
    return(true);
}

// macOS has no user space large pages for anonymous memory
internal bool
os_commit_large(void *ptr, u64 size)
{
    return(os_commit(ptr, size));
}

////////////////////////////////////////////////////////////////
//
// Stacktrace
//
internal void
os_print_stacktrace(void)
{
#if DOT_DEBUG
    void *ips[OS_STACKTRACE_MAX_FRAMES];
    int ips_count = backtrace(ips, OS_STACKTRACE_MAX_FRAMES);

    fprintf(stderr, "Callstack:\n");
    for(int frame = 1; frame < ips_count; frame += 1){
        Dl_info info = {0};
        dladdr(ips[frame], &info);
        String8 module = info.dli_fname ? string8_skip_last_slash(string8_from_cstring(cast(char*)info.dli_fname)) : str_comp("???");
        String8 func   = info.dli_sname ? string8_from_cstring(cast(char*)info.dli_sname) : str_comp("???");
        os_stacktrace_line_print_(cast(u32)frame, (u64)ips[frame], module, func, (String8){0}, 0, false);
    }
#endif // !DOT_DEBUG
}
