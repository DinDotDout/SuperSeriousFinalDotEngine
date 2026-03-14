#include <intrin.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "DbgHelp.lib") // ????

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
    thread_local static void* buffer[4096];
    USHORT frames = CaptureStackBackTrace(0, 64, stack, NULL);
    for (USHORT i = 0; i < frames; i++){
        printf("%p\n", stack[i]);
    }
}


void os_print_stacktrace() {
    thread_local static void* buffer[4096];
    USHORT frames = CaptureStackBackTrace(0, ARRAY_COUNT(buffer), buffer, NULL);

    HANDLE process = GetCurrentProcess();
    SymInitialize(process, NULL, TRUE);

    SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256, 1);
    symbol->MaxNameLen = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    for (USHORT i = 0; i < frames; i++) {
        DWORD64 addr = (DWORD64)stack[i];
        if (SymFromAddr(process, addr, 0, symbol)) {
            printf("%u: %s - 0x%0llX\n", i, symbol->Name, symbol->Address);
        }
    }

    free(symbol);
}

////////////////////////////////////////////////////////////////
//
// Platform Memory
//
internal inline void*
os_reserve(usize size)
{
    DOT_TODO("OS windows");
    // void* result = VirtualAlloc(); // reserve
    // return result;
    return NULL;
}

// Me must pre-reserve large pages on windows which cannot be done
// in user space so this will work as regular pages
// either way large pages
internal inline void*
os_reserve_large(usize size)
{
    DOT_TODO("OS windows");
    // void* result = VirtualAlloc(); // reserve
    // return result;
    return NULL;
}

internal inline b32
os_commit(void *ptr, u64 size)
{
    DOT_TODO("OS windows");
    // b32 result = VirtualAlloc(); // commit
    return true;
}

// See os_reserve_large
internal inline b32
os_commit_large(void *ptr, u64 size)
{
    DOT_TODO("OS windows");
    // b32 result = VirtualAlloc(); // commit
    return result;
}

internal inline void
os_release(void *ptr, u64 size)
{
    DOT_TODO("OS windows");
}
