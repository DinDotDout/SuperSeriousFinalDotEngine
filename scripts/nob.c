#define NOB_IMPLEMENTATION
#define NOB_NO_ECHO
#include "../src/third_party/nob.h/nob.h"
Nob_Log_Level nob_minimal_log_level = NOB_NO_LOGS;

#define VA_ARG_COUNT_T(T, ...) (sizeof((T[])__VA_ARGS__) / sizeof(T))
#define SLICE(T, ...) \
    .items = (T[])__VA_ARGS__, .count = VA_ARG_COUNT_T(T, __VA_ARGS__)

#define ARG_LIST(...)                                               \
    (ArgList) {                                                     \
        .items = (const char *[]){__VA_ARGS__},                     \
        .count = (sizeof((char *[]){__VA_ARGS__}) / sizeof(char *)) \
    }

#define OUTPUT_FOLDER "build/"
#define SRC_FOLDER   "src/dot_engine/"

#define SCRIPTS_FOLDER "scripts/"

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
    PlatformKind_Auto= AUTO_PLATFORM,
    PlatformKind_Count,
}PlatformKind;

typedef enum {
    OptimizationLevelKind_Debug,
    OptimizationLevelKind_Release,
    OptimizationLevelKind_Final,
}OptimizationLevelKind;

typedef enum {
    CompilerKind_None,
    CompilerKind_Clang,
    CompilerKind_Msvc,
    CompilerKind_Auto = AUTO_COMPILER,
    CompilerKind_Count,
}CompilerKind;

static const char *compiler_kind_str[] = {
    [CompilerKind_Clang] = "clang",
    [CompilerKind_Msvc]  = "cl.exe",
};

typedef struct{
    const char **items;
    int count;
}ArgList;

typedef struct {
    PlatformKind platform;
    OptimizationLevelKind opt;
    CompilerKind compiler;
}Flags;

static Flags flags = {
    .opt      = OptimizationLevelKind_Debug,
    .platform = PlatformKind_Auto,
    .compiler = CompilerKind_Auto,
};

typedef struct {
    const char *flag;
    const char *description;
    void (*apply)(void);
}FlagEntry;

static void set_x11(void)    { flags.platform = PlatformKind_LinuxX11; }
static void set_wayland(void){ flags.platform = PlatformKind_LinuxWayland; }
static void set_windows(void){ flags.platform = PlatformKind_Windows; }

static void set_clang(void)  { flags.compiler = CompilerKind_Clang; }
static void set_msvc(void)   { flags.compiler = CompilerKind_Msvc; }

static void set_debug(void)  { flags.opt = OptimizationLevelKind_Debug; }
static void set_release(void){ flags.opt = OptimizationLevelKind_Release; }
static void set_final(void)  { flags.opt = OptimizationLevelKind_Final; }

static const FlagEntry flag_table[] = {
    { "--x11",     "Use X11 backend",              set_x11     },
    { "--wayland", "Use Wayland backend",          set_wayland },
    { "--window",  "Use Windows backend",          set_windows },

    { "--clang",   "Use Clang compiler",           set_clang   },
    { "--cl",      "Use MSVC compiler",            set_msvc    },

    { "--debug",   "Debug build with sanitizers",  set_debug   },
    { "--release", "Release build with debug info",set_release },
    { "--final",   "Optimized final build",        set_final   },
};

static void print_help(const char *prog) {
    printf("Usage: %s [flags]\n\n", prog);
    printf("Flags:\n");
    for (size_t i = 0; i < ARRAY_LEN(flag_table); i++) {
        printf("  %-10s %s\n", flag_table[i].flag, flag_table[i].description);
    }
    printf("  %-10s Show this help\n", "--help");
}

// ─────────────────────────────────────────────────────────────────────────────
// Platform config
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    ArgList defines;
    ArgList libs;
    ArgList inputs;
} PlatformConfig;

