#include <errno.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <execinfo.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dlfcn.h> // Needs _GNU_SOURCE
#include <unistd.h>
#include <pthread.h>

////////////////////////////////////////////////////////////////
//
// Setup
//
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
os_install_handlers(void)
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

internal bool
os_working_directory_set(String8 path)
{
    return(chdir(path.cstr) == 0);
}

////////////////////////////////////////////////////////////////
//
// Memory
//
internal void*
os_reserve(u64 size)
{
    void *result = mmap(NULL, size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if(DOT_UNLIKELY(result == MAP_FAILED)){
        result = NULL;
    }
    return(result);
}

// Large pages in user space only work through THP, so this does the same as os_reserve
internal void*
os_reserve_large(u64 size)
{
    return(os_reserve(size));
}

internal void
os_release(void *ptr, u64 size)
{
    munmap(ptr, size);
}

////////////////////////////////////////////////////////////////
//
// Files
//
internal OS_File
os_file_open(String8 path, OS_FileAccessFlags flags)
{
    OS_File result = {0};
    bool read  = (flags & OS_FileAccessFlag_Read) != 0;
    bool write = (flags & OS_FileAccessFlag_Write) != 0;
    int open_flags = O_CLOEXEC;
    if(read && write){
        open_flags |= O_RDWR;
    }else if(write){
        open_flags |= O_WRONLY;
    }else{
        open_flags |= O_RDONLY;
    }
    if(flags & OS_FileAccessFlag_Create){
        open_flags |= O_CREAT;
    }
    if(flags & OS_FileAccessFlag_Truncate){
        open_flags |= O_TRUNC;
    }

    int fd = open(path.cstr, open_flags, 0644);
    if(fd < 0){
        DOT_WARNING("Could not open file %S: %s", path, strerror(errno));
        return(result);
    }
    if(fd == 0){
        int moved_fd = fcntl(fd, F_DUPFD_CLOEXEC, 1);
        close(fd);
        if(moved_fd < 0){
            DOT_WARNING("Could not move file %S off fd 0: %s", path, strerror(errno));
            return(result);
        }
        fd = moved_fd;
    }
    result.h[0] = cast(u64)fd;
    return(result);
}

internal void
os_file_close(OS_File *file)
{
    if(os_file_is_open(file)){
        close(cast(int)file->h[0]);
    }
    MEMORY_ZERO_STRUCT(file);
}

internal bool
os_file_is_open(OS_File *file)
{
    return(file->h[0] != 0);
}

internal u64
os_file_size(OS_File *file)
{
    DOT_ASSERT(os_file_is_open(file));
    struct stat st;
    if(fstat(cast(int)file->h[0], &st) != 0){
        return(0);
    }
    return(cast(u64)st.st_size);
}

internal bool
os_file_read_at(OS_File *file, u64 offset, void *dst, u64 size)
{
    DOT_ASSERT(os_file_is_open(file));
    int fd = cast(int)file->h[0];
    u8 *cursor = dst;
    u64 remaining = size;
    while(remaining > 0){
        i64 read_size = pread(fd, cursor, remaining, offset);
        if(read_size < 0 && errno == EINTR){
            continue;
        }
        if(read_size <= 0){
            return(false);
        }
        cursor    += read_size;
        offset    += cast(u64)read_size;
        remaining -= cast(u64)read_size;
    }
    return(true);
}

internal bool
os_file_write_at(OS_File *file, u64 offset, void const *src, u64 size)
{
    DOT_ASSERT(os_file_is_open(file));
    int fd = cast(int)file->h[0];
    u8 const *cursor = src;
    u64 remaining = size;
    while(remaining > 0){
        i64 write_size = pwrite(fd, cursor, remaining, offset);
        if(write_size < 0 && errno == EINTR){
            continue;
        }
        if(write_size <= 0){
            return(false);
        }
        cursor    += write_size;
        offset    += cast(u64)write_size;
        remaining -= cast(u64)write_size;
    }
    return(true);
}

internal OS_FileProperties
os_file_properties(String8 path)
{
    OS_FileProperties result = {0};
    struct stat st;
    if(stat(path.cstr, &st) == 0){
        result.exists   = true;
        result.size     = cast(u64)st.st_size;
        result.modified = cast(u64)st.st_mtime;
    }
    return(result);
}

internal bool
os_file_delete(String8 path)
{
    return(unlink(path.cstr) == 0);
}

internal bool
os_file_move(String8 src, String8 dst)
{
    if(rename(src.cstr, dst.cstr) != 0){
        DOT_WARNING("Could not move %S to %S: %s", src, dst, strerror(errno));
        return(false);
    }
    return(true);
}

internal bool
os_directory_create_if_not_exists(String8 path)
{
    if(mkdir(path.cstr, 0755) == 0){
        return(true);
    }
    if(errno == EEXIST){
        return(true);
    }
    DOT_WARNING("Failed to create directory %S: %s", path, strerror(errno));
    return(false);
}

////////////////////////////////////////////////////////////////
//
// Time
//
internal u64
os_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return(cast(u64)ts.tv_sec * TO_NSEC(1) + cast(u64)ts.tv_nsec);
}

////////////////////////////////////////////////////////////////
//
// Process
//
internal FILE*
os_popen(String8 cmd, OS_FileAccessFlags flags)
{
    DOT_ASSERT(flags == OS_FileAccessFlag_Read || flags == OS_FileAccessFlag_Write, "popen pipes are read only or write only");
    return(popen(cmd.cstr, flags == OS_FileAccessFlag_Write ? "w" : "r"));
}

internal i32
os_pclose(FILE *stream)
{
    i32 status = pclose(stream);
    if(status == -1){
        return(-1);
    }
    return(WIFEXITED(status) ? WEXITSTATUS(status) : -1);
}

////////////////////////////////////////////////////////////////
//
// Dynamic library
//
internal void*
os_library_open(String8 path)
{
    void *library = dlopen(path.cstr, RTLD_NOW | RTLD_LOCAL);
    if(!library){
        DOT_WARNING("Failed to load library %S: %s", path, dlerror());
    }
    return(library);
}

internal void*
os_library_symbol(void *library, String8 name)
{
    return(dlsym(library, name.cstr));
}

internal void
os_library_close(void *library)
{
    dlclose(library);
}

internal bool
os_library_is_loaded(String8 name)
{
    void *library = dlopen(name.cstr, RTLD_NOW | RTLD_NOLOAD);
    if(library){
        dlclose(library);
    }
    return(library != NULL);
}

////////////////////////////////////////////////////////////////
//
// Mutex
//
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
