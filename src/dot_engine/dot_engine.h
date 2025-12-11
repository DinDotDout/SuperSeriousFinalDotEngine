#ifndef DOT_ENGINE_H
#define DOT_ENGINE_H

#define _GNU_SOURCE

#define STB_SPRINTF_DECORATE(name) dot_##name
#define STB_SPRINTF_IMPLEMENTATION
// #include "../extra/stb_sprintf.h"

#define RGFW_VULKAN
#define RGFW_WAYLAND
#define RGFW_IMPLEMENTATION
#include "../third_party/RGFW/RGFW.h"
// #include "third_party/RGFW/RGFW.h"
#endif
