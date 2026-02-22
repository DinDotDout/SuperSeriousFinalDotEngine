internal String8
shader_cache_get_compiled_path(Arena *arena, String8 path){
    u64 shader_hash = shader_cache_hash(path);
    return string8_format(arena, DOT_COMPILED_SHADER_PATH"%u.spv", shader_hash);
}

internal b8
shader_compile_from_path(String8 input_path, String8 output_path)
{
    TempArena temp = threadctx_get_temp(0,0);
    DOT_PRINT("Compiling with: slangc %s -target spirv -stage compute -o %s 2>&1 -entry main", input_path.str, output_path.str);

    String8 cmd = string8_format(
        temp.arena,
        "slangc %s -target spirv -stage compute -o %s 2>&1 -entry main",
        input_path.str, output_path.str);

    FILE* pipe = popen(cast(char*)cmd.str, "r");
    int ret = -1;
    if(pipe){
        ret = pclose(pipe);
    }
    temp_arena_restore(temp);
    return ret != -1;
}

internal void
shader_cache_init(Arena *arena, ShaderCache *shader_cache, ShaderCacheConfig *shader_cache_config)
{
    shader_cache->shader_modules_count = shader_cache_config->shader_modules_count;
    shader_cache->shader_modules = PUSH_ARRAY(
        arena,
        ShaderCacheNode*,
        shader_cache->shader_modules_count
    );
}

internal void
shader_cache_end(ShaderCache *shader_cache)
{
    UNUSED(shader_cache);
}

internal u64
shader_cache_hash(String8 shader_path)
{
    u64 shader_hash = u64_hash_from_string8(shader_path, 0);
    return shader_hash;
}

internal HashIdx
shader_cache_hash_idx(ShaderCache *shader_cache, String8 shader_path)
{
    HashIdx shader_hash = shader_cache_hash(shader_path) % shader_cache->shader_modules_count;
    return shader_hash;
}

internal void
shader_cache_push(Arena *arena, ShaderCache *shader_cache, DOT_ShaderModule *shader_module)
{
    ShaderCacheNode *new_node = PUSH_STRUCT(arena, ShaderCacheNode);
    new_node->shader_module = shader_module;

    hashset(ShaderCacheNode*) shader_modules = shader_cache->shader_modules;
    HashIdx shader_hash_idx = shader_cache_hash_idx(shader_cache, shader_module->path);
    ShaderCacheNode *node = shader_modules[shader_hash_idx];
    if(node){
        node->next = new_node;
    }
    shader_modules[shader_hash_idx] = new_node;
}

internal DOT_ShaderModule*
shader_module_create(Arena* arena, String8 path, String8 compiled_path)
{
    DOT_ShaderModule *shader_module = PUSH_STRUCT(arena, DOT_ShaderModule);
    shader_module->compiled_path = compiled_path;
    shader_module->path = path;
    return shader_module;
}

internal b8
shader_module_initialized(DOT_ShaderModule *shader_module)
{
    return shader_module->shader_module_handle.handle[0] != 0;
}

internal DOT_ShaderModule*
shader_cache_get_or_create(Arena *arena, ShaderCache *shader_cache, String8 shader_path, String8 compiled_path)
{
    HashIdx shader_hash_idx = shader_cache_hash_idx(shader_cache, shader_path);
    ShaderCacheNode *node = shader_cache->shader_modules[shader_hash_idx];
    DOT_ShaderModule *shader_module = NULL;
    for EACH_NODE(it, ShaderCacheNode, node){
        if(string8_equal(shader_path, node->shader_module->path)){
            shader_module = node->shader_module;
            break;
        }
    }
    if(!shader_module){
        shader_module = shader_module_create(arena, shader_path, compiled_path);
        shader_cache_push(arena, shader_cache, shader_module);
    }
    return shader_module;
}
