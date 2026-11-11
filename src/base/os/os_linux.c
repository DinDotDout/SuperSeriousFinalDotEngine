#include <link.h>

typedef struct OS_Symbolizer
{
    pid_t pid;
    FILE *to_child;
    FILE *from_child;
}OS_Symbolizer;

////////////////////////////////////////////////////////////////
//
// Setup
//
internal String8
os_executable_path(char *buffer, u64 capacity)
{
    i64 len = readlink("/proc/self/exe", buffer, capacity - 1);
    if(len <= 0){
        DOT_WARNING("Could not resolve executable path: %s", strerror(errno));
        return((String8){0});
    }
    buffer[len] = 0;
    return(string8(cast(u8*)buffer, cast(u64)len));
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
    madvise(ptr, size, MADV_POPULATE_WRITE);
    return(true);
}

// To maximize THP taking effect we must pass in aligned to 2M pages
internal bool
os_commit_large(void *ptr, u64 size)
{
    if(DOT_UNLIKELY(mprotect(ptr, size, PROT_READ|PROT_WRITE) != 0)){
        DOT_ERROR("Failed to commit memory (errno=%d): %s\n", errno, strerror(errno));
        return(false);
    }
    madvise(ptr, size, MADV_HUGEPAGE);
    madvise(ptr, size, MADV_POPULATE_WRITE);
    return(true);
}

////////////////////////////////////////////////////////////////
//
// Stacktrace
//
internal String8
os_symbolizer_line_trim_(char *line)
{
    String8 result = string8_from_cstring(line);
    while(result.size > 0 && (result.str[result.size - 1] == '\n' || result.str[result.size - 1] == '\r')){
        result.size -= 1;
    }
    return(result);
}

internal OS_Symbolizer
os_symbolizer_spawn_(void)
{
    OS_Symbolizer result = { .pid = -1 };
    int in_pipe[2], out_pipe[2];
    if(pipe(in_pipe) != 0){
        return(result);
    }
    if(pipe(out_pipe) != 0){
        close(in_pipe[0]);
        close(in_pipe[1]);
        return(result);
    }

    pid_t pid = fork();
    if(pid == 0){
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        closefrom(3);
        execlp("llvm-symbolizer", "llvm-symbolizer", "--functions=linkage", "--inlines", "--no-debuginfod", (char*)NULL);
        _exit(127);
    }
    close(in_pipe[0]);
    close(out_pipe[1]);
    if(pid < 0){
        close(in_pipe[1]);
        close(out_pipe[0]);
        return(result);
    }

    result.pid        = pid;
    result.to_child   = fdopen(in_pipe[1], "w");
    result.from_child = fdopen(out_pipe[0], "r");
    return(result);
}

internal void
os_symbolizer_kill_(OS_Symbolizer *symbolizer)
{
    if(symbolizer->to_child){
        fclose(symbolizer->to_child);
    }
    if(symbolizer->from_child){
        fclose(symbolizer->from_child);
    }
    if(symbolizer->pid > 0){
        int status;
        waitpid(symbolizer->pid, &status, 0);
    }
    MEMORY_ZERO_STRUCT(symbolizer);
}

internal void
os_print_stacktrace(void)
{
#if DOT_DEBUG
    void *ips[OS_STACKTRACE_MAX_FRAMES];
    int ips_count = backtrace(ips, OS_STACKTRACE_MAX_FRAMES);

    sigset_t pipe_set, old_set;
    sigemptyset(&pipe_set);
    sigaddset(&pipe_set, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &pipe_set, &old_set);

    OS_Symbolizer symbolizer = os_symbolizer_spawn_();
    bool symbolizer_alive = symbolizer.to_child && symbolizer.from_child;

    local_persist thread_local char exe_path[OS_PATH_MAX];
    bool have_exe_path = os_executable_path(exe_path, sizeof(exe_path)).size > 0;

    fprintf(stderr, "Callstack:\n");
    u32 line_index = 0;
    for(int frame = 1; frame < ips_count; frame += 1){
        u64 address = (u64)ips[frame];
        Dl_info info = {0};
        struct link_map *map = NULL;
        dladdr1(ips[frame], &info, (void**)&map, RTLD_DL_LINKMAP);
        String8 module = info.dli_fname ? string8_skip_last_slash(string8_from_cstring(cast(char*)info.dli_fname)) : str_comp("???");

        local_persist thread_local char funcs[OS_STACKTRACE_MAX_INLINED][OS_STACKTRACE_LINE_SIZE];
        local_persist thread_local char files[OS_STACKTRACE_MAX_INLINED][OS_STACKTRACE_LINE_SIZE];
        u32 entry_count = 0;
        if(symbolizer_alive && info.dli_fname && map){
            char const *module_path = (have_exe_path && map->l_name[0] == 0) ? exe_path : info.dli_fname;
            u64 module_address = address - 1 - (u64)map->l_addr;
            symbolizer_alive = fprintf(symbolizer.to_child, "\"%s\" 0x%" PRIx64 "\n", module_path, module_address) > 0 && fflush(symbolizer.to_child) == 0;
            while(symbolizer_alive){
                u32 slot = DOT_MIN(entry_count, OS_STACKTRACE_MAX_INLINED - 1);
                char *func_line = funcs[slot];
                char *file_line = files[slot];
                if(!fgets(func_line, OS_STACKTRACE_LINE_SIZE, symbolizer.from_child)){
                    symbolizer_alive = false;
                    break;
                }
                if(func_line[0] == '\n'){
                    break;
                }
                if(!fgets(file_line, OS_STACKTRACE_LINE_SIZE, symbolizer.from_child)){
                    symbolizer_alive = false;
                    break;
                }
                if(entry_count < OS_STACKTRACE_MAX_INLINED){
                    entry_count += 1;
                }
            }
        }

        if(entry_count == 0){
            line_index += 1;
            String8 func = info.dli_sname ? string8_from_cstring(cast(char*)info.dli_sname) : str_comp("???");
            os_stacktrace_line_print_(line_index, address, module, func, (String8){0}, 0, false);
            continue;
        }

        for EACH_INDEX(entry, entry_count){
            String8 func = os_symbolizer_line_trim_(funcs[entry]);
            String8 file = string8_skip_last_slash(os_symbolizer_line_trim_(files[entry]));
            if(string8_starts_with(func, str_comp("??"))){
                func = str_comp("???");
            }
            if(string8_starts_with(file, str_comp("??"))){
                file = (String8){0};
            }
            line_index += 1;
            os_stacktrace_line_print_(line_index, address, module, func, file, 0, entry + 1 < entry_count);
        }
    }

    os_symbolizer_kill_(&symbolizer);

    struct timespec no_wait = {0};
    while(sigtimedwait(&pipe_set, NULL, &no_wait) > 0){}
    pthread_sigmask(SIG_SETMASK, &old_set, NULL);
#endif // !DOT_DEBUG
}
