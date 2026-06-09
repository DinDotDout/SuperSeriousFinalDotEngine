#ifndef DOT_MODEL_H
#define DOT_MODEL_H

typedef struct DOT_Vec3Array{
    u32     count;
    vec3    *data;
}DOT_Vec3Array;

typedef struct DOT_Vec2Array{
    u32     count;
    vec2    *data;
}DOT_Vec2Array;

typedef struct DOT_Primitive{
    u32  index_count;
    u32 *indices;

    DOT_Vec3Array positions;
    DOT_Vec3Array normals;
    DOT_Vec2Array uvs;
}DOT_Primitive;

typedef struct DOT_Mesh{
    u32             primitive_count;
    DOT_Primitive  *primitives;
}DOT_Mesh;

typedef struct DOT_Model{
    SLICE(DOT_Mesh) meshes;
    SLICE(DOT_SamplerAsset) samplers;
    SLICE(DOT_BufferAsset)  buffers;
    SLICE(DOT_TextureAsset) textures;
}DOT_Model;

internal void*
dot_cgltf_alloc(void *user, cgltf_size size)
{
    Arena *arena = (Arena*)user;
    void *result = PUSH_SIZE(arena, size);
    // DOT_PRINT("allocating %u onto %p", size, result);
    return result;
}

internal void
dot_cgltf_free(void *user, void* ptr)
{
    // DOT_PRINT("freeing memory %p", ptr);
    DOT_UNUSED(user); DOT_UNUSED(ptr);
}

internal void
dot_extract_vec2(Arena *arena, DOT_Vec2Array *out, cgltf_accessor *acc)
{
    DOT_ASSERT(acc->count <= U32_MAX);
    out->count = cast(u32)acc->count;
    out->data = PUSH_ARRAY(arena, vec2, out->count);
    for (size_t i = 0; i < acc->count; i++) {
        float v[2];
        cgltf_accessor_read_float(acc, i, v, 2);
        out->data[i] = v2(v[0], v[1]);
    }
}

internal void 
dot_extract_vec3(Arena *arena, DOT_Vec3Array *out, cgltf_accessor *acc)
{
    DOT_ASSERT(acc->count <= U32_MAX);
    out->count = cast(u32)acc->count;
    out->data = PUSH_ARRAY(arena, vec3, out->count);

    for(size_t i = 0; i < acc->count; i++){
        float v[3];
        cgltf_accessor_read_float(acc, i, v, 3);
        out->data[i] = v3(v[0], v[1], v[2]);
    }
}

internal void
dot_extract_primitive(Arena *arena, DOT_Primitive *dst, cgltf_primitive *src)
{
    if(src->indices){
        cgltf_accessor* acc = src->indices;
        DOT_ASSERT(acc->count <= U32_MAX);
        u32 count = cast(u32)acc->count;
        dst->index_count = count;
        dst->indices = PUSH_ARRAY(arena, u32, count);

        for(size_t i = 0; i < count; i++){
            dst->indices[i] = cast(u32)cgltf_accessor_read_index(acc, i);
        }
    }

    for(size_t ai = 0; ai < src->attributes_count; ai++){
        cgltf_attribute *attr = &src->attributes[ai];
        cgltf_accessor *acc = attr->data;

        for (int _dot_once = 1; _dot_once; _dot_once = 0) {
            DOT_ALLOW_PARTIAL_SWITCH;
            switch (attr->type) {
            case cgltf_attribute_type_position:
                dot_extract_vec3(arena, &dst->positions, acc); break;
            case cgltf_attribute_type_normal:
                dot_extract_vec3(arena, &dst->normals, acc); break;
            case cgltf_attribute_type_texcoord:
                dot_extract_vec2(arena, &dst->uvs, acc); break;
            default: break;
            }
            DOT_RESTORE_PARTIAL_SWITCH;
        }
        // add tangents, colors, joints, weights later
    }
}