static PlatformConfig platform_cfg[] = {
    [PlatformKind_LinuxX11] = {
        .defines = {},
        .libs = ARG_LIST("-lX11", "-lXcursor", "-lXrandr", "-lXfixes",),
        .inputs = {},
    },
    [PlatformKind_LinuxWayland] = {
        .defines = ARG_LIST("-DRGFW_WAYLAND"),
        .libs = ARG_LIST("-lwayland-cursor", "-lwayland-client", "-lxkbcommon",),
        .inputs = ARG_LIST("src/gen-wayland/xdg-shell.c",
            "src/gen-wayland/relative-pointer-unstable-v1.c",
            "src/gen-wayland/pointer-constraints-unstable-v1.c",
            "src/gen-wayland/xdg-toplevel-icon-v1.c",
            "src/gen-wayland/xdg-output-unstable-v1.c",
            "src/gen-wayland/xdg-decoration-unstable-v1.c",
            "src/gen-wayland/pointer-warp-v1.c",),
    },
    [PlatformKind_Windows] = {
        .defines = {},
        .libs = ARG_LIST("-lvulkan-1", "-lgdi32", "-lshell32", "-luser32",),
        .inputs = {},
    },
};

// ─────────────────────────────────────────────────────────────────────────────
// Compiler base + optimization flags
// ─────────────────────────────────────────────────────────────────────────────
typedef struct {
    ArgList base;
    ArgList opt[3]; // Debug, Release, Final
} CompilerConfig;

static CompilerConfig compiler_cfg[] = {
    [CompilerKind_Clang] = {
        .base = ARG_LIST("-std=c99",
            "-Wall", "-Wextra", "-Wno-override-init",
            "-Wdiv-by-zero", "-Wno-unused-function",
            // "-Wno-unused-parameter",
            "-Werror=vla",
        ),
        .opt = {
            [OptimizationLevelKind_Debug] =
                ARG_LIST(
                    "-O0", 
                    "-g",
                "-fsanitize=address,undefined",
                "-fsanitize-recover=address,undefined",
                ),
            [OptimizationLevelKind_Release] =
                ARG_LIST("-O2", "-g",
                // "-fsanitize=address,undefined",
                // "-fsanitize-recover=address,undefined",
            ),
            [OptimizationLevelKind_Final] =
                ARG_LIST("-O3", "-DNDEBUG",),
        },
    },
    [CompilerKind_Msvc] = {
        .base = ARG_LIST("/W4", "/D", "/nologo"),
        .opt = {
            [OptimizationLevelKind_Debug]   = ARG_LIST("/Od", "/Zi", "/MDd",),
            [OptimizationLevelKind_Release] = ARG_LIST("/O2", "/Zi", "/MD",),
            [OptimizationLevelKind_Final]   = ARG_LIST("/O2", "/GL", "/DNDEBUG", "/MD",),
        },
    },
};

static bool program_exists(const char *name) {
#if defined(_WIN32)
    Nob_Cmd check = {0};
    nob_cmd_append(&check, "where", name);
#else
    Nob_Cmd check = {0};
    nob_cmd_append(&check, "which", name);
#endif
    bool result = nob_cmd_run_sync(check);
    nob_cmd_free(check);
    return result;
}

// TODO: This prints where/which, which somehow ends up in my qf list and moves my cursor
// disabling for now as CompilerKind_Auto is good enough
static CompilerKind detect_compiler(void) {
    CompilerKind kind = CompilerKind_None;
    if(PlatformKind_Auto == PlatformKind_Windows){
        if(program_exists("cl.exe")){
            kind = CompilerKind_Msvc;
        }
    }
    if(kind == CompilerKind_None){
        if(program_exists("clang")){
            kind = CompilerKind_Clang;
        }
    }
    if(kind != CompilerKind_None){
        return(kind);
    }
    nob_log(NOB_ERROR, "No supported compiler found (tried clang, cl.exe)");
    exit(1);
}

// typedef struct ExternalLibs ExternalLibs;
// typedef struct ExternalLibs{
//     struct Lib{
//         const char *URL;
//         const char *rev;
//     } *items;
//     int count;
// }ExternalLibs;
typedef enum ExternalKind{
    ExternalKind_Git,
    ExternalKind_RegularAsset,
}ExternalKind;

