#define QS_NUMERIC_COMPARE(a,b) (((a) < (b)) ? -1 : ((a) > (b)) ? 1 : 0)

#define QS_T u32
#define QS_COMPARE QS_NUMERIC_COMPARE
#include "sort.h"

#define QS_T u64
#define QS_COMPARE QS_NUMERIC_COMPARE
#include "sort.h"

#define QS_T i32
#define QS_COMPARE QS_NUMERIC_COMPARE
#include "sort.h"

#define QS_T i64
#define QS_COMPARE QS_NUMERIC_COMPARE
#include "sort.h"

#define QS_T f32
#define QS_COMPARE QS_NUMERIC_COMPARE
#include "sort.h"

#define QS_T f64
#define QS_COMPARE QS_NUMERIC_COMPARE
#include "sort.h"

#undef QS_NUMERIC_COMPARE
