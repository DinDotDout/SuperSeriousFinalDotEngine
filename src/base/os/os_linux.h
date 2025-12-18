#include <linux/perf_event.h>
#include <errno.h>
#include <x86intrin.h>
#include <sys/time.h>
#include <sys/mman.h>

internal u64 Platform_OSGetTimerFreq(){
    return 1000000;
}

internal u64 Platform_OSReadTimer(){
    struct timeval value;
    gettimeofday(&value, 0);
    u64 result = Platform_OSGetTimerFreq()*(u64)value.tv_sec + (u64)value.tv_usec;
    return result;
}

////////////////////////////////////////////////////////////////
//
// Platform Memory
//
internal inline void* OS_Reserve(usize size){
    void* result = mmap(NULL, size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if(DOT_Unlikely(result == MAP_FAILED)){
        result = NULL;
    }
    return result;
}

// Large pages in user space only work through THP, so this does the same as OS_Reserve
internal inline void* OS_ReserveLarge(usize size){
    void* result = mmap(NULL, size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if(DOT_Unlikely(result == MAP_FAILED)){
        result = NULL;
    }
    return result;
}

internal inline b8 OS_Commit(void *ptr, u64 size){
    if(DOT_Unlikely(mprotect(ptr, size, PROT_READ|PROT_WRITE) != 0)){
        DOT_ERROR("Failed to commit memory (errno=%d): %s\n", errno, strerror(errno));
        return false;
    }
    madvise(ptr, size, MADV_POPULATE_WRITE);
    return true;
}

// To maximize THP taking effect we must pass in aligned to 2M pages
internal inline b8 OS_CommitLarge(void *ptr, u64 size){
    if(DOT_Unlikely(mprotect(ptr, size, PROT_READ|PROT_WRITE) != 0)){
        DOT_ERROR("Failed to commit memory (errno=%d): %s\n", errno, strerror(errno));
        return false;
    }
    madvise(ptr, size, MADV_HUGEPAGE);
    madvise(ptr, size, MADV_POPULATE_WRITE);
    return true;
}

internal inline void OS_Release(void *ptr, u64 size){
    munmap(ptr, size);
}
