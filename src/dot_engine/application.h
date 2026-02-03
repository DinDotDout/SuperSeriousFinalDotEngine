#ifndef DOT_APPLICATION_H
#define DOT_APPLICATION_H
typedef struct Application{
    DOT_Renderer renderer;
    DOT_Window window;
    Arena* permanent_arena;
} Application;

internal void application_init(Application* app);
internal void application_run(Application* app);
internal void application_shutdown(Application* app);
#endif
