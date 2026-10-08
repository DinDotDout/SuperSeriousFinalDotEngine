#define NOB_IMPLEMENTATION
#define NOB_NO_ECHO

// NOTE: Should probably just allow msvc or clang (not clang cl)

#include "../src/third_party/nob.h/nob.h"

Nob_Log_Level nob_minimal_log_level = NOB_INFO;

#define OUTPUT_FOLDER           "build/"
#define SRC_FOLDER              "src/dot_engine/"

#define LINKER_SCRIPTS_FOLDER   "scripts/"

#if defined(_WIN32)
#   define AUTO_PLATFORM PlatformKind_Windows
#   define AUTO_COMPILER CompilerKind_Msvc
#elif defined(__unix__) || defined(__linux__)
#   define AUTO_PLATFORM PlatformKind_LinuxX11
#   define AUTO_COMPILER CompilerKind_Clang
#else
#   define AUTO_PLATFORM PlatformKind_None
#   error "Unsupported platform"
#endif

typedef enum {
    PlatformKind_None,
    PlatformKind_LinuxWayland,
    PlatformKind_LinuxX11,
    PlatformKind_Windows,
    PlatformKind_Auto = AUTO_PLATFORM,
    PlatformKind_Count,
}PlatformKind;

static const char *g_platform_kind_str[] = {
    [PlatformKind_LinuxWayland] = "Linux Wayland",
    [PlatformKind_LinuxX11] = "Linux X11",
    [PlatformKind_Windows] = "Windows",
};

typedef enum {
    OptimizationLevelKind_Debug,
    OptimizationLevelKind_Release,
    OptimizationLevelKind_Final,
}OptimizationLevelKind;

typedef enum {
    CompilerKind_Clang,
    CompilerKind_Msvc,
    CompilerKind_Auto = AUTO_COMPILER,
    CompilerKind_Count,
}CompilerKind;

static const char *g_compiler_kind_str[] = {
    [CompilerKind_Clang] = "clang",
    [CompilerKind_Msvc]  = "cl.exe",
};

typedef struct {
    PlatformKind platform;
    OptimizationLevelKind opt;
    CompilerKind compiler;
    bool use_asan;
}BuildFlags;

static BuildFlags g_flags = {
    .opt      = OptimizationLevelKind_Debug,
    .platform = PlatformKind_Auto,
    .compiler = CompilerKind_Auto,
};

typedef struct {
    const char *flag;
    const char *description;
    void (*apply)(void);
}FlagEntry;

static void set_x11(void)    { g_flags.platform = PlatformKind_LinuxX11; }
static void set_wayland(void){ g_flags.platform = PlatformKind_LinuxWayland; }
static void set_windows(void){ g_flags.platform = PlatformKind_Windows; }

static void set_clang(void)  { g_flags.compiler = CompilerKind_Clang; }
static void set_msvc(void)   { g_flags.compiler = CompilerKind_Msvc; }

static void set_debug(void)  { g_flags.opt = OptimizationLevelKind_Debug; }
static void set_release(void){ g_flags.opt = OptimizationLevelKind_Release; }
static void set_final(void)  { g_flags.opt = OptimizationLevelKind_Final; }

static void set_use_asan(void)  { g_flags.use_asan = true; }

static void set_log_error(void)  {  nob_minimal_log_level = NOB_ERROR; }
static void set_log_warn(void) {  nob_minimal_log_level = NOB_WARNING; }

static const FlagEntry flag_table[] = {
    { "-log_error","Log None",                      set_log_error},
    { "-log_warn","Log Warn",                      set_log_warn},
    { "-x11",     "Use X11 backend",               set_x11     },
    { "-wayland", "Use Wayland backend",           set_wayland },
    { "-windows", "Use Windows backend",           set_windows },

    { "-clang",   "Use Clang compiler",            set_clang   },
    { "-cl",      "Use MSVC compiler",             set_msvc    },

    { "-debug",   "Debug build with sanitizers",   set_debug   },
    { "-release", "Release build with debug info", set_release },
    { "-final",   "Optimized final build",         set_final   },

    { "-asan",   "Use asan",                       set_use_asan },
};

