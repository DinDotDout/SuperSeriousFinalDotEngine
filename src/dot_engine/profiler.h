#ifndef DOT_PROFILER_H
#define DOT_PROFILER_H

#if DOT_ENABLED(TRACY_ENABLE)
#   include "tracy/TracyC.h"
    #define DOT_PROFILE_BLOCK(label) DEFER_LOOP( \
        TracyCZoneCtx DOT_CONCAT(tracy_zone_, __LINE__) = ___tracy_emit_zone_begin_alloc( \
            ___tracy_alloc_srcloc_name(__LINE__, __FILE__, sizeof(__FILE__) - 1, __func__, sizeof(__func__) - 1, (label), strlen(label), 0), 1), \
        ___tracy_emit_zone_end(DOT_CONCAT(tracy_zone_, __LINE__)))
    #define DOT_PROFILE_FRAME_MARK() ___tracy_emit_frame_mark(0)
#else
    #define DOT_PROFILE_BLOCK(label) DEFER_LOOP((void)0, (void)0)
    #define DOT_PROFILE_FRAME_MARK() ((void)0)
#endif

#endif // !DOT_PROFILER_H
