typedef void (*JobFn)(void *ctx);
typedef struct Job Job;
struct Job{
    JobFn fn;
    void *ctx;
};

static inline void job_run(Job *j) {
    j->fn(j->ctx);
}

static inline Job job_make(Arena *arena, JobFn fn,
    const void *ctx_init,
    usize ctx_size,
    usize ctx_align)
{
    // (void)ctx_align; /* malloc is sufficiently aligned */
        // void *ctx = malloc(ctx_size);
    void *ctx = ARENA_PUSH(arena, ctx_size, ctx_align, false);
    MEMORY_COPY(ctx, ctx_init, ctx_size);
    return (Job){ fn, ctx };
}

#define JOB_DEFINE(fn, T)\
void fn(T args); \
static void tramp_##fn(void *p){ \
    T ctx = (T)p; \
    fn(ctx); \
}

#define JOB_DEFINE_INLINE(fn, T, var_name) \
    JOB_DEFINE(fn, T) \
    void fn(T var_name)

#define JOB_CREATE(a, fn, T, ptr)       job_make(a, tramp_##fn, (ptr), sizeof(T), DOT_ALIGNOF(T))
#define JOB_CREATE_LIT(a, fn, T, ...)   job_make(a, tramp_##fn, &(T){ __VA_ARGS__ }, sizeof(T), DOT_ALIGNOF(T))

typedef struct DOT_TestJobSystem{
    int x;
    int y;
}DOT_TestJobSystem;

void random_name(DOT_TestJobSystem *t)
{
    (void)t;
}

JOB_DEFINE(random_name, DOT_TestJobSystem*);
JOB_DEFINE_INLINE(random_name2, DOT_TestJobSystem*, arg)
{
    (void)arg;
}

DOT_TEST_SUITE(dot_job_system){
    Arena *a = ARENA_CREATE();
    Job job = JOB_CREATE_LIT(a, random_name, DOT_TestJobSystem, .x = 5);
    DOT_TestJobSystem b = {0};
    Job job2 = JOB_CREATE(a, random_name, DOT_TestJobSystem, &b);
    (void)job;
    (void)job2;
    return (DOT_TestResults){0};
}