internal DOT_Model
dot_model_from_cgltf(DOT_Renderer *renderer, const cgltf_data *data, String8 gltf_path)
{
    DOT_ASSERT(data->textures_count <= U32_MAX); DOT_ASSERT(data->meshes_count <= U32_MAX);
    DOT_ASSERT(data->samplers_count <= U32_MAX); DOT_ASSERT(data->buffers_count <= U32_MAX);
    DOT_Model model = {
        .textures   = SLICE_CREATE_FROM_ARENA(renderer->transient_arena, DOT_TextureAsset, cast(u32)data->textures_count),
        .buffers    = SLICE_CREATE_FROM_ARENA(renderer->transient_arena, DOT_BufferAsset, cast(u32)data->buffer_views_count),
        .samplers   = SLICE_CREATE_FROM_ARENA(renderer->transient_arena, DOT_SamplerAsset, cast(u32)data->samplers_count),
        .meshes     = SLICE_CREATE_FROM_ARENA(renderer->transient_arena, DOT_Mesh, cast(u32)data->meshes_count),
    };

    TempArena temp = threadctx_get_temp(0);
    String8 folder = string8_chop_last_slash(gltf_path);
    for (usize mesh_idx = 0; mesh_idx < data->meshes_count; mesh_idx++) {
        cgltf_mesh *src_mesh = &data->meshes[mesh_idx];
        DOT_Mesh *dst_mesh = &SLICE_GET(model.meshes, mesh_idx);

        DOT_ASSERT(src_mesh->primitives_count <= U32_MAX);
        dst_mesh->primitive_count = cast(u32)src_mesh->primitives_count;
        dst_mesh->primitives = PUSH_ARRAY(renderer->transient_arena, DOT_Primitive, dst_mesh->primitive_count);

        for (usize primitive_idx = 0; primitive_idx < src_mesh->primitives_count; primitive_idx++) {
            dot_extract_primitive(renderer->transient_arena, &dst_mesh->primitives[primitive_idx], &src_mesh->primitives[primitive_idx]);
        }
    }

    // (jd) NOTE: For now we are going to assume that all images are on a separate path
    for(u64 i = 0; i < data->images_count; ++i){
        cgltf_image *image = &data->images[i];
        String8 name = image->name ? string8_from_cstring(image->name) : String8Lit("Default");
        String8 full_path = string8_format(temp.arena, "%S/%s", folder, image->uri);
        DOT_PRINT("name: %S, image uri: %S", name, full_path);
        SLICE_GET(model.textures, i) = renderer_texture_asset_create(
            renderer,
            DOT_ASSET_CREATE_INFO(.name = name, .path = full_path),
            0);

    }

    for(u64 i = 0; i < data->samplers_count; ++i){
        cgltf_sampler *sampler = &data->samplers[i];
        String8 name = sampler->name ? string8_from_cstring(sampler->name) : String8Lit("Default");
        SLICE_GET(model.samplers, i) = renderer_sampler_asset_create(
            renderer,
            DOT_ASSET_CREATE_INFO(.name = name, .path = gltf_path),
            RENDER_TYPES_SAMPLER_DESC(
                .min_filter = sampler->min_filter == cgltf_filter_type_linear ? RenderTypes_SamplerFilterKind_Linear : RenderTypes_SamplerFilterKind_Nearest,
                .mag_filter = sampler->mag_filter == cgltf_filter_type_linear ? RenderTypes_SamplerFilterKind_Linear : RenderTypes_SamplerFilterKind_Nearest,
            ));
    }

    // (jd) NOTE: We already loaded buffers with cgltf_load_buffers, maybe do this ourselves?
    for(u64 i = 0; i < data->buffer_views_count; ++i){
        cgltf_buffer_view *buffer_view = &data->buffer_views[i];
        cgltf_buffer *buffer = buffer_view->buffer;
        String8 name = buffer_view->name ? string8_from_cstring(buffer_view->name) : String8Lit("Default");
        u8 *buffer_data = cast(u8*)buffer->data + buffer_view->offset;
        SLICE_GET(model.buffers, i) = renderer_buffer_asset_create(
            renderer,
            DOT_ASSET_CREATE_INFO(.name = name, .path = gltf_path),
            RENDER_TYPES_BUFFER_DESC(.size = buffer_view->size,
                .resource_usage = RenderTypes_ResourceUsageKind_GPUOnly,
                .buffer_usage_flags = RenderTypes_BufferUsageBit_Vertex
                                      | RenderTypes_BufferUsageBit_Index),
            buffer_data);
    }
    temp_arena_restore(temp);
    return(model);
}

