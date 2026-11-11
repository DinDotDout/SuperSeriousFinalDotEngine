#include "dot_engine.h"

DOT_BOOT_SETTING(u32,"Engine", g_engine_memory_size_b, DOT_MB(512));
DOT_BOOT_SETTING(u32,"ThreadContext", g_per_thread_temp_arena_size_b, DOT_MB(40));
DOT_BOOT_SETTING(u32,"ThreadContext", g_per_thread_temp_arena_count_b, 2);
DOT_BOOT_SETTING(u32,"Game", g_game_permanent_memory_size_b, DOT_MB(20));
DOT_BOOT_SETTING(u32,"Game", g_game_transient_memory_size_b, DOT_KB(32));

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
#if DOT_OS_LINUX
    { .flag = str_lit("-x11"),        .flag_desc = str_lit("Use X11 instead of Wayland"),   .set_int = dot_window_set_system, .int_value = DOT_WindowSystemKind_X11      },
    { .flag = str_lit("-wayland"),    .flag_desc = str_lit("Use Wayland (default, falls back to X11)"), .set_int = dot_window_set_system, .int_value = DOT_WindowSystemKind_Wayland },
#endif
};

internal void
dot_settings_ui(struct nk_context *ctx)
{
    Scratch s = scratch_begin(0,0);
    HashMapIter it = {0};
    int scope_id = 0;
    for(DOT_SettingScope *scope = hm_iter_next(&g_settings.setting_scopes, &it); scope; scope = hm_iter_next(&g_settings.setting_scopes, &it)){
        char *scope_label = rfl_ui_label_from_string8(s.arena, scope->scope_name);
        if(nk_tree_push_id(ctx, NK_TREE_NODE, scope_label, NK_MINIMIZED, scope_id++)){
            for(DOT_Setting *setting = scope->setting; setting; setting = setting->next){
                RFL_Any value = rfl_any_from_type(setting->type_def, setting->data);
                char *name = rfl_ui_label_from_string8(s.arena, setting->setting_name);
                if(setting->boot){
                    nk_layout_row_dynamic(ctx, RFL_UI_ROW_HEIGHT, 2);
                    nk_label(ctx, name, NK_TEXT_LEFT);
                    nk_label_colored(ctx, cstr_format(s.arena, "%S", rfl_string8_from_any(s.arena, value)), NK_TEXT_LEFT, nk_rgb(140, 140, 140));
                }else{
                    rfl_ui_any(ctx, name, value);
                }
            }
            nk_tree_pop(ctx);
        }
    }
    scratch_end(s);
}

int
entry_point(int argc, String8 arg_values[])
{
    Arena *core_arena = ARENA_CREATE(.reserve_size = DOT_MB(16), .name = "Core Arena");
    rfl_init(core_arena);
    dot_settings_apply_config(core_arena, str_comp(DOT_ENGINE_CONFIG_PATH));

    b32 show_help = program_entry_flags_apply(DOT_ARRAY_COUNT(flag_table), flag_table, argc, arg_values);
    if(show_help){
        ARENA_DESTROY(core_arena);
        return(0);
    }
    b32 headless = g_engine_mode != DOT_EngineRunModeKind_Default;

    DOT_Engine engine = {.engine_name = str_comp("Super Serious Final Dot Engine")};
    dot_engine_init(&engine, headless);

    partial_switch_begin(g_engine_mode){
    case DOT_EngineRunModeKind_TestsRun:  dot_test_suites_run();   ARENA_DESTROY(core_arena); return 0;
    case DOT_EngineRunModeKind_TestsList: dot_test_suites_print(); ARENA_DESTROY(core_arena); return 0;
    }partial_switch_end;

    dot_engine_run(&engine);
    dot_engine_shutdown(&engine);
    ARENA_DESTROY(core_arena);

    return(0);
}

