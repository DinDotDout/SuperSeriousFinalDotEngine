#ifndef RENDER_BACKEND_INCL_H
#define RENDER_BACKEND_INCL_H

#if DOT_ENABLED(RB_BACKEND_DX12) && !DOT_ENABLED(DOT_OS_WINDOWS)
#   error "DX12 backend is only available on Windows"
#endif

#if DOT_ENABLED(RB_BACKEND_SINGLE)
#   if (DOT_ENABLED(RB_BACKEND_VK) && DOT_ENABLED(RB_BACKEND_DX12))
#       error "RB_BACKEND_SINGLE needs exactly one backend"
#   endif
#   if (!DOT_ENABLED(RB_BACKEND_VK) && !DOT_ENABLED(RB_BACKEND_DX12))
#       error "RB_BACKEND_SINGLE has no backend, define at least 1"
#   endif
#endif

#include "base/base_include.h"

// .h
#include "render_backend.h"

#if !DOT_ENABLED(RB_BACKEND_SINGLE)
#   include "render_backend_null.h"
#endif

#if DOT_ENABLED(RB_BACKEND_VK)
#   include "vulkan/render_backend_vk.h"
#endif

#if DOT_ENABLED(RB_BACKEND_DX12)
#   include "dx12/render_backend_dx12.h"
#endif

// .c
#if DOT_ENABLED(RENDER_BACKEND_UTILITY_INCLUDE)
#   include "render_backend_utility.h"
#endif
#include "render_backend.c"
#if DOT_ENABLED(RB_BACKEND_VK)
#   include "vulkan/render_backend_vk.c"
#endif
#if DOT_ENABLED(RENDER_BACKEND_UTILITY_INCLUDE)
#   include "render_backend_utility.c"
#endif

#endif // !RENDER_BACKEND_INCL_H
