#ifndef DOT_SHADER_H
#define DOT_SHADER_H

#define DOT_COMPILED_SHADER_PATH "src/game/compiled_shaders/"
#define DOT_SHADER_INCLUDE_PATH  "src/"

#define RN_EXT_VERT    "vert"
#define RN_EXT_FRAG    "frag"
#define RN_EXT_COMP    "comp"

#define RN_EXT_RGEN    "rgen"
#define RN_EXT_RAHIT   "rahit"
#define RN_EXT_RCHIT   "rchit"
#define RN_EXT_RMISS   "rmiss"
#define RN_EXT_RINT    "rint"
#define RN_EXT_CALL    "call"

#define RN_EXT_MESH    "mesh"
#define RN_EXT_TASK    "task"

#define RN_EXT_TESC    "tesc"
#define RN_EXT_TESE    "tese"
#define RN_EXT_GEOM    "geom"

#if DOT_OS_WINDOWS
#define DOT_SLANG_LIBRARY_DIR  "src\\third_party\\slang\\windows-x86_64"
#define DOT_SLANG_LIBRARY_PATH DOT_SLANG_LIBRARY_DIR "\\slang-compiler.dll"
#else
#define DOT_SLANG_LIBRARY_DIR  "src/third_party/slang/linux-x86_64"
#define DOT_SLANG_LIBRARY_PATH DOT_SLANG_LIBRARY_DIR "/libslang-compiler.so"
#endif

////////////////////////////////////////////////////////////////
/// SLANG_API_MIRROR
// Mirrors slang.h / slang-deprecated.h (v2026.19) for the subset we use.
// Keep in sync with the vendored library when upgrading.

typedef struct SlangSession SlangSession;
typedef struct SlangCompileRequest SlangCompileRequest;
typedef i32 SlangResult;
typedef i32 SlangCapabilityID;
typedef u32 SlangDiagnosticFlags;

typedef enum SlangCompileTarget
{
    SLANG_SPIRV = 6,
}SlangCompileTarget;

typedef enum SlangPassThrough
{
    SLANG_PASS_THROUGH_SPIRV_OPT = 11,
}SlangPassThrough;

typedef enum SlangSourceLanguage
{
    SLANG_SOURCE_LANGUAGE_SLANG = 1,
}SlangSourceLanguage;

typedef enum SlangOptimizationLevel
{
    SLANG_OPTIMIZATION_LEVEL_NONE    = 0,
    SLANG_OPTIMIZATION_LEVEL_DEFAULT = 1,
    SLANG_OPTIMIZATION_LEVEL_HIGH    = 2,
    SLANG_OPTIMIZATION_LEVEL_MAXIMAL = 3,
}SlangOptimizationLevel;

typedef enum SlangDebugInfoLevel
{
    SLANG_DEBUG_INFO_LEVEL_NONE     = 0,
    SLANG_DEBUG_INFO_LEVEL_MINIMAL  = 1,
    SLANG_DEBUG_INFO_LEVEL_STANDARD = 2,
    SLANG_DEBUG_INFO_LEVEL_MAXIMAL  = 3,
}SlangDebugInfoLevel;

typedef enum SlangFloatingPointMode
{
    SLANG_FLOATING_POINT_MODE_DEFAULT = 0,
    SLANG_FLOATING_POINT_MODE_FAST    = 1,
    SLANG_FLOATING_POINT_MODE_PRECISE = 2,
}SlangFloatingPointMode;

enum
{
    SLANG_DIAGNOSTIC_FLAG_TREAT_WARNINGS_AS_ERRORS = 0x02,
};

typedef enum SlangMatrixLayoutMode
{
    SLANG_MATRIX_LAYOUT_COLUMN_MAJOR = 2,
}SlangMatrixLayoutMode;

typedef enum SlangStage
{
    SLANG_STAGE_NONE           = 0,
    SLANG_STAGE_VERTEX         = 1,
    SLANG_STAGE_HULL           = 2,
    SLANG_STAGE_DOMAIN         = 3,
    SLANG_STAGE_GEOMETRY       = 4,
    SLANG_STAGE_FRAGMENT       = 5,
    SLANG_STAGE_COMPUTE        = 6,
    SLANG_STAGE_RAY_GENERATION = 7,
    SLANG_STAGE_INTERSECTION   = 8,
    SLANG_STAGE_ANY_HIT        = 9,
    SLANG_STAGE_CLOSEST_HIT    = 10,
    SLANG_STAGE_MISS           = 11,
    SLANG_STAGE_CALLABLE       = 12,
    SLANG_STAGE_MESH           = 13,
    SLANG_STAGE_AMPLIFICATION  = 14,
}SlangStage;

// slang::IGlobalSession and slang::ICompileRequest are COM objects. Methods with no exported
// C function are called through the vtable; indices count ISlangUnknown's 3 methods first.
struct SlangSession
{
    void * const *vtable;
};

struct SlangCompileRequest
{
    void * const *vtable;
};

