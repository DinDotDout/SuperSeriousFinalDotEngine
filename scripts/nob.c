#define NOB_IMPLEMENTATION
#define NOB_NO_ECHO
#include "../src/third_party/nob.h/nob.h"
Nob_Log_Level nob_minimal_log_level = NOB_NO_LOGS;

#define BUILD_FOLDER "build/"
#define SRC_FOLDER   "src/dot_engine/"

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

#define ARG_LIST(...)                                               \
    (ArgList) {                                                     \
        .items = (const char *[]){__VA_ARGS__},                     \
        .count = (sizeof((char *[]){__VA_ARGS__}) / sizeof(char *)) \
    }

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
            "-Werror=vla",),
        .opt = {
            [OptimizationLevelKind_Debug] =
                ARG_LIST("-O0", "-g",
                "-fsanitize=address,undefined",
                "-fsanitize-recover=address,undefined",),
            [OptimizationLevelKind_Release] =
                ARG_LIST("-O2", "-g",
                "-fsanitize=address,undefined",
                "-fsanitize-recover=address,undefined",),
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

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);
    mkdir_if_not_exists(BUILD_FOLDER);
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
        nob_cmd_append(&cmd, "-I", "src/", "-Wl,-Tscripts/plugins_section.ld");
    }

    nob_cc_inputs(&cmd, SRC_FOLDER "dot_engine.c");
    nob_cc_output(&cmd, BUILD_FOLDER "dot_engine");

    nob_log(INFO, "[*] Compiling...");
    if (!nob_cmd_run_sync(cmd)) return 1;

    return 0;
}
