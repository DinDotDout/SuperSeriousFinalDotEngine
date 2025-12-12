#include <intrin.h>
#include <windows.h>

internal u64 Platform_OSGetTimerFreq(){
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return freq.QuadPart;
}

internal u64 Platform_OSReadTimer(){
    LARGE_INTEGER value;
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

////////////////////////////////////////////////////////////////
//
// Platform Memory
//
internal inline void* OS_Reserve(usize size){
    DOT_TODO("OS windows");
    // void* result = VirtualAlloc(); // reserve
    // return result;
    return NULL;
}

// Me must pre-reserve large pages on windows which cannot be done
// in user space so this will work as regular pages
// either way large pages
internal inline void* OS_ReserveLarge(usize size){
    DOT_TODO("OS windows");
    // void* result = VirtualAlloc(); // reserve
    // return result;
    return NULL;
}

internal inline b8 OS_Commit(void *ptr, u64 size){
    DOT_TODO("OS windows");
    // b8 result = VirtualAlloc(); // commit
    return true;
}

// See OS_ReserveLarge
internal inline b8 OS_CommitLarge(void *ptr, u64 size){
    DOT_TODO("OS windows");
    // b8 result = VirtualAlloc(); // commit
    return result;
}

internal inline void OS_Release(void *ptr, u64 size){
    DOT_TODO("OS windows");
}
