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
#define DOT_PROFILER_IMPL
#include "dot_engine/profiler.h"
#include "dot_engine/game.h"
#include "dot_engine/application.h"

#include "dot_engine/window.c"
#include "dot_engine/game.c"
#include "dot_engine/application.c"

#include "game/my_game.c"

#endif // !DOT_ENGINE_H
