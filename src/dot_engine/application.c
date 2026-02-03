#include "dot_engine/application_config.h"

internal void
application_init(Application* app){
    ApplicationConfig* app_config = application_config_get();
    app->permanent_arena = ARENA_ALLOC(.reserve_size = app_config->app_memory_size, .name = "Application Arena");
    // NOTE: Should make entry points per thread
    threadctx_init(&app_config->thread_options, 0);
    plugins_init();
    dot_window_init(&app->window);
    renderer_init(app->permanent_arena, &app->renderer, &app->window, &app_config->renderer_config);
}

internal void
application_run(Application* app){
    (void) app;
    // u8 running = true;
    // while (running && !dot_window_shouldclose(&app->window)){
    // }
}

internal void
application_shutdown(Application* app){
    renderer_shutdown(&app->renderer);
    dot_window_destroy(&app->window);
    plugins_end();
    // NOTE: we do this to keep asan happy tho he os will reclaim the memory
    // either way we could disable cleanup on final builds to speed up shutdown
    threadctx_destroy();
    arena_free(app->permanent_arena);
}
