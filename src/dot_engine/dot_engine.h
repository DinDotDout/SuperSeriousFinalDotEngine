#ifndef DOT_ENGINE_H
#define DOT_ENGINE_H

#define _GNU_SOURCE

#ifdef USE_VOLK
    #define VK_NO_PROTOTYPES
    #define VK_USE_PLATFORM_WAYLAND_KHR
    #define VOLK_IMPLEMENTATION
    #include "third_party/volk/volk.h"
#endif

#define RGFW_VULKAN
#define RGFW_IMPLEMENTATION
#define RGFWDEF static inline
#include "third_party/RGFW/RGFW.h"

#define DOT_INT_SKIP // Already included ints by rgfw
#include "base/base_include.h"
#include "dot_engine/window.h"
#include "renderer/renderer_include.h"
#include "dot_engine/plugin.h"
#include "dot_engine/game.h"

#define DOT_PROFILER_IMPL
#include "dot_engine/profiler.h"
#include "dot_engine/window.c"
#include "dot_engine/game.c"
#include "dot_engine/dot_engine_config.h"

#include "game/my_game.c"


typedef struct DOT_Engine{
    DOT_Renderer renderer;
    DOT_Window window;
    DOT_Game *game;
    // DOT_AssetDB *asset_db;
    Arena *permanent_arena;
}DOT_Engine;

internal void dot_engine_init(DOT_Engine *app);
internal void dot_engine_run(DOT_Engine *app);
internal void dot_engine_shutdown(DOT_Engine *app);

#endif // !DOT_ENGINE_H
