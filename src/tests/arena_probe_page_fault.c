#include "base/dot.h"
#include "base/dot.c"
#include "base/platform.h"

#include "base/arena.h"
#include "base/arena.c"

#include <sys/resource.h>

void test_basic_allocation() {
    Arena *arena = Arena_Alloc(.reserve_size = MB(4), .commit_size = KB(64));

    DOT_ASSERT(arena != NULL);
    DOT_ASSERT(arena->reserved >= MB(4));
    DOT_ASSERT(arena->committed >= KB(64));
    DOT_ASSERT(arena->used == sizeof(Arena));

    Arena_Free(arena);
}

void test_alignment() {
    Arena *arena = Arena_Alloc(.reserve_size = MB(2), .commit_size = KB(64));

    u8 *p1 = PushArrayAligned(arena, u8, 32, 8);
    DOT_ASSERT(((uintptr_t)p1 % 8) == 0);

    u8 *p2 = PushArrayAligned(arena, u8, 64, 64);
    DOT_ASSERT(((uintptr_t)p2 % 64) == 0);

    Arena_Free(arena);
}

void test_out_of_bounds() {
    Arena *arena = Arena_Alloc(.reserve_size = MB(1), .commit_size = KB(64));

    PushArray(arena, u8, MB(1) - sizeof(Arena) - 128);

    void *p = PushArrayNoZero(arena, u8, KB(256));
    DOT_ASSERT(p == NULL);

    Arena_Free(arena);
}

void test_reset() {
    Arena *arena = Arena_Alloc(.reserve_size = MB(2), .commit_size = KB(64));

    PushArray(arena, u8, 128);
    DOT_ASSERT(arena->used > sizeof(Arena));

    Arena_Reset(arena);

    DOT_ASSERT(arena->used == 0);
    DOT_ASSERT(arena->reserved >= MB(2));
    DOT_ASSERT(arena->committed >= KB(64));

    Arena_Free(arena);
}

void test_large_pages() {
    Arena *arena = Arena_Alloc(
        .reserve_size = MB(32),
        .commit_size = MB(2),
        .large_pages = true
    );

    DOT_ASSERT(arena->large_pages == true);
    DOT_ASSERT(arena->committed % PLATFORM_LARGE_PAGE_SIZE == 0);
    DOT_ASSERT(arena->reserved % PLATFORM_LARGE_PAGE_SIZE == 0);

    Arena_Free(arena);
}

void test_padding_behavior() {
    Arena *arena = Arena_Alloc(.reserve_size = MB(1), .commit_size = KB(64));

    u64 before = arena->used;
    u8 *p = PushArrayAligned(arena, u8, 16, 64);

    DOT_ASSERT(p != NULL);
    DOT_ASSERT(((uintptr_t)p % 64) == 0);
    DOT_ASSERT(arena->used >= before + 16);

    Arena_Free(arena);
}

void print_hugepage_info(void* ptr) {
    FILE* f = fopen("/proc/self/smaps", "r");
    if (!f) {
        perror("fopen");
        return;
    }

    uintptr_t target = (uintptr_t)ptr;
    char line[256];
    int in_region = 0;

    while (fgets(line, sizeof(line), f)) {
        // Detect region header: "address-start - address-end ..."
        uintptr_t start, end;
        if (sscanf(line, "%lx-%lx", &start, &end) == 2) {
            in_region = (target >= start && target < end);
        }

        if (in_region) {
            if (strstr(line, "KernelPageSize") ||
                strstr(line, "MMUPageSize") ||
                strstr(line, "AnonHugePages")) {
                printf("%s", line);
            }
        }
    }
    fclose(f);
}

// WARN: Even though we tagged as NO_ASAN, running with ASAN still causes
// faulting behaviour to not work properly. Disable before running this
NO_ASAN void test_page_fault_progression(){
    Arena *arena = Arena_Alloc(
        .reserve_size = MB(300),
        .commit_size = MB(200),
        // .commit_expand_size = MB(100),
        // .large_pages = true,
    );

    struct rusage before, after;

    u64 pages = (arena->reserved / PLATFORM_REGULAR_PAGE_SIZE) - 1;

    u64 nofault_start = 0;
    u64 nofault_count = 0;

    for (u64 i = 0; i < pages; ++i) {
        getrusage(RUSAGE_SELF, &before);

        u8 *mem = PushArrayNoZero(arena, u8, PLATFORM_REGULAR_PAGE_SIZE);
        if (mem) mem[0] = 1;

        getrusage(RUSAGE_SELF, &after);

        long delta = after.ru_minflt - before.ru_minflt;

        if (delta == 0) {
            // extend no‑fault run
            if (nofault_count == 0)
                nofault_start = i;
            nofault_count++;
        } else {
            // flush no‑fault run before printing the fault
            if (nofault_count > 0) {
                DOT_PRINT(
                    "No‑fault run: %lu KB (%lu pages) from page %lu to %lu",
                    nofault_count * (PLATFORM_REGULAR_PAGE_SIZE / 1024),
                    nofault_count,
                    nofault_start,
                    nofault_start + nofault_count - 1
                );
                nofault_count = 0;
            }

            DOT_PRINT(
                "FAULT: %ld minor faults at page %lu (offset %M)\n",
                delta,
                i,
                i * PLATFORM_REGULAR_PAGE_SIZE
            );
        }
    }

    // flush trailing no‑fault run
    if (nofault_count > 0) {
        DOT_PRINT(
            "No‑fault run: %lu KB (%lu pages) from page %lu to %lu\n",
            nofault_count * (PLATFORM_REGULAR_PAGE_SIZE / 1024),
            nofault_count,
            nofault_start,
            nofault_start + nofault_count - 1
        );
    }
    // print_hugepage_info(arena->base);
    printf("PID = %d — press ENTER to continue...\n", getpid());
    getchar();
    Arena_Free(arena);
}

// TODO: Move page faulting behaviour to OS files and generalize
int main(){
    test_page_fault_progression();
    // Arena* arena = Arena_Alloc(.reserve_size = MB(20), .large_pages = false, .commit_expand_size = MB(10));
    // struct rusage before;
    // struct rusage after;
    // long last_faults = 0;
    // u64 pages = (arena->reserved / PLATFORM_REGULAR_PAGE_SIZE)-1; // force round down like this for now
    // for(u64 i = 0; i < pages; ++i){
    //     getrusage(RUSAGE_SELF, &before);
    //     u8* mem = PushArrayNoZero(arena, u8, PLATFORM_REGULAR_PAGE_SIZE);
    //     if(mem){
    //         mem[0] = 1;   // trigger page fault if page not resident
    //     }
    //     getrusage(RUSAGE_SELF, &after);
    //     long delta = after.ru_minflt - before.ru_minflt;
    //     if (delta > 0 && delta != last_faults) {
    //         DOT_PRINT( "Minor faults: %ld at %M (page index %lu)\n",
    //             delta,
    //             i * PLATFORM_REGULAR_PAGE_SIZE,
    //             i
    //         );
    //         last_faults = delta;
    //     }
    // }
}
