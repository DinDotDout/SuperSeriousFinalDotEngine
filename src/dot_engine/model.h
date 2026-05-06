#ifndef DOT_MODEL_H
#define DOT_MODEL_H

#include "base/arena.h"
#include "base/string.h"
#include "base/thread_ctx.h"
#include "dot_engine/dot_engine.h"
#include "renderer/renderer.h"
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
    u32         mesh_count;
    DOT_Mesh   *meshes;

    u32 texture_count;
    DOT_TextureAsset *textures;
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
    out->count = acc->count;
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
    out->count = acc->count;
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
        size_t count = acc->count;
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
            DOT_RESTORE_PARTIAL_SWITCH
        }
        // add tangents, colors, joints, weights later
    }
}

internal void
dot_model_from_cgltf(DOT_Renderer *renderer, const cgltf_data *data, String8 gltf_path, DOT_Model *out)
{
    TempArena temp = threadctx_get_temp(0,0);
    String8 folder = string8_chop_last_slash(gltf_path);
    out->mesh_count = data->meshes_count;
    out->meshes = PUSH_ARRAY(renderer->transient_arena, DOT_Mesh, out->mesh_count);

    for (usize mesh_idx = 0; mesh_idx < data->meshes_count; mesh_idx++) {
        cgltf_mesh* src_mesh = &data->meshes[mesh_idx];
        DOT_Mesh* dst_mesh = &out->meshes[mesh_idx];

        dst_mesh->primitive_count = src_mesh->primitives_count;
        dst_mesh->primitives = PUSH_ARRAY(renderer->transient_arena, DOT_Primitive, dst_mesh->primitive_count);

        for (usize primitive_idx = 0; primitive_idx < src_mesh->primitives_count; primitive_idx++) {
            dot_extract_primitive(renderer->transient_arena, &dst_mesh->primitives[primitive_idx], &src_mesh->primitives[primitive_idx]);
        }
    }

    // (jd) NOTE: For now we are going to assume that all images are on a separate path
    for(u64 i = 0; i < data->images_count; ++i){
        TempArena local_temp = temp_arena_get(temp.arena);
        cgltf_image *image = &data->images[i];
        DOT_ASSERT(image);
        DOT_ASSERT(image->uri);
        String8 name = String8Lit("Default");
        if(image->name){
            name = string8_from_cstring(image->name);
        }
        String8 full_path = string8_format(
            temp.arena,
            "%S/%s",
            folder,
            image->uri
            );

        // String8 path = string8_from_cstring(image->uri);
        DOT_PRINT("image name: %S, image uri: %s", name, full_path);
        renderer_texture_asset_create(
            renderer,
            &(DOT_AssetCreateInfo){
                .name = name,
                .path = full_path,
            }, 0);

        temp_arena_restore(local_temp);
    }
    temp_arena_restore(temp);
}

unsigned char* dot_gltf_load_image(
    String8 gltf_path,
    const cgltf_options* options,
    cgltf_image* image,
    int* out_w,
    int* out_h,
    int* out_comp)
{
    TempArena temp = threadctx_get_temp(0,0);
    u8* bytes = NULL;
    usize size = 0;

    if(image->buffer_view){
        cgltf_buffer_view* view = image->buffer_view;
        bytes = (u8*)view->buffer->data + view->offset;
        size  = view->size;
    }else if(image->uri && strncmp(image->uri, "data:", 5) == 0){

        const char* comma = strchr(image->uri, ',');
        const char* base64_data = comma + 1;

        // decode base64 using cgltf's decoder
        void* decoded = NULL;
        cgltf_size decoded_size = strlen(base64_data) * 3 / 4; // safe upper bound

        cgltf_result r = cgltf_load_buffer_base64(
            options,
            decoded_size,
            base64_data,
            &decoded
        );

        if (r != cgltf_result_success) {
            return NULL;
        }

        bytes = (u8*)decoded;
        size  = decoded_size;
    }else if (image->uri){
        String8 folder = string8_chop_last_slash(gltf_path);
        String8 full_path = string8_format(
            temp.arena,
            "%S/%s",
            folder,
            image->uri
        );

        String8 buff = platform_read_entire_file(temp.arena, full_path);
        if (buff.size == 0) return NULL;
    }else{
        // Invalid glTF image
        return NULL;
    }

    // Decode PNG/JPG/WebP/etc using stb_image
    //
    int w = 0, h = 0, comp = 0;
    u8 *pixels = pixels;
    if(bytes != NULL){
        pixels = stbi_load_from_memory(bytes, (int)size, &w, &h, &comp, 4);
    }

    *out_w = w;
    *out_h = h;
    *out_comp = comp;
    temp_arena_restore(temp);
    return pixels;
}

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
    TempArena temp = threadctx_get_temp(0,0);
    DOT_Model model = {0};
    cgltf_data *gltf = dot_gltf_load_from_path(temp.arena, path);
    if(gltf){
        dot_model_from_cgltf(renderer, gltf, path, &model);
    }
    temp_arena_restore(temp);
	return(model);
}

#endif // DOT_MODEL_H!
