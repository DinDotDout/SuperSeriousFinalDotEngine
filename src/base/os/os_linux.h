#include <linux/perf_event.h>
#include <errno.h>
#include <x86intrin.h>
#include <sys/time.h>
#include <sys/mman.h>

internal u64
platform_os_get_timer_freq(){
    return 1000000;
}

internal u64
platform_os_read_timer(){
    struct timeval value;
    gettimeofday(&value, 0);
    u64 result = platform_os_get_timer_freq()*(u64)value.tv_sec + (u64)value.tv_usec;
    return result;
}

////////////////////////////////////////////////////////////////
//
// Platform Memory
//
internal inline void*
os_reserve(usize size){
    void* result = mmap(NULL, size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if(DOT_UNLIKELY(result == MAP_FAILED)){
        result = NULL;
    }
    return result;
}

// Large pages in user space only work through THP, so this does the same as os_reserve
internal inline void*
os_reserve_large(usize size){
    void* result = mmap(NULL, size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if(DOT_UNLIKELY(result == MAP_FAILED)){
        result = NULL;
    }
    return result;
}

internal inline b8
os_commit(void *ptr, u64 size){
    if(DOT_UNLIKELY(mprotect(ptr, size, PROT_READ|PROT_WRITE) != 0)){
        DOT_ERROR("Failed to commit memory (errno=%d): %s\n", errno, strerror(errno));
        return false;
    }
    madvise(ptr, size, MADV_POPULATE_WRITE);
    return true;
}

// To maximize THP taking effect we must pass in aligned to 2M pages
internal inline b8
os_commit_large(void *ptr, u64 size){
    if(DOT_UNLIKELY(mprotect(ptr, size, PROT_READ|PROT_WRITE) != 0)){
        DOT_ERROR("Failed to commit memory (errno=%d): %s\n", errno, strerror(errno));
        return false;
    }
    madvise(ptr, size, MADV_HUGEPAGE);
    madvise(ptr, size, MADV_POPULATE_WRITE);
    return true;
}

internal inline
void os_release(void *ptr, u64 size){
    munmap(ptr, size);
}
