#ifndef PLUGIN_H
#define PLUGIN_H

// NOTE: This is kind of magic but this will place whatever we tag as PLUGIN_SECTION in
// a section specifically created for "ONLY" Plugin structs. Priority will determine the
// ordering of how the plugins are placed in the section in case we have cross plugin
// init dependencies.
// We can then iterate over the section and run all init plugins whenever we want


typedef struct Plugin Plugin;
typedef void (*PluginInitFn)();
typedef void (*PluginEndFn)();
internal inline void plugin_init_stub(){ }
internal inline void plugin_end_stub(){ }
internal const PluginInitFn plugin_init_stub_pfn = plugin_init_stub;
internal const PluginEndFn plugin_end_stub_pfn = plugin_end_stub;
struct Plugin{
    const char *name;
    char *file;
    int line;
    void *user_data;
    PluginInitFn init;
    PluginEndFn end;
};

// TODO: Needs testing on windows
#if defined(DOT_COMPILER_MSVC)
    #pragma section("plugins$a", read)
    #pragma section("plugins$m", read)
    #pragma section("plugins$z", read)

    #define PLUGIN_SECTION __declspec(allocate("plugins$m"))
    __declspec(allocate("plugins$a")) extern const Plugin __start_plugins;
    __declspec(allocate("plugins$z")) extern const Plugin __stop_plugins;
#else
    // Plugins will be used as an array, so we have to tell asan to not poison in between elements
    #define PLUGIN_SECTION(priority) \
        __attribute__((used, section(".plugins." #priority))) \
        __attribute__((no_sanitize("address")))

    extern const Plugin __start_plugins[];
    extern const Plugin __stop_plugins[];
#endif

#define PluginParams(...) (Plugin){ \
    .name = "Unnamed", \
    .file = __FILE__, \
    .line = __LINE__, \
    .init = plugin_init_stub, \
    .end = plugin_end_stub, \
    __VA_ARGS__ \
}

#define PluginRegister(n, priority, ...) \
    PLUGIN_SECTION(priority) static const Plugin DOT_CONCAT(__Plugin_, n) = (Plugin){ \
        .name = #n, \
        .file = __FILE__, \
        .line = __LINE__, \
        .init = plugin_init_stub_pfn, \
        .end = plugin_end_stub_pfn, \
        __VA_ARGS__ \
    }

void plugins_init(void) {
    Plugin* begin = (Plugin*) &__start_plugins[0];
    // char* help = (char*)begin;
    // help = help-8;
    // begin = (Plugin*)help;
    const Plugin* end = &__stop_plugins[0];
    DOT_PRINT("PLUGINS INIT!!");
    DOT_PRINT("Plugin count: %td", end - begin);

    DOT_PRINT("%p", begin);
    DOT_PRINT("%p", end);
    for (const Plugin* p = begin; p != end; ++p){
        DOT_PRINT_FL(p->file, p->line, "PLUGIN: %s->Init()", p->name);
        p->init();
    }
}

internal inline void plugins_end(){
    const Plugin* begin = &__start_plugins[0];
    const Plugin* end = &__stop_plugins[0];
    DOT_PRINT("PLUGINS END!!");
    DOT_PRINT("Plugin count: %td", end - begin);
    DOT_PRINT("%p", begin);
    DOT_PRINT("%p", end);
    for (const Plugin* p = begin; p != end; ++p){
        DOT_PRINT_FL(p->file, p->line, "PLUGIN: %s->End()", p->name);
        p->end();
    }
}
#endif // !PLUGIN_H
