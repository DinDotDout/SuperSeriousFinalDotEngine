#include "dot_engine.h"

internal void
dot_engine_init(DOT_Engine* engine)
{
    DOT_EngineConfig *engine_config = dot_engine_config_get();
    engine->permanent_arena = ARENA_ALLOC(.reserve_size = engine_config->engine_memory_size, .name = "DOT_Engine Arena");
    // NOTE: Should make entry points per thread
    threadctx_init(&engine_config->thread_options, 0);
    plugins_init();
    dot_window_init(&engine->window);
    renderer_init(engine->permanent_arena, &engine->renderer, &engine->window, &engine_config->renderer_config);

    engine->game = PUSH_STRUCT(engine->permanent_arena, DOT_Game);

    u8 *permanent_memory = PUSH_SIZE(engine->permanent_arena, engine_config->game_config.permanent_memory_size);
    u8 *transient_memory = PUSH_SIZE(engine->permanent_arena, engine_config->game_config.transient_memory_size);
    dot_game_init(
        engine->game,
        &engine->renderer,
        permanent_memory, engine_config->game_config.permanent_memory_size,
        transient_memory, engine_config->game_config.transient_memory_size);
}

internal void
dot_engine_run(DOT_Engine* engine)
{
    while(!dot_window_should_close(&engine->window)){
        renderer_begin_frame(&engine->renderer);
        dot_game_run(engine->game);
        renderer_end_frame(&engine->renderer);
    }
}

internal void
dot_engine_shutdown(DOT_Engine* engine)
{
    dot_game_shutdown(engine->game);
    renderer_shutdown(&engine->renderer);
    dot_window_shutdown(&engine->window);
    plugins_shutdown();
    threadctx_shutdown();
    ARENA_FREE(engine->permanent_arena);
}

int main() {
    DOT_Engine engine;
    dot_engine_init(&engine);
    dot_engine_run(&engine);
    dot_engine_shutdown(&engine);
    return 0;
}
