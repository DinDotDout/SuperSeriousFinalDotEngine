#ifndef BASE_TESTS_H
#define BASE_TESTS_H

typedef struct DOT_TestResults{
    u32 total_tests;
    u32 passed;
}DOT_TestResults;

typedef struct DOT_Test{
    DOT_TestResults (*test_func)(void);
    const char *name;
    const char *file;
    int line;
}DOT_Test;
DECLARE_SECTION(DOT_Test, DOT_tests);

#define DOT_TEST_BEGIN() \
    DOT_TestResults __res = { .total_tests = 1, .passed = 1 }

#define DOT_TEST_CHECK(test, name, expr) do { \
    test.total_tests++; \
    const char *status = "failed"; \
    if (expr) { \
        status = "passed"; \
        test.passed++; \
    } \
    DOT_PRINT("\tCheck %s %s: %s %i %s", status, name, __FILE__, __LINE__, #expr); \
} while(0)

// #undef DOT_DEBUG
#ifdef DOT_DEBUG
#define REGISTER_TEST(fn) \
    internal inline DOT_TestResults fn(void);\
    SECTION_ITEM(DOT_tests, 0) static const DOT_Test DOT_CONCAT(__DOT_Test_, fn) = (DOT_Test){ \
        .name = #fn, \
        .file = __FILE__, \
        .line = __LINE__, \
        .test_func = fn, \
    }; \
internal inline DOT_TestResults fn()
#else
#define REGISTER_TEST(fn) internal inline DOT_TestResults fn()
#endif // !DOT_DEBUG

internal b8
tests_run(void)
{
    DOT_Test *begin = cast(DOT_Test*) &__start_DOT_tests[0];
    const DOT_Test* end = &__stop_DOT_tests[0];
    DOT_PRINT("Running %td Tests", end - begin);
    b8 succeeded = true;
    for (const DOT_Test* test = begin; test != end; ++test){
        DOT_PRINT_FL(test->file, test->line, "Test: %s", test->name);
        DOT_TestResults res = test->test_func();
        DOT_PRINT("succeeded %u/%u", res.passed, res.total_tests);
        if(res.passed != res.total_tests){
            succeeded = false;
        }
    }
    return succeeded;
}

#endif // !BASE_TESTS_H 
