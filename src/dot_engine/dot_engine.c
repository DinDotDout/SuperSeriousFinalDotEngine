#include "dot_engine.h"

DOT_SETTING_U32("Engine", g_engine_memory_size_b, DOT_MB(512));
DOT_SETTING_U32("ThreadContext", g_per_thread_temp_arena_size_b, DOT_MB(40));
DOT_SETTING_U32("ThreadContext", g_per_thread_temp_arena_count_b, 2);
DOT_SETTING_U32("Game", g_game_permanent_memory_size_b, DOT_MB(20));
DOT_SETTING_U32("Game", g_game_transient_memory_size_b, DOT_KB(32));

typedef ProgramEntryFlag DOT_EngineModeKind;
enum{
    DOT_EngineRunModeKind_Default,
    DOT_EngineRunModeKind_TestsRun,
    DOT_EngineRunModeKind_TestsList,
};

global DOT_EngineModeKind g_engine_mode = DOT_EngineRunModeKind_Default;

internal void
dot_engine_set_mode(DOT_EngineModeKind mode)
{
    g_engine_mode = mode;
}

internal read_only ProgramEntryFlagCtx flag_table[] = {
    { .flag = str_lit("-tests-run"),  .flag_desc = str_lit("Run test suite"),               .set_int = dot_engine_set_mode, .int_value = DOT_EngineRunModeKind_TestsRun  },
    { .flag = str_lit("-tests-list"), .flag_desc = str_lit("Show test suite"),              .set_int = dot_engine_set_mode, .int_value = DOT_EngineRunModeKind_TestsList },
    { .flag = str_lit("-log_warn"),   .flag_desc = str_lit("Only log warnings and errors"), .set_int = dot_set_log_level,   .int_value = DOT_LogLevelKind_Warning        },
    { .flag = str_lit("-log_error"),  .flag_desc = str_lit("Only log errors"),              .set_int = dot_set_log_level,   .int_value = DOT_LogLevelKind_Error          },
};

int
entry_point(int argc, String8 arg_values[])
{
    b32 show_help = program_entry_flags_apply(DOT_ARRAY_COUNT(flag_table), flag_table, argc, arg_values);
    if(show_help){
        return(0);
    }
    b32 headless = g_engine_mode != DOT_EngineRunModeKind_Default;

    DOT_Engine engine = {.engine_name = str_comp("Super Serious Final Dot Engine")};
    dot_engine_init(&engine, headless);

    partial_switch_begin(g_engine_mode){
    case DOT_EngineRunModeKind_TestsRun:  dot_test_suites_run();    return 0;
    case DOT_EngineRunModeKind_TestsList: dot_test_suites_print();  return 0;
    }partial_switch_end;

    dot_engine_run(&engine);
    dot_engine_shutdown(&engine);

    return(0);
}

internal void
dot_engine_init(DOT_Engine *engine, b32 headless)
{
    engine->permanent_arena = ARENA_CREATE(.reserve_size = g_engine_memory_size_b, .name = "DOT_Engine Arena");
    threadctx_init(engine->permanent_arena, g_per_thread_temp_arena_count_b, g_per_thread_temp_arena_size_b, 1);
    rfl_init(engine->permanent_arena);
    rfl_ui_init(engine->permanent_arena);
    dot_settings_register_all(engine->permanent_arena);
    plugins_init();

    if(!headless){
        dot_window_init(&engine->window, engine->engine_name);
        rn_init(engine->permanent_arena, &engine->renderer, dot_window_native_get(&engine->window));
        nk_init_default(&engine->ui_ctx, NULL);
        ui_input_init(&engine->ui_input, &engine->ui_ctx);
        engine->ui_render = PUSH_STRUCT(engine->permanent_arena, UI_Render);
        ui_render_init(engine->ui_render, &engine->renderer, &engine->ui_ctx);

        engine->game = PUSH_STRUCT(engine->permanent_arena, DOT_Game);
        u8 *permanent_memory = PUSH_SIZE(engine->permanent_arena, g_game_permanent_memory_size_b);
        u8 *transient_memory = PUSH_SIZE(engine->permanent_arena, g_game_transient_memory_size_b);
        dot_game_init(
            engine->game,
            &engine->renderer,
            &engine->window,
            permanent_memory, g_game_permanent_memory_size_b,
            transient_memory, g_game_transient_memory_size_b);
    }
}

