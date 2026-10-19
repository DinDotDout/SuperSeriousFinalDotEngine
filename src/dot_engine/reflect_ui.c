internal i64
rfl_ui_int_read(void *data, u32 size, b32 is_signed)
{
    switch(size){
    case 1:  return is_signed ? cast(i64)*cast(i8*)data  : cast(i64)*cast(u8*)data;
    case 2:  return is_signed ? cast(i64)*cast(i16*)data : cast(i64)*cast(u16*)data;
    case 4:  return is_signed ? cast(i64)*cast(i32*)data : cast(i64)*cast(u32*)data;
    default: return *cast(i64*)data;
    }
}

internal void
rfl_ui_int_write(void *data, u32 size, i64 value)
{
    switch(size){
    case 1:  *cast(u8*)data  = cast(u8)value;  break;
    case 2:  *cast(u16*)data = cast(u16)value; break;
    case 4:  *cast(u32*)data = cast(u32)value; break;
    default: *cast(i64*)data = value;          break;
    }
}

internal void
rfl_ui_int(struct nk_context *ctx, char *label, RFL_Any v)
{
    Scratch s = scratch_begin(0,0);
    b32 is_signed = v.type->kind == RFL_TypeKind_I32;
    i64 value = rfl_ui_int_read(v.data, v.type->size, is_signed);
    if(v.type->size > 4){
        nk_label(ctx, cstr_format(s.arena, "%s: 0x%llx", label, value), NK_TEXT_LEFT);
    }else{
        i64 max = is_signed ? (1ll << (v.type->size * 8 - 1)) - 1 : (1ll << (v.type->size * 8)) - 1;
        i64 min = is_signed ? -max - 1 : 0;
        max = DOT_MIN(max, cast(i64)INT32_MAX);
        int edited = cast(int)DOT_CLAMP(value, min, max);
        nk_property_int(ctx, cstr_format(s.arena, "#%s", label), cast(int)min, &edited, cast(int)max, 1, 1.0f);
        if(edited != value){
            rfl_ui_int_write(v.data, v.type->size, edited);
        }
    }
    scratch_end(s);
}

internal void
rfl_ui_float(struct nk_context *ctx, char *label, RFL_Any v)
{
    Scratch s = scratch_begin(0,0);
    char *name = cstr_format(s.arena, "#%s", label);
    if(v.type->size == sizeof(f64)){
        f64 *value = cast(f64*)v.data;
        f64 step = *value == 0.0 ? 0.1 : DOT_MAX(fabs(*value) * 0.1, 1e-6);
        nk_property_double(ctx, name, -DBL_MAX, value, DBL_MAX, step, cast(f32)(step * 0.05));
    }else{
        f32 *value = cast(f32*)v.data;
        f32 step = *value == 0.0f ? 0.1f : DOT_MAX(fabsf(*value) * 0.1f, 1e-6f);
        nk_property_float(ctx, name, -FLT_MAX, value, FLT_MAX, step, step * 0.05f);
    }
    scratch_end(s);
}

internal void
rfl_ui_bool(struct nk_context *ctx, char *label, RFL_Any v)
{
    nk_bool value = rfl_ui_int_read(v.data, v.type->size, false) != 0;
    if(nk_checkbox_label(ctx, label, &value)){
        rfl_ui_int_write(v.data, v.type->size, value ? 1 : 0);
    }
}

internal void
rfl_ui_enum(struct nk_context *ctx, char *label, RFL_Any v)
{
    Scratch s = scratch_begin(0,0);
    i64 value = rfl_ui_int_read(v.data, v.type->size, true);
    int selected = -1;
    const char **items = PUSH_ARRAY(s.arena, const char*, v.type->value_count);
    for(u32 i = 0; i < v.type->value_count; ++i){
        items[i] = cstr_format(s.arena, "%S", v.type->values[i].name);
        if(selected < 0 && v.type->values[i].value == value){
            selected = cast(int)i;
        }
    }
    if(selected < 0){
        nk_layout_row_dynamic(ctx, RFL_UI_ROW_HEIGHT, 1);
        rfl_ui_int(ctx, label, v);
    }else{
        nk_layout_row_dynamic(ctx, RFL_UI_ROW_HEIGHT, 2);
        nk_label(ctx, label, NK_TEXT_LEFT);
        int picked = nk_combo(ctx, items, cast(int)v.type->value_count, selected, RFL_UI_ROW_HEIGHT, nk_vec2(220, 200));
        if(picked != selected){
            rfl_ui_int_write(v.data, v.type->size, v.type->values[picked].value);
        }
    }
    scratch_end(s);
}

internal void
rfl_ui_color(struct nk_context *ctx, char *label, RFL_Any v)
{
    f32 *c = cast(f32*)v.data;
    b32 has_alpha = v.type->size == 4 * sizeof(f32);
    struct nk_colorf color = {.r = c[0], .g = c[1], .b = c[2], .a = has_alpha ? c[3] : 1.0f};
    nk_layout_row_dynamic(ctx, RFL_UI_ROW_HEIGHT, 2);
    nk_label(ctx, label, NK_TEXT_LEFT);
    if(nk_combo_begin_color(ctx, nk_rgb_cf(color), nk_vec2(220, 300))){
        nk_layout_row_dynamic(ctx, 160, 1);
        color = nk_color_picker(ctx, color, has_alpha ? NK_RGBA : NK_RGB);
        nk_combo_end(ctx);
        c[0] = color.r;
        c[1] = color.g;
        c[2] = color.b;
        if(has_alpha){
            c[3] = color.a;
        }
    }
}

