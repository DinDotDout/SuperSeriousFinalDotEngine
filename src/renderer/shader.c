global RN_Slang rn_g_slang;

internal String8
rn_shader_cache_get_compiled_path(Arena *arena, String8 path, RN_ShaderCompileOptions const *options){
    u64 options_hash = hash_fnv1a_u64(cast(void*)options, sizeof(*options));
    u64 shader_hash = u64_hash_from_string8(path, options_hash);
    return string8_format(arena, DOT_COMPILED_SHADER_PATH"%u.spv", shader_hash);
}

internal b32
rn_shader_module_is_initialized(RN_ShaderCachedData *shader_module)
{
    return shader_module->byte_code.size != 0;
}

///////////////////////////////////////////////////////////////////
// RB_ShaderStageKind
//

// (jd) didn't really need to do the hash thing but I wanted to test this for
// where it may matter MSVC doesn't fold string-literal indexing (used inside
// STATIC_FNV1A_N) into an integer constant expression, so these can't be switch
// case labels like they can under clang/gcc; compare at runtime instead.
RB_ShaderStageKind
rn_shader_stage_kind_from_ext(String8 ext)
{
    DOT_STATIC_ASSERT(RB_ShaderStageKind_Count == 15, "If this is changed, add ext_map entries and rb_shader_stage_from_ext");

    String8 match = {0};
    RB_ShaderStageKind shader_kind = RB_ShaderStageKind_None;

    u64 h = hash_fnv1a_u64(ext.str, ext.size);

    if(h == STATIC_FNV1A(RN_EXT_VERT)){       match = str_comp(RN_EXT_VERT);  shader_kind = RB_ShaderStageKind_Vertex; }
    else if(h == STATIC_FNV1A(RN_EXT_FRAG)){  match = str_comp(RN_EXT_FRAG);  shader_kind = RB_ShaderStageKind_Fragment; }
    else if(h == STATIC_FNV1A(RN_EXT_COMP)){  match = str_comp(RN_EXT_COMP);  shader_kind = RB_ShaderStageKind_Compute; }

    else if(h == STATIC_FNV1A(RN_EXT_RGEN)){  match = str_comp(RN_EXT_RGEN);  shader_kind = RB_ShaderStageKind_RayGen; }
    else if(h == STATIC_FNV1A(RN_EXT_RAHIT)){ match = str_comp(RN_EXT_RAHIT); shader_kind = RB_ShaderStageKind_RayHitAny; }
    else if(h == STATIC_FNV1A(RN_EXT_RCHIT)){ match = str_comp(RN_EXT_RCHIT); shader_kind = RB_ShaderStageKind_RayHitClosest; }
    else if(h == STATIC_FNV1A(RN_EXT_RMISS)){ match = str_comp(RN_EXT_RMISS); shader_kind = RB_ShaderStageKind_RayHitMiss; }
    else if(h == STATIC_FNV1A(RN_EXT_RINT)){  match = str_comp(RN_EXT_RINT);  shader_kind = RB_ShaderStageKind_RayHitIntersection; }
    else if(h == STATIC_FNV1A(RN_EXT_CALL)){  match = str_comp(RN_EXT_CALL);  shader_kind = RB_ShaderStageKind_Callable; }

    else if(h == STATIC_FNV1A(RN_EXT_MESH)){  match = str_comp(RN_EXT_MESH);  shader_kind = RB_ShaderStageKind_Mesh; }
    else if(h == STATIC_FNV1A(RN_EXT_TASK)){  match = str_comp(RN_EXT_TASK);  shader_kind = RB_ShaderStageKind_Task; }

    else if(h == STATIC_FNV1A(RN_EXT_TESC)){  match = str_comp(RN_EXT_TESC);  shader_kind = RB_ShaderStageKind_TesselationControl; }
    else if(h == STATIC_FNV1A(RN_EXT_TESE)){  match = str_comp(RN_EXT_TESE);  shader_kind = RB_ShaderStageKind_TesselationEvaluation; }
    else if(h == STATIC_FNV1A(RN_EXT_GEOM)){  match = str_comp(RN_EXT_GEOM);  shader_kind = RB_ShaderStageKind_Geometry; }

    if(RB_ShaderStageKind_None == shader_kind){
        DOT_WARNING("Ambiguous file extension name '%S', returning RB_ShaderStageKind_Fragment! Try specifying RB_ShaderStageKind", ext);
        return(RB_ShaderStageKind_Fragment);
    }

    if(string8_equal(ext, match)){
        return(shader_kind);
    }
    DOT_ERROR("Hash collision for %S, consider using another hash function", match);
}

