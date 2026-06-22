#ifndef BASE_INC_H
#define BASE_INC_H

#define STB_SPRINTF_STATIC
#define STB_SPRINTF_NOUNALIGNED
#define STB_SPRINTF_DECORATE(name) dot_##name
#include "third_party/stb/stb_sprintf.h"
#define DOT_INT_SKIP
#include "base/dot.h"
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

#endif // !BASE_INC_H
