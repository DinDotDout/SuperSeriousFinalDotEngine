typedef struct Application{
        DOT_Renderer renderer; // NOTE: Will keep this a pointer as we'll probably hot reload it
        DOT_Window window;
        Arena permanent_arena; // NOTE: Should store arena check points to restore memory
        // RGFW_window *window; // HACK: Create window here for now
} Application;

void Application_Init(Application* app);
void Application_Run(Application* app);
void Application_ShutDown(Application* app);