internal SlangStage
rn_slang_stage_from_shader_stage_kind(RB_ShaderStageKind kind)
{
    partial_switch_begin(kind){
    default: DOT_ASSERT(false, "Invalid stage");    return SLANG_STAGE_COMPUTE;
    case RB_ShaderStageKind_Vertex:                 return SLANG_STAGE_VERTEX;
    case RB_ShaderStageKind_Fragment:               return SLANG_STAGE_FRAGMENT;
    case RB_ShaderStageKind_Compute:                return SLANG_STAGE_COMPUTE;

    case RB_ShaderStageKind_RayGen:                 return SLANG_STAGE_RAY_GENERATION;
    case RB_ShaderStageKind_RayHitAny:              return SLANG_STAGE_ANY_HIT;
    case RB_ShaderStageKind_RayHitClosest:          return SLANG_STAGE_CLOSEST_HIT;
    case RB_ShaderStageKind_RayHitMiss:             return SLANG_STAGE_MISS;
    case RB_ShaderStageKind_RayHitIntersection:     return SLANG_STAGE_INTERSECTION;
    case RB_ShaderStageKind_Callable:               return SLANG_STAGE_CALLABLE;

    case RB_ShaderStageKind_Mesh:                   return SLANG_STAGE_MESH;
    case RB_ShaderStageKind_Task:                   return SLANG_STAGE_AMPLIFICATION;

    case RB_ShaderStageKind_TesselationControl:     return SLANG_STAGE_HULL;
    case RB_ShaderStageKind_TesselationEvaluation:  return SLANG_STAGE_DOMAIN;
    case RB_ShaderStageKind_Geometry:               return SLANG_STAGE_GEOMETRY;
    }partial_switch_end;
}

global read_only SlangOptimizationLevel rn_slang_optimization_level_table[RN_ShaderOptimizationLevel_Count] = {
    [RN_ShaderOptimizationLevel_None]    = SLANG_OPTIMIZATION_LEVEL_NONE,
    [RN_ShaderOptimizationLevel_Default] = SLANG_OPTIMIZATION_LEVEL_DEFAULT,
    [RN_ShaderOptimizationLevel_High]    = SLANG_OPTIMIZATION_LEVEL_HIGH,
    [RN_ShaderOptimizationLevel_Maximal] = SLANG_OPTIMIZATION_LEVEL_MAXIMAL,
};

global read_only SlangDebugInfoLevel rn_slang_debug_info_level_table[RN_ShaderDebugInfoLevel_Count] = {
    [RN_ShaderDebugInfoLevel_None]     = SLANG_DEBUG_INFO_LEVEL_NONE,
    [RN_ShaderDebugInfoLevel_Minimal]  = SLANG_DEBUG_INFO_LEVEL_MINIMAL,
    [RN_ShaderDebugInfoLevel_Standard] = SLANG_DEBUG_INFO_LEVEL_STANDARD,
    [RN_ShaderDebugInfoLevel_Maximal]  = SLANG_DEBUG_INFO_LEVEL_MAXIMAL,
};

global read_only SlangFloatingPointMode rn_slang_floating_point_mode_table[RN_ShaderFloatingPointMode_Count] = {
    [RN_ShaderFloatingPointMode_Default] = SLANG_FLOATING_POINT_MODE_DEFAULT,
    [RN_ShaderFloatingPointMode_Fast]    = SLANG_FLOATING_POINT_MODE_FAST,
    [RN_ShaderFloatingPointMode_Precise] = SLANG_FLOATING_POINT_MODE_PRECISE,
};

