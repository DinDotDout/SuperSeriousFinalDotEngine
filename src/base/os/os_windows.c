#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>

typedef struct OS_StacktraceSymbol
{
    SYMBOL_INFO info;
    char        name[OS_STACKTRACE_NAME_SIZE];
}OS_StacktraceSymbol;

global SRWLOCK  os_stacktrace_lock = SRWLOCK_INIT;
global bool     os_stacktrace_symbols_initialized;

////////////////////////////////////////////////////////////////
//
// Setup
//
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
os_install_handlers(void)
{
    SetUnhandledExceptionFilter(os_exception_handler);
}

internal String8
os_executable_path(char *buffer, u64 capacity)
{
    u32 len = GetModuleFileNameA(NULL, buffer, cast(u32)capacity);
    if(len == 0 || len == capacity){
        DOT_WARNING("Could not resolve executable path (error=%lu)", GetLastError());
        return((String8){0});
    }
    return(string8(cast(u8*)buffer, len));
}

internal bool
os_working_directory_set(String8 path)
{
    return(SetCurrentDirectoryA(path.cstr) != 0);
}

////////////////////////////////////////////////////////////////
//
// Stacktrace
//
internal void
os_stacktrace_entry_print_(HANDLE process, u32 index, u64 address, u64 lookup, String8 module, u32 inline_context, bool inlined)
{
    OS_StacktraceSymbol symbol = {0};
    symbol.info.SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol.info.MaxNameLen   = OS_STACKTRACE_NAME_SIZE;

    IMAGEHLP_LINE64 line_info = {0};
    line_info.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

    u64   symbol_displacement = 0;
    DWORD line_displacement   = 0;
    bool have_symbol = false;
    bool have_line   = false;
    if(inlined){
        have_symbol = SymFromInlineContext(process, lookup, inline_context, &symbol_displacement, &symbol.info);
        have_line   = SymGetLineFromInlineContext(process, lookup, inline_context, 0, &line_displacement, &line_info);
    }else{
        have_symbol = SymFromAddr(process, lookup, &symbol_displacement, &symbol.info);
        have_line   = SymGetLineFromAddr64(process, lookup, &line_displacement, &line_info);
    }

    String8 func = have_symbol ? string8(cast(u8*)symbol.info.Name, symbol.info.NameLen) : str_comp("???");
    String8 file = have_line ? string8_skip_last_slash(string8_from_cstring(line_info.FileName)) : (String8){0};
    u32     line = have_line ? cast(u32)line_info.LineNumber : 0;
    os_stacktrace_line_print_(index, address, module, func, file, line, inlined);
}

internal void
os_print_stacktrace(void)
{
    void *frames[OS_STACKTRACE_MAX_FRAMES];
    u16 frame_count = CaptureStackBackTrace(1, OS_STACKTRACE_MAX_FRAMES, frames, NULL);

    AcquireSRWLockExclusive(&os_stacktrace_lock);
    HANDLE process = GetCurrentProcess();
    if(!os_stacktrace_symbols_initialized){
        SymSetOptions(SymGetOptions() | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
        SymInitialize(process, NULL, TRUE);
        os_stacktrace_symbols_initialized = true;
    }else{
        SymRefreshModuleList(process);
    }

    fprintf(stderr, "Callstack:\n");
    u32 line_index = 0;
    for(u16 frame = 0; frame < frame_count; frame += 1){
        u64 address = cast(u64)frames[frame];
        u64 lookup  = address - 1;

        char module_path[MAX_PATH] = {0};
        HMODULE module_handle = NULL;
        if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, cast(LPCSTR)frames[frame], &module_handle)){
            GetModuleFileNameA(module_handle, module_path, MAX_PATH);
        }
        String8 module = module_path[0] ? string8_skip_last_slash(string8_from_cstring(module_path)) : str_comp("???");

        u32   inline_count   = SymAddrIncludeInlineTrace(process, lookup);
        DWORD inline_context = 0;
        DWORD frame_index    = 0;
        if(inline_count > 0 && SymQueryInlineTrace(process, lookup, 0, lookup, lookup, &inline_context, &frame_index)){
            for(u32 inline_index = 0; inline_index < inline_count; inline_index += 1){
                line_index += 1;
                os_stacktrace_entry_print_(process, line_index, address, lookup, module, inline_context + inline_index, true);
            }
        }
        line_index += 1;
        os_stacktrace_entry_print_(process, line_index, address, lookup, module, 0, false);
    }
    ReleaseSRWLockExclusive(&os_stacktrace_lock);
}

