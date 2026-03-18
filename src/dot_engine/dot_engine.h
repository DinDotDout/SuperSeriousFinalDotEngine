#ifndef DOT_ENGINE_H
#define DOT_ENGINE_H

//////////////////////////////////////////////////////
/// THIRD PARTY
///

#define _GNU_SOURCE

#ifdef DOT_USE_VOLK
    #define VK_NO_PROTOTYPES
    #define VK_USE_PLATFORM_WAYLAND_KHR
    #define VK_USE_PLATFORM_XLIB_KHR
    #define VOLK_IMPLEMENTATION
    #include "third_party/volk/volk.h"
#endif

#define RGFW_VULKAN
#define RGFW_IMPLEMENTATION
#define RGFWDEF static inline

#include "third_party/RGFW/RGFW.h"

#define NK_INCLUDE_FONT_BAKING  // STB_TRUETYPE_IMPLEMENTATION, STB_RECT_PACK_IMPLEMENTATION
#define NK_PRIVATE
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_DEFAULT_ALLOCATOR // TODO: We want to eventually use our own allocator
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include "third_party/Nuklear/nuklear.h"

//////////////////////////////////////////////////////
/// DOT
///
#if DOT_OS_WINDOWS
#  if DOT_ENGINE_BUILD
#    define DOT_ENGINE_API __declspec(dllexport)
#  else
#    define DOT_ENGINE_API __declspec(dllimport)
#  endif
#else
#  define DOT_ENGINE_API
#endif

#define DOT_INT_SKIP // Already included ints by rgfw
#include "base/base_include.h"

DIAGNOSTIC_PUSH
#if defined(__clang__) || defined(__GNUC__)
    DIAGNOSTIC_ERROR("-Wimplicit-fallthrough")
    DIAGNOSTIC_ERROR("-Wswitch-enum")
    DIAGNOSTIC_ERROR("-Wvla")
#elif defined(_MSC_VER)
#endif

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

internal void dot_engine_init(DOT_Engine *engine);
internal void dot_engine_run(DOT_Engine *engine);
internal void dot_engine_shutdown(DOT_Engine *engine);
DIAGNOSTIC_POP
#endif // !DOT_ENGINE_H
