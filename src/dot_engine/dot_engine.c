#include "dot_engine.h"
#include "base/dot.h"
#include "base/arena.h"

#include "base/platform.h"
#include "base/profiler.h"
#include "base/thread_ctx.h"
#include "dot_engine/window.h"
#include "renderer/renderer_backend.h"
#include "renderer/vulkan/vkrenderer.h"
#include "renderer/renderer.h"
#include "dot_engine/application.h"
#include "dot_engine/game.h"
// #include "dot_engine/game.c"

#include "base/arena.c"
#include "base/thread_ctx.c" 
#include "dot_engine/window.c"
#include "renderer/renderer_backend.c"
#include "renderer/vulkan/vkrenderer.c"
#include "renderer/renderer.c"
#include "dot_engine/application.c"
#include "dot_engine/game.c"

int main() {
    DOT_ProfilerBegin();
    Application app;
    Application_Init(&app);
    Application_Run(&app);
    Application_ShutDown(&app);

    DOT_ProfilerEnd();
    DOT_ProfilerPrint();
    return 0;
}
