internal String8
rn_shader_cache_get_compiled_path(Arena *arena, String8 path){
    u64 shader_hash = hash_map_RN_ShaderModule_hash(path);
    return string8_format(arena, DOT_COMPILED_SHADER_PATH"%u.spv", shader_hash);
}

internal b32
rn_shader_module_is_initialized(RN_ShaderModule *shader_module)
{
    return shader_module->shader_module_handle.handle[0] != 0;
}

internal b32
rn_shader_compile_from_path(String8 input_path, String8 output_path)
{
    TempArena temp = threadctx_temp_begin(0);
    DOT_PRINT("Compiling with: slangc %s -target spirv -stage compute -o %s 2>&1 -entry main", input_path.str, output_path.str);

    String8 cmd = string8_format(
        temp.arena,
        "slangc %s -target spirv -stage compute -o %s 2>&1 -entry main",
        input_path.str, output_path.str);

    FILE *pipe = popen(cmd.cstr, "r");
    int ret = -1;
    if(pipe){
        ret = pclose(pipe);
    }
    threadctx_temp_end(temp);
    return ret != -1;
}
