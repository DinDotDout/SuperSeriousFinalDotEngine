#ifndef BASE_TESTS_H
#define BASE_TESTS_H

enum
{
    DOT_TEST_SUITE_MAX = 512,
};

typedef struct DOT_TestResults{
    u32 total_tests;
    u32 passed;
}DOT_TestResults;

typedef struct DOT_Test{
    u32 test_fn_count;
    DOT_TestResults (*test_fn)(void);

    char   *name;
    char   *file;
    int     line;
}DOT_Test;

typedef struct DOT_TestSuites DOT_TestSuites;
struct DOT_TestSuites{
    u32 total_suites;
    DOT_Test test_suites[DOT_TEST_SUITE_MAX];
}g_dot_test_suites = {0};

#define DOT_TEST_CHECK(test, name, expr) do { \
    test.total_tests++; \
    const char *status = "failed"; \
    if (expr) { \
        status = "passed"; \
        test.passed++; \
    } \
    DOT_PRINT("\tCheck %s %s: %s %i %s", status, name, __FILE__, __LINE__, #expr); \
} while(0)

#define DOT_MERGE_TEST_RESULTS(into, from) do { \
    (into).total_tests += (from).total_tests; \
    (into).passed      += (from).passed; \
} while(0)

// #if DOT_DEBUG
#define DOT_TEST_SUITE(fn) \
internal inline DOT_TestResults fn(void);\
static const DOT_Test DOT_CONCAT(__DOT_Test_, fn) = (DOT_Test){ \
    .name = #fn, \
    .file = __FILE__, \
    .line = __LINE__, \
    .test_fn = fn, \
}; \
DOT_CONSTRUCTOR(dot_test_suite_register##fn)(void) \
{ \
    dot_test_suite_register(&DOT_CONCAT(__DOT_Test_, fn)); \
} \
internal inline DOT_TestResults fn()
// #else
// #define DOT_TEST_SUITE(fn) internal inline DOT_TestResults fn()
// #endif

internal inline void
dot_test_suite_register(const DOT_Test *test)
{
    DOT_PRINT("registering suite %s", test->name);
    g_dot_test_suites.test_suites[g_dot_test_suites.total_suites] = *test;
    g_dot_test_suites.total_suites += 1;
}


internal inline b8
dot_test_suites_print()
{
    DOT_PRINT("Total suites: ", g_dot_test_suites.total_suites);
    b8 succeeded = true;
    for(u32 i = 0; i < g_dot_test_suites.total_suites; ++i){
        const DOT_Test *test = &g_dot_test_suites.test_suites[i];
        DOT_PRINT("Suite name: %s at %s:%d", test->name, test->file, test->line);
    }
    return succeeded;
}

internal inline b8
dot_test_suites_run()
{
    DOT_PRINT("Running %td Tests Suites", g_dot_test_suites.total_suites);
    b8 succeeded = true;
    for(u32 i = 0; i < g_dot_test_suites.total_suites; ++i){
        const DOT_Test *test = &g_dot_test_suites.test_suites[i];
        DOT_TestResults res = test->test_fn();
        DOT_PRINT("succeeded %u/%u", res.passed, res.total_tests);
        if(res.passed != res.total_tests){
            succeeded = false;
        }
    }
    return succeeded;
}

#endif // !BASE_TESTS_H 
