#include <errno.h>
#include <signal.h>
#include <time.h>
#include <execinfo.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dlfcn.h> 
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/sysctl.h> // Necesario en macOS para consultar rutas de procesos

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
            
            // CAMBIO PARA MAC: macOS no tiene 'closefrom()'. 
            // Recorremos los descriptores de archivos para cerrar los sobrantes de forma segura.
            int fd_limit = (int)sysconf(_SC_OPEN_MAX);
            if (fd_limit < 0) fd_limit = 1024;
            for (int i = 3; i < fd_limit; i++) {
                close(i);
            }

            execlp("llvm-symbolizer", "llvm-symbolizer", "--relative-address", "-f", "--no-debuginfod", (char*)NULL);
            _exit(127); 
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
                fgets(blank_sep, sizeof(blank_sep), from_child); 
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
#endif 
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
    
    // CAMBIO PARA MAC: macOS no tiene '/proc/self/exe'. 
    // Usamos proc_pidpath (o sysctl con KERN_PROCARGS2) para obtener la ruta del binario en Apple.
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
    size_t cb = sizeof(path);
    if (sysctl(mib, 4, path, &cb, NULL, 0) != 0) {
        DOT_WARNING("Could not resolve executable path via sysctl: %s", strerror(errno));
        return;
    }

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
#endif 
    os_set_working_directory_to_project_root();
}

// CAMBIO/ADICIÓN PARA MAC: Implementación de os_commit_large
// En macOS, las páginas grandes de memoria se gestionan de forma nativa a través de la bandera VM_FLAGS_SUPERPAGE.
// Al contrario de Linux, no es necesario hacer un commit posterior: se reservan y comprometen directamente.
internal void
os_commit_large(void *ptr, u64 size)
{
    // macOS procesa las páginas grandes (Superpages) directamente a nivel de kernel durante la reserva de memoria virtual.
    // Si la memoria ya está mapeada con mmap estándar, se puede sugerir el uso de páginas grandes mediante madvise.
#if defined(MADV_THP_TO_LARGE)
    madvise(ptr, (size_t)size, MADV_THP_TO_LARGE);
#else
    // Si tu mmap original no usó superpáginas, este fallback reduce la probabilidad de paginación a disco.
    madvise(ptr, (size_t)size, MADV_WILLNEED);
#endif
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
        DOT_WARNING("Failed to load library %s: %s", path.cstr, dlerror());
    }
    return library;
}
