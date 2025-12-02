void Application_Init(Application* app){
    ApplicationConfig app_config = ApplicationConfig_Get();
    ThreadCtx_Init(&(ThreadCtxOpts){
        .memory_size = app_config.thread_mem_size,
    });
    app->permanent_arena = Arena_Alloc(.capacity = app_config.mem_size, .name = "Application");

    // // NOTE: Move into windowing class
    // RGFW_window *window = RGFW_createWindow("Vulkan Example", 0, 0, 500, 500,
    //                                     RGFW_windowAllowDND | RGFW_windowCenter);
    // RGFW_window_setExitKey(window, RGFW_escape);
    // RGFW_setMousePosCallback(Window_MousePosCallback);
    //
    // app->window = window;
    // app->renderer = PushStruct(&app->arena, DOT_Renderer);
    DOT_Window_Init(&app->window);
    DOT_Renderer_Init(&app->permanent_arena, &app->renderer, &app->window, app_config.renderer_config);
}

// TODO: Move this functions to DOT_Window itslef
void Application_Run(Application* app){
    u8 running = true;
    while (running && !DOT_Window_ShouldClose(&app->window)) {
    }
}

void Application_ShutDown(Application* app){
    DOT_Renderer_Shutdown(&app->renderer);
    DOT_Window_Destroy(&app->window);
    // RGFW_window_close(app->window);
}
