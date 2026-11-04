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

#define DOT_SETTING(T, scope, var, ...) \
    T var = __VA_ARGS__; \
    SECTION_ITEM(SettingsSection) static DOT_Setting DOT_CONCAT(__DOT_Setting_, var) = { \
        .scope_name   = str_lit(scope), \
        .setting_name = str_lit(#var), \
        .type_def     = rfl_type_def(T), \
        .data         = &(var), \
        .file         = __FILE__, \
        .line         = __LINE__, \
    }

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

#endif // !SETTING_H
