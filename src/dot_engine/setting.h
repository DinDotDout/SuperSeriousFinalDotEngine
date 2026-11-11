#ifndef SETTING_H
#define SETTING_H
//
// (jd) NOTE: This isn't really a limit but performance will degrade and become a linear search if load
// factor is high enough
enum{ SETTINGS_SCOPE_BUCKETS_MAX = 32 };
enum{ SETTINGS_BUCKETS_MAX = 64 };

typedef struct DOT_Setting DOT_Setting;
struct alignas(16) DOT_Setting
{
    String8 scope_name;
    String8 setting_name;

    RFL_Type *type_def;
    void     *data;
    b32       boot;

    char   *file;
    int     line;

    DOT_Setting *next;
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

#define DOT_SETTING_(is_boot, T, scope, var, ...) \
    T var = __VA_ARGS__; \
    SECTION_ITEM(SettingsSection) static DOT_Setting DOT_CONCAT(__DOT_Setting_, var) = { \
        .scope_name   = str_lit(scope), \
        .setting_name = str_lit(#var), \
        .type_def     = rfl_type_def(T), \
        .data         = &(var), \
        .boot         = (is_boot), \
        .file         = __FILE__, \
        .line         = __LINE__, \
    }

#define DOT_SETTING(T, scope, var, ...)      DOT_SETTING_(false, T, scope, var, __VA_ARGS__)
#define DOT_BOOT_SETTING(T, scope, var, ...) DOT_SETTING_(true,  T, scope, var, __VA_ARGS__)

internal inline
void dot_settings_register_all(Arena *arena)
{
    Scratch s = scratch_begin(0,0);
    hm_init_string8(arena, &g_settings.setting_scopes, SETTINGS_SCOPE_BUCKETS_MAX);
    hm_init_string8(arena, &g_settings.settings, SETTINGS_BUCKETS_MAX);
    for EACH_SECTION_ITEM(SettingsSection, DOT_Setting, setting_it){
        *hm_get_or_create(arena, &g_settings.settings, setting_it->setting_name) = setting_it;
        DOT_SettingScope *setting_scope = hm_get_or_create(arena, &g_settings.setting_scopes, setting_it->scope_name);

        setting_scope->scope_name = setting_it->scope_name;
        SLL_STACK_PUSH(setting_scope->setting, setting_it);
        String8 setting_value = rfl_string8_from_any(s.arena, rfl_any_from_type(setting_it->type_def, setting_it->data));
        DOT_PRINT("Setting scope: %S name: %S val: %S", setting_scope->scope_name, setting_it->setting_name, setting_value);
    }
    scratch_end(s);
}

internal String8
dot_settings_config_trim(String8 s)
{
    while(s.size > 0 && (s.str[0] == ' ' || s.str[0] == '\t' || s.str[0] == '\r')){
        s.str  += 1;
        s.size -= 1;
    }
    while(s.size > 0 && (s.str[s.size - 1] == ' ' || s.str[s.size - 1] == '\t' || s.str[s.size - 1] == '\r')){
        s.size -= 1;
    }
    return(s);
}

internal void
dot_settings_apply_config(Arena *arena, String8 path)
{
    if(!os_file_exists(path)){
        DOT_PRINT("No config at %S, using defaults", path);
        return;
    }
    String8 content = os_read_entire_file(arena, path);

    String8 scope = {0};
    u32 line_number = 0;
    i64 line_start = 0;
    for(i64 c = 0; c <= content.size; ++c){
        if(c < content.size && content.str[c] != '\n'){
            continue;
        }
        String8 line = dot_settings_config_trim(string8(content.str + line_start, cast(u64)(c - line_start)));
        line_start = c + 1;
        line_number += 1;

        if(line.size == 0 || line.str[0] == '#' || line.str[0] == ';'){
            continue;
        }
        if(line.str[0] == '['){
            if(line.str[line.size - 1] != ']'){
                DOT_WARNING("%S:%u: missing ']' in scope", path, line_number);
                continue;
            }
            scope = dot_settings_config_trim(string8(line.str + 1, cast(u64)line.size - 2));
            continue;
        }

        i64 equal_at = -1;
        for(i64 i = 0; i < line.size; ++i){
            if(line.str[i] == '='){ equal_at = i; break; }
        }
        if(equal_at < 0){
            DOT_WARNING("%S:%u: expected 'name = value'", path, line_number);
            continue;
        }
        String8 name  = dot_settings_config_trim(string8(line.str, cast(u64)equal_at));
        String8 value = dot_settings_config_trim(string8(line.str + equal_at + 1, cast(u64)(line.size - equal_at - 1)));

        DOT_Setting *found = NULL;
        for EACH_SECTION_ITEM(SettingsSection, DOT_Setting, setting_it){
            if(string8_equal(setting_it->scope_name, scope) && string8_equal(setting_it->setting_name, name)){
                found = setting_it;
                break;
            }
        }
        if(!found){
            DOT_WARNING("%S:%u: unknown setting [%S] %S", path, line_number, scope, name);
        }else if(!rfl_any_set_from_string8(rfl_any_from_type(found->type_def, found->data), value)){
            DOT_WARNING("%S:%u: invalid value '%S' for [%S] %S", path, line_number, value, scope, name);
        }
    }
}

#endif // !SETTING_H
