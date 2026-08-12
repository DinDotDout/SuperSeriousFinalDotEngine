#ifndef BASE_INC_H
#define BASE_INC_H

#include "base/build_ctx.h"
#include "base/dot.h"

DOT_DIAGNOSTIC_PUSH
#   if DOT_COMPILER_MSVC
        DOT_DIAGNOSTIC_IGNORE(4456)
#   endif
#define STB_SPRINTF_STATIC
#define STB_SPRINTF_NOUNALIGNED
#define STB_SPRINTF_DECORATE(name) dot_##name
#include "third_party/stb/stb_sprintf.h"
DOT_DIAGNOSTIC_POP

DOT_DIAGNOSTIC_PUSH
#if DOT_COMPILER_CLANG || defined(__GNUC__)
    DOT_DIAGNOSTIC_ERROR("-Wimplicit-fallthrough")
    DOT_DIAGNOSTIC_ERROR("-Wswitch-enum")
    DOT_DIAGNOSTIC_ERROR("-Wvla")
    DOT_DIAGNOSTIC_ERROR("-Wconversion")
    DOT_DIAGNOSTIC_ERROR("-Wimplicit-int-conversion")
    DOT_DIAGNOSTIC_ERROR("-Wsign-conversion")
#elif defined(_MSC_VER)
    DOT_DIAGNOSTIC_IGNORE(5287)
    DOT_DIAGNOSTIC_IGNORE(4456)
#endif

#include "base/platform.h"
#include "base/containters.h"
#include "base/test_suite.h"
#include "base/arena.h"
#include "base/string.h"
#include "base/math.h"
#include "base/thread_ctx.h"
#include "base/templates/sort_include.h"

#define STB_SPRINTF_IMPLEMENTATION
#include "third_party/stb/stb_sprintf.h"
#include "base/dot.c"
#include "base/platform.c"
#include "base/containters.c"
#include "base/test_suite.c"
#include "base/arena.c"
#include "base/string.c"
#include "base/thread_ctx.c"
DOT_DIAGNOSTIC_POP

#endif // !BASE_INC_H
