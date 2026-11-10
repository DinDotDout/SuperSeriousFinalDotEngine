#define _GNU_SOURCE
#include "dot_engine/third_party.h"

// (jd) Ignore lib warnings that aren't caused by us
DOT_DIAGNOSTIC_PUSH_SILENT

#define RGFW_IMPLEMENTATION
#include "third_party/RGFW/RGFW.h"

#define NK_IMPLEMENTATION
#include "third_party/Nuklear/nuklear.h"

#if DOT_DEBUG_ALLOCATORS && !DOT_SPLIT_THIRD_PARTY
    static void *malloc_debug(u64 size, char *file, u32 line);
    static void  free_debug(void *ptr, char *file, u32 line);
#   define CGLTF_MALLOC(size) malloc_debug(size, __FILE__, __LINE__)
#   define CGLTF_FREE(ptr) free_debug(ptr, __FILE__, __LINE__)
#endif

#define CGLTF_IMPLEMENTATION
#include "third_party/cgltf/cgltf.h"

#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb/stb_image.h"

DOT_DIAGNOSTIC_POP
