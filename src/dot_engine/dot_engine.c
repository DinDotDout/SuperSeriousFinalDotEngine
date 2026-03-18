#include "dot_engine.h"
// #include "renderer/nk_dot_window.h"
internal void
dot_engine_init(DOT_Engine *engine)
{
    engine->permanent_arena = ARENA_ALLOC(.reserve_size = DOT_ENGINE_CONFIG.engine_memory_size, .name = "DOT_Engine Arena");
    threadctx_init(DOT_ENGINE_CONFIG.thread_options, 0); // NOTE: Should make entry points per thread
    plugins_init();
    dot_window_init(&engine->window);
    renderer_init(engine->permanent_arena, &engine->renderer, &engine->window, DOT_ENGINE_CONFIG.renderer_config);
    // nk_dot_init(&engine->renderer, &engine->window);

    engine->game = PUSH_STRUCT(engine->permanent_arena, DOT_Game);
    u8 *permanent_memory = PUSH_SIZE(engine->permanent_arena, DOT_ENGINE_CONFIG.game_config->permanent_memory_size);
    u8 *transient_memory = PUSH_SIZE(engine->permanent_arena, DOT_ENGINE_CONFIG.game_config->transient_memory_size);
    dot_game_init(
        engine->game,
        &engine->renderer,
        permanent_memory, DOT_ENGINE_CONFIG.game_config->permanent_memory_size,
        transient_memory, DOT_ENGINE_CONFIG.game_config->transient_memory_size);
}

internal void
dot_engine_run(DOT_Engine *engine)
{
    // struct nk_context *ctx = &g_nk_dot.ctx;
    // struct nk_colorf bg = {
    //     .r = 0.10f,
    //     .g = 0.18f,
    //     .b = 0.24f,
    //     .a = 1.0f,
    // };
    while(!dot_window_should_close(&engine->window)){
        dot_window_poll_events(&engine->window);
        // nk_dot_new_frame(&engine->window);
        // if (nk_begin(ctx, "Demo", nk_rect(50, 50, 230, 250),
        //         NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
        //         NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE)){
        //     enum { EASY, HARD };
        //     static int op = EASY;
        //     static int property = 20;
        //     nk_layout_row_static(ctx, 30, 80, 1);
        //     if (nk_button_label(ctx, "button"))
        //         fprintf(stdout, "button pressed\n");
        //
        //     nk_layout_row_dynamic(ctx, 30, 2);
        //     if (nk_option_label(ctx, "easy", op == EASY))
        //         op = EASY;
        //     if (nk_option_label(ctx, "hard", op == HARD))
        //         op = HARD;
        //
        //     nk_layout_row_dynamic(ctx, 25, 1);
        //     nk_property_int(ctx, "Compression:", 0, &property, 100, 10, 1);
        //
        //     nk_layout_row_dynamic(ctx, 20, 1);
        //     nk_label(ctx, "background:", NK_TEXT_LEFT);
        //     nk_layout_row_dynamic(ctx, 25, 1);
        //     if (nk_combo_begin_color(ctx, nk_rgb_cf(bg),
        //             nk_vec2(nk_widget_width(ctx), 400))) {
        //         nk_layout_row_dynamic(ctx, 120, 1);
        //         bg = nk_color_picker(ctx, bg, NK_RGBA);
        //         nk_layout_row_dynamic(ctx, 25, 1);
        //         bg.r = nk_propertyf(ctx, "#R:", 0, bg.r, 1.0f, 0.01f, 0.005f);
        //         bg.g = nk_propertyf(ctx, "#G:", 0, bg.g, 1.0f, 0.01f, 0.005f);
        //         bg.b = nk_propertyf(ctx, "#B:", 0, bg.b, 1.0f, 0.01f, 0.005f);
        //         bg.a = nk_propertyf(ctx, "#A:", 0, bg.a, 1.0f, 0.01f, 0.005f);
        //         nk_combo_end(ctx);
        //     }
        // }
        // nk_end(ctx);

        renderer_begin_frame(&engine->renderer);
        dot_game_run(engine->game);
        // nk_dot_render(&engine->renderer);
        renderer_end_frame(&engine->renderer);
    }
}

internal void
dot_engine_shutdown(DOT_Engine *engine)
{
    dot_game_shutdown(engine->game);
    // nk_dot_shutdown(&engine->renderer);
    renderer_shutdown(&engine->renderer);
    dot_window_shutdown(&engine->window);
    plugins_shutdown();
    threadctx_shutdown();
    ARENA_FREE(engine->permanent_arena);
}

int main() {
    DOT_Engine engine = {0};
    dot_engine_init(&engine);
    dot_engine_run(&engine);
    dot_engine_shutdown(&engine);
    return 0;
}
