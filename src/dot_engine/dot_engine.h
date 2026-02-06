#ifndef DOT_ENGINE_H
#define DOT_ENGINE_H

// third_party
#define _GNU_SOURCE

#ifdef USE_VOLK
#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WAYLAND_KHR
#include "third_party/volk/volk.h"
#endif

#define RGFW_VULKAN

#ifndef _WIN32
#define RGFW_WAYLAND // wayland is be in debug mode by default for now
#endif

#define RGFW_IMPLEMENTATION
#define RGFWDEF static inline
#include "third_party/RGFW/RGFW.h"

#define DOT_INT_SKIP // Already included ints by rgfw
#include "base/base_include.h"

#include "dot_engine/window.h"
#include "renderer/renderer.h"
#include "renderer/renderer_backend_null.h"
#include "renderer/vulkan/renderer_backend_vk.h"
#include "dot_engine/plugin.h"
#define DOT_PROFILER_IMPL
#include "dot_engine/profiler.h"
#include "dot_engine/application.h"
#include "dot_engine/game.h"

#include "dot_engine/window.c"
#include "renderer/renderer.c"
#include "renderer/vulkan/renderer_backend_vk.c"
#include "dot_engine/application.c"
#include "dot_engine/game.c"
#endif // !DOT_ENGINE_H
