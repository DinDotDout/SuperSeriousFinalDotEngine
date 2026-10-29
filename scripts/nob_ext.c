#ifndef NOB_H_
#include "../src/third_party/nob.h/nob.h"
#endif

////////////////////////////////////////////////////////////////
//
// Target kinds

typedef enum
{
    OsKind_Linux,
    OsKind_Windows,
    OsKind_Count,
}OsKind;

static const char *g_os_kind_str[] = {
    [OsKind_Linux]   = "Linux",
    [OsKind_Windows] = "Windows",
};

typedef enum
{
    WindowKind_X11,
    WindowKind_Wayland,
    WindowKind_Win32,
    WindowKind_Count,
}WindowKind;

static const char *g_window_kind_str[] = {
    [WindowKind_X11]     = "X11",
    [WindowKind_Wayland] = "Wayland",
    [WindowKind_Win32]   = "Win32",
};

typedef enum
{
    CompilerKind_Clang,
    CompilerKind_Msvc,
    CompilerKind_Count,
}CompilerKind;

static const char *g_compiler_kind_str[] = {
    [CompilerKind_Clang] = "clang",
    [CompilerKind_Msvc]  = "cl.exe",
};

typedef enum
{
    OptimizationLevelKind_Debug,
    OptimizationLevelKind_Release,
    OptimizationLevelKind_Final,
}OptimizationLevelKind;

typedef uint32_t b32;

typedef struct
{
    OsKind os;
    WindowKind window;
    CompilerKind compiler;
    OptimizationLevelKind opt;
    b32 asan;
    b32 analyze;
    const char *msvc_sysroot;
}Target;

////////////////////////////////////////////////////////////////
//
// String lists

#ifndef STR_LIST_MAX
#   define STR_LIST_MAX 32
#endif

typedef struct
{
    const char *items[STR_LIST_MAX];
}StrList;

static void
str_list_push(StrList *list, const char *item)
{
    for(size_t i = 0; i < STR_LIST_MAX; i++){
        if(!list->items[i]){
            list->items[i] = item;
            return;
        }
    }
    NOB_ASSERT(false && "StrList is full, raise STR_LIST_MAX");
}

static void
append_list(Nob_Cmd *cmd, const char *fmt, const StrList *list)
{
    for(size_t i = 0; i < STR_LIST_MAX && list->items[i]; i++){
        nob_cmd_append(cmd, nob_temp_sprintf(fmt, list->items[i]));
    }
}

////////////////////////////////////////////////////////////////
//
// Per platform and per compiler config

typedef struct
{
    StrList defines;
    StrList libs;
}PlatformConfig;

typedef struct
{
    StrList flags;
    StrList warnings;
    StrList link_flags;
}CompilerConfig;

////////////////////////////////////////////////////////////////
//
// Files

static bool
mkdir_parents(const char *file_path)
{
    char *path = nob_temp_strdup(file_path);
    for(char *p = path; *p; ++p){
        if(*p == '/' && p != path){
            *p = 0;
            bool ok = nob_mkdir_if_not_exists(path);
            *p = '/';
            if(!ok){
                return(false);
            }
        }
    }
    return(true);
}

////////////////////////////////////////////////////////////////
//
// Flags

_Static_assert(sizeof(OsKind)                == sizeof(uint32_t), "FlagWrite needs 4-byte fields");
_Static_assert(sizeof(WindowKind)            == sizeof(uint32_t), "FlagWrite needs 4-byte fields");
_Static_assert(sizeof(CompilerKind)          == sizeof(uint32_t), "FlagWrite needs 4-byte fields");
_Static_assert(sizeof(OptimizationLevelKind) == sizeof(uint32_t), "FlagWrite needs 4-byte fields");
_Static_assert(sizeof(Nob_Log_Level)         == sizeof(uint32_t), "FlagWrite needs 4-byte fields");

typedef struct
{
    void *dst;      // must point at a 4-byte field
    uint32_t value;
}FlagWrite;

#ifndef FLAG_WRITES_MAX
#   define FLAG_WRITES_MAX 3
#endif

typedef struct
{
    const char *flag;
    const char *description;
    FlagWrite writes[FLAG_WRITES_MAX];
    void (*apply)(void);
}FlagEntry;

static void
flag_apply(const FlagEntry *entry)
{
    for(size_t i = 0; i < NOB_ARRAY_LEN(entry->writes) && entry->writes[i].dst; i++){
        *(uint32_t *)entry->writes[i].dst = entry->writes[i].value;
    }
    if(entry->apply) entry->apply();
}

