#include <intrin.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
// #pragma comment(lib, "DbgHelp.lib") // ????

internal u64
platform_os_get_timer_freq()
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return freq.QuadPart;
}

internal u64
platform_os_read_timer()
{
    LARGE_INTEGER value;
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

internal void
os_print_stacktrace()
{
#if DOT_DEBUG
    thread_local local_persist void* buffer[4096];
    USHORT frames = CaptureStackBackTrace(0, DOT_ARRAY_COUNT(buffer), buffer, NULL);

    HANDLE process = GetCurrentProcess();
    SymInitialize(process, NULL, TRUE);

    SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256, 1);
    symbol->MaxNameLen = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    IMAGEHLP_LINE64 line = {0};
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

    fprintf(stderr, "Callstack:\n");
    for (USHORT i = 0; i < frames; i++) {
        DWORD64 addr = (DWORD64)buffer[i];
        DWORD displacement = 0;
        if (SymFromAddr(process, addr, 0, symbol)) {
            if (SymGetLineFromAddr64(process, addr, &displacement, &line)) {
                fprintf(stderr, "%u. [0x%016llX] %s, %s:%lu\n", i+1, symbol->Address, symbol->Name, line.FileName, line.LineNumber);
            } else {
                fprintf(stderr, "%u. [0x%016llX] %s\n", i+1, symbol->Address, symbol->Name);
            }
        } else {
            fprintf(stderr, "%u. [0x%016llX] ???\n", i+1, addr);
        }
    }

    free(symbol);
#endif // !DOT_DEBUG
}

internal LONG WINAPI
os_exception_handler(EXCEPTION_POINTERS *exception_info)
{
    local_persist volatile u32 first = 0;
    if(ins_atomic_u32_eval_cond_assign(&first, 1, 0) != 0){
        for(;;){
            Sleep(INFINITE);
        }
    }
    fprintf(stderr, "A fatal exception was received: 0x%08lX. The process is terminating.\n", exception_info->ExceptionRecord->ExceptionCode);
    os_print_stacktrace();
    _exit(1);
}

internal void
os_install_handlers()
{
    SetUnhandledExceptionFilter(os_exception_handler);
}

internal void
os_set_working_directory_to_project_root()
{
    char path[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
    if(len == 0 || len == MAX_PATH){
        DOT_WARNING("Could not resolve executable path (error=%lu)", GetLastError());
        return;
    }
    for(i32 strip = 0; strip < 2; strip++){
        char *slash = strrchr(path, '\\');
        char *fwd   = strrchr(path, '/');
        if(fwd > slash){
            slash = fwd;
        }
        if(!slash){
            return;
        }
        *slash = 0;
    }
    if(!SetCurrentDirectoryA(path)){
        DOT_WARNING("Could not change working directory to %s (error=%lu)", path, GetLastError());
    }
}

internal void
os_init()
{
    os_install_handlers();
    os_set_working_directory_to_project_root();
}

////////////////////////////////////////////////////////////////
//
// Process
//
internal FILE*
os_popen(String8 cmd, char const *mode)
{
    return _popen(cmd.cstr, mode);
}

internal i32
os_pclose(FILE *stream)
{
    return _pclose(stream);
}

////////////////////////////////////////////////////////////////
//
// Dynamic Library
//
internal void*
os_library_open(String8 path)
{
    void *library = (void*)LoadLibraryA(path.cstr);
    if(!library){
        DOT_WARNING("Failed to load library %S: error %u", path, (u32)GetLastError());
    }
    return library;
}

internal void*
os_library_symbol(void *library, char const *name)
{
    return (void*)GetProcAddress((HMODULE)library, name);
}

internal void
os_library_close(void *library)
{
    FreeLibrary((HMODULE)library);
}

////////////////////////////////////////////////////////////////
//
// Filesystem
//
internal b32
os_directory_create_if_not_exists(String8 path)
{
    if(CreateDirectoryA(path.cstr, NULL)){
        return true;
    }
    if(GetLastError() == ERROR_ALREADY_EXISTS){
        return true;
    }
    DOT_WARNING("Failed to create directory %S (error=%lu)", path, GetLastError());
    return false;
}

////////////////////////////////////////////////////////////////
//
// Platform Memory
//
internal inline void*
os_reserve(u64 size)
{
    void* result = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
    return result;
}

// Large pages require SeLockMemoryPrivilege which is not typically
// available in user space, so this falls back to regular reserve.
internal inline void*
os_reserve_large(usize size)
{
    void* result = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
    return result;
}

internal inline b32
os_commit(void *ptr, u64 size)
{
    void* result = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
    if(DOT_UNLIKELY(result == NULL)){
        DOT_ERROR("Failed to commit memory (error=%lu)\n", GetLastError());
    }
    return true;
}

// See os_reserve_large
internal inline b32
os_commit_large(void *ptr, u64 size)
{
    void* result = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
    if(DOT_UNLIKELY(result == NULL)){
        DOT_ERROR("Failed to commit memory (error=%lu)\n", GetLastError());
    }
    return true;
}

internal inline void
os_release(void *ptr, u64 size)
{
    DOT_UNUSED(size);
    VirtualFree(ptr, 0, MEM_RELEASE);
}

DOT_STATIC_ASSERT(sizeof(SRWLOCK) <= sizeof(Mutex));

internal void
mutex_init(Mutex *mutex)
{
    InitializeSRWLock(cast(SRWLOCK*)mutex->h);
}

internal void
mutex_release(Mutex *mutex)
{
    (void)mutex;
}

internal void
mutex_lock(Mutex *mutex)
{
    AcquireSRWLockExclusive(cast(SRWLOCK*)mutex->h);
}

internal void
mutex_unlock(Mutex *mutex)
{
    ReleaseSRWLockExclusive(cast(SRWLOCK*)mutex->h);
}
