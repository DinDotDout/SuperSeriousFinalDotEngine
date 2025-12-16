#include "base/dot.h"
#include "base/dot.c"
#include "base/platform.h"

#include "base/arena.h"
#include "base/arena.c"

#include <sys/resource.h>

// TODO: Move page faulting behaviour to OS files and generalize
int main(){
    Arena* arena = Arena_Alloc(.reserve_size = MB(20), .large_pages = false, .expand_commit_size = MB(10));
    struct rusage before;
    struct rusage after;
    long last_faults = 0;
    u64 pages = arena->reserved / REGULAR_PAGE_SIZE;
    for(u64 i = 0; i < pages; ++i){
        getrusage(RUSAGE_SELF, &before);
        u8* mem = PushArrayNoZero(arena, u8, REGULAR_PAGE_SIZE);
        if(mem){
            mem[0] = 1;   // trigger page fault if page not resident
        }
        getrusage(RUSAGE_SELF, &after);
        long delta = after.ru_minflt - before.ru_minflt;
        if (delta > 0 && delta != last_faults) {
            DOT_PRINT( "Minor faults: %ld at %M (page index %lu)\n",
                delta,
                i * REGULAR_PAGE_SIZE,
                i
            );
            last_faults = delta;
        }
    }
}
