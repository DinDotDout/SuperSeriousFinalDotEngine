#ifndef THIRD_PARTY_H
#define THIRD_PARTY_H

#include "base/build_ctx.h"
#include "base/base_types.h"

#ifndef DOT_SPLIT_THIRD_PARTY
#   define DOT_SPLIT_THIRD_PARTY 0
#endif

DOT_DIAGNOSTIC_PUSH_SILENT

#define RGFW_INT_DEFINED // Included in base_types already
#define RGFW_NO_INCLUDE_VULKAN
#if DOT_SPLIT_THIRD_PARTY
#   define RGFWDEF
#else
#   define RGFWDEF static inline
#endif
#include "third_party/RGFW/RGFW.h"

#define NK_INCLUDE_FONT_BAKING  // STB_TRUETYPE_IMPLEMENTATION, STB_RECT_PACK_IMPLEMENTATION
#if !DOT_SPLIT_THIRD_PARTY
#   define NK_PRIVATE
#endif
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_DEFAULT_ALLOCATOR // TODO: We want to eventually use our own allocator
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_MAX_FLOAT_PRECISION 9
#include "third_party/Nuklear/nuklear.h"

#include "third_party/cgltf/cgltf.h"
#include "third_party/stb/stb_image.h"

DOT_DIAGNOSTIC_POP

#endif // !THIRD_PARTY_H
