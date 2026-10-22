#define NOB_IMPLEMENTATION
#define NOB_NO_ECHO
#include "../src/third_party/nob.h/nob.h"

Nob_Log_Level nob_minimal_log_level = NOB_INFO;

#define OUTPUT_FOLDER "build/"
#define OBJ_FOLDER    "build/obj/"

typedef enum
{
    PlatformKind_LinuxX11,
    PlatformKind_LinuxWayland,
    PlatformKind_Windows,
}PlatformKind;

typedef enum
{
    OptimizationLevelKind_Debug,
    OptimizationLevelKind_Release,
    OptimizationLevelKind_Final,
}OptimizationLevelKind;

typedef enum {
    CompilerKind_Clang,
    CompilerKind_Msvc,
}CompilerKind;

#define WAYLAND_GEN_FOLDER "src/gen-wayland/"

typedef struct {
    const char *xml;
    const char *stem;
}WaylandProtocol;

static const WaylandProtocol g_wayland_protocols[] = {
    {"/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml",                                   "xdg-shell"},
    {"/usr/share/wayland-protocols/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml",           "xdg-decoration-unstable-v1"},
    {"/usr/share/wayland-protocols/staging/xdg-toplevel-icon/xdg-toplevel-icon-v1.xml",               "xdg-toplevel-icon-v1"},
    {"/usr/share/wayland-protocols/unstable/relative-pointer/relative-pointer-unstable-v1.xml",       "relative-pointer-unstable-v1"},
    {"/usr/share/wayland-protocols/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml", "pointer-constraints-unstable-v1"},
    {"/usr/share/wayland-protocols/unstable/xdg-output/xdg-output-unstable-v1.xml",                   "xdg-output-unstable-v1"},
    {"/usr/share/wayland-protocols/staging/pointer-warp/pointer-warp-v1.xml",                         "pointer-warp-v1"},
};

static const char *g_compiler_kind_str[] = {
    [CompilerKind_Clang] = "clang",
    [CompilerKind_Msvc]  = "cl.exe",
};

typedef struct {
    PlatformKind platform;
    OptimizationLevelKind opt;
    CompilerKind compiler;
    bool use_asan;
    bool unity;
    bool no_assets;
    const char *msvc_sysroot;
}BuildFlags;

static BuildFlags g_flags = {
#if defined(_WIN32)
    .platform = PlatformKind_Windows,
    .compiler = CompilerKind_Msvc,
#else
    .platform = PlatformKind_LinuxX11,
    .compiler = CompilerKind_Clang,
#endif
    .opt      = OptimizationLevelKind_Debug,
};

// We don't want libs emmitting warning they were not accounting for
static void
append_compile_flags(Nob_Cmd *cmd, bool warnings)
{
    switch(g_flags.compiler){
    case CompilerKind_Msvc:{
        nob_cmd_append(cmd, warnings ? "/W4" : "/W0", "/std:c11", "/nologo");
        nob_cmd_append(cmd, "/wd4116", "/wd4324");
        switch(g_flags.opt){
            case OptimizationLevelKind_Debug:   nob_cmd_append(cmd, "/Od", "/Zi", "/DDEBUG", "/MTd"); break;
            case OptimizationLevelKind_Release: nob_cmd_append(cmd, "/O2", "/Zi", "/MD"); break;
            case OptimizationLevelKind_Final:   nob_cmd_append(cmd, "/O2", "/DNDEBUG", "/GL", "/MT"); break;
        }
    }break;
    case CompilerKind_Clang:{
        nob_cmd_append(cmd, "-std=c11", "-fvisibility=hidden");
        if(warnings){
            nob_cmd_append(cmd,
                "-Wall", "-Wextra", "-Wno-override-init", "-Wno-incompatible-pointer-types-discards-qualifiers",
                "-Wdiv-by-zero", "-Wno-unused-function",
                "-Werror=vla", "-Werror=implicit-fallthrough", "-Werror=switch-enum",
                "-Werror=conversion", "-Werror=implicit-int-conversion");
        }else{
            nob_cmd_append(cmd, "-w");
        }
        switch(g_flags.opt){
            case OptimizationLevelKind_Debug:   nob_cmd_append(cmd, "-O0", "-g", "-DDEBUG"); break;
            case OptimizationLevelKind_Release: nob_cmd_append(cmd, "-O2", "-g"); break;
            case OptimizationLevelKind_Final:   nob_cmd_append(cmd, "-O3", "-DNDEBUG", "-Wno-unused-but-set-variable"); break;
        }
        if(g_flags.use_asan && g_flags.opt == OptimizationLevelKind_Debug){
            nob_cmd_append(cmd, "-fsanitize=address,undefined", "-fsanitize-recover=address,undefined");
        }
        if(g_flags.platform == PlatformKind_Windows){
            nob_cmd_append(cmd, "--target=x86_64-pc-windows-msvc");
            if(g_flags.msvc_sysroot){
                nob_cmd_append(cmd, "-Xmicrosoft-windows-sys-root", g_flags.msvc_sysroot);
            }
        }
    }break;
    }

    switch(g_flags.platform){
        case PlatformKind_LinuxX11:     nob_cmd_append(cmd, "-DDOT_PLATFORM_LINUX"); break;
        case PlatformKind_LinuxWayland: nob_cmd_append(cmd, "-DRGFW_WAYLAND", "-DRGFW_OPENGL", "-DDOT_PLATFORM_LINUX"); break;
        case PlatformKind_Windows:      nob_cmd_append(cmd, "-DDOT_PLATFORM_WINDOWS", "-D_CRT_SECURE_NO_WARNINGS"); break;
    }

    nob_cmd_append(cmd, "-I", "src/");
    nob_cmd_append(cmd, "-I", "src/third_party/vulkan-headers/include");
    nob_cmd_append(cmd, "-I", "src/third_party/vulkan-utility-libraries/include");
}