static void
print_help(const char *prog, const FlagEntry *flags, size_t count)
{
    printf("Usage: %s [flags]\n\n", prog);
    printf("flags:\n");
    for(size_t i = 0; i < count; i++){
        printf("  %-16s %s\n", flags[i].flag, flags[i].description);
    }
    printf("  %-16s Show this help\n", "-help");
}

static bool
parse_flags(int argc, char **argv, const FlagEntry *flags, size_t count)
{
    for(int i = 1; i < argc; i++){
        if(strcmp(argv[i], "-help") == 0){
            print_help(argv[0], flags, count);
            exit(0);
        }
        bool matched = false;
        for(size_t f = 0; f < count; f++){
            if(strcmp(argv[i], flags[f].flag) == 0){
                flag_apply(&flags[f]);
                matched = true;
                break;
            }
        }
        if(!matched){
            fprintf(stderr, "Unknown flag: %s\n", argv[i]);
            print_help(argv[0], flags, count);
            return(false);
        }
    }
    return(true);
}

////////////////////////////////////////////////////////////////
//
// Target setup

static Target
target_host(void)
{
    Target target = {0};
#if defined(_WIN32)
    target.os       = OsKind_Windows;
    target.window   = WindowKind_Win32;
    target.compiler = CompilerKind_Msvc;
#else
    target.os       = OsKind_Linux;
    target.window   = WindowKind_X11;
    target.compiler = CompilerKind_Clang;
#endif
    return(target);
}

static bool
target_check(Target *target)
{
    if((target->os == OsKind_Windows) != (target->window == WindowKind_Win32)){
        nob_log(NOB_ERROR, "Invalid target: %s can't use %s windowing", g_os_kind_str[target->os], g_window_kind_str[target->window]);
        return(false);
    }
    if(target->compiler == CompilerKind_Msvc && target->os != OsKind_Windows){
        nob_log(NOB_ERROR, "cl can only target windows (use -windows)");
        return(false);
    }
    if(target->analyze && target->compiler != CompilerKind_Msvc){
        nob_log(NOB_ERROR, "-analyze needs the MSVC compiler (use -windows -cl)");
        return(false);
    }
#if !defined(_WIN32)
    if(target->compiler == CompilerKind_Clang && target->os == OsKind_Windows){
        target->msvc_sysroot = getenv("MSVC_SYSROOT");
        if(!target->msvc_sysroot || !nob_file_exists(target->msvc_sysroot)){
            nob_log(NOB_ERROR, "MSVC_SYSROOT must point to an msvc-wine install to target windows from this host");
            return(false);
        }
    }
#endif
    return(true);
}

static void
append_clang_windows_flags(Nob_Cmd *cmd, const Target *target, bool link)
{
    nob_cmd_append(cmd, "--target=x86_64-pc-windows-msvc"); // plain clang driver emitting MSVC-ABI code, not clang-cl
    if(link){
        nob_cmd_append(cmd, "-fuse-ld=lld");                // link with lld-link instead of link.exe
    }
    if(target->msvc_sysroot){
        nob_cmd_append(cmd, "-Xmicrosoft-windows-sys-root", target->msvc_sysroot); // Windows SDK + CRT location when not on windows
    }
}

////////////////////////////////////////////////////////////////
//
// Downloads

typedef struct
{
    const char *url;
    const char *rev;
    const char *dir;
}Download;

static bool
extract_archive(const char *archive_path, const char *dst_dir)
{
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "tar", "-xzf", archive_path, "-C", dst_dir);
    if(!nob_cmd_run(&cmd)){
        nob_log(NOB_ERROR, "Failed to extract %s", archive_path);
        return(false);
    }
    return(true);
}

static bool
extract_zip_member(const char *zip_path, const char *member, const char *dst_dir)
{
    Nob_Cmd cmd = {0};
#if defined(_WIN32)
    nob_cmd_append(&cmd, "tar", "-xf", zip_path, "-C", dst_dir, member);
#else
    nob_cmd_append(&cmd, "unzip", "-o", "-q", "-j", zip_path, member, "-d", dst_dir);
#endif
    if(!nob_cmd_run(&cmd)){
        nob_log(NOB_ERROR, "Failed to extract %s from %s", member, zip_path);
#if !defined(_WIN32)
        nob_log(NOB_ERROR, "Is unzip installed? (it is packaged as unzip on most distros)");
#endif
        return(false);
    }
    return(true);
}

