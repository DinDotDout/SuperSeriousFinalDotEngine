// typedef DOT_ENUM(u8, FileModeKind){
//     Platform_FileModeKind_R,
//     Platform_FileModeKind_W,
//     Platform_FileModeKind_RW,
//     Platform_FileModeKind_A,
// };
//
// internal String8
// platform_get_file_mode_from_kind(FileModeKind kind)
// {
//     switch(kind){
//     case Platform_FileModeKind_R: return String8Lit("r");
//     case Platform_FileModeKind_W: return String8Lit("w");
//     case Platform_FileModeKind_RW: return String8Lit("w");
//     case Platform_FileModeKind_A: return String8Lit("a");
//     }
// }

internal bool platform_file_exists(String8 file_path)
{
#if _WIN32
    return GetFileAttributesA(file_path) != INVALID_FILE_ATTRIBUTES;
#else
    return access(file_path.cstr, F_OK) == 0;
#endif
}

internal String8
platform_read_entire_file(Arena *arena, String8 path)
{
    String8 file_buffer = {0};
    FILE *f = fopen(path.cstr, "rb");
    if(f){
        fseek(f, 0, SEEK_END);
        file_buffer.size = ftell(f);
        if(file_buffer.size > 0){
            fseek(f, 0, SEEK_SET);
            file_buffer.str = PUSH_ARRAY(arena, u8, file_buffer.size);
            fread(file_buffer.str, 1, file_buffer.size, f);
        }
        fclose(f);
    }else{
        DOT_WARNING("Could not open file %S %s", path, strerror(errno)); // Retrieve error code
    }
    return file_buffer;
}

b32 platform_file_is_newer(String8 a, String8 b)
{
#ifdef _WIN32
    struct _stat sa, sb;
    if (_stat(a, &sa) != 0) return false;
    if (_stat(b, &sb) != 0) return true;
#else
    struct stat sa, sb;
    if (stat(a.cstr, &sa) != 0) return false;
    if (stat(b.cstr, &sb) != 0) return true;
#endif

    return sa.st_mtime > sb.st_mtime;
}

u64 platform_get_time_ns(void)
{
    const u64 scale_factor = TO_NSEC(1);
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (u64)((counter.QuadPart * scale_factor) / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * scale_factor + (u64)ts.tv_nsec;
#endif
}

////////////////////////////////////////////////////////////////
//
// Metrics
//
internal u64
platform_cpu_read_timer(void)
{
    return __rdtsc(); // NOTE: Only works on x86, update to support ARM
}

internal u64
platform_cpu_estimate_freq()
{
    u64 msec_to_wait = 100;
    u64 os_freq = platform_os_get_timer_freq();

    u64 cpu_start = platform_cpu_read_timer();
    u64 os_start = platform_os_read_timer();
    u64 os_end = 0;
    u64 os_elapsed = 0;
    u64 wait_time_msec = msec_to_wait * os_freq / 1000;
    while(os_elapsed < wait_time_msec){
        os_end = platform_os_read_timer();
        os_elapsed = os_end - os_start;
    }
    u64 cpu_end = platform_cpu_read_timer();
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