#define RN_SLANG_GLOBAL_SESSION_SET_DOWNSTREAM_COMPILER_PATH 5
typedef void RN_SlangSetDownstreamCompilerPath(SlangSession *session, SlangPassThrough pass_through, char const *path);

#define RN_SLANG_COMPILE_REQUEST_SET_TARGET_FORCE_C_LAYOUT 83
typedef void RN_SlangSetTargetForceCLayout(SlangCompileRequest *request, int target_index, b8 value);

#define RN_SlangProcedureXList(X) \
X(SlangSession*,        spCreateSession,                (char const *deprecated)) \
X(SlangCapabilityID,    spFindCapability,               (SlangSession *session, char const *name)) \
X(SlangCompileRequest*, spCreateCompileRequest,         (SlangSession *session)) \
X(void,                 spDestroyCompileRequest,        (SlangCompileRequest *request)) \
X(int,                  spAddCodeGenTarget,             (SlangCompileRequest *request, SlangCompileTarget target)) \
X(void,                 spAddTargetCapability,          (SlangCompileRequest *request, int target_index, SlangCapabilityID capability)) \
X(void,                 spSetOptimizationLevel,         (SlangCompileRequest *request, SlangOptimizationLevel level)) \
X(void,                 spSetMatrixLayoutMode,          (SlangCompileRequest *request, SlangMatrixLayoutMode mode)) \
X(void,                 spSetDebugInfoLevel,            (SlangCompileRequest *request, SlangDebugInfoLevel level)) \
X(void,                 spSetTargetFloatingPointMode,   (SlangCompileRequest *request, int target_index, SlangFloatingPointMode mode)) \
X(void,                 spSetDiagnosticFlags,           (SlangCompileRequest *request, SlangDiagnosticFlags flags)) \
X(void,                 spAddSearchPath,                (SlangCompileRequest *request, char const *path)) \
X(int,                  spAddTranslationUnit,           (SlangCompileRequest *request, SlangSourceLanguage language, char const *name)) \
X(void,                 spAddTranslationUnitSourceFile, (SlangCompileRequest *request, int translation_unit_index, char const *path)) \
X(int,                  spAddEntryPoint,                (SlangCompileRequest *request, int translation_unit_index, char const *name, SlangStage stage)) \
X(SlangResult,          spCompile,                      (SlangCompileRequest *request)) \
X(char const*,          spGetDiagnosticOutput,          (SlangCompileRequest *request)) \
X(void const*,          spGetEntryPointCode,            (SlangCompileRequest *request, int entry_point_index, size_t *out_size))

////////////////////////////////////////////////////////////////
/// RN_SHADER_COMPILE

typedef enum RN_ShaderOptimizationLevel
{
    RN_ShaderOptimizationLevel_None,
    RN_ShaderOptimizationLevel_Default,
    RN_ShaderOptimizationLevel_High,
    RN_ShaderOptimizationLevel_Maximal,
    RN_ShaderOptimizationLevel_Count,
}RN_ShaderOptimizationLevel;

typedef enum RN_ShaderDebugInfoLevel
{
    RN_ShaderDebugInfoLevel_None,
    RN_ShaderDebugInfoLevel_Minimal,
    RN_ShaderDebugInfoLevel_Standard,
    RN_ShaderDebugInfoLevel_Maximal,
    RN_ShaderDebugInfoLevel_Count,
}RN_ShaderDebugInfoLevel;

typedef enum RN_ShaderFloatingPointMode
{
    RN_ShaderFloatingPointMode_Default,
    RN_ShaderFloatingPointMode_Fast,
    RN_ShaderFloatingPointMode_Precise,
    RN_ShaderFloatingPointMode_Count,
}RN_ShaderFloatingPointMode;

typedef u32 RN_ShaderCompileFlags;
enum
{
    RN_ShaderCompileFlag_WarningsAsErrors = 1 << 0,
};

typedef struct RN_ShaderCompileOptions
{
    RN_ShaderOptimizationLevel optimization;
    RN_ShaderDebugInfoLevel    debug_info;
    RN_ShaderFloatingPointMode floating_point;
    RN_ShaderCompileFlags      flags;
}RN_ShaderCompileOptions;

typedef struct RN_Slang
{
    void *library;
    SlangSession *session;
    SlangCapabilityID descriptor_heap_capability;
#define X(ret, name, params) ret (*name) params;
    RN_SlangProcedureXList(X)
#undef X
}RN_Slang;

typedef struct RN_ShaderCachedData{
    String8 compiled_path;
    String8 path;
    String8 byte_code;
}RN_ShaderCachedData;

// NOTE: Slang is loaded at runtime from the vendored library through its C API.
// I don't want to have to deal with glslang either and have multiple shader compilation
// backends
internal b32        rn_shader_compile_from_path(RB_ShaderStageKind stage, RN_ShaderCompileOptions const *options, String8 input_path, String8 output_path);
internal String8    rn_shader_cache_get_compiled_path(Arena *arena, String8 path, RN_ShaderCompileOptions const *options);
internal b32        rn_shader_module_is_initialized(RN_ShaderCachedData *shader_module);

#endif // !DOT_SHADER_H
