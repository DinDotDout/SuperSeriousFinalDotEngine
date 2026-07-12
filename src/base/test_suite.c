internal inline void
dot_test_suite_register(const DOT_Test *test)
{
    DOT_PRINT("registering suite %s", test->name);
    ARRAY_PUSH(g_dot_test_suites, *test);
}

internal inline b8
dot_test_suites_print()
{
    DOT_PRINT("Total suites: ", g_dot_test_suites.count);
    b8 succeeded = true;
    for(u32 i = 0; i < g_dot_test_suites.count; ++i){
        const DOT_Test *test = &ARRAY_GET(g_dot_test_suites, i);
        DOT_PRINT("Suite name: %s at %s:%d", test->name, test->file, test->line);
    }
    return succeeded;
}

internal inline b8
dot_test_suites_run()
{
    DOT_PRINT("Running %td Tests Suites", g_dot_test_suites.count);
    b8 succeeded = true;
    for(u32 i = 0; i < g_dot_test_suites.count; ++i){
        const DOT_Test *test = &ARRAY_GET(g_dot_test_suites, i);
        DOT_TestResults res = test->test_fn();
        DOT_PRINT("succeeded %u/%u", res.passed, res.total_tests);
        if(res.passed != res.total_tests){
            succeeded = false;
        }
    }
    return succeeded;
}