static void
append_link_flags(Nob_Cmd *cmd, const char *exe)
{
    switch(g_flags.compiler){
    case CompilerKind_Msvc:{
        nob_cmd_append(cmd, "/nologo", nob_temp_sprintf("/Fe:%s", exe));
        if(g_flags.opt != OptimizationLevelKind_Final){
            nob_cmd_append(cmd, "/Zi");
        }
        nob_cmd_append(cmd, "/link", "/INCREMENTAL:NO", "dbghelp.lib");
    }break;
    case CompilerKind_Clang:{
        switch(g_flags.platform){
        case PlatformKind_Windows:{
            nob_cmd_append(cmd, "--target=x86_64-pc-windows-msvc", "-fuse-ld=lld");
            if(g_flags.msvc_sysroot){
                nob_cmd_append(cmd, "-Xmicrosoft-windows-sys-root", g_flags.msvc_sysroot);
            }
            nob_cmd_append(cmd, "-ldbghelp");
        }break;
        case PlatformKind_LinuxX11:{
            nob_cmd_append(cmd, "-lm", "-lpthread", "-lX11", "-lXcursor", "-lXrandr", "-lXfixes");
        }break;
        case PlatformKind_LinuxWayland:{
            for(size_t i = 0; i < NOB_ARRAY_LEN(g_wayland_protocols); i++){
                nob_cmd_append(cmd, nob_temp_sprintf("%s%s.o", OBJ_FOLDER, g_wayland_protocols[i].stem));
            }
            nob_cmd_append(cmd, "-lm", "-lpthread", "-lwayland-cursor", "-lwayland-client", "-lwayland-egl", "-lxkbcommon");
        }break;
        }
        if(g_flags.use_asan && g_flags.opt == OptimizationLevelKind_Debug){
            nob_cmd_append(cmd, "-fsanitize=address,undefined");
        }
        nob_cmd_append(cmd, "-o", exe);
    }break;
    }
}

static bool
generate_wayland_file(const char *mode, const char *xml, const char *out)
{
    if(nob_file_exists(out)) return(true);
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "wayland-scanner", mode, xml, out);
    return(nob_cmd_run(&cmd));
}

static bool
compile_object(Nob_Procs *procs, const char *src, const char *obj, bool warnings)
{
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, g_compiler_kind_str[g_flags.compiler]);
    append_compile_flags(&cmd, warnings);
    if(!g_flags.unity){
        nob_cmd_append(&cmd, "-DDOT_SPLIT_THIRD_PARTY=1");
    }
    switch(g_flags.compiler){
        case CompilerKind_Msvc:  nob_cmd_append(&cmd, "/c", src, nob_temp_sprintf("/Fo:%s", obj), nob_temp_sprintf("/Fd:%s.pdb", obj)); break;
        case CompilerKind_Clang: nob_cmd_append(&cmd, "-c", src, "-o", obj); break;
    }
    nob_log(NOB_INFO, "[*] Compiling %s", src);
    return(nob_cmd_run(&cmd, .async = procs));
}