internal b32
rn_slang_load(void)
{
    if(rn_g_slang.session){
        return true;
    }
    rn_g_slang.library = os_library_open(str_comp(DOT_SLANG_LIBRARY_PATH));
    if(!rn_g_slang.library){
        return false;
    }
#define X(ret, name, params) \
    rn_g_slang.name = cast(ret (*) params)os_library_symbol(rn_g_slang.library, #name); \
    if(!rn_g_slang.name){ \
        DOT_WARNING("Missing Slang symbol %s", #name); \
        return false; \
    }
    RN_SlangProcedureXList(X)
#undef X
    rn_g_slang.session = rn_g_slang.spCreateSession(0);
    if(!rn_g_slang.session){
        return false;
    }
    RN_SlangSetDownstreamCompilerPath *set_downstream_path = cast(RN_SlangSetDownstreamCompilerPath*)rn_g_slang.session->vtable[RN_SLANG_GLOBAL_SESSION_SET_DOWNSTREAM_COMPILER_PATH];
    set_downstream_path(rn_g_slang.session, SLANG_PASS_THROUGH_SPIRV_OPT, DOT_SLANG_LIBRARY_DIR);
    rn_g_slang.descriptor_heap_capability = rn_g_slang.spFindCapability(rn_g_slang.session, "spvDescriptorHeapEXT");
    return true;
}

internal void
rn_slang_target_force_c_layout_set(SlangCompileRequest *request, int target_index, b8 value)
{
    RN_SlangSetTargetForceCLayout *set = cast(RN_SlangSetTargetForceCLayout*)request->vtable[RN_SLANG_COMPILE_REQUEST_SET_TARGET_FORCE_C_LAYOUT];
    set(request, target_index, value);
}

internal b32
rn_shader_compile_from_path(RB_ShaderStageKind stage, RN_ShaderCompileOptions const *options, String8 input_path, String8 output_path)
{
    DOT_ASSERT(options->optimization < RN_ShaderOptimizationLevel_Count);
    DOT_ASSERT(options->debug_info < RN_ShaderDebugInfoLevel_Count);
    DOT_ASSERT(options->floating_point < RN_ShaderFloatingPointMode_Count);
    os_directory_create_if_not_exists(str_comp(DOT_COMPILED_SHADER_PATH));
    if(!rn_slang_load()){
        return false;
    }

    if(stage == RB_ShaderStageKind_None){
        String8 ext = string8_file_extension(input_path);
        stage = rn_shader_stage_kind_from_ext(ext);
    }
    DOT_PRINT("Compiling %S -> %S", input_path, output_path);

    SlangCompileRequest *request = rn_g_slang.spCreateCompileRequest(rn_g_slang.session);
    int target = rn_g_slang.spAddCodeGenTarget(request, SLANG_SPIRV);
    rn_g_slang.spAddTargetCapability(request, target, rn_g_slang.descriptor_heap_capability);
    rn_slang_target_force_c_layout_set(request, target, true);
    rn_g_slang.spSetTargetFloatingPointMode(request, target, rn_slang_floating_point_mode_table[options->floating_point]);
    rn_g_slang.spSetOptimizationLevel(request, rn_slang_optimization_level_table[options->optimization]);
    rn_g_slang.spSetDebugInfoLevel(request, rn_slang_debug_info_level_table[options->debug_info]);
    rn_g_slang.spSetMatrixLayoutMode(request, SLANG_MATRIX_LAYOUT_COLUMN_MAJOR);
    rn_g_slang.spAddSearchPath(request, DOT_SHADER_INCLUDE_PATH);
    if(options->flags & RN_ShaderCompileFlag_WarningsAsErrors){
        rn_g_slang.spSetDiagnosticFlags(request, SLANG_DIAGNOSTIC_FLAG_TREAT_WARNINGS_AS_ERRORS);
    }

    int translation_unit = rn_g_slang.spAddTranslationUnit(request, SLANG_SOURCE_LANGUAGE_SLANG, 0);
    rn_g_slang.spAddTranslationUnitSourceFile(request, translation_unit, input_path.cstr);
    rn_g_slang.spAddEntryPoint(request, translation_unit, "main", rn_slang_stage_from_shader_stage_kind(stage));

    b32 result = rn_g_slang.spCompile(request) >= 0;

    char const *diagnostics = rn_g_slang.spGetDiagnosticOutput(request);
    if(diagnostics && diagnostics[0]){
        DOT_PRINT("%s", diagnostics);
    }
    if(result){
        size_t size = 0;
        void const *code = rn_g_slang.spGetEntryPointCode(request, 0, &size);
        result = code && platform_write_entire_file(output_path, code, size);
    }
    rn_g_slang.spDestroyCompileRequest(request);
    return result;
}