// unsigned char* dot_gltf_load_image(
//     String8 gltf_path,
//     const cgltf_options* options,
//     cgltf_image* image,
//     int* out_w,
//     int* out_h,
//     int* out_comp)
// {
//     TempArena temp = threadctx_get_temp(0,0);
//     u8* bytes = NULL;
//     usize size = 0;
//
//     if(image->buffer_view){
//         cgltf_buffer_view* view = image->buffer_view;
//         bytes = (u8*)view->buffer->data + view->offset;
//         size  = view->size;
//     }else if(image->uri && strncmp(image->uri, "data:", 5) == 0){
//
//         const char* comma = strchr(image->uri, ',');
//         const char* base64_data = comma + 1;
//
//         // decode base64 using cgltf's decoder
//         void* decoded = NULL;
//         cgltf_size decoded_size = strlen(base64_data) * 3 / 4; // safe upper bound
//
//         cgltf_result r = cgltf_load_buffer_base64(
//             options,
//             decoded_size,
//             base64_data,
//             &decoded
//         );
//
//         if (r != cgltf_result_success) {
//             return NULL;
//         }
//
//         bytes = (u8*)decoded;
//         size  = decoded_size;
//     }else if (image->uri){
//         String8 folder = string8_chop_last_slash(gltf_path);
//         String8 full_path = string8_format(
//             temp.arena,
//             "%S/%s",
//             folder,
//             image->uri
//         );
//
//         String8 buff = platform_read_entire_file(temp.arena, full_path);
//         if (buff.size == 0) return NULL;
//     }else{
//         // Invalid glTF image
//         return NULL;
//     }
//
//     // Decode PNG/JPG/WebP/etc using stb_image
//     //
//     int w = 0, h = 0, comp = 0;
//     u8 *pixels = pixels;
//     if(bytes != NULL){
//         pixels = stbi_load_from_memory(bytes, (int)size, &w, &h, &comp, 4);
//     }
//
//     *out_w = w;
//     *out_h = h;
//     *out_comp = comp;
//     temp_arena_restore(temp);
//     return(pixels);
// }

internal cgltf_data*
dot_gltf_load_from_path(Arena *arena, String8 path)
{
    String8 buff = platform_read_entire_file(arena, path);
    cgltf_options options = {
        .memory = {
            .alloc_func = dot_cgltf_alloc,
            .free_func  = dot_cgltf_free,
            .user_data  = arena,
        }
    };

    cgltf_data *data = NULL;
    cgltf_result result = cgltf_parse(&options, buff.str, buff.size, &data);
    b32 fail_parse = false;
    if(result != cgltf_result_success){
        fail_parse = true;
        DOT_WARNING("cgltf_parse failed: %d", result);
    }
    b32 fail_load_buffers = false;
    if(!fail_parse){
        result = cgltf_load_buffers(&options, data, path.cstr);
        fail_load_buffers = result != cgltf_result_success;
    }
    if(fail_load_buffers){
        DOT_WARNING("cgltf_load_buffers failed: %d", result);
        data = NULL;
    }
	return(data);
}

internal DOT_Model
dot_model_load_from_path(DOT_Renderer *renderer, String8 path)
{
    TempArena temp = threadctx_get_temp(0);
    cgltf_data *gltf = dot_gltf_load_from_path(temp.arena, path);
    DOT_Model model = {0};
    if(gltf){
        model = dot_model_from_cgltf(renderer, gltf, path);
        // cgltf_free(gltf);
    }
    temp_arena_restore(temp);
	return(model);
}

#endif // DOT_MODEL_H!
