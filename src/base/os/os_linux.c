#include <linux/perf_event.h>
#include <errno.h>
#include <signal.h>
#include <x86intrin.h>
#include <time.h>
#include <execinfo.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>

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

internal void
os_print_stacktrace()
{
    local_persist void *ips[4096];
    int ips_count = backtrace(ips, DOT_ARRAY_COUNT(ips));

    fprintf(stderr, "Callstack:\n");
    for EACH_INDEX(i, cast(u32)ips_count){
        Dl_info info = {0};
        dladdr(ips[i], &info);

        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "llvm-symbolizer --relative-address -f -e %s %lu", info.dli_fname, (unsigned long)ips[i] - (unsigned long)info.dli_fbase);
        FILE *f = popen(cmd, "r");
        if(f){
            char func_name[256], file_name[256];
            if(fgets(func_name, sizeof(func_name), f) && fgets(file_name, sizeof(file_name), f)){
                String8 func = string8_from_cstring(func_name);
                if(func.size > 0) func.size -= 1;
                String8 module = string8_skip_last_slash(string8_from_cstring(cast(char*)info.dli_fname));
                String8 file   = string8_skip_last_slash(string8_cstring_capped(file_name, file_name + sizeof(file_name)));
                if(file.size > 0) file.size -= 1;

                b32 no_func = string8_equal(func, String8Lit("??"));
                b32 no_file = string8_equal(file, String8Lit("??"));
                if(no_func) { func = (String8){0}; }
                if(no_file) { file = (String8){0}; }

                fprintf(stderr, "%ld. [0x%016lx] %.*s%s%.*s %.*s\n", i+1, (unsigned long)ips[i], (int)module.size, module.str, (!no_func || !no_file) ? ", " : "", (int)func.size, func.str, (int)file.size, file.str);
            }
            pclose(f);
        }else{
            fprintf(stderr, "%ld. [0x%016lx] %s\n", i+1, cast(unsigned long)ips[i], info.dli_fname);
        }
    }
}

internal void
os_signal_handler(int sig, siginfo_t *_info, void *_arg)
{
    DOT_UNUSED(_info); DOT_UNUSED(_arg);
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
os_install_handlers(){
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
