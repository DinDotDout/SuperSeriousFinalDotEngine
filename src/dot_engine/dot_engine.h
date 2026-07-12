#ifndef DOT_ENGINE_H
#define DOT_ENGINE_H

//////////////////////////////////////////////////////
/// THIRD PARTY
///

#define _GNU_SOURCE

// #define DOT_USE_VOLK
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
#   define DOT_ENGINE_API __attribute__((visibility("default")))
#endif

#define DOT_INT_SKIP // Already included ints by rgfw
#include "base/base_include.h"
void* malloc_debug(u64 sz, char *file, u32 line)
{
    void *mem = malloc(sz);
    DOT_PRINT("malloc debug %p %M", mem, sz, file, line);
    return mem;
}

void* realloc_debug(void* ptr, u64 new_sz, char *file, u32 line)
{
    DOT_PRINT("realloc %M", new_sz, file, line);
    return realloc(ptr, new_sz);
}

void free_debug(void *ptr, char *file, u32 line)
{
    DOT_PRINT("free debug %p", ptr, file, line);
    free(ptr);
}

#define STBI_MALLOC(size) malloc_debug(size, __FILE__, __LINE__)
#define STBI_REALLOC(size,new_sz) realloc_debug(size, new_sz, __FILE__, __LINE__)
#define STBI_FREE(size) free_debug(size, __FILE__, __LINE__)
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb/stb_image.h"

#define CGLTF_MALLOC(size) malloc_debug(size, __FILE__, __LINE__)
#define CGLTF_FREE(ptr) free_debug(ptr, __FILE__, __LINE__)
#define CGLTF_IMPLEMENTATION

#define CGLTF_FREE(ptr) free_debug(ptr, __FILE__, __LINE__)
#define CGLTF_IMPLEMENTATION
#include "third_party/cgltf/cgltf.h"

// (jd) TODO: base doesn't have those enabled for now
// Compile all libs separately and only enable this for my code
DOT_DIAGNOSTIC_PUSH
#if DOT_COMPILER_CLANG || defined(__GNUC__)
    DOT_DIAGNOSTIC_ERROR("-Wimplicit-fallthrough")
    DOT_DIAGNOSTIC_ERROR("-Wswitch-enum")
    DOT_DIAGNOSTIC_ERROR("-Wvla")
    DOT_DIAGNOSTIC_ERROR("-Wconversion")
    DOT_DIAGNOSTIC_ERROR("-Wimplicit-int-conversion")
    DOT_DIAGNOSTIC_ERROR("-Wsign-conversion")
#elif defined(_MSC_VER)
#endif

#include "dot_engine/window.h"

#include "dot_engine/setting.h"
#include "renderer/renderer_include.h"
#include "dot_engine/plugin.h"

#include "dot_engine/model.h"
#include "dot_engine/game.h"
#include "dot_engine/reflect.h"
#include "dot_engine/dot_engine_config.h"

#define DOT_PROFILER_IMPL
#include "dot_engine/profiler.h"
#include "dot_engine/window.c"

#include "dot_engine/model.c"
#include "dot_engine/game.c"

#include "game/my_game.c"

typedef struct DOT_Engine{
    RN_RenderCtx renderer;
    DOT_Window window;
    DOT_Game *game;
    // DOT_AssetDB *asset_db;
    Arena *permanent_arena;
}DOT_Engine;

internal void dot_engine_init(DOT_Engine *engine, b32 tests_only);
internal void dot_engine_run(DOT_Engine *engine);
internal void dot_engine_shutdown(DOT_Engine *engine);
DOT_DIAGNOSTIC_POP
#endif // !DOT_ENGINE_H
