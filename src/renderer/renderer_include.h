#ifndef RENDERER_INCLUDE_H
#define RENDERER_INCLUDE_H

#include "base/base_include.h"

// (jd) TODO: Should eventually move defining those flags to nob.c build config
#define RB_BACKEND_VK
#define RB_BACKEND_SINGLE
#define RENDER_BACKEND_UTILITY_INCLUDE
#include "render_backend/render_backend_incl.h"
#include "shaders/shader_shared.h"
#include "shader.h"
#include "renderer.h"
#include "shaders/ui_shared.h"
#include "shaders/tonemap_shared.h"
#include "shaders/bc7_encode_shared.h"
#include "shaders/bc5_encode_shared.h"
#include "shaders/mip_downsample_shared.h"
#include "ui_render.h"

#include "shader.c"
#include "renderer.c"
#include "ui_render.c"

#endif // !RENDERER_INCLUDE_H