static bool
download_file(const char *url, const char *dst_path)
{
    nob_log(NOB_INFO, "downloading %s (delete it to redownload)", url);
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "curl", "-L", "--fail", "-o", dst_path, url);
    if(!nob_cmd_run(&cmd)){
        nob_log(NOB_ERROR, "Failed to download %s", url);
        return(false);
    }
    return(true);
}

static bool
download_all(const Download *downloads, size_t count)
{
    bool ok = true;
    for(size_t i = 0; ok && i < count; ++i){
        const Download *download = &downloads[i];
        size_t temp_checkpoint = nob_temp_save();
        if(!mkdir_parents(nob_temp_sprintf("%s/", download->dir))){
            return(false);
        }

        const char *name          = nob_path_name(download->url);
        const char *final_path    = nob_temp_sprintf("%s/%s", download->dir, name);
        const char *download_path = final_path;
        const char *source_url    = download->url;
        if(download->rev){
            source_url    = nob_temp_sprintf("%s/archive/%s.tar.gz", download->url, download->rev);
            download_path = nob_temp_sprintf("%s/%s.tar.gz", download->dir, name);
        }

        if(nob_file_exists(final_path)){
            nob_log(NOB_INFO, "Already have: %s (delete it to redownload)", final_path);
        }else{
            ok = download_file(source_url, download_path);
            if(ok && download->rev){
                const char *extracted = nob_temp_sprintf("%s/%s-%s", download->dir, name, download->rev);
                ok = extract_archive(download_path, download->dir) && nob_rename(extracted, final_path);
            }
        }
        nob_temp_rewind(temp_checkpoint);
    }
    return(ok);
}

////////////////////////////////////////////////////////////////
//
// Prebuilt libraries

typedef struct
{
    const char *name;
    const char *lib;
    const char *dirs[OsKind_Count];
    StrList includes;
    StrList defines;
    PlatformConfig os[OsKind_Count];
    const char *missing_hint;
}Prebuilt;

static const char *
prebuilt_link_file(const Target *target, const Prebuilt *prebuilt)
{
    if(target->os == OsKind_Windows){
        return(nob_temp_sprintf("%s%s.lib", prebuilt->dirs[target->os], prebuilt->lib));
    }
    return(nob_temp_sprintf("%slib%s.so", prebuilt->dirs[target->os], prebuilt->lib));
}

static bool
prebuilt_ready(const Target *target, const Prebuilt *prebuilt, const char *out_dir)
{
    const char *link_file = prebuilt_link_file(target, prebuilt);
    if(!nob_file_exists(link_file)){
        nob_log(NOB_ERROR, "%s is not built for %s (missing %s)", prebuilt->name, g_os_kind_str[target->os], link_file);
        if(prebuilt->missing_hint){
            nob_log(NOB_ERROR, "%s", prebuilt->missing_hint);
        }
        return(false);
    }
    if(target->os == OsKind_Windows){
        return(nob_copy_file(nob_temp_sprintf("%s%s.dll", prebuilt->dirs[target->os], prebuilt->lib), nob_temp_sprintf("%s%s.dll", out_dir, prebuilt->lib)));
    }
    return(true);
}

////////////////////////////////////////////////////////////////
//
// C++ recipes (maintainer only, builds a Prebuilt)

typedef struct
{
    const char *src_dir;
    const char *obj_dir;
    StrList sources;
    StrList defines;
    PlatformConfig os[OsKind_Count];
    const Prebuilt *output;
}CppRecipe;