////////////////////////////////////////////////////////////////
//
// Memory
//
internal void*
os_reserve(u64 size)
{
    return(VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS));
}

// Large pages require SeLockMemoryPrivilege which is not typically
// available in user space, so this falls back to regular reserve.
internal void*
os_reserve_large(u64 size)
{
    return(os_reserve(size));
}

internal bool
os_commit(void *ptr, u64 size)
{
    void *result = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
    if(DOT_UNLIKELY(result == NULL)){
        DOT_ERROR("Failed to commit memory (error=%lu)\n", GetLastError());
        return(false);
    }
    return(true);
}

// See os_reserve_large
internal bool
os_commit_large(void *ptr, u64 size)
{
    return(os_commit(ptr, size));
}

internal void
os_release(void *ptr, u64 size)
{
    DOT_UNUSED(size);
    VirtualFree(ptr, 0, MEM_RELEASE);
}

////////////////////////////////////////////////////////////////
//
// Files
//
internal OS_File
os_file_open(String8 path, OS_FileAccessFlags flags)
{
    OS_File result = {0};
    u32 access = 0;
    if(flags & OS_FileAccessFlag_Read){
        access |= GENERIC_READ;
    }
    if(flags & OS_FileAccessFlag_Write){
        access |= GENERIC_WRITE;
    }
    bool create   = (flags & OS_FileAccessFlag_Create) != 0;
    bool truncate = (flags & OS_FileAccessFlag_Truncate) != 0;
    u32 disposition = OPEN_EXISTING;
    if(create && truncate){
        disposition = CREATE_ALWAYS;
    }else if(create){
        disposition = OPEN_ALWAYS;
    }else if(truncate){
        disposition = TRUNCATE_EXISTING;
    }

    HANDLE handle = CreateFileA(path.cstr, access, FILE_SHARE_READ, NULL, disposition, FILE_ATTRIBUTE_NORMAL, NULL);
    if(handle == INVALID_HANDLE_VALUE){
        DOT_WARNING("Could not open file %S (error=%lu)", path, GetLastError());
        return(result);
    }
    result.h[0] = cast(u64)handle;
    return(result);
}

