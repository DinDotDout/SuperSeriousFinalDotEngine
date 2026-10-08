#ifndef DOT_ENGINE_H
#define DOT_ENGINE_H

#define _GNU_SOURCE
#include "base/build_ctx.h"

//////////////////////////////////////////////////////
/// THIRD PARTY
///
//#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
# ifdef DOT_PLATFORM_LINUX
#   define VK_USE_PLATFORM_WAYLAND_KHR
#   define VK_USE_PLATFORM_XLIB_KHR
# elif defined(DOT_PLATFORM_WINDOWS)
#   define VK_USE_PLATFORM_WIN32_KHR
# endif

#define VOLK_IMPLEMENTATION
#include "third_party/volk/volk.h"

DOT_DIAGNOSTIC_PUSH
#if DOT_COMPILER_MSVC
DOT_DIAGNOSTIC_IGNORE(4244) // conversion from 'u64' to 'u32', possible loss of data
DOT_DIAGNOSTIC_IGNORE(4116) // enum operands of different types
DOT_DIAGNOSTIC_IGNORE(5287) // operands are different enum types
#endif

#define RGFW_VULKAN
#define RGFW_IMPLEMENTATION
//#define RGFW_INT_DEFINED
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
DOT_DIAGNOSTIC_POP

#if DOT_OS_WINDOWS
#  if DOT_ENGINE_IMPORT
#    define DOT_ENGINE_API __declspec(dllimport)
#  else
#    define DOT_ENGINE_API __declspec(dllexport)
#  endif
#else
#   define DOT_ENGINE_API __attribute__((visibility("default")))
#endif

//////////////////////////////////////////////////////
/// DOT
//
#include "base/base_include.h"

#if DOT_DEBUG_ALLOCATORS
void* malloc_debug(u64 sz, char *file, u32 line)
{
    void *mem = malloc(sz);
    DOT_PRINT("malloc debug %p %M", mem, sz, file, line);
    return mem;
}

void* realloc_debug(void* ptr, u64 new_sz, char *file, u32 line)
{
    DOT_PRINT("realloc %d", new_sz, file, line);
    return realloc(ptr, new_sz);
}

void free_debug(void *ptr, char *file, u32 line)
{
    DOT_PRINT("free debug %p", ptr, file, line);
    free(ptr);
}
#endif

#if DOT_DEBUG_ALLOCATORS
#   define CGLTF_MALLOC(size) malloc_debug(size, __FILE__, __LINE__)
#   define CGLTF_FREE(ptr) free_debug(ptr, __FILE__, __LINE__)
#endif

#define CGLTF_IMPLEMENTATION
#include "third_party/cgltf/cgltf.h"

#if DOT_DEBUG_ALLOCATORS
#   define STBI_MALLOC(size) malloc_debug(size, __FILE__, __LINE__)
#   define STBI_REALLOC(size,new_sz) realloc_debug(size, new_sz, __FILE__, __LINE__)
#   define STBI_FREE(size) free_debug(size, __FILE__, __LINE__)
#else
// Everything stb allocates comes from the arena set with stbi_allocator_set (g_allocator inside
// stb_image.h), so realloc copies into a new block and free is a no-op.
internal void *
dot_stbi_arena_realloc(Arena *arena, void *ptr, usize old_size, usize new_size)
{
    void *result = PUSH_SIZE_NO_ZERO(arena, new_size);
    if(ptr && result){ MEMORY_COPY(result, ptr, DOT_MIN(old_size, new_size)); }
    return(result);
}
#   define STBI_MALLOC(size)                    dot_stbi_arena_realloc(g_allocator->arena, NULL, 0, size)
#   define STBI_REALLOC_SIZED(ptr,old_sz,new_sz) dot_stbi_arena_realloc(g_allocator->arena, ptr, old_sz, new_sz)
#   define STBI_FREE(ptr)                       ((void)(ptr))
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb/stb_image.h"

// (jd) TODO: base doesn't have those enabled for now
// Compile all libs separately and only enable this for my code
DOT_DIAGNOSTIC_PUSH
#if DOT_COMPILER_CLANG || defined(__GNUC__)
    DOT_DIAGNOSTIC_ERROR("-Wimplicit-fallthrough")
    DOT_DIAGNOSTIC_ERROR("-Wswitch-enum")
    DOT_DIAGNOSTIC_ERROR("-Wvla")
    DOT_DIAGNOSTIC_ERROR("-Wconversion")
    DOT_DIAGNOSTIC_ERROR("-Wimplicit-int-conversion")
    // DOT_DIAGNOSTIC_ERROR("-Wsign-conversion")
#elif defined(_MSC_VER)
#endif

#include "dot_engine/dot_engine_config.h"
#include "dot_engine/window.h"
#include "dot_engine/ui_input.h"

#include "dot_engine/setting.h"
#include "renderer/renderer_include.h"
#include "dot_engine/plugin.h"
#include "game/shaders/model_shared.h"
#include "dot_engine/model.h"
#include "dot_engine/game.h"
#include "dot_engine/reflect.h"

#define DOT_PROFILER_IMPL
#include "dot_engine/profiler.h"
#include "dot_engine/window.c"
#include "dot_engine/ui_input.c"

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
