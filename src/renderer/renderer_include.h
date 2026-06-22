#ifndef RENDERER_INCLUDE_H
#define RENDERER_INCLUDE_H

#include "render_config.h"
#include "shader.h"

#include "render_types.h"
#include "renderer.h"

#include "renderer_backend_null.h"
#if !defined(DOT_RENDER_BACKEND_ONLY_DX12) && !defined(DOT_RENDER_BACKEND_ONLY_VK)
#   include "vulkan/vk_include.h"
#   include "dx12/dx12_include.h"
#elif !defined(DOT_RENDER_BACKEND_ONLY_DX12)
#   include "vulkan/vk_include.h"
#endif

#include "shader.c"
#include "renderer.c"
#include "vulkan/vk_include.c"
#include "render_types.c"

#include "nk_dot_window.h"
#endif // !RENDERER_INCLUDE_H