internal void
dot_engine_init(DOT_Engine *engine, b32 headless)
{
    engine->permanent_arena = ARENA_CREATE(.reserve_size = g_engine_memory_size_b, .name = "DOT_Engine Arena");
    threadctx_init(engine->permanent_arena, g_per_thread_temp_arena_count_b, g_per_thread_temp_arena_size_b);
    rfl_ui_init(engine->permanent_arena);
    dot_settings_register_all(engine->permanent_arena);
    plugins_init();

    if(!headless){
#if DOT_OS_LINUX
        if(rn_renderdoc_attached()){
            if(dot_g_window_system == DOT_WindowSystemKind_Default){
                DOT_PRINT("RenderDoc detected, using X11 since it can't capture Wayland");
                dot_window_set_system(DOT_WindowSystemKind_X11);
            }else if(dot_g_window_system == DOT_WindowSystemKind_Wayland){
                DOT_WARNING("RenderDoc detected but -wayland was requested; RenderDoc can't capture Wayland");
            }
        }
#endif
        DOT_PROFILE_BLOCK("Window init"){ dot_window_init(&engine->window, engine->engine_name); }
        DOT_PROFILE_BLOCK("Renderer init"){ rn_init(engine->permanent_arena, &engine->renderer, dot_window_native_get(&engine->window)); }
        nk_init_default(&engine->ui_ctx, NULL);
        ui_input_init(&engine->ui_input, &engine->ui_ctx);
        engine->ui_render = PUSH_STRUCT(engine->permanent_arena, UI_Render);
        ui_render_init(engine->ui_render, &engine->renderer, &engine->ui_ctx);

        engine->game = PUSH_STRUCT(engine->permanent_arena, DOT_Game);
        u8 *permanent_memory = PUSH_SIZE(engine->permanent_arena, g_game_permanent_memory_size_b);
        u8 *transient_memory = PUSH_SIZE(engine->permanent_arena, g_game_transient_memory_size_b);
        DOT_PROFILE_BLOCK("Game init"){
            dot_game_init(
                engine->game,
                &engine->renderer,
                &engine->window,
                permanent_memory, g_game_permanent_memory_size_b,
                transient_memory, g_game_transient_memory_size_b);
        }
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
    u64 fps_window_start_ns = os_time_ns();
    u32 fps_frame_count     = 0;
    f64 fps                 = 0.0;
    f64 frame_ms            = 0.0;
    while(!dot_window_should_close(&engine->window)){
        DOT_PROFILE_BLOCK("Poll events"){ dot_window_poll_events(&engine->window); }
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

        fps_frame_count += 1;
        u64 now_ns = os_time_ns();
        u64 fps_elapsed_ns = now_ns - fps_window_start_ns;
        if(fps_elapsed_ns >= 500000000ull){
            fps                 = cast(f64)fps_frame_count * 1e9 / cast(f64)fps_elapsed_ns;
            frame_ms            = cast(f64)fps_elapsed_ns / 1e6 / cast(f64)fps_frame_count;
            fps_frame_count     = 0;
            fps_window_start_ns = now_ns;
        }
        i32x2 window_size = dot_window_get_size(&engine->window);
        f32 fps_width  = 150.0f;
        f32 fps_height = 34.0f;
        struct nk_rect fps_rect = nk_rect(cast(f32)window_size.x - fps_width - 10.0f, 10.0f, fps_width, fps_height);
        if(nk_begin(ctx, "FPS", fps_rect, NK_WINDOW_NO_INPUT | NK_WINDOW_NO_SCROLLBAR)){
            nk_window_set_position(ctx, "FPS", nk_vec2(fps_rect.x, fps_rect.y));
            char fps_text[64];
            snprintf(fps_text, sizeof(fps_text), "%.0f fps  %.2f ms", fps, frame_ms);
            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label(ctx, fps_text, NK_TEXT_RIGHT);
        }
        nk_end(ctx);

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
            if(nk_tree_push(ctx, NK_TREE_TAB, "Sun", NK_MINIMIZED)){
                rfl_ui_inspect(ctx, rfl_any(DOT_SunLight, &g_sun));
                nk_tree_pop(ctx);
            }
            if(nk_tree_push(ctx, NK_TREE_TAB, "Shadows", NK_MINIMIZED)){
                rfl_ui_inspect(ctx, rfl_any(DOT_ShadowParams, &g_shadow_params));
                nk_tree_pop(ctx);
            }
            if(nk_tree_push(ctx, NK_TREE_TAB, "Tonemap", NK_MINIMIZED)){
                rfl_ui_any(ctx, "exposure", rfl_any(f32, &engine->renderer.exposure));
                nk_tree_pop(ctx);
            }
            if(nk_tree_push(ctx, NK_TREE_TAB, "Clouds", NK_MINIMIZED)){
                rfl_ui_inspect(ctx, rfl_any(DOT_CloudParams, &g_cloud_params));
                nk_tree_pop(ctx);
            }
            if(nk_tree_push(ctx, NK_TREE_TAB, "Settings", NK_MINIMIZED)){
                dot_settings_ui(ctx);
                nk_tree_pop(ctx);
            }
        }
        nk_end(ctx);

        DOT_PROFILE_BLOCK("Frame begin"){ rn_frame_begin(&engine->renderer, dot_window_get_framebuffer_size(&engine->window)); }

        DOT_PROFILE_BLOCK("Game run"){ dot_game_run(engine->game); }
        DOT_PROFILE_BLOCK("Tonemap"){ rn_tonemap(&engine->renderer); }
        DOT_PROFILE_BLOCK("UI draw"){ ui_render_draw(engine->ui_render, &engine->renderer, ctx, dot_window_get_size(&engine->window)); }
        nk_clear(ctx);
        DOT_PROFILE_BLOCK("Frame end"){ rn_frame_end(&engine->renderer); }

        DOT_PROFILE_FRAME_MARK();
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
