#include "dot_engine.h"

#include "core/dot.h"
#include "core/std/util.h"
#include "core/base_core.h"
#include "thread_ctx.h"

#include "renderer/backend/dot_renderer_backend.h"
#include "core/dot_window.h"
#include "renderer/backend/dot_vkrenderer.h"
#include "renderer/dot_renderer.h"
#include "core/application_config.h"
#include "core/application.h"

#include "renderer/backend/dot_vkrenderer.c"
#include "core/dot_window.c"
#include "renderer/backend/dot_renderer_backend.c"
#include "renderer/dot_renderer.c"
#include "thread_ctx.c" 
#include "core/application.c"

int main() {
    // NOTE: Create app/hot reload path.Move this to engine layer
    // Game game = {0}; This shoud be passed in too
    Application app;
    Application_Init(&app);
    Application_Run(&app);
    Application_ShutDown(&app);
    return 0;
}
