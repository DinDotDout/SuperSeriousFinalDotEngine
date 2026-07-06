#ifndef PLUGIN_H
#define PLUGIN_H
#if DOT_COMPILER_MSVC
#   pragma section(".plugin", read)
#   pragma comment(linker, "/merge:.plugin=.data")
#endif

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
internal const PluginInitFn plugin_init_stub_pfn    = plugin_init_stub;
internal const PluginEndFn plugin_end_stub_pfn      = plugin_end_stub;
struct Plugin{
    const char *name;
    char *file;
    u32 line;
    void *user_data;
    PluginInitFn init;
    PluginEndFn end;
};
DECLARE_SECTION(const Plugin, PluginsSection);

#define PLUGIN_REGISTER(n, priority, ...) \
    SECTION_ITEM(DOT_plugins, priority) static const Plugin DOT_CONCAT(__DOT_Plugin_, n) = (Plugin){ \
        .name = #n, \
        .file = __FILE__, \
        .line = __LINE__, \
        .init = plugin_init_stub_pfn, \
        .end = plugin_end_stub_pfn, \
        __VA_ARGS__ \
    }

void plugins_init(void) {
    for EACH_IN_SECTION(PluginsSection, const Plugin, p){
        DOT_PRINT_FL(p->file, p->line, "PLUGIN: %s->Init()", p->name);
        p->init();
    }
}

internal inline void plugins_shutdown(){
    for EACH_IN_SECTION(PluginsSection, const Plugin, p){
        DOT_PRINT_FL(p->file, p->line, "PLUGIN: %s->End()", p->name);
        p->end();
    }
}
#endif // !PLUGIN_H
