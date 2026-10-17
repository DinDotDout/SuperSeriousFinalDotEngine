#ifndef BASE_TESTS_H
#define BASE_TESTS_H

enum{ DOT_TEST_SUITE_MAX = 512 };

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

global ARRAY(DOT_Test, DOT_TEST_SUITE_MAX) g_dot_test_suites;

#define DOT_TEST_CHECK(test, name, expr) do { \
    test.total_tests++; \
    String8 status = str_lit("Failed"); \
    if (expr) { \
        status = str_comp("Passed"); \
        test.passed++; \
    } \
    DOT_PRINT("\tUNIT TEST '%s' %S: %s %i %s", name, status, __FILE__, __LINE__, #expr); \
} while(0)

#define DOT_MERGE_TEST_RESULTS(into, from) do{ \
    DOT_TestResults _temp = (from); \
    (into).total_tests += _temp.total_tests; \
    (into).passed      += _temp.passed; \
}while(0)

#define DOT_TEST_SUITE(fn) \
    internal inline DOT_TestResults fn(void); \
    static const DOT_Test DOT_CONCAT(__DOT_Test_, fn) = { \
        .name = #fn, \
        .file = __FILE__, \
        .line = __LINE__, \
        .test_fn = fn, \
    }; \
    DOT_CONSTRUCTOR(dot_test_suite_register##fn) \
    { \
        dot_test_suite_register(&DOT_CONCAT(__DOT_Test_, fn)); \
    } \
    internal inline DOT_TestResults fn()

internal inline void    dot_test_suite_register(const DOT_Test *test);
internal inline b8      dot_test_suites_print();
internal inline b8      dot_test_suites_run();

#endif // !BASE_TESTS_H 
