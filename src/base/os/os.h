#ifndef DOT_OS_H
#define DOT_OS_H

#if DOT_OS_WINDOWS
// Reserve granularity is 64KB even though commit granularity is 4KB
enum
{
    OS_REGULAR_PAGE_SIZE    = DOT_KB(64),
    OS_LARGE_PAGE_SIZE      = DOT_MB(2),
};
#elif DOT_OS_MAC && DOT_ARCH_ARM64
enum
{
    OS_REGULAR_PAGE_SIZE    = DOT_KB(16),
    OS_LARGE_PAGE_SIZE      = DOT_MB(2),
};
#else
enum
{
    OS_REGULAR_PAGE_SIZE    = DOT_KB(4),
    OS_LARGE_PAGE_SIZE      = DOT_MB(2),
};
#endif

typedef u32 OS_FileAccessFlags;
enum
{
    OS_FileAccessFlag_Read      = (1 << 0),
    OS_FileAccessFlag_Write     = (1 << 1),
    OS_FileAccessFlag_Create    = (1 << 2), // create the file if it doesn't exist
    OS_FileAccessFlag_Truncate  = (1 << 3), // drop the existing contents
};

// Opaque storage for an fd / HANDLE, implemented in os_posix.c / os_windows.c. Zero when not open
typedef struct OS_File
{
    u64 h[1];
}OS_File;

typedef struct OS_FileProperties
{
    u64  size;
    u64  modified; // OS specific units, only comparable with each other
    bool exists;
}OS_FileProperties;

typedef enum OS_WindowSystemKind
{
    OS_WindowSystemKind_None,
    OS_WindowSystemKind_Win32,
    OS_WindowSystemKind_Xlib,
    OS_WindowSystemKind_Wayland,
    OS_WindowSystemKind_Count,
}OS_WindowSystemKind;

typedef struct OS_NativeWindow
{
    OS_WindowSystemKind kind;
    void *display;
    void *window;
}OS_NativeWindow;

////////////////////////////////////////////////////////////////
//
// Setup
//
internal void       os_init(void);
internal void       os_install_handlers(void);
internal void       os_print_stacktrace(void);
internal String8    os_executable_path(char *buffer, u64 capacity);
internal bool       os_working_directory_set(String8 path);

////////////////////////////////////////////////////////////////
//
// Memory
//
internal void*  os_reserve(u64 size);
internal void*  os_reserve_large(u64 size);
internal bool   os_commit(void *ptr, u64 size);
internal bool   os_commit_large(void *ptr, u64 size);
internal void   os_release(void *ptr, u64 size);

////////////////////////////////////////////////////////////////
//
// Files
//
internal OS_File            os_file_open(String8 path, OS_FileAccessFlags flags);
internal void               os_file_close(OS_File *file);
internal bool               os_file_is_open(OS_File *file);
internal u64                os_file_size(OS_File *file);
internal bool               os_file_read_at(OS_File *file, u64 offset, void *dst, u64 size);
internal bool               os_file_write_at(OS_File *file, u64 offset, void const *src, u64 size);
internal OS_FileProperties  os_file_properties(String8 path);
internal bool               os_file_exists(String8 path);
internal bool               os_file_is_newer(String8 a, String8 b);
internal bool               os_file_delete(String8 path);
internal bool               os_file_move(String8 src, String8 dst);
internal String8            os_read_entire_file(Arena *arena, String8 path);
internal bool               os_write_entire_file(String8 path, void const *data, u64 size);
internal bool               os_directory_create_if_not_exists(String8 path);

////////////////////////////////////////////////////////////////
//
// Time
//
internal u64 os_time_ns(void);
internal u64 os_cpu_timer_read(void);
internal u64 os_cpu_timer_estimate_freq(void);

////////////////////////////////////////////////////////////////
//
// Process
//
internal FILE*  os_popen(String8 cmd, OS_FileAccessFlags flags);
internal i32    os_pclose(FILE *stream);

////////////////////////////////////////////////////////////////
//
// Dynamic library
//
internal void*  os_library_open(String8 path);
internal void*  os_library_symbol(void *library, String8 name);
internal void   os_library_close(void *library);
internal bool   os_library_is_loaded(String8 name);

////////////////////////////////////////////////////////////////
//
// Windowing
//
internal bool       os_window_system_is_native(OS_WindowSystemKind kind);
internal String8    os_window_system_name(OS_WindowSystemKind kind);

#endif // !DOT_OS_H
