#ifndef SETTING_H 
#define SETTING_H
//
// (jd) NOTE: This isn't really a limit but performance will degrade and become a linear search if load
// factor is high enough
enum{ SETTINGS_SCOPE_BUCKETS_MAX = 32 };
enum{ SETTINGS_BUCKETS_MAX = 64 };

#define DOT_SETTING_KINDS(X) \
    X(DOT_SettingKind, I32) \
    X(DOT_SettingKind, U32) \
    X(DOT_SettingKind, Bool) \
    X(DOT_SettingKind, F32) \
    X(DOT_SettingKind, F32x2) \
    X(DOT_SettingKind, F32x3) \
    X(DOT_SettingKind, F32x4) \
    X(DOT_SettingKind, Mat3) \
    X(DOT_SettingKind, Mat4) \
    X(DOT_SettingKind, String8)
DOT_ENUM_REFLECT_TYPED(u8, DOT_SettingKind, DOT_SETTING_KINDS);

#define DOT_SETTING_VISUALIZATION_KINDS(X) \
    X(DOT_SettingVisualizationKind, Default) \
    X(DOT_SettingVisualizationKind, Slider) \
    X(DOT_SettingVisualizationKind, ColorPicker)
DOT_ENUM_REFLECT_TYPED(u8, DOT_SettingVisualizationKind, DOT_SETTING_VISUALIZATION_KINDS);

typedef struct DOT_Setting DOT_Setting;
struct alignas(16) DOT_Setting
{
    String8 scope_name;
    String8 setting_name;

    String8 whole_name;

    DOT_SettingKind kind;
    DOT_SettingVisualizationKind visualization;
    union{
        i32    *i32_value;
        u32    *u32_value;
        bool   *bool_value;
        f32    *f32_value;
        f32x2  *f32x2_value;
        f32x3  *f32x3_value;
        f32x4  *f32x4_value;
        String8 *String8_value;
    };

    char   *file;
    int     line;

    DOT_Setting *next_setting;
};

typedef struct DOT_SettingScope
{
    String8 scope_name;
    DOT_Setting *setting;
}DOT_SettingScope;

typedef struct DOT_Settings
{
    HashMap(String8, DOT_SettingScope) setting_scopes;
    HashMap(String8, DOT_Setting*) settings;
}DOT_Settings;

global DOT_Settings g_settings = {0};

SECTION_DECLARE(DOT_Setting, SettingsSection);

#define DOT_SETTING(T, section, name, value) DOT_SETTING_TYPED(T, section, name, value, DOT_SettingVisualizationKind_Default)
#define DOT_SETTING_TYPED(k, ctype, section, var, value, viz) \
    ctype var = (value); \
    SECTION_ITEM(SettingsSection) static DOT_Setting DOT_CONCAT(__DOT_Setting_, var) = { \
        .whole_name     = str_lit(section DOT_STR("_"#var)), \
        .scope_name     = str_lit(section), \
        .setting_name   = str_lit(#var), \
        .kind           =  k, \
        .visualization  = (viz), \
        .ctype##_value  = &(var), \
        .file = __FILE__, \
        .line = __LINE__, \
    };

#define DOT_SETTING_I32(section, var, value)       DOT_SETTING_TYPED(DOT_SettingKind_I32, i32, section, var, value, DOT_SettingVisualizationKind_Default)
#define DOT_SETTING_U32(section, var, value)       DOT_SETTING_TYPED(DOT_SettingKind_U32, u32, section, var, value, DOT_SettingVisualizationKind_Default)
#define DOT_SETTING_BOOL(section, var, value)      DOT_SETTING_TYPED(DOT_SettingKind_Bool, bool, section, var, value, DOT_SettingVisualizationKind_Default)
#define DOT_SETTING_F32(section, var, value)       DOT_SETTING_TYPED(DOT_SettingKind_F32, f32, section, var, value, DOT_SettingVisualizationKind_Default)
#define DOT_SETTING_F32X2(section, var, value)     DOT_SETTING_TYPED(DOT_SettingKind_F32x2, f32x2, section, var, value, DOT_SettingVisualizationKind_Default)
#define DOT_SETTING_F32X3(section, var, value)     DOT_SETTING_TYPED(DOT_SettingKind_F32x3, f32x3, section, var, value, DOT_SettingVisualizationKind_Default)
#define DOT_SETTING_F32X4(section, var, value)     DOT_SETTING_TYPED(DOT_SettingKind_F32x4, f32x4, section, var, value, DOT_SettingVisualizationKind_Default)
#define DOT_SETTING_STRING8(section, var, value)   DOT_SETTING_TYPED(DOT_SettingKind_String8, String8, section, var, str_comp(value), DOT_SettingVisualizationKind_Default)
#define DOT_SETTING_COLOR3(section, var, value)    DOT_SETTING_TYPED(DOT_SettingKind_F32x3, f32x3, section, var, value, DOT_SettingVisualizationKind_ColorPicker)
#define DOT_SETTING_COLOR4(section, var, value)    DOT_SETTING_TYPED(DOT_SettingKind_F32x4, f32x4, section, var, value, DOT_SettingVisualizationKind_ColorPicker)

internal inline void dot_settings_register_all(Arena *arena);
internal inline String8 dot_setting_to_string8(Arena *arena, DOT_Setting *setting);

internal inline
void dot_settings_register_all(Arena *arena)
{
    Scratch s = scratch_begin(0,0);
    hm_init_string8(arena, &g_settings.setting_scopes, SETTINGS_SCOPE_BUCKETS_MAX);
    hm_init_string8(arena, &g_settings.settings, SETTINGS_BUCKETS_MAX);
    for EACH_IN_SECTION(SettingsSection, DOT_Setting, setting_it){
        *hm_get_or_create(arena, &g_settings.settings, setting_it->whole_name) = setting_it;
        DOT_SettingScope *setting_scope = hm_get_or_create(arena, &g_settings.setting_scopes, setting_it->scope_name);

        setting_scope->scope_name = setting_it->scope_name;
        setting_it->next_setting = setting_scope->setting;
        setting_scope->setting = setting_it;
        String8 setting_value = dot_setting_to_string8(s.arena, setting_it);
        DOT_PRINT("Setting scope: %S name: %S val: %S", setting_scope->scope_name, setting_it->setting_name, setting_value);
    }
    scratch_end(s);
}

internal String8
dot_setting_to_string8(Arena *a, DOT_Setting *setting)
{
    switch (setting->kind){
    case DOT_SettingKind_I32:   return string8_format(a, "%d", *setting->i32_value);
    case DOT_SettingKind_U32:   return string8_format(a, "%u", *setting->u32_value);
    case DOT_SettingKind_Bool:  return string8_format(a, "%s", *setting->bool_value ? "true" : "false");
    case DOT_SettingKind_F32:   return string8_format(a, "%f", *setting->f32_value);
    case DOT_SettingKind_F32x2:  return string8_format(a, "(%f, %f)", setting->f32x2_value->x, setting->f32x2_value->y);
    case DOT_SettingKind_F32x3:  return string8_format(a, "(%f, %f, %f)", setting->f32x3_value->x, setting->f32x3_value->y, setting->f32x3_value->z);
    case DOT_SettingKind_F32x4:
            return string8_format(a, "(%f, %f, %f, %f)",
                setting->f32x4_value->x, setting->f32x4_value->y,
                setting->f32x4_value->z, setting->f32x4_value->w);

    case DOT_SettingKind_String8: return *setting->String8_value;
    default: return (String8){.str = (u8 *)("<unknown>"), .size = sizeof("<unknown>") - 1};
    }
}

#endif // !SETTING_H
