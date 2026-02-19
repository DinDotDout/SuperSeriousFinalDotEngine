#include "base/arena.h"
#include "dot_engine/application_config.h"

internal void
application_init(Application* app)
{
    ApplicationConfig* app_config = application_config_get();
    app->permanent_arena = ARENA_ALLOC(.reserve_size = app_config->app_memory_size, .name = "Application Arena");
    // NOTE: Should make entry points per thread
    threadctx_init(&app_config->thread_options, 0);
    plugins_init();
    dot_window_init(&app->window);
    renderer_init(app->permanent_arena, &app->renderer, &app->window, &app_config->renderer_config);

    app->game = PUSH_STRUCT(app->permanent_arena, DOT_Game);
    dot_game_init(app->permanent_arena, app->game, &app->renderer);
}

// void reload_game_dll() {
//     unload_library(game_lib);
//     game_lib = load_library("game.dll");
//
//     g_game_api.run      = get_symbol(game_lib, "dot_game_run");
//     g_game_api.init     = get_symbol(game_lib, "dot_game_init");
//     g_game_api.shutdown = get_symbol(game_lib, "dot_game_shutdown");
// }

internal void
application_run(Application* app)
{
    while(!dot_window_should_close(&app->window)){
        renderer_begin_frame(&app->renderer);
        dot_game_run(app->game);
        // &app->game
        // renderer_clear_background(&app->renderer);
        renderer_end_frame(&app->renderer);
    }
}

internal void
application_shutdown(Application* app)
{
    dot_game_shutdown(app->game);
    renderer_shutdown(&app->renderer);
    dot_window_destroy(&app->window);
    plugins_end();
    // NOTE: we do this to keep asan happy tho he os will reclaim the memory
    // either way we could disable cleanup on final builds to speed up shutdown
    threadctx_destroy();
    arena_free(app->permanent_arena);
}