static void print_help(const char *prog)
{
    printf("Usage: %s [g_flags]\n\n", prog);
    printf("g_flags:\n");
    for (size_t i = 0; i < ARRAY_LEN(flag_table); i++) {
        printf("  %-10s %s\n", flag_table[i].flag, flag_table[i].description);
    }
    printf("  %-10s Show this help\n", "--help");
}

// ─────────────────────────────────────────────────────────────────────────────
// Wayland protocol codegen
// ─────────────────────────────────────────────────────────────────────────────

#define WAYLAND_GEN_FOLDER "src/gen-wayland/"

typedef struct
{
    const char *xml;
    const char *stem;
} WaylandProtocol;

static const WaylandProtocol g_wayland_protocols[] = {
    {"/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml",                             "xdg-shell"},
    {"/usr/share/wayland-protocols/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml",     "xdg-decoration-unstable-v1"},
    {"/usr/share/wayland-protocols/staging/xdg-toplevel-icon/xdg-toplevel-icon-v1.xml",         "xdg-toplevel-icon-v1"},
    {"/usr/share/wayland-protocols/unstable/relative-pointer/relative-pointer-unstable-v1.xml", "relative-pointer-unstable-v1"},
    {"/usr/share/wayland-protocols/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml", "pointer-constraints-unstable-v1"},
    {"/usr/share/wayland-protocols/unstable/xdg-output/xdg-output-unstable-v1.xml",             "xdg-output-unstable-v1"},
    {"/usr/share/wayland-protocols/staging/pointer-warp/pointer-warp-v1.xml",                   "pointer-warp-v1"},
};

static bool
generate_wayland_file(const char *mode, const char *xml, const char *out) {
    if(nob_file_exists(out)) return true;
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "wayland-scanner", mode, xml, out);
    return nob_cmd_run(&cmd);
}

