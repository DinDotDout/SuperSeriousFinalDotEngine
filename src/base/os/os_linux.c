#include <linux/perf_event.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <execinfo.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dlfcn.h> // Needs _GNU_SOURCE
#include <unistd.h>
#include <pthread.h>

internal u64
platform_os_get_timer_freq()
{
    return 1000000;
}

internal u64
platform_os_read_timer()
{
    struct timeval value;
    gettimeofday(&value, 0);
    u64 result = platform_os_get_timer_freq()*(u64)value.tv_sec + (u64)value.tv_usec;
    return result;
}

// Symbolizing a stacktrace used to popen() a brand new `llvm-symbolizer` process
// PER FRAME: that's a fork+exec+dynamic-link+re-parse-of-DWARF-info for every
// single line of the callstack, which is why this was so slow (and only gets
// worse the deeper the stack). llvm-symbolizer supports a persistent/batch mode:
// fed "<module> <address>" on stdin, it prints "<func>\n<file:line>\n\n" per
// query and keeps its parsed debug info cached for the life of the process. So
// instead we spawn ONE llvm-symbolizer for the whole stacktrace and round-trip
// each frame through its stdin/stdout, which turns N process spawns into 1.
//
// This intentionally spawns+tears down its own private pipes/child per call
// rather than keeping one alive globally across calls: no shared handle means
// no shared state to race on once this is called from more than one thread
// (e.g. two threads faulting at once) -- each caller gets its own independent
// child. The existing `first` gate in os_signal_handler already serializes the
// crash path; this just keeps that property true for any other caller too.
internal void
os_print_stacktrace()
{
#if DOT_DEBUG
    local_persist void *ips[4096];
    int ips_count = backtrace(ips, DOT_ARRAY_COUNT(ips));

    fprintf(stderr, "Callstack:\n");

    int in_pipe[2], out_pipe[2];
    b32 have_pipes = pipe(in_pipe) == 0 && pipe(out_pipe) == 0;
    pid_t child = -1;
    if(have_pipes){
        child = fork();
        if(child == 0){
            dup2(in_pipe[0], STDIN_FILENO);
            dup2(out_pipe[1], STDOUT_FILENO);
            close(in_pipe[0]); close(in_pipe[1]);
            close(out_pipe[0]); close(out_pipe[1]);
            // Everything else we (the engine) had open -- GPU device fds, the
            // Vulkan pipeline cache, sockets, our own stdio pipes, whatever --
            // is still sitting in this child's fd table past this point, since
            // fork() duplicates the whole table and nothing here sets
            // FD_CLOEXEC on it. A stray duplicate of one of OUR pipes surviving
            // inside this unrelated child is enough to stop whoever reads that
            // pipe from ever seeing EOF, which is a real deadlock, not a
            // theoretical one: it wedged the engine's main thread on a stalled
            // write() the first time this ran for real. fd 0/1 are our two
            // pipes (just dup2'd above) and 2 is stderr, which we want the
            // child to keep so its own errors are visible; drop everything else.
            closefrom(3000);
            // --no-debuginfod: without this, any frame lacking local debug info
            // (libc, libvulkan, libX11, ...) makes llvm-symbolizer block on a
            // network fetch from the distro's debuginfod server. That network
            // stall dwarfed even the old one-process-per-frame cost (a single
            // unresolved frame could take on the order of a minute), so a crash
            // handler must never let it happen.
            execlp("llvm-symbolizer", "llvm-symbolizer", "--relative-address", "-f", "--no-debuginfod", (char*)NULL);
            _exit(127); // execlp only returns on failure
        }
        close(in_pipe[0]);
        close(out_pipe[1]);
        if(child < 0){
            close(in_pipe[1]);
            close(out_pipe[0]);
        }
    }

    FILE *to_child   = (have_pipes && child > 0) ? fdopen(in_pipe[1], "w") : NULL;
    FILE *from_child = (have_pipes && child > 0) ? fdopen(out_pipe[0], "r") : NULL;

    for EACH_INDEX(i, cast(u32)ips_count){
        Dl_info info = {0};
        dladdr(ips[i], &info);

        b32 resolved = false;
        char func_name[256], file_name[256];
        if(to_child && from_child && info.dli_fname){
            fprintf(to_child, "%s 0x%lx\n", info.dli_fname, (unsigned long)ips[i] - (unsigned long)info.dli_fbase);
            fflush(to_child);
            if(fgets(func_name, sizeof(func_name), from_child) && fgets(file_name, sizeof(file_name), from_child)){
                char blank_sep[8];
                fgets(blank_sep, sizeof(blank_sep), from_child); // consume the "\n" separating replies
                resolved = true;
            }
        }

        if(resolved){
            String8 func = string8_from_cstring(func_name);
            if(func.size > 0) func.size -= 1;
            String8 module = string8_skip_last_slash(string8_from_cstring(cast(char*)info.dli_fname));
            String8 file   = string8_skip_last_slash(string8_from_cstring_capped(file_name, file_name + sizeof(file_name)));
            if(file.size > 0) file.size -= 1;

            b32 no_func = string8_equal(func, str_comp("??"));
            b32 no_file = string8_equal(file, str_comp("??"));
            if(no_func) { func = (String8){0}; }
            if(no_file) { file = (String8){0}; }

            fprintf(stderr, "%ld. [0x%016lx] %.*s%s%.*s %.*s\n", i+1, (unsigned long)ips[i], (int)module.size, module.str, (!no_func || !no_file) ? ", " : "", (int)func.size, func.str, (int)file.size, file.str);
        }else{
            fprintf(stderr, "%ld. [0x%016lx] %s\n", i+1, cast(unsigned long)ips[i], info.dli_fname);
        }
    }

    if(to_child)   fclose(to_child);   else if(have_pipes && child > 0) close(in_pipe[1]);
    if(from_child) fclose(from_child); else if(have_pipes && child > 0) close(out_pipe[0]);
    if(child > 0){
        int status;
        waitpid(child, &status, 0);
    }
#endif // !DOT_DEBUG
}

