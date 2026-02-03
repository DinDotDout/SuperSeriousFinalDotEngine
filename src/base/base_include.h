#ifndef BASE_INC_H
#define BASE_INC_H

#define STB_SPRINTF_STATIC
#define STB_SPRINTF_NOUNALIGNED
#define STB_SPRINTF_DECORATE(name) dot_##name
#include "third_party/stb_sprintf.h"

#define DOT_INT_SKIP
#include "base/dot.h"
#include "base/platform.h"
#include "base/arena.h"
#include "base/string.h"
#include "base/thread_ctx.h"

#include "base/dot.c"
#define STB_SPRINTF_IMPLEMENTATION
#include "third_party/stb_sprintf.h"
#include "base/arena.c"
#include "base/string.c"
#include "base/thread_ctx.c"

#endif // BASE_INC_H