internal b32
rfl_ui_is_scalar(RFL_Type *type)
{
    return type->kind == RFL_TypeKind_I32 || type->kind == RFL_TypeKind_U32 || type->kind == RFL_TypeKind_F32;
}

internal b32
rfl_ui_struct_is_compact(RFL_Type *type)
{
    if(type->field_count == 0 || type->field_count > 4){
        return false;
    }
    for(u32 i = 0; i < type->field_count; ++i){
        RFL_Field *field = &type->fields[i];
        if(field->pointer_depth || field->array_count != 1 || !rfl_ui_is_scalar(field->type)){
            return false;
        }
    }
    return true;
}

internal void
rfl_ui_scalar(struct nk_context *ctx, char *label, RFL_Any v)
{
    if(v.type->kind == RFL_TypeKind_F32){
        rfl_ui_float(ctx, label, v);
    }else{
        rfl_ui_int(ctx, label, v);
    }
}

internal void
rfl_ui_field(struct nk_context *ctx, RFL_Any parent, RFL_Field *field)
{
    if(string8_equal(field->tag, str_comp("rfl_skip"))){
        return;
    }
    Scratch s = scratch_begin(0,0);
    char *label = cstr_format(s.arena, "%S", field->name);
    if(field->pointer_depth){
        void *ptr = *cast(void**)(cast(u8*)parent.data + field->offset);
        nk_layout_row_dynamic(ctx, RFL_UI_ROW_HEIGHT, 1);
        nk_label(ctx, cstr_format(s.arena, "%s: %S* %p", label, field->type_name, ptr), NK_TEXT_LEFT);
    }else if(field->array_count > 1){
        char *title = cstr_format(s.arena, "%s[%u]", label, field->array_count);
        void *hash = cast(u8*)parent.data + field->offset;
        if(nk_tree_push_hashed(ctx, NK_TREE_NODE, title, NK_MINIMIZED, cast(char*)&hash, sizeof(hash), cast(int)cast(uptr)field)){
            for(u32 i = 0; i < field->array_count; ++i){
                rfl_ui_any(ctx, cstr_format(s.arena, "[%u]", i), rfl_any_field(parent, field, i));
            }
            nk_tree_pop(ctx);
        }
    }else{
        rfl_ui_any(ctx, label, rfl_any_field(parent, field, 0));
    }
    scratch_end(s);
}

internal void
rfl_ui_inspect(struct nk_context *ctx, RFL_Any v)
{
    if(v.type->kind != RFL_TypeKind_Struct){
        Scratch s = scratch_begin(0,0);
        rfl_ui_any(ctx, cstr_format(s.arena, "%S", v.type->name), v);
        scratch_end(s);
        return;
    }
    for(u32 i = 0; i < v.type->field_count; ++i){
        rfl_ui_field(ctx, v, &v.type->fields[i]);
    }
}

internal void
rfl_ui_any(struct nk_context *ctx, char *label, RFL_Any v)
{
    if(v.type == reflect_type(f32rgb) || v.type == reflect_type(f32rgba)){
        rfl_ui_color(ctx, label, v);
        return;
    }
    switch(v.type->kind){
    case RFL_TypeKind_I32:
    case RFL_TypeKind_U32:
    case RFL_TypeKind_F32:
        nk_layout_row_dynamic(ctx, RFL_UI_ROW_HEIGHT, 1);
        rfl_ui_scalar(ctx, label, v);
        break;
    case RFL_TypeKind_Bool:
        nk_layout_row_dynamic(ctx, RFL_UI_ROW_HEIGHT, 1);
        rfl_ui_bool(ctx, label, v);
        break;
    case RFL_TypeKind_Enum:
        rfl_ui_enum(ctx, label, v);
        break;
    case RFL_TypeKind_Struct:
        if(rfl_ui_struct_is_compact(v.type)){
            nk_layout_row_dynamic(ctx, RFL_UI_ROW_HEIGHT, 1);
            nk_label(ctx, label, NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, RFL_UI_ROW_HEIGHT, cast(int)v.type->field_count);
            for(u32 i = 0; i < v.type->field_count; ++i){
                Scratch s = scratch_begin(0,0);
                rfl_ui_scalar(ctx, cstr_format(s.arena, "%S", v.type->fields[i].name), rfl_any_field(v, &v.type->fields[i], 0));
                scratch_end(s);
            }
        }else if(nk_tree_push_hashed(ctx, NK_TREE_NODE, label, NK_MINIMIZED, cast(char*)&v.data, sizeof(v.data), cast(int)cast(uptr)v.type)){
            rfl_ui_inspect(ctx, v);
            nk_tree_pop(ctx);
        }
        break;
    default:
        break;
    }
}
