enum
{
    OS_STACKTRACE_MAX_FRAMES    = 256,
    OS_STACKTRACE_MAX_INLINED   = 32,
    OS_STACKTRACE_LINE_SIZE     = 1024,
    OS_STACKTRACE_NAME_SIZE     = 512,
    OS_PATH_MAX                 = 4096,
};

////////////////////////////////////////////////////////////////
//
// Setup
//
internal void
os_set_working_directory_to_project_root(void)
{
    char buffer[OS_PATH_MAX];
    String8 path = os_executable_path(buffer, sizeof(buffer));
    path = string8_chop_last_slash(string8_chop_last_slash(path));
    if(path.size == 0){
        DOT_WARNING("Could not resolve the project root from the executable path");
        return;
    }
    buffer[path.size] = 0;
    if(!os_working_directory_set(path)){
        DOT_WARNING("Could not change working directory to %S", path);
    }
}

internal void
os_init(void)
{
#if DOT_DEBUG
#   if DOT_USE_HANDLERS
    os_install_handlers();
#   endif
#endif
    os_set_working_directory_to_project_root();
}

////////////////////////////////////////////////////////////////
//
// Stacktrace
//
internal void
os_stacktrace_printf_(char const *fmt, ...)
{
    char line[OS_STACKTRACE_LINE_SIZE];
    va_list args;
    va_start(args, fmt);
    dot_vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    fputs(line, stderr);
}

internal void
os_stacktrace_line_print_(u32 index, u64 address, String8 module, String8 func, String8 file, u32 line, bool inlined)
{
    String8 inlined_str = inlined ? str_comp(" (inlined)") : (String8){0};
    if(file.size > 0 && line > 0){
        os_stacktrace_printf_("%3u. [0x%016" PRIx64 "] %S, %S %S:%u%S\n", index, address, module, func, file, line, inlined_str);
    }else if(file.size > 0){
        os_stacktrace_printf_("%3u. [0x%016" PRIx64 "] %S, %S %S%S\n", index, address, module, func, file, inlined_str);
    }else{
        os_stacktrace_printf_("%3u. [0x%016" PRIx64 "] %S, %S%S\n", index, address, module, func, inlined_str);
    }
}

////////////////////////////////////////////////////////////////
//
// Files
//
internal bool
os_file_exists(String8 path)
{
    return(os_file_properties(path).exists);
}

internal bool
os_file_is_newer(String8 a, String8 b)
{
    OS_FileProperties props_a = os_file_properties(a);
    OS_FileProperties props_b = os_file_properties(b);
    if(!props_a.exists){
        return(false);
    }
    if(!props_b.exists){
        return(true);
    }
    return(props_a.modified > props_b.modified);
}

internal String8
os_read_entire_file(Arena *arena, String8 path)
{
    String8 result = {0};
    OS_File file = os_file_open(path, OS_FileAccessFlag_Read);
    if(!os_file_is_open(&file)){
        return(result);
    }
    u64 size = os_file_size(&file);
    if(size > 0){
        u8 *data = PUSH_ARRAY(arena, u8, size + 1);
        if(os_file_read_at(&file, 0, data, size)){
            result = string8(data, size);
        }else{
            DOT_WARNING("Could not read file %S", path);
        }
    }
    os_file_close(&file);
    return(result);
}

internal bool
os_write_entire_file(String8 path, void const *data, u64 size)
{
    OS_File file = os_file_open(path, OS_FileAccessFlag_Write | OS_FileAccessFlag_Create | OS_FileAccessFlag_Truncate);
    if(!os_file_is_open(&file)){
        return(false);
    }
    bool result = os_file_write_at(&file, 0, data, size);
    if(!result){
        DOT_WARNING("Could not write file %S", path);
    }
    os_file_close(&file);
    return(result);
}

////////////////////////////////////////////////////////////////
//
// Time
//
internal u64
os_cpu_timer_read(void)
{
#if DOT_ARCH_X64 || DOT_ARCH_X86
    return(__rdtsc());
#elif DOT_ARCH_ARM64 && (DOT_COMPILER_CLANG || DOT_COMPILER_GCC)
    u64 result;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(result));
    return(result);
#else
#   error "os_cpu_timer_read not implemented for this architecture"
#endif
}

internal u64
os_cpu_timer_estimate_freq(void)
{
    u64 wait_ns   = TO_NSEC(1) / 10;
    u64 cpu_start = os_cpu_timer_read();
    u64 os_start  = os_time_ns();
    u64 os_elapsed = 0;
    while(os_elapsed < wait_ns){
        os_elapsed = os_time_ns() - os_start;
    }
    u64 cpu_elapsed = os_cpu_timer_read() - cpu_start;
    return(cpu_elapsed * TO_NSEC(1) / os_elapsed);
}

////////////////////////////////////////////////////////////////
//
// Windowing
//
internal bool
os_window_system_is_native(OS_WindowSystemKind kind)
{
#if DOT_OS_WINDOWS
    return(kind == OS_WindowSystemKind_Win32);
#elif DOT_OS_LINUX
    return(kind == OS_WindowSystemKind_Xlib || kind == OS_WindowSystemKind_Wayland);
#else
    DOT_UNUSED(kind);
    return(false);
#endif
}

internal String8
os_window_system_name(OS_WindowSystemKind kind)
{
    switch(kind){
    case OS_WindowSystemKind_None:      return(str_comp("None"));
    case OS_WindowSystemKind_Win32:     return(str_comp("Win32"));
    case OS_WindowSystemKind_Xlib:      return(str_comp("Xlib"));
    case OS_WindowSystemKind_Wayland:   return(str_comp("Wayland"));
    case OS_WindowSystemKind_Count:     break;
    }
    return(str_comp("Unknown"));
}

////////////////////////////////////////////////////////////////
//
// Backends
//
#if DOT_OS_POSIX
#   include "base/os/os_posix.c"
#endif
#if DOT_OS_LINUX
#   include "base/os/os_linux.c"
#elif DOT_OS_MAC
#   include "base/os/os_mac.c"
#elif DOT_OS_WINDOWS
#   include "base/os/os_windows.c"
#endif
