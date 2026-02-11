#ifndef DOT_PROFILER_H
#define DOT_PROFILER_H
// NOTE: Make accessible across tus by either adding extern var or accessor method

#ifndef NDEBUG
#define DOT_PROFILING_ENABLE
#endif

typedef struct DOT_ProfileAnchor{
    u64 tsc_elapsed_exclusive;
    u64 tsc_elapsed_inclusive;
    u64 hit_count;
    char const *label;
}DOT_ProfileAnchor;

typedef struct DOT_Profiler{
    u64 start_tsc;
    u64 end_tsc;
	u32 current_profile_block_parent;
	DOT_ProfileAnchor profiler_anchors[4096]; // NOTE: We can eventually just alloc this
}DOT_Profiler;
global DOT_Profiler g_profiler = {0};

typedef struct DOT_ProfileBlock{
    char const *label;
    u64 old_tsc_elapsed_inclusive;
    u64 start_tsc;
    u32 parent_index;
    u32 anchor_index;
}DOT_ProfileBlock;

internal void dot_profiler_init();
internal void dot_profiler_end();
internal inline void dot_print_time_elapsed(u64 total_tsc_elapsed, DOT_ProfileAnchor *anchor);
internal inline void dot_profiler_print();
internal inline DOT_ProfileBlock dot_profile_block_begin(char const *label, u32 anchor_index);
internal inline void dot_profile_block_end(DOT_ProfileBlock* profile_block);

#ifdef DOT_PROFILING_ENABLE
PluginRegister(Profiler, 0,
    .init = dot_profiler_init,
    .end = dot_profiler_end,
);
#endif

#ifdef DOT_PROFILING_ENABLE
// WARN: This will break across TU's. We might be able to tag with a specific section to register
// them all at program start
    #define DOT_PROFILE_BLOCK(label) DEFER_LOOP( \
        DOT_ProfileBlock DOT_CONCAT(profile_block_, __LINE__) =  dot_profile_block_begin((label), __COUNTER__ + 1), \
        dot_profile_block_end(&DOT_CONCAT(profile_block_,  __LINE__)))

// This or something else to test we don't go over max profile anchors
    #define PROFILER_END_OF_COMPILATION_UNIT DOT_STATIC_ASSERT(__COUNTER__ < ARRAY_COUNT(g_profiler.profiler_anchors))
#else
    #define DOT_PROFILE_BLOCK(label) ((void)0)
#endif

#endif // !DOT_PROFILER_H

#ifdef DOT_PROFILER_IMPL
internal void
dot_print_time_elapsed(u64 total_tsc_elapsed, DOT_ProfileAnchor *anchor){
    f64 percent = 100.0 * ((f64)anchor->tsc_elapsed_exclusive / (f64)total_tsc_elapsed);
    printf("  %s[%lu]: %lu (%.2f%%", anchor->label, anchor->hit_count, anchor->tsc_elapsed_exclusive, percent);
    if(anchor->tsc_elapsed_inclusive != anchor->tsc_elapsed_exclusive){
        f64 percent_with_children = 100.0 * ((f64)anchor->tsc_elapsed_inclusive / (f64)total_tsc_elapsed);
        printf(", %.2f%% cycles with children", percent_with_children);
    }
    printf(")\n");
}

internal inline void
dot_profiler_print(){
    u64 cpu_frequency = platform_cpu_estimate_freq();
    u64 total_cpu_elapsed = g_profiler.end_tsc - g_profiler.start_tsc;
    if(cpu_frequency){
        printf("\nTotal time: %0.4fms (CPU freq %lu)\n", 1000.0 * (f64)total_cpu_elapsed / (f64)cpu_frequency, cpu_frequency);
    }
    for(u32 idx = 0; idx < ARRAY_COUNT(g_profiler.profiler_anchors); ++idx){
        DOT_ProfileAnchor *anchor = g_profiler.profiler_anchors + idx;
        if(anchor->tsc_elapsed_inclusive) {
            dot_print_time_elapsed(total_cpu_elapsed, anchor);
        }
    }
}

internal void
dot_profiler_init(){
    g_profiler.start_tsc = platform_cpu_read_timer();
}

internal void
dot_profiler_end(){
    g_profiler.end_tsc = platform_cpu_read_timer();
    dot_profiler_print();
}

internal inline DOT_ProfileBlock
dot_profile_block_begin(char const *label, u32 anchor_index){
    DOT_ProfileBlock profile_block = {
    	.parent_index = g_profiler.current_profile_block_parent,
    	.anchor_index = anchor_index,
    	.label = label,
    };

    DOT_ProfileAnchor *anchor = &g_profiler.profiler_anchors[anchor_index];
    profile_block.old_tsc_elapsed_inclusive = anchor->tsc_elapsed_inclusive;
    g_profiler.current_profile_block_parent = anchor_index;
    profile_block.start_tsc = platform_cpu_read_timer();
    return profile_block;
}

internal inline void
dot_profile_block_end(DOT_ProfileBlock* profile_block){
    u64 elapsed = platform_cpu_read_timer() - profile_block->start_tsc;
    g_profiler.current_profile_block_parent = profile_block->parent_index;

    DOT_ProfileAnchor *parent = &g_profiler.profiler_anchors[profile_block->parent_index];
    DOT_ProfileAnchor *anchor = &g_profiler.profiler_anchors[profile_block->anchor_index];

    parent->tsc_elapsed_exclusive -= elapsed;
    anchor->tsc_elapsed_exclusive += elapsed;
    anchor->tsc_elapsed_inclusive = profile_block->old_tsc_elapsed_inclusive + elapsed;
    ++anchor->hit_count;
    anchor->label = profile_block->label;
}
#endif // !DOT_PROFILER_IMPL
