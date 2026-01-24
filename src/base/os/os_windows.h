#include <intrin.h>
#include <windows.h>

internal u64
platform_os_get_timer_freq(){
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return freq.QuadPart;
}

internal u64
platform_os_read_timer(){
    LARGE_INTEGER value;
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

////////////////////////////////////////////////////////////////
//
// Platform Memory
//
internal inline void*
os_reserve(usize size){
    DOT_TODO("OS windows");
    // void* result = VirtualAlloc(); // reserve
    // return result;
    return NULL;
}

// Me must pre-reserve large pages on windows which cannot be done
// in user space so this will work as regular pages
// either way large pages
internal inline void*
os_reserve_large(usize size){
    DOT_TODO("OS windows");
    // void* result = VirtualAlloc(); // reserve
    // return result;
    return NULL;
}

internal inline b8
os_commit(void *ptr, u64 size){
    DOT_TODO("OS windows");
    // b8 result = VirtualAlloc(); // commit
    return true;
}

// See os_reserve_large
internal inline b8
os_commit_large(void *ptr, u64 size){
    DOT_TODO("OS windows");
    // b8 result = VirtualAlloc(); // commit
    return result;
}

internal inline void
os_release(void *ptr, u64 size){
    DOT_TODO("OS windows");
}
