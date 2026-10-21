#ifndef DOT_ENGINE_H
#define DOT_ENGINE_H

#define _GNU_SOURCE
#include "base/build_ctx.h"
#include "base/base_types.h"

//////////////////////////////////////////////////////
/// THIRD PARTY
///
#include "dot_engine/third_party.h"

//////////////////////////////////////////////////////
/// DOT

#if DOT_OS_WINDOWS
#  if DOT_ENGINE_IMPORT
#    define DOT_ENGINE_API __declspec(dllimport)
#  else
#    define DOT_ENGINE_API __declspec(dllexport)
#  endif
#else
#   define DOT_ENGINE_API __attribute__((visibility("default")))
#endif

#include "base/base_include.h"

// (jd) TODO: base doesn't have those enabled for now
// Compile all libs separately and only enable this for my code
DOT_DIAGNOSTIC_PUSH
#if DOT_COMPILER_CLANG || defined(__GNUC__)
    DOT_DIAGNOSTIC_ERROR("-Wimplicit-fallthrough")
    DOT_DIAGNOSTIC_ERROR("-Wswitch-enum")
    DOT_DIAGNOSTIC_ERROR("-Wvla")
    DOT_DIAGNOSTIC_ERROR("-Wconversion")
    DOT_DIAGNOSTIC_ERROR("-Wimplicit-int-conversion")
#elif defined(_MSC_VER)
#endif

#include "dot_engine/dot_engine_config.h"
#include "dot_engine/window.h"
#include "dot_engine/ui_input.h"

#include "dot_engine/setting.h"
#include "renderer/renderer_include.h"
#include "dot_engine/plugin.h"
#include "dot_engine/reflect_ui.h"
#include "game/shaders/model_shared.h"
#include "dot_engine/model.h"
#include "dot_engine/game.h"

#define DOT_PROFILER_IMPL
#include "dot_engine/profiler.h"
#include "dot_engine/window.c"
#include "dot_engine/ui_input.c"
#include "dot_engine/reflect_ui.c"

#include "dot_engine/model.c"
#include "dot_engine/game.c"

#include "game/my_game.c"

typedef struct DOT_Engine{
    String8 engine_name;
    RN_RenderCtx renderer;
    DOT_Window window;
    struct nk_context ui_ctx;
    UI_Input ui_input;
    UI_Render *ui_render;
    DOT_Game *game;
    b32 ui_mode; // F1 toggle; holding Alt also frees the mouse for the UI
    // DOT_AssetDB *asset_db;
    Arena *permanent_arena;
}DOT_Engine;

internal void dot_engine_init(DOT_Engine *engine, b32 headless);
internal void dot_engine_run(DOT_Engine *engine);
internal void dot_engine_shutdown(DOT_Engine *engine);
DOT_DIAGNOSTIC_POP
#endif // !DOT_ENGINE_H