static void
generate_wayland_protocols(void) {
    mkdir_if_not_exists(WAYLAND_GEN_FOLDER);
    for(size_t i = 0; i < ARRAY_LEN(g_wayland_protocols); i++){
        const WaylandProtocol *p = &g_wayland_protocols[i];
        const char *header = nob_temp_sprintf("%s%s.h", WAYLAND_GEN_FOLDER, p->stem);
        const char *code   = nob_temp_sprintf("%s%s.c", WAYLAND_GEN_FOLDER, p->stem);
        if(!generate_wayland_file("client-header", p->xml, header)){
            nob_log(NOB_ERROR, "Failed to generate %s", header);
            exit(1);
        }
        if(!generate_wayland_file("public-code", p->xml, code)){
            nob_log(NOB_ERROR, "Failed to generate %s", code);
            exit(1);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Platform config
// ─────────────────────────────────────────────────────────────────────────────

static void apply_platform_flags(Nob_Cmd *cmd, PlatformKind platform) {
    switch(platform) {
        case PlatformKind_LinuxX11:
            nob_cmd_append(cmd, "-DDOT_PLATFORM_LINUX");
            nob_cmd_append(cmd, "-lX11", "-lXcursor", "-lXrandr", "-lXfixes");
            break;
        case PlatformKind_LinuxWayland:
            generate_wayland_protocols();
            nob_cmd_append(cmd, "-DRGFW_WAYLAND", "-DRGFW_OPENGL", "-DDOT_PLATFORM_LINUX");
            nob_cmd_append(cmd, "-lwayland-cursor", "-lwayland-client", "-lwayland-egl", "-lxkbcommon");
            nob_cmd_append(cmd,
                "src/gen-wayland/xdg-shell.c",
                "src/gen-wayland/relative-pointer-unstable-v1.c",
                "src/gen-wayland/pointer-constraints-unstable-v1.c",
                "src/gen-wayland/xdg-toplevel-icon-v1.c",
                "src/gen-wayland/xdg-output-unstable-v1.c",
                "src/gen-wayland/xdg-decoration-unstable-v1.c",
                "src/gen-wayland/pointer-warp-v1.c");
            break;
        case PlatformKind_Windows:
            nob_cmd_append(cmd, "/DDOT_PLATFORM_WINDOWS", "/D_CRT_SECURE_NO_WARNINGS");
            break;
        default:
            nob_log(NOB_ERROR, "Unsupported platform");
            exit(1);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Compiler base + optimization g_flags
// ─────────────────────────────────────────────────────────────────────────────

static void apply_compiler_flags(Nob_Cmd *cmd, CompilerKind compiler, OptimizationLevelKind opt) {
    switch(compiler) {
        case CompilerKind_Clang:
            nob_cmd_append(cmd,
                "-std=c11",
                "-Wall", "-Wextra", "-Wno-override-init", "-Wno-incompatible-pointer-types-discards-qualifiers",
                "-Wdiv-by-zero", "-Wno-unused-function",
                "-Werror=vla",
                // "-Wconversion",
                // "-Wimplicit-int-conversion",
                // "-Wsign-conversion",
                // "-Wno-unused-parameter",
                "-fvisibility=hidden");
            switch(opt) {
                case OptimizationLevelKind_Debug:
                    nob_cmd_append(cmd,
                        "-O0", "-g",
                        "-DDEBUG");
                    if (g_flags.use_asan){
                        nob_cmd_append(cmd,
                            "-fsanitize=address,undefined",
                            "-fsanitize-recover=address,undefined");

                    }
                    break;
                case OptimizationLevelKind_Release:
                    nob_cmd_append(cmd, "-O2", "-g");
                    break;
                case OptimizationLevelKind_Final:
                    nob_cmd_append(cmd, "-O3", "-DNDEBUG", "-Wno-unused-but-set-variable");
                    break;
            }
            break;
        case CompilerKind_Msvc:
            nob_cmd_append(cmd, "/W4", "/std:c11", "/nologo");
            switch(opt) {
                case OptimizationLevelKind_Debug:
                    nob_cmd_append(cmd, "/wd4324", "/Od", "/Zi", "/MDd", "/DDEBUG");
                    break;
                case OptimizationLevelKind_Release:
                    nob_cmd_append(cmd, "/O2", "/Zi", "/MD");
                    break;
                case OptimizationLevelKind_Final:
                    nob_cmd_append(cmd, "/O2", "/GL", "/DNDEBUG", "/MD");
                    break;
            }
            break;
        default:
            nob_log(NOB_ERROR, "Unsupported compiler");
            exit(1);
    }
}

typedef enum ExternalAssetKind
{
    ExternalAssetKind_GitZip,
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

#define EXTERNAL_ASSETS(path, list) ((ExternalAssets){ .output_path = (path), .items = (list), .count = ARRAY_LEN(list) })

static const ExternalAsset assets_list[] = {
    {.kind = ExternalAssetKind_GitZip, .git_asset = {"https://github.com/KhronosGroup/glTF-Sample-Assets", "edc7c9e67c639d230715049ee31f9a96a6babbbe"}},
};

static bool
mkdir_parents(const char *file_path)
{
    char *path = nob_temp_strdup(file_path);
    for(char *p = path; *p; ++p){
        if(*p == '/' && p != path){
            *p = 0;
            bool ok = mkdir_if_not_exists(path);
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
#if defined(_WIN32)
    nob_cmd_append(&cmd, "tar", "-xf", archive_path, "-C", dst_dir);
#else
    if(nob_sv_end_with(nob_sv_from_cstr(archive_path), ".zip")){
        nob_cmd_append(&cmd, "unzip", "-q", archive_path, "-d", dst_dir);
    }else{
        nob_cmd_append(&cmd, "tar", "-xzf", archive_path, "-C", dst_dir);
    }
#endif
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
        case ExternalAssetKind_GitZip:{
            const char *repo = nob_path_name(asset->git_asset.URL);
            source_url    = nob_temp_sprintf("%s/archive/%s.zip", asset->git_asset.URL, asset->git_asset.rev);
            final_path    = nob_temp_sprintf("%s/%s", assets.output_path, repo);
            download_path = nob_temp_sprintf("%s/%s.zip", assets.output_path, repo);
        }break;
        }

        if(nob_file_exists(final_path)){
            nob_log(NOB_INFO, "Already have: %s (delete it to redownload)", final_path);
        }else{
            ok = download_file(source_url, download_path);
            if(ok && asset->kind == ExternalAssetKind_GitZip){
                const char *extracted = nob_temp_sprintf("%s/%s-%s", assets.output_path, nob_path_name(asset->git_asset.URL), asset->git_asset.rev);
                ok = extract_archive(download_path, assets.output_path) && nob_rename(extracted, final_path);
            }
        }
        nob_temp_rewind(temp_checkpoint);
    }
    return(ok);
}

int
main(int argc, char *argv[])
{
    NOB_GO_REBUILD_URSELF(argc, argv);
    mkdir_if_not_exists(OUTPUT_FOLDER);
    for(int i = 1; i < argc; i++){
        if(strcmp(argv[i], "--help") == 0){
            print_help(argv[0]);
            return 0;
        }

        bool matched = false;
        for(size_t f = 0; f < ARRAY_LEN(flag_table); f++){
            if(strcmp(argv[i], flag_table[f].flag) == 0){
                flag_table[f].apply();
                matched = true;
                break;
            }
        }
        if(!matched){
            fprintf(stderr, "Unknown flag: %s\n", argv[i]);
            return 1;
        }
    }

    download_assets(EXTERNAL_ASSETS("assets", assets_list));
    Nob_Cmd cmd = {0};
    nob_log(NOB_INFO, "Using compiler %s for target %s", g_compiler_kind_str[g_flags.compiler], g_platform_kind_str[g_flags.platform]);
    nob_cmd_append(&cmd, g_compiler_kind_str[g_flags.compiler]);
    nob_cc_inputs(&cmd, SRC_FOLDER "dot_engine.c");

    apply_compiler_flags(&cmd, g_flags.compiler, g_flags.opt);
    apply_platform_flags(&cmd, g_flags.platform);

    if(g_flags.compiler == CompilerKind_Msvc){
        if(g_flags.platform != PlatformKind_Windows){
            nob_log(NOB_ERROR, "cl is only available on windows");
            return 1;
        }
        nob_cmd_append(&cmd, "/I", "src/third_party/vulkan-headers/include");
        nob_cmd_append(&cmd, "/I", "src/third_party/vulkan-utility-libraries/include");
        nob_cmd_append(&cmd, "/I", ".");
        nob_cmd_append(&cmd, "/I", "src/");
        nob_cc_output(&cmd, OUTPUT_FOLDER "dot_engine");
        nob_cmd_append(&cmd, "/link");
        // NOTE: incremental linking pads the custom "$a/$m/$z" sections used by SECTION_DECLARE
        // (settings, plugins, reflect) with unpredictable filler, which breaks their tight-packing
        // assumption and corrupts iteration. Must stay off.
        nob_cmd_append(&cmd, "/INCREMENTAL:NO");
        nob_cmd_append(&cmd, "Dbghelp.lib");
    }else{
        nob_cmd_append(&cmd, "-lm", "-lpthread");
        nob_cmd_append(&cmd, "-I", "src/");
        nob_cmd_append(&cmd, "-I", "src/third_party/vulkan-headers/include");
        nob_cmd_append(&cmd, "-I", "src/third_party/vulkan-utility-libraries/include");
        nob_cmd_append(&cmd, "-Wl,-T"LINKER_SCRIPTS_FOLDER"plugins_section.ld");


        // nob_cmd_append(&cmd, " -Wl,-z,rosegment ");
        nob_cc_output(&cmd, OUTPUT_FOLDER "dot_engine");
    }

    nob_log(NOB_INFO, "[*] Compiling...");
    if(!nob_cmd_run(&cmd)) return 1;

    return 0;
}