static bool
generate_wayland_protocols(Nob_Procs *procs)
{
    if(!nob_mkdir_if_not_exists(WAYLAND_GEN_FOLDER)) return(false);
    for(size_t i = 0; i < NOB_ARRAY_LEN(g_wayland_protocols); i++){
        const WaylandProtocol *p = &g_wayland_protocols[i];
        const char *header = nob_temp_sprintf("%s%s.h", WAYLAND_GEN_FOLDER, p->stem);
        const char *code   = nob_temp_sprintf("%s%s.c", WAYLAND_GEN_FOLDER, p->stem);
        const char *obj    = nob_temp_sprintf("%s%s.o", OBJ_FOLDER, p->stem);
        bool generated = !nob_file_exists(code);
        if(!generate_wayland_file("client-header", p->xml, header) || !generate_wayland_file("public-code", p->xml, code)){
            nob_log(NOB_ERROR, "Failed to generate %s", p->stem);
            return(false);
        }
        if(generated || !nob_file_exists(obj)){
            if(!compile_object(procs, code, obj, false)) return(false);
        }
    }
    return(true);
}

typedef enum ExternalAssetKind
{
    ExternalAssetKind_GitTarGz,
    ExternalAssetKind_File,
}ExternalAssetKind;

typedef struct ExternalAsset
{
    ExternalAssetKind kind;
    union
    {
        struct
        {
            const char *URL;
            const char *rev;
        }git_asset;
        struct
        {
            const char *URL;
        }asset;
    };
}ExternalAsset;

typedef struct ExternalAssets{
    const ExternalAsset *items;
    size_t count;
    const char* output_path;
}ExternalAssets;

#define EXTERNAL_ASSETS(path, list) ((ExternalAssets){ .output_path = (path), .items = (list), .count = NOB_ARRAY_LEN(list) })

static const ExternalAsset assets_list[] = {
    {.kind = ExternalAssetKind_GitTarGz, .git_asset = {"https://github.com/KhronosGroup/glTF-Sample-Assets", "edc7c9e67c639d230715049ee31f9a96a6babbbe"}},
};

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
download_assets(ExternalAssets assets)
{
    if(!mkdir_parents(nob_temp_sprintf("%s/", assets.output_path))){
        return(false);
    }
    bool ok = true;
    for(size_t i = 0; ok && i < assets.count; ++i){
        const ExternalAsset *asset = &assets.items[i];
        size_t temp_checkpoint = nob_temp_save();

        const char *final_path = NULL;
        const char *download_path = NULL;
        const char *source_url = NULL;
        switch(asset->kind){
        case ExternalAssetKind_File:{
            source_url    = asset->asset.URL;
            final_path    = nob_temp_sprintf("%s/%s", assets.output_path, nob_path_name(source_url));
            download_path = final_path;
        }break;
        case ExternalAssetKind_GitTarGz:{
            const char *repo = nob_path_name(asset->git_asset.URL);
            source_url    = nob_temp_sprintf("%s/archive/%s.tar.gz", asset->git_asset.URL, asset->git_asset.rev);
            final_path    = nob_temp_sprintf("%s/%s", assets.output_path, repo);
            download_path = nob_temp_sprintf("%s/%s.tar.gz", assets.output_path, repo);
        }break;
        }

        if(nob_file_exists(final_path)){
            nob_log(NOB_INFO, "Already have: %s (delete it to redownload)", final_path);
        }else{
            ok = download_file(source_url, download_path);
            if(ok && asset->kind == ExternalAssetKind_GitTarGz){
                const char *extracted = nob_temp_sprintf("%s/%s-%s", assets.output_path, nob_path_name(asset->git_asset.URL), asset->git_asset.rev);
                ok = extract_archive(download_path, assets.output_path) && nob_rename(extracted, final_path);
            }
        }
        nob_temp_rewind(temp_checkpoint);
    }
    return(ok);
}

