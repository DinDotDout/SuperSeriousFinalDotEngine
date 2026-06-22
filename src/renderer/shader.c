internal String8
shader_cache_get_compiled_path(Arena *arena, String8 path){
    u64 shader_hash = hash_map_DOT_ShaderModule_hash(path);
    return string8_format(arena, DOT_COMPILED_SHADER_PATH"%u.spv", shader_hash);
}

internal b32
shader_module_initialized(DOT_ShaderModule *shader_module)
{
    return shader_module->shader_module_handle.handle[0] != 0;
}

internal b32
shader_compile_from_path(String8 input_path, String8 output_path)
{
    TempArena temp = threadctx_get_temp(0);
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
    temp_arena_restore(temp);
    return ret != -1;
}