internal void
os_file_close(OS_File *file)
{
    if(os_file_is_open(file)){
        CloseHandle(cast(HANDLE)file->h[0]);
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
    LARGE_INTEGER size = {0};
    if(!GetFileSizeEx(cast(HANDLE)file->h[0], &size)){
        return(0);
    }
    return(cast(u64)size.QuadPart);
}

internal bool
os_file_read_at(OS_File *file, u64 offset, void *dst, u64 size)
{
    DOT_ASSERT(os_file_is_open(file));
    HANDLE handle = cast(HANDLE)file->h[0];
    u8 *cursor = dst;
    u64 remaining = size;
    while(remaining > 0){
        u32 chunk = cast(u32)DOT_MIN(remaining, cast(u64)UINT32_MAX);
        OVERLAPPED overlapped = {0};
        overlapped.Offset     = cast(u32)offset;
        overlapped.OffsetHigh = cast(u32)(offset >> 32);
        DWORD read_size = 0;
        if(!ReadFile(handle, cursor, chunk, &read_size, &overlapped) || read_size == 0){
            return(false);
        }
        cursor    += read_size;
        offset    += read_size;
        remaining -= read_size;
    }
    return(true);
}

internal bool
os_file_write_at(OS_File *file, u64 offset, void const *src, u64 size)
{
    DOT_ASSERT(os_file_is_open(file));
    HANDLE handle = cast(HANDLE)file->h[0];
    u8 const *cursor = src;
    u64 remaining = size;
    while(remaining > 0){
        u32 chunk = cast(u32)DOT_MIN(remaining, cast(u64)UINT32_MAX);
        OVERLAPPED overlapped = {0};
        overlapped.Offset     = cast(u32)offset;
        overlapped.OffsetHigh = cast(u32)(offset >> 32);
        DWORD write_size = 0;
        if(!WriteFile(handle, cursor, chunk, &write_size, &overlapped) || write_size == 0){
            return(false);
        }
        cursor    += write_size;
        offset    += write_size;
        remaining -= write_size;
    }
    return(true);
}

internal OS_FileProperties
os_file_properties(String8 path)
{
    OS_FileProperties result = {0};
    WIN32_FILE_ATTRIBUTE_DATA data = {0};
    if(GetFileAttributesExA(path.cstr, GetFileExInfoStandard, &data)){
        result.exists   = true;
        result.size     = (cast(u64)data.nFileSizeHigh << 32) | data.nFileSizeLow;
        result.modified = (cast(u64)data.ftLastWriteTime.dwHighDateTime << 32) | data.ftLastWriteTime.dwLowDateTime;
    }
    return(result);
}

internal bool
os_file_delete(String8 path)
{
    return(DeleteFileA(path.cstr) != 0);
}

internal bool
os_file_move(String8 src, String8 dst)
{
    if(!MoveFileExA(src.cstr, dst.cstr, MOVEFILE_REPLACE_EXISTING)){
        DOT_WARNING("Could not move %S to %S (error=%lu)", src, dst, GetLastError());
        return(false);
    }
    return(true);
}

internal bool
os_directory_create_if_not_exists(String8 path)
{
    if(CreateDirectoryA(path.cstr, NULL)){
        return(true);
    }
    if(GetLastError() == ERROR_ALREADY_EXISTS){
        return(true);
    }
    DOT_WARNING("Failed to create directory %S (error=%lu)", path, GetLastError());
    return(false);
}

////////////////////////////////////////////////////////////////
//
// Time
//
internal u64
os_time_ns(void)
{
    local_persist u64 freq = 0;
    if(freq == 0){
        LARGE_INTEGER value;
        QueryPerformanceFrequency(&value);
        freq = cast(u64)value.QuadPart;
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    u64 ticks = cast(u64)counter.QuadPart;
    return((ticks / freq) * TO_NSEC(1) + (ticks % freq) * TO_NSEC(1) / freq);
}

////////////////////////////////////////////////////////////////
//
// Process
//
internal FILE*
os_popen(String8 cmd, OS_FileAccessFlags flags)
{
    DOT_ASSERT(flags == OS_FileAccessFlag_Read || flags == OS_FileAccessFlag_Write, "popen pipes are read only or write only");
    return(_popen(cmd.cstr, flags == OS_FileAccessFlag_Write ? "w" : "r"));
}

internal i32
os_pclose(FILE *stream)
{
    return(_pclose(stream));
}

////////////////////////////////////////////////////////////////
//
// Dynamic library
//
internal void*
os_library_open(String8 path)
{
    void *library = cast(void*)LoadLibraryA(path.cstr);
    if(!library){
        DOT_WARNING("Failed to load library %S: error %u", path, cast(u32)GetLastError());
    }
    return(library);
}

internal void*
os_library_symbol(void *library, String8 name)
{
    return(cast(void*)GetProcAddress(cast(HMODULE)library, name.cstr));
}

internal void
os_library_close(void *library)
{
    FreeLibrary(cast(HMODULE)library);
}

internal bool
os_library_is_loaded(String8 name)
{
    return(GetModuleHandleA(name.cstr) != NULL);
}

////////////////////////////////////////////////////////////////
//
// Mutex
//
DOT_STATIC_ASSERT(sizeof(SRWLOCK) <= sizeof(Mutex));

internal void
mutex_init(Mutex *mutex)
{
    InitializeSRWLock(cast(SRWLOCK*)mutex->h);
}

internal void
mutex_release(Mutex *mutex)
{
    DOT_UNUSED(mutex);
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
