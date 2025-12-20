#include "dot_engine/application_config.h"

void Application_Init(Application* app){
    ApplicationConfig app_config = ApplicationConfig_Get();
    app->permanent_arena = Arena_Alloc(.reserve_size = app_config.mem_size, .name = "Application");
    ThreadCtx_Init(&(ThreadCtxOpts){
        .memory_size = app_config.thread_mem_size,
        .thread_id   = 0,
    });
    DOT_Window_Init(&app->window);
    DOT_Renderer_Init(app->permanent_arena, &app->renderer, &app->window, app_config.renderer_config);
}

void Application_Run(Application* app){
    u8 running = true;
    while (running && !DOT_Window_ShouldClose(&app->window)){
    }
}

void Application_Shutdown(Application* app){
    DOT_Renderer_Shutdown(&app->renderer);
    DOT_Window_Destroy(&app->window);
    ThreadCtx_Destroy();
    Arena_Free(app->permanent_arena);
}