internal void
dot_engine_run(DOT_Engine *engine)
{
    struct nk_context *ctx = &engine->ui_ctx;
    struct nk_colorf bg = {
        .r = 0.10f,
        .g = 0.18f,
        .b = 0.24f,
        .a = 1.0f,
    };
    while(!dot_window_should_close(&engine->window)){
        dot_window_poll_events(&engine->window);
        {
            RGFW_window *win = engine->window.window;
            if(RGFW_window_isKeyPressed(win, RGFW_keyF1)){ engine->ui_mode = !engine->ui_mode; }
            b32 alt_down = RGFW_window_isKeyDown(win, RGFW_keyAltL) || RGFW_window_isKeyDown(win, RGFW_keyAltR);
            b32 capture  = !(engine->ui_mode || alt_down);
            if(capture != RGFW_window_isCaptured(win)){
                RGFW_window_captureRawMouse(win, cast(RGFW_bool)capture);
                RGFW_window_showMouse(win, cast(RGFW_bool)!capture);
            }
        }
        ui_input_update(&engine->ui_input, ctx, &engine->window);
        if (nk_begin(ctx, "Demo", nk_rect(50, 50, 230, 250),
                NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
                NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE)){
            enum { EASY, HARD };
            static int op = EASY;
            static int property = 20;
            nk_layout_row_static(ctx, 30, 80, 1);
            if (nk_button_label(ctx, "button"))
                fprintf(stdout, "button pressed\n");

            nk_layout_row_dynamic(ctx, 30, 2);
            if (nk_option_label(ctx, "easy", op == EASY))
                op = EASY;
            if (nk_option_label(ctx, "hard", op == HARD))
                op = HARD;

            nk_layout_row_dynamic(ctx, 25, 1);
            nk_property_int(ctx, "Compression:", 0, &property, 100, 10, 1);

            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label(ctx, "background:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 25, 1);
            if (nk_combo_begin_color(ctx, nk_rgb_cf(bg),
                    nk_vec2(nk_widget_width(ctx), 400))) {
                nk_layout_row_dynamic(ctx, 120, 1);
                bg = nk_color_picker(ctx, bg, NK_RGBA);
                nk_layout_row_dynamic(ctx, 25, 1);
                bg.r = nk_propertyf(ctx, "#R:", 0, bg.r, 1.0f, 0.01f, 0.005f);
                bg.g = nk_propertyf(ctx, "#G:", 0, bg.g, 1.0f, 0.01f, 0.005f);
                bg.b = nk_propertyf(ctx, "#B:", 0, bg.b, 1.0f, 0.01f, 0.005f);
                bg.a = nk_propertyf(ctx, "#A:", 0, bg.a, 1.0f, 0.01f, 0.005f);
                nk_combo_end(ctx);
            }
        }
        nk_end(ctx);

        if(nk_begin(ctx, "Inspector", nk_rect(300, 50, 320, 400),
                NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
                NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE)){
            if(nk_tree_push(ctx, NK_TREE_TAB, "Camera", NK_MAXIMIZED)){
                rfl_ui_inspect(ctx, rfl_any(DOT_Camera, &g_camera));
                nk_tree_pop(ctx);
            }
        }
        nk_end(ctx);

        rn_frame_begin(&engine->renderer, dot_window_get_framebuffer_size(&engine->window));

        dot_game_run(engine->game);
        ui_render_draw(engine->ui_render, &engine->renderer, ctx, dot_window_get_size(&engine->window));
        nk_clear(ctx);
        rn_frame_end(&engine->renderer);
    }
}

internal void
dot_engine_shutdown(DOT_Engine *engine)
{
    dot_game_shutdown(engine->game);
    ui_render_shutdown(engine->ui_render, &engine->renderer);
    nk_free(&engine->ui_ctx);
    rn_shutdown(&engine->renderer);
    dot_window_shutdown(&engine->window);
    plugins_shutdown();
    threadctx_shutdown();
    ARENA_DESTROY(engine->permanent_arena);
}