static bool
cpp_recipe_build(const Target *target, const CppRecipe *recipe)
{
    const Prebuilt *out = recipe->output;
    if(!nob_file_exists(recipe->src_dir)){
        nob_log(NOB_ERROR, "%s sources are missing (%s)", out->name, recipe->src_dir);
        return(false);
    }

    const char *folder = out->dirs[target->os];
    if(!mkdir_parents(folder) || !mkdir_parents(recipe->obj_dir)) return(false);

    StrList sources = {0};
    for(size_t i = 0; i < STR_LIST_MAX && recipe->sources.items[i]; i++){
        sources.items[i] = nob_temp_sprintf("%s%s", recipe->src_dir, recipe->sources.items[i]);
    }

    Nob_Cmd cmd = {0};
    switch(target->compiler){
    case CompilerKind_Msvc:{
        nob_cmd_append(&cmd, "cl.exe", "/nologo", "/std:c++17", "/O2", "/W0", "/EHsc", "/MT", "/LD");
        append_list(&cmd, "%s", &sources);
        append_list(&cmd, "/D%s", &recipe->defines);
        append_list(&cmd, "/D%s", &recipe->os[target->os].defines);
        nob_cmd_append(&cmd, nob_temp_sprintf("/Fe:%s%s.dll", folder, out->lib), nob_temp_sprintf("/Fo:%s", recipe->obj_dir));
        nob_cmd_append(&cmd, "/link", "/Brepro"); // Leave time stamp alone so it stops showing as changes in git
        append_list(&cmd, "%s.lib", &recipe->os[target->os].libs);
    }break;
    case CompilerKind_Clang:{
        nob_cmd_append(&cmd, "clang++", "-std=c++17", "-O2", "-w", "-shared", "-fvisibility=hidden");
        append_list(&cmd, "%s", &sources);
        append_list(&cmd, "-D%s", &recipe->defines);
        append_list(&cmd, "-D%s", &recipe->os[target->os].defines);
        append_list(&cmd, "-l%s", &recipe->os[target->os].libs);
        if(target->os == OsKind_Windows){
            append_clang_windows_flags(&cmd, target, true);
            nob_cmd_append(&cmd, "-fms-runtime-lib=static", "-Wl,/Brepro");
            nob_cmd_append(&cmd, "-o", nob_temp_sprintf("%s%s.dll", folder, out->lib));
        }else{
            nob_cmd_append(&cmd, "-fPIC", "-static-libstdc++", "-static-libgcc");
            nob_cmd_append(&cmd, "-o", nob_temp_sprintf("%slib%s.so", folder, out->lib));
        }
    }break;
    case CompilerKind_Count: NOB_UNREACHABLE("CompilerKind_Count");
    }
    nob_log(NOB_INFO, "[*] Building %s for %s", out->name, g_os_kind_str[target->os]);
    return(nob_cmd_run(&cmd));
}

////////////////////////////////////////////////////////////////
//
// Executable

typedef struct
{
    const char *path;
    bool quiet;
}Source;

typedef struct
{
    const char *name;
    const char *out_dir;
    const char *obj_dir;
    Source sources[STR_LIST_MAX];
    StrList includes;
    StrList defines;
    PlatformConfig os[OsKind_Count];
    PlatformConfig window[WindowKind_Count];
    CompilerConfig compiler[CompilerKind_Count];
    const Prebuilt *prebuilts[4];
}Exe;

static void
exe_add_source(Exe *exe, const char *path, bool quiet)
{
    for(size_t i = 0; i < STR_LIST_MAX; i++){
        if(!exe->sources[i].path){
            exe->sources[i] = (Source){ .path = path, .quiet = quiet };
            return;
        }
    }
    NOB_ASSERT(false && "Exe sources are full, raise STR_LIST_MAX");
}

