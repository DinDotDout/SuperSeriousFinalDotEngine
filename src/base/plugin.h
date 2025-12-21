#ifndef PLUGIN_H
#define PLUGIN_H

// NOTE: This is kind of magic but this will place whatever we tag as PLUGIN_SECTION in there
// We can then iterate over the section and run all init plugins whenever we want

typedef struct Plugin Plugin;
// Needs testing on windows
#if defined(DOT_COMPILER_MSVC)
    #pragma section("plugins$a", read)
    #pragma section("plugins$m", read)
    #pragma section("plugins$z", read)

    #define PLUGIN_SECTION __declspec(allocate("plugins$m"))
    __declspec(allocate("plugins$a")) extern const Plugin __start_plugins;
    __declspec(allocate("plugins$z")) extern const Plugin __stop_plugins;
#else
    #define PLUGIN_SECTION __attribute__((section("plugins")))
    extern const Plugin __start_plugins;
    extern const Plugin __stop_plugins;
#endif

// NOTE: Should we pass in Plugin*? the fn already operate on their data but
// we can maybe keep this for debugging
typedef void (*Plugin_InitFn)(Plugin *);
typedef void (*Plugin_EndFn)(Plugin *);

internal inline void Plugin_InitStub(Plugin* p){ Unused(p); }
internal inline void Plugin_EndStub(Plugin* p){ Unused(p); }
internal const Plugin_InitFn plugin_init_stub = Plugin_InitStub;
internal const Plugin_EndFn plugin_end_stub = Plugin_EndStub;
struct Plugin {
    const char *name;
    char *file;
    int line;
    void *user_data;
    i16 init_end_priority; // lower runs earlier
    Plugin_InitFn Init;
    Plugin_EndFn End;
};

#define PluginParams(...) (Plugin){ \
    .name = "Unnamed", \
    .file = __FILE__, \
    .line = __LINE__, \
    .init_end_priority = 0, \
    .Init = Plugin_InitStub, \
    .End = Plugin_EndStub, \
    __VA_ARGS__ \
}

#define PluginRegister(n, ...) \
    PLUGIN_SECTION static const Plugin DOT_CONCAT(Plugin_, name) = (Plugin){ \
        .name = #n, \
        .file = __FILE__, \
        .line = __LINE__, \
        .init_end_priority = 0, \
        .Init = Plugin_InitStub, \
        .End = Plugin_EndStub, \
        __VA_ARGS__ \
    }

void Plugin_RunInit(void) {
    const Plugin* begin = &__start_plugins;
    const Plugin* end = &__stop_plugins;
    for (const Plugin* p = begin; p < end; ++p){
        DOT_PRINT_FL(p->file, p->line, "PLUGIN: %s->Init()", p->name);
        p->Init((Plugin*)p);
    }
}

internal inline void Plugin_RunEnd(){
    const Plugin* begin = &__start_plugins;
    const Plugin* end = &__stop_plugins;
    for (const Plugin* p = begin; p < end; ++p){
        DOT_PRINT_FL(p->file, p->line, "PLUGIN: %s->End()", p->name);
        p->End((Plugin*)p);
    }
}
#endif // !PLUGIN_H
