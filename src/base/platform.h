#if defined(_WIN32)
#define DOT_OS_WIN32
#else
#define DOT_OS_POSIX
#endif


#if defined(DOT_OS_WIN32)
#include <intrin.h>
#include <windows.h>
static u64 OS_GetTimerFreq(){
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return freq.QuadPart;
}

static u64 OS_ReadTimer(){
    LARGE_INTEGER value;
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}
#elif defined(DOT_OS_POSIX)
#include <x86intrin.h>
#include <sys/time.h>

static u64 Platform_OSGetTimerFreq(){
    return 1000000;
}

static u64 Platform_OSReadTimer(){
    struct timeval value;
    gettimeofday(&value, 0);
    u64 result = Platform_OSGetTimerFreq()*(u64)value.tv_sec + (u64)value.tv_usec;
    return result;
}
#endif

static inline u64 Platform_ReadCPUTimer(void){
	return __rdtsc(); // NOTE: Only works on x86, update to support ARM
}

u64 OS_EstimateCpuFreq(){
    u64 msec_to_wait = 100;
    u64 os_freq = Platform_OSGetTimerFreq();

    u64 cpu_start = Platform_ReadCPUTimer();
    u64 os_start = Platform_OSReadTimer();
    u64 os_end = 0;
    u64 os_elapsed = 0;
    u64 wait_time_msec = msec_to_wait * os_freq / 1000;
    while(os_elapsed < wait_time_msec){
        os_end = Platform_OSReadTimer();
        os_elapsed = os_end - os_start;
    }
    u64 cpu_end = Platform_ReadCPUTimer();
    u64 cpu_elapsed = cpu_end - cpu_start;
    u64 cpu_freq = 0;
    if(os_elapsed){
        cpu_freq = os_freq * cpu_elapsed / os_elapsed;
    }

    // printf("os elapsed: %lu\n", os_elapsed);
    // printf("cpu elapsed: %lu\n", cpu_elapsed);
    // printf("cpu freq: %lu\n", cpu_freq);
    return cpu_freq;
}