static void
append_compile_flags(Nob_Cmd *cmd, const Target *target, const Exe *exe, bool warnings)
{
    const CompilerConfig *compiler = &exe->compiler[target->compiler];
    append_list(cmd, "%s", &compiler->flags);
    if(warnings){
        append_list(cmd, "%s", &compiler->warnings);
    }

    switch(target->compiler){
    case CompilerKind_Msvc:{
        nob_cmd_append(cmd, "/nologo");
        if(!warnings){
            nob_cmd_append(cmd, "/W0");
        }else if(target->analyze){
            nob_cmd_append(cmd, "/analyze");
        }
        switch(target->opt){
            case OptimizationLevelKind_Debug:   nob_cmd_append(cmd, "/Od", "/Zi", "/DDEBUG", "/MTd"); break; // static debug CRT, the debug CRT dlls only ship with Visual Studio
            case OptimizationLevelKind_Release: nob_cmd_append(cmd, "/O2", "/Zi", "/MD"); break;             // dll CRT (vcruntime140.dll + ucrtbase.dll)
            case OptimizationLevelKind_Final:   nob_cmd_append(cmd, "/O2", "/DNDEBUG", "/GL", "/MT"); break; // /GL whole program optimization
        }
    }break;
    case CompilerKind_Clang:{
        if(!warnings){
            nob_cmd_append(cmd, "-w");
        }
        switch(target->opt){
            case OptimizationLevelKind_Debug:   nob_cmd_append(cmd, "-O0", "-g", "-DDEBUG"); break;
            case OptimizationLevelKind_Release: nob_cmd_append(cmd, "-O2", "-g"); break;
            case OptimizationLevelKind_Final:   nob_cmd_append(cmd, "-O3", "-DNDEBUG", "-Wno-unused-but-set-variable"); break;
        }
        if(target->asan && target->opt == OptimizationLevelKind_Debug){
            nob_cmd_append(cmd, "-fsanitize=address,undefined", "-fsanitize-recover=address,undefined");
        }
        if(target->os == OsKind_Windows){
            append_clang_windows_flags(cmd, target, false);
        }
    }break;
    case CompilerKind_Count: NOB_UNREACHABLE("CompilerKind_Count");
    }

    append_list(cmd, "-I%s", &exe->includes);
    append_list(cmd, "-D%s", &exe->defines);
    append_list(cmd, "-D%s", &exe->os[target->os].defines);
    append_list(cmd, "-D%s", &exe->window[target->window].defines);
    for(size_t i = 0; i < NOB_ARRAY_LEN(exe->prebuilts) && exe->prebuilts[i]; i++){
        append_list(cmd, "-I%s", &exe->prebuilts[i]->includes);
        append_list(cmd, "-D%s", &exe->prebuilts[i]->defines);
        append_list(cmd, "-D%s", &exe->prebuilts[i]->os[target->os].defines);
    }
}

static void
append_link_flags(Nob_Cmd *cmd, const Target *target, const Exe *exe, const char *exe_path)
{
    switch(target->compiler){
    case CompilerKind_Msvc:{
        nob_cmd_append(cmd, "/nologo", nob_temp_sprintf("/Fe:%s", exe_path));
        if(target->opt != OptimizationLevelKind_Final){
            nob_cmd_append(cmd, "/Zi");
        }
        nob_cmd_append(cmd, "/link"); // everything after this goes to link.exe, keep it last
        append_list(cmd, "%s", &exe->compiler[target->compiler].link_flags);
        append_list(cmd, "%s.lib", &exe->os[target->os].libs);
        append_list(cmd, "%s.lib", &exe->window[target->window].libs);
        for(size_t i = 0; i < NOB_ARRAY_LEN(exe->prebuilts) && exe->prebuilts[i]; i++){
            append_list(cmd, "%s.lib", &exe->prebuilts[i]->os[target->os].libs);
        }
    }break;
    case CompilerKind_Clang:{
        if(target->os == OsKind_Windows){
            append_clang_windows_flags(cmd, target, true);
        }
        append_list(cmd, "%s", &exe->compiler[target->compiler].link_flags);
        append_list(cmd, "-l%s", &exe->os[target->os].libs);
        append_list(cmd, "-l%s", &exe->window[target->window].libs);
        for(size_t i = 0; i < NOB_ARRAY_LEN(exe->prebuilts) && exe->prebuilts[i]; i++){
            append_list(cmd, "-l%s", &exe->prebuilts[i]->os[target->os].libs);
        }
        if(target->asan && target->opt == OptimizationLevelKind_Debug){
            nob_cmd_append(cmd, "-fsanitize=address,undefined");
        }
    }break;
    case CompilerKind_Count: NOB_UNREACHABLE("CompilerKind_Count");
    }

    for(size_t i = 0; i < NOB_ARRAY_LEN(exe->prebuilts) && exe->prebuilts[i]; i++){
        const Prebuilt *prebuilt = exe->prebuilts[i];
        if(target->os == OsKind_Windows){
            nob_cmd_append(cmd, prebuilt_link_file(target, prebuilt));
        }else{
            nob_cmd_append(cmd, nob_temp_sprintf("-L%s", prebuilt->dirs[target->os]), nob_temp_sprintf("-l%s", prebuilt->lib));
            nob_cmd_append(cmd, nob_temp_sprintf("-Wl,-rpath,$ORIGIN/../%s", prebuilt->dirs[target->os]));
        }
    }

    if(target->compiler == CompilerKind_Clang){
        nob_cmd_append(cmd, "-o", exe_path);
    }
}

static const char *
object_path(const Target *target, const Exe *exe, const char *source)
{
    const char *name = nob_path_name(source);
    const char *dot  = strrchr(name, '.');
    int length = dot ? (int)(dot - name) : (int)strlen(name);
    return(nob_temp_sprintf("%s%.*s%s", exe->obj_dir, length, name, target->compiler == CompilerKind_Msvc ? ".obj" : ".o"));
}

