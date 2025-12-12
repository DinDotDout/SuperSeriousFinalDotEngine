#ifndef DOT_APPLICATION_H
#define DOT_APPLICATION_H
typedef struct Application{
        DOT_Renderer renderer;
        DOT_Window window;
        Arena permanent_arena;
} Application;

void Application_Init(Application* app);
void Application_Run(Application* app);
void Application_Shutdown(Application* app);
#endif