typedef struct ExternalAsset{
    ExternalKind kind;
    union {
        struct{
            enum GitAssetKind{
                GitAssetKind_AsGit,
                GitAssetKind_AsZip,
            }kind;
            const char *URL;
            const char *rev;
        }git_asset;
        struct{
            const char *URL;
        }asset;
    };
}ExternalAsset;

typedef struct ExternalAssets{
    ExternalAsset *items;
    int count;
    const char* output_path;
}ExternalAssets;

static const ExternalAssets assets = {
    .output_path = "assets/",
    SLICE(ExternalAsset, {
        {.kind = ExternalKind_Git, .git_asset = {GitAssetKind_AsZip, "https://github.com/KhronosGroup/glTF-Sample-Models", "8e9a5a6ad1a2790e2333e3eb48a1ee39f9e0e31b"}},
    }
)};

void download_assets(void)
{
    // Ensure the root output folder exists
    mkdir_if_not_exists(assets.output_path);
    for (int i = 0; i < assets.count; ++i) {
        ExternalAsset *asset = &assets.items[i];

        //
        // Regular asset (simple file download)
        //
        if (asset->kind == ExternalKind_RegularAsset) {
            const char *url = asset->asset.URL;

            const char *last_slash = strrchr(url, '/');
            const char *filename = last_slash ? last_slash + 1 : "asset.bin";

            char out_path[512];
            snprintf(out_path, sizeof(out_path),
                     "%s/%s", assets.output_path, filename);

            if(nob_file_exists(out_path)){
                printf("Already have regular asset: %s\n", out_path);
                continue;
            }

            Nob_Cmd cmd = {0};
            nob_cmd_append(&cmd, "curl");
            nob_cmd_append(&cmd, "-L");
            nob_cmd_append(&cmd, "-o");
            nob_cmd_append(&cmd, out_path);
            nob_cmd_append(&cmd, url);
            nob_cmd_run(&cmd);

            continue;
        }

        //
        // Git asset
        //
        const char *url = asset->git_asset.URL;
        const char *rev = asset->git_asset.rev;

        const char *last_slash = strrchr(url, '/');
        const char *repo_name = last_slash ? last_slash + 1 : "repo";

        //
        // ZIP MODE
        //
        if (asset->git_asset.kind == GitAssetKind_AsZip) {

            char extract_dir[512];
            snprintf(extract_dir, sizeof(extract_dir), "%s/%s-%s",
                     assets.output_path, repo_name, rev);

            if(nob_file_exists(extract_dir)) {
                printf("Already have git-zip asset: %s\n", extract_dir);
                continue;
            }

            mkdir_if_not_exists(extract_dir);

            char archive_url[512];
            snprintf(archive_url, sizeof(archive_url),
                     "%s/archive/%s.zip", url, rev);

            char zip_path[512];
            snprintf(zip_path, sizeof(zip_path),
                     "%s/%s-%s.zip",
                     assets.output_path, repo_name, rev);

            // curl -L -o zip_path archive_url
            {
                Nob_Cmd cmd = {0};
                nob_cmd_append(&cmd, "curl");
                nob_cmd_append(&cmd, "-L");
                nob_cmd_append(&cmd, "-o");
                nob_cmd_append(&cmd, zip_path);
                nob_cmd_append(&cmd, archive_url);
                nob_cmd_run(&cmd);
            }

            // unzip zip_path -d extract_dir
            {
                Nob_Cmd cmd = {0};
                nob_cmd_append(&cmd, "unzip");
                nob_cmd_append(&cmd, zip_path);
                nob_cmd_append(&cmd, "-d");
                nob_cmd_append(&cmd, extract_dir);
                nob_cmd_run(&cmd);
            }

            continue;
        }

        //
        // GIT MODE
        //
        if (asset->git_asset.kind == GitAssetKind_AsGit) {

            char clone_dir[512];
            snprintf(clone_dir, sizeof(clone_dir),
                     "%s/%s", assets.output_path, repo_name);

            if(nob_file_exists(clone_dir) == 1){
                printf("Already have git asset: %s\n", clone_dir);
                continue;
            }

            // git clone --recurse-submodules url clone_dir
            {
                Nob_Cmd cmd = {0};
                nob_cmd_append(&cmd, "git");
                nob_cmd_append(&cmd, "clone");
                nob_cmd_append(&cmd, "--recurse-submodules");
                nob_cmd_append(&cmd, url);
                nob_cmd_append(&cmd, clone_dir);
                nob_cmd_run(&cmd);
            }

            // git -C clone_dir checkout rev
            {
                Nob_Cmd cmd = {0};
                nob_cmd_append(&cmd, "git");
                nob_cmd_append(&cmd, "-C");
                nob_cmd_append(&cmd, clone_dir);
                nob_cmd_append(&cmd, "checkout");
                nob_cmd_append(&cmd, rev);
                nob_cmd_run(&cmd);
            }

            // git -C clone_dir submodule update --init --recursive
            {
                Nob_Cmd cmd = {0};
                nob_cmd_append(&cmd, "git");
                nob_cmd_append(&cmd, "-C");
                nob_cmd_append(&cmd, clone_dir);
                nob_cmd_append(&cmd, "submodule");
                nob_cmd_append(&cmd, "update");
                nob_cmd_append(&cmd, "--init");
                nob_cmd_append(&cmd, "--recursive");
                nob_cmd_run(&cmd);
            }

            continue;
        }
    }
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);
    mkdir_if_not_exists(OUTPUT_FOLDER);
    // See detect_compiler
    // flags.compiler = detect_compiler();
    for (int i = 1; i < argc; i++) {
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

    // download_assets();
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, compiler_kind_str[flags.compiler]);
    CompilerConfig *ccfg = &compiler_cfg[flags.compiler];
    nob_cmd_extend(&cmd, &ccfg->base);
    nob_cmd_extend(&cmd, &ccfg->opt[flags.opt]);

    PlatformConfig *pc = &platform_cfg[flags.platform];
    nob_cmd_extend(&cmd, &pc->defines);
    nob_cmd_extend(&cmd, &pc->libs);
    nob_cmd_extend(&cmd, &pc->inputs);

    const char *vulkan_sdk = getenv("VULKAN_SDK");
    if(flags.compiler == CompilerKind_Msvc){
        if (!vulkan_sdk){
            nob_log(NOB_ERROR, "VULKAN_SDK environment variable not set");
            return 1;
        }
        nob_cmd_append(&cmd, nob_temp_sprintf("/I%s/Include", vulkan_sdk));
        nob_cmd_append(&cmd, "/I", "src/");
        nob_cmd_append(&cmd, nob_temp_sprintf("/link /LIBPATH:%s/Lib", vulkan_sdk));
        nob_cmd_append(&cmd, "vulkan-1.lib");
    }else if(flags.platform == PlatformKind_Windows){
        if (!vulkan_sdk){
            nob_log(NOB_ERROR, "VULKAN_SDK environment variable not set");
            return 1;
        }
        nob_cmd_append(&cmd, nob_temp_sprintf("-I%s/Include", vulkan_sdk));
        nob_cmd_append(&cmd, nob_temp_sprintf("-L%s/Lib", vulkan_sdk));
        nob_cmd_append(&cmd, "-I", "src/");
        // libs already added via platform_cfg for Windows
    }else{
        nob_cmd_append(&cmd, "-lm", "-lvulkan",  "-lpthread");
        nob_cmd_append(&cmd, "-I", "src/");
        // nob_cmd_append(&cmd, "-Wl,-T"SCRIPTS_FOLDER"phdrs.ld");
        // nob_cmd_append(&cmd, "-Wl,-T"SCRIPTS_FOLDER"rodata.ld");
        nob_cmd_append(&cmd, "-Wl,-T"SCRIPTS_FOLDER"plugins_section.ld");


        // nob_cmd_append(&cmd, " -Wl,-z,rosegment ");
    }

    nob_cc_inputs(&cmd, SRC_FOLDER "dot_engine.c");
    nob_cc_output(&cmd, OUTPUT_FOLDER "dot_engine");

    nob_log(INFO, "[*] Compiling...");
    if (!nob_cmd_run(&cmd)) return 1;

    return 0;
}