static bool
compile_object(Nob_Procs *procs, const Target *target, const Exe *exe, const char *source, const char *object, bool warnings)
{
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, g_compiler_kind_str[target->compiler]);
    append_compile_flags(&cmd, target, exe, warnings);
    switch(target->compiler){
        case CompilerKind_Msvc:  nob_cmd_append(&cmd, "/c", source, nob_temp_sprintf("/Fo:%s", object), nob_temp_sprintf("/Fd:%s.pdb", object)); break;
        case CompilerKind_Clang: nob_cmd_append(&cmd, "-c", source, "-o", object); break;
        case CompilerKind_Count: NOB_UNREACHABLE("CompilerKind_Count");
    }
    nob_log(NOB_INFO, "[*] Compiling %s", source);
    return(nob_cmd_run(&cmd, .async = procs));
}

static bool
exe_build(const Target *target, const Exe *exe)
{
    if(!mkdir_parents(exe->out_dir) || !mkdir_parents(exe->obj_dir)) return(false);
    for(size_t i = 0; i < NOB_ARRAY_LEN(exe->prebuilts) && exe->prebuilts[i]; i++){
        if(!prebuilt_ready(target, exe->prebuilts[i], exe->out_dir)) return(false);
    }

    nob_log(NOB_INFO, "Using compiler %s for target %s %s", g_compiler_kind_str[target->compiler], g_os_kind_str[target->os], g_window_kind_str[target->window]);
    Nob_Procs procs = {0};
    Nob_Cmd link = {0};
    nob_cmd_append(&link, g_compiler_kind_str[target->compiler]);
    for(size_t i = 0; i < STR_LIST_MAX && exe->sources[i].path; i++){
        const Source *source = &exe->sources[i];
        const char *object = object_path(target, exe, source->path);
        if(!compile_object(&procs, target, exe, source->path, object, !source->quiet)) return(false);
        nob_cmd_append(&link, object);
    }
    if(!nob_procs_flush(&procs)) return(false);

    const char *exe_path = nob_temp_sprintf("%s%s%s", exe->out_dir, exe->name, target->os == OsKind_Windows ? ".exe" : "");
    append_link_flags(&link, target, exe, exe_path);
    nob_log(NOB_INFO, "[*] Linking %s", exe_path);
    return(nob_cmd_run(&link));
}

////////////////////////////////////////////////////////////////
//
// Wayland protocols (generated sources added to an Exe)

typedef struct
{
    const char *xml;
    const char *stem;
}WaylandProtocol;

static bool
wayland_scan(const char *mode, const char *xml, const char *out)
{
    if(nob_file_exists(out)) return(true);
    if(!nob_file_exists(xml)){
        nob_log(NOB_ERROR, "Missing Wayland protocol %s", xml);
        nob_log(NOB_ERROR, "Install the protocol XMLs (Arch: wayland-protocols, Debian/Ubuntu: wayland-protocols, Fedora: wayland-protocols-devel)");
        nob_log(NOB_ERROR, "If it is installed, it may be too old to ship this protocol; or build with -x11 instead");
        return(false);
    }
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "wayland-scanner", mode, xml, out);
    if(!nob_cmd_run(&cmd)){
        nob_log(NOB_ERROR, "wayland-scanner failed on %s", xml);
        nob_log(NOB_ERROR, "Is it installed? (Arch: wayland, Debian/Ubuntu: libwayland-bin, Fedora: wayland-devel)");
        return(false);
    }
    return(true);
}

static bool
wayland_protocols_add(Exe *exe, const char *gen_dir, const WaylandProtocol *protocols, size_t count)
{
    if(!mkdir_parents(gen_dir)) return(false);
    for(size_t i = 0; i < count; i++){
        const WaylandProtocol *p = &protocols[i];
        const char *header = nob_temp_sprintf("%s%s.h", gen_dir, p->stem);
        const char *code   = nob_temp_sprintf("%s%s.c", gen_dir, p->stem);
        if(!wayland_scan("client-header", p->xml, header) || !wayland_scan("public-code", p->xml, code)){
            return(false);
        }
        exe_add_source(exe, code, true);
    }
    str_list_push(&exe->includes, gen_dir);
    return(true);
}
