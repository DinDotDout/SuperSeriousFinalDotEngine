typedef struct DOT_ProfileAnchor{
    u64 tsc_elapsed_exclusive;
    u64 tsc_elapsed_inclusive;
    u64 hit_count;
    char const *label;
}DOT_ProfileAnchor;

global thread_local DOT_ProfileAnchor profiler_anchors[4096];
global thread_local  u32 profile_block_parent;

typedef struct DOT_ProfileBlock{
    char const *label;
    u64 old_tsc_elapsed_inclusive;
    u64 start_tsc;
    u32 parent_index;
    u32 anchor_index;
}DOT_ProfileBlock;

typedef struct DOT_Profiler{
    u64 start_tsc;
    u64 end_tsc;
}DOT_Profiler;

global thread_local DOT_Profiler profiler;

internal inline void DOT_ProfilerBegin(){
    profiler.start_tsc = Platform_ReadCPUTimer();
}

internal inline void DOT_ProfilerEnd(){
    profiler.end_tsc = Platform_ReadCPUTimer();
}

static void DOT_PrintTimeElapsed(u64 total_tsc_elapsed, DOT_ProfileAnchor *anchor){
    f64 percent = 100.0 * ((f64)anchor->tsc_elapsed_exclusive / (f64)total_tsc_elapsed);
    printf("  %s[%lu]: %lu (%.2f%%", anchor->label, anchor->hit_count, anchor->tsc_elapsed_exclusive, percent);
    if(anchor->tsc_elapsed_inclusive != anchor->tsc_elapsed_exclusive)
    {
        f64 percent_with_children = 100.0 * ((f64)anchor->tsc_elapsed_inclusive / (f64)total_tsc_elapsed);
        printf(", %.2f%% cycles with children", percent_with_children);
    }
    printf(")\n");
}

internal inline void DOT_ProfilerPrint(){
    u64 cpu_frequency = OS_EstimateCpuFreq();
    u64 total_cpu_elapsed = profiler.end_tsc - profiler.start_tsc;
    if(cpu_frequency){
        printf("\nTotal time: %0.4fms (CPU freq %lu)\n", 1000.0 * (f64)total_cpu_elapsed / (f64)cpu_frequency, cpu_frequency);
    }
    for(u32 idx = 0; idx < ArrayCount(profiler_anchors); ++idx){
        DOT_ProfileAnchor *anchor = profiler_anchors + idx;
        if(anchor->tsc_elapsed_inclusive) {
            DOT_PrintTimeElapsed(total_cpu_elapsed, anchor);
        }
    }
}

internal inline DOT_ProfileBlock DOT_ProfileBlock_Begin(char const *label, u32 anchor_index){
    DOT_ProfileBlock profile_block = {0};
    profile_block.parent_index = profile_block_parent;
    profile_block.anchor_index = anchor_index;
    profile_block.label = label;

    DOT_ProfileAnchor *anchor = profiler_anchors + anchor_index;
    profile_block.old_tsc_elapsed_inclusive = anchor->tsc_elapsed_inclusive;

    profile_block_parent = anchor_index;
    profile_block.start_tsc = Platform_ReadCPUTimer();
    return profile_block;
}

internal inline void DOT_ProfileBlock_End(DOT_ProfileBlock* profile_block){
    u64 elapsed = Platform_ReadCPUTimer() - profile_block->start_tsc;
    profile_block_parent = profile_block->parent_index;

    DOT_ProfileAnchor *parent = profiler_anchors + profile_block->parent_index;
    DOT_ProfileAnchor *anchor = profiler_anchors + profile_block->anchor_index;

    parent->tsc_elapsed_exclusive -= elapsed;
    anchor->tsc_elapsed_exclusive += elapsed;
    anchor->tsc_elapsed_inclusive = profile_block->old_tsc_elapsed_inclusive + elapsed;
    ++anchor->hit_count;
    anchor->label = profile_block->label;
}
#ifndef NDEBUG
#define DOT_PROFILING_ENABLE
#endif

#ifdef DOT_PROFILING_ENABLE
#define DOT_PROFILE_BLOCK(label) DeferLoop( \
            DOT_ProfileBlock DOT_CONCAT(profile_block_, __LINE__) =  DOT_ProfileBlock_Begin((label), __COUNTER__ + 1), \
            DOT_ProfileBlock_End(&DOT_CONCAT(profile_block_,  __LINE__)))

#define ProfilerEndOfCompilationUnit DOT_STATIC_ASSERT(__COUNTER__ < ArrayCount(profiler_anchors)) // Does not work for now
#else
#define DOT_PROFILE_BLOCK(label) ((void)0)
#endif