internal void
os_signal_handler(int sig, siginfo_t *_info, void *_arg)
{
    DOT_UNUSED(_info, _arg);
    local_persist volatile u32 first = 0;
    if(ins_atomic_u32_eval_cond_assign(&first, 1, 0) != 0){
        for(;;){
            sleep(UINT32_MAX);
        }
    }
    fprintf(stderr, "A fatal signal was received: %s (%d). The process is terminating.\n", strsignal(sig), sig);
    os_print_stacktrace();
    _exit(1);
}

internal void
os_install_handlers()
{
    struct sigaction handler = { .sa_sigaction = os_signal_handler, .sa_flags = SA_SIGINFO, };
    sigfillset(&handler.sa_mask);
    sigaction(SIGILL, &handler, NULL);
    sigaction(SIGTRAP, &handler, NULL);
    sigaction(SIGABRT, &handler, NULL);
    sigaction(SIGFPE, &handler, NULL);
    sigaction(SIGBUS, &handler, NULL);
    sigaction(SIGSEGV, &handler, NULL);
    sigaction(SIGQUIT, &handler, NULL);
}

internal void
os_set_working_directory_to_project_root()
{
    char path[4096];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if(len <= 0){
        DOT_WARNING("Could not resolve executable path: %s", strerror(errno));
        return;
    }
    path[len] = 0;
    for(i32 strip = 0; strip < 2; strip++){
        char *slash = strrchr(path, '/');
        if(!slash || slash == path){
            return;
        }
        *slash = 0;
    }
    if(chdir(path) != 0){
        DOT_WARNING("Could not change working directory to %s: %s", path, strerror(errno));
    }
}

internal void
os_init()
{
#if DOT_USE_HANDLERS
    os_install_handlers();
#endif // !DOT_USE_HANDLERS
    os_set_working_directory_to_project_root();
}

////////////////////////////////////////////////////////////////
//
// Process
//
internal FILE*
os_popen(String8 cmd, char const *mode)
{
    return popen(cmd.cstr, mode);
}

internal i32
os_pclose(FILE *stream)
{
    i32 status = pclose(stream);
    if(status == -1){
        return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

////////////////////////////////////////////////////////////////
//
// Dynamic Library
//
internal void*
os_library_open(String8 path)
{
    void *library = dlopen(path.cstr, RTLD_NOW | RTLD_LOCAL);
    if(!library){
        DOT_WARNING("Failed to load library %S: %s", path, dlerror());
    }
    return library;
}

internal void*
os_library_symbol(void *library, char const *name)
{
    return dlsym(library, name);
}

internal void
os_library_close(void *library)
{
    dlclose(library);
}

////////////////////////////////////////////////////////////////
//
// Filesystem
//
internal b32
os_directory_create_if_not_exists(String8 path)
{
    if(mkdir(path.cstr, 0755) == 0){
        return true;
    }
    if(errno == EEXIST){
        return true;
    }
    DOT_WARNING("Failed to create directory %S: %s", path, strerror(errno));
    return false;
}

////////////////////////////////////////////////////////////////
//
// Platform Memory
//
internal inline void *
os_reserve(u64 size)
{
    void* result = mmap(NULL, size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if(DOT_UNLIKELY(result == MAP_FAILED)){
        result = NULL;
    }
    return result;
}

// Large pages in user space only work through THP, so this does the same as os_reserve
internal inline void*
os_reserve_large(usize size)
{
    void* result = mmap(NULL, size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if(DOT_UNLIKELY(result == MAP_FAILED)){
        result = NULL;
    }
    return result;
}

internal inline b32
os_commit(void *ptr, u64 size)
{
    if(DOT_UNLIKELY(mprotect(ptr, size, PROT_READ|PROT_WRITE) != 0)){
        DOT_ERROR("Failed to commit memory (errno=%d): %s\n", errno, strerror(errno));
        return false;
    }
    madvise(ptr, size, MADV_POPULATE_WRITE);
    return true;
}

// To maximize THP taking effect we must pass in aligned to 2M pages
internal inline b32
os_commit_large(void *ptr, u64 size)
{
    if(DOT_UNLIKELY(mprotect(ptr, size, PROT_READ|PROT_WRITE) != 0)){
        DOT_ERROR("Failed to commit memory (errno=%d): %s\n", errno, strerror(errno));
        return false;
    }
    madvise(ptr, size, MADV_HUGEPAGE);
    madvise(ptr, size, MADV_POPULATE_WRITE);
    return true;
}

internal inline
void os_release(void *ptr, u64 size)
{
    munmap(ptr, size);
}

DOT_STATIC_ASSERT(sizeof(pthread_mutex_t) <= sizeof(Mutex));

internal void
mutex_init(Mutex *mutex)
{
    pthread_mutex_init(cast(pthread_mutex_t*)mutex->h, NULL);
}

internal void
mutex_release(Mutex *mutex)
{
    pthread_mutex_destroy(cast(pthread_mutex_t*)mutex->h);
}

internal void
mutex_lock(Mutex *mutex)
{
    pthread_mutex_lock(cast(pthread_mutex_t*)mutex->h);
}

internal void
mutex_unlock(Mutex *mutex)
{
    pthread_mutex_unlock(cast(pthread_mutex_t*)mutex->h);
}
