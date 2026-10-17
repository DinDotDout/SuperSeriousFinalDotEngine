#ifndef BASE_INC_H
#define BASE_INC_H

#include "base/build_ctx.h"
#include "base/dot.h"

DOT_DIAGNOSTIC_PUSH
DOT_DIAGNOSTIC_POP

DOT_DIAGNOSTIC_PUSH
#if DOT_COMPILER_CLANG || defined(__GNUC__)
    DOT_DIAGNOSTIC_ERROR("-Wimplicit-fallthrough")
    DOT_DIAGNOSTIC_ERROR("-Wswitch-enum")
    DOT_DIAGNOSTIC_ERROR("-Wvla")
    DOT_DIAGNOSTIC_ERROR("-Wconversion")
    DOT_DIAGNOSTIC_ERROR("-Wimplicit-int-conversion")
    DOT_DIAGNOSTIC_IGNORE("-Wsign-conversion")
#elif DOT_COMPILER_MSVC
    DOT_DIAGNOSTIC_IGNORE(5287) // operands are different enum types
    DOT_DIAGNOSTIC_IGNORE(4456) // declaration hides previous local declaration
    DOT_DIAGNOSTIC_IGNORE(4456) // declaration hides previous local declaration
#endif

#include "base/platform.h"
#include "base/containters.h"
#include "base/test_suite.h"
#include "base/arena.h"
#include "base/debug_alloc.h"
#include "base/string.h"
#include "base/hashmap.h"
#include "base/math.h"
#include "base/reflect.h"
#include "base/thread_ctx.h"
#include "base/templates/sort_include.h"

DOT_DIAGNOSTIC_PUSH
#if DOT_COMPILER_CLANG || DOT_COMPILER_GCC
    DOT_DIAGNOSTIC_IGNORE("-Wimplicit-fallthrough")
#endif
#   define STB_SPRINTF_STATIC
#   define STB_SPRINTF_NOUNALIGNED
#   define STB_SPRINTF_DECORATE(name) dot_##name
#   define STB_SPRINTF_IMPLEMENTATION
#   include "third_party/stb/stb_sprintf.h"
DOT_DIAGNOSTIC_POP

#include "base/dot.c"
#include "base/platform.c"
#include "base/containters.c"
#include "base/test_suite.c"
#include "base/arena.c"
#include "base/debug_alloc.c"
#include "base/string.c"
#include "base/hashmap.c"
#include "base/reflect.c"
#include "base/thread_ctx.c"
#include "base/program_entry_point.c"
DOT_DIAGNOSTIC_POP

#endif // !BASE_INC_H
