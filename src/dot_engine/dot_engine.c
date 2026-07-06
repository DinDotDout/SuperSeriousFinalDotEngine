#include "dot_engine.h"
DOT_SETTING_U32("Engine", g_engine_memory_size_b, DOT_MB(512));
DOT_SETTING_U32("ThreadContext", g_per_thread_temp_arena_size_b, DOT_MB(40));
DOT_SETTING_U32("ThreadContext", g_per_thread_temp_arena_count_b, 2);
DOT_SETTING_U32("Game", g_game_permanent_memory_size_b, DOT_MB(20));
DOT_SETTING_U32("Game", g_game_transient_memory_size_b, DOT_KB(32));

internal void
dot_engine_init(DOT_Engine *engine, b32 tests_only)
{
    engine->permanent_arena = ARENA_CREATE(.reserve_size = g_engine_memory_size_b, .name = "DOT_Engine Arena");
    threadctx_init(engine->permanent_arena, g_per_thread_temp_arena_count_b, g_per_thread_temp_arena_size_b, 1);
    if(tests_only){
        return;
    }
    dot_settings_register_all(engine->permanent_arena);
    plugins_init();
    dot_window_init(&engine->window);
    rn_init(engine->permanent_arena, &engine->renderer, &engine->window);
    // nk_dot_init(&engine->renderer, &engine->window);

    engine->game = PUSH_STRUCT(engine->permanent_arena, DOT_Game);
    u8 *permanent_memory = PUSH_SIZE(engine->permanent_arena, g_game_permanent_memory_size_b);
    u8 *transient_memory = PUSH_SIZE(engine->permanent_arena, g_game_transient_memory_size_b);
    dot_game_init(
        engine->game,
        &engine->renderer,
        permanent_memory, g_game_permanent_memory_size_b,
        transient_memory, g_game_transient_memory_size_b);
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

        rn_frame_begin(&engine->renderer);

        dot_game_run(engine->game);
        // nk_dot_render(&engine->renderer);
        rn_frame_end(&engine->renderer);
    }
}

internal void
dot_engine_shutdown(DOT_Engine *engine)
{
    dot_game_shutdown(engine->game);
    // nk_dot_shutdown(&engine->renderer);
    rn_shutdown(&engine->renderer);
    dot_window_shutdown(&engine->window);
    plugins_shutdown();
    threadctx_shutdown();
    ARENA_DESTROY(engine->permanent_arena);
}

// TODO: Make os level entry points call into actual engine code
int
main(int argc, char *argv[])
{
    platform_init();
    Arena *a = ARENA_CREATE();
    ARENA_RESET(a);
    DOT_Engine engine = {0};
    b32 tests_only = false;
    b32 tests_list_only = false;
    b32 any_arg = false;
    if(argc > 1){
        String8 arg1 = string8_from_cstring(argv[1]);
        String8 tests = string8_lit("-tests");
        String8 tests_list = string8_lit("-tests_list");
        tests_only = string8_equal(arg1, tests);
        tests_list_only = string8_equal(arg1, tests_list);
        any_arg = tests_only || tests_list_only;

    }
    dot_engine_init(&engine, any_arg);
    if(tests_only){
        return(dot_test_suites_run());
    }
    if(tests_list_only){
        dot_test_suites_print();
        return(0);
    }
    dot_engine_run(&engine);
    dot_engine_shutdown(&engine);
    return(0);
}