int
main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    for(int i = 1; i < argc; i++){
        if     (strcmp(argv[i], "-x11") == 0)      g_flags.platform = PlatformKind_LinuxX11;
        else if(strcmp(argv[i], "-wayland") == 0)  g_flags.platform = PlatformKind_LinuxWayland;
        else if(strcmp(argv[i], "-windows") == 0)  g_flags.platform = PlatformKind_Windows;
        else if(strcmp(argv[i], "-clang") == 0)    g_flags.compiler = CompilerKind_Clang;
        else if(strcmp(argv[i], "-cl") == 0)       g_flags.compiler = CompilerKind_Msvc;
        else if(strcmp(argv[i], "-debug") == 0)    g_flags.opt = OptimizationLevelKind_Debug;
        else if(strcmp(argv[i], "-release") == 0)  g_flags.opt = OptimizationLevelKind_Release;
        else if(strcmp(argv[i], "-final") == 0)    g_flags.opt = OptimizationLevelKind_Final;
        else if(strcmp(argv[i], "-asan") == 0)     g_flags.use_asan = true;
        else if(strcmp(argv[i], "-unity") == 0)    g_flags.unity = true;
        else if(strcmp(argv[i], "-no_assets") == 0) g_flags.no_assets = true;
        else if(strcmp(argv[i], "-log_warn") == 0) nob_minimal_log_level = NOB_WARNING;
        else{
            fprintf(stderr, "Unknown flag: %s\n", argv[i]);
            fprintf(stderr, "Flags: -x11 -wayland -windows -clang -cl -debug -release -final -asan -unity -no_assets -log_warn\n");
            return 1;
        }
    }

    if(g_flags.opt == OptimizationLevelKind_Final){
        g_flags.unity = true;
    }

    if(g_flags.compiler == CompilerKind_Msvc && g_flags.platform != PlatformKind_Windows){
        nob_log(NOB_ERROR, "cl can only target windows (use -windows)");
        return 1;
    }
#if !defined(_WIN32)
    if(g_flags.compiler == CompilerKind_Clang && g_flags.platform == PlatformKind_Windows){
        g_flags.msvc_sysroot = getenv("MSVC_SYSROOT");
        if(!g_flags.msvc_sysroot || !nob_file_exists(g_flags.msvc_sysroot)){
            nob_log(NOB_ERROR, "MSVC_SYSROOT must point to an msvc-wine install to target windows from this host");
            return 1;
        }
    }
#endif

    if(!nob_mkdir_if_not_exists(OUTPUT_FOLDER) || !nob_mkdir_if_not_exists(OBJ_FOLDER)){
        return 1;
    }

    const char *engine_obj = NULL;
    const char *third_party_obj = NULL;
    switch(g_flags.compiler){
    case CompilerKind_Msvc:{
        engine_obj      = OBJ_FOLDER "dot_engine.obj";
        third_party_obj = OBJ_FOLDER "third_party.obj";
    }break;
    case CompilerKind_Clang:{
        engine_obj      = OBJ_FOLDER "dot_engine.o";
        third_party_obj = OBJ_FOLDER "third_party.o";
    }break;
    }
    const char *exe = g_flags.platform == PlatformKind_Windows ? OUTPUT_FOLDER "dot_engine.exe" : OUTPUT_FOLDER "dot_engine";

    if(!g_flags.no_assets){
        download_assets(EXTERNAL_ASSETS("assets", assets_list));
    }

    Nob_Procs procs = {0};
    if(g_flags.platform == PlatformKind_LinuxWayland){
        if(!generate_wayland_protocols(&procs)) return 1;
    }
    if(!compile_object(&procs, "src/dot_engine/dot_engine.c", engine_obj, true)) return 1;
    if(!g_flags.unity){
        if(!compile_object(&procs, "src/dot_engine/third_party.c", third_party_obj, false)) return 1;
    }
    if(!nob_procs_flush(&procs)) return 1;

    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, g_compiler_kind_str[g_flags.compiler], engine_obj);
    if(!g_flags.unity){
        nob_cmd_append(&cmd, third_party_obj);
    }
    append_link_flags(&cmd, exe);
    nob_log(NOB_INFO, "[*] Linking %s", exe);
    if(!nob_cmd_run(&cmd)) return 1;

    return 0;
}
