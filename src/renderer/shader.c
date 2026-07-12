internal String8
rn_shader_cache_get_compiled_path(Arena *arena, String8 path){
    u64 shader_hash = hash_map_RN_ShaderCachedData_hash(path);
    return string8_format(arena, DOT_COMPILED_SHADER_PATH"%u.spv", shader_hash);
}

internal b32
rn_shader_module_is_initialized(RN_ShaderCachedData *shader_module)
{
    return shader_module->shader_stage_handle.handle != 0;
}

internal b32
rn_shader_compile_from_path(u32 argument_count, String8 arguments[], String8 input_path, String8 output_path)
{
    DOT_ASSERT(arguments);
    DOT_UNUSED(argument_count);
    TempArena temp = threadctx_temp_begin(0,0);

    String8 cmd = string8_format(temp.arena, "slangc %S -target spirv -stage %S -o %S 2>&1 -entry main", input_path, arguments[0], output_path);
    DOT_PRINT("Compiling with: %S", cmd);

    FILE *pipe = popen(cmd.cstr, "r");
    threadctx_temp_end(temp);

    int result = 0;
    static char k_process_output_buffer[ 1025 ];
    b32 execute_success = true;
    if(pipe){
        result = wait( NULL );
        u64 read_chunk_size = 1024;
        if (result != -1 ) {
            String8 str = { .size = 1024, .str = PUSH_SIZE(temp.arena, 1024)};
            u64 bytes_read = fread(str.str, 1, str.size, pipe);
            while(bytes_read == read_chunk_size){
                k_process_output_buffer[ bytes_read ] = 0;
                String8 read = str;
                read.size = bytes_read;
                DOT_PRINT("%S", read);
                bytes_read = fread( str.str, 1, str.size, pipe );
            }

            String8 read = str;
            read.size = bytes_read;
            DOT_PRINT("%S", read);
            if(string8_contains(str, string8_lit("error"))){
                execute_success = false;
            }
        }else{
            int err = errno;
            DOT_PRINT( "Execute process error.\n Exe: \"%S\"", output_path);
            DOT_PRINT( "Error: %d\n", err );
            execute_success = false;
        }
        pclose(pipe);
    }else{
        int err = errno;
        DOT_WARNING( "Error: %d\n", err );
        execute_success = false;
    }
    threadctx_temp_end(temp);
    return execute_success;
}
