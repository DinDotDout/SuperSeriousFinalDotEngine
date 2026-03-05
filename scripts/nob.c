#define NOB_IMPLEMENTATION
#include "../src/third_party/nob.h/nob.h"

#define BUILD_FOLDER "build/"
#define SRC_FOLDER   "src/dot_engine/"

typedef enum {
    PlatformKind_None,
    PlatformKind_LinuxWayland,
    PlatformKind_LinuxX11,
    PlatformKind_Windows,
    PlatformKind_Count,
}PlatformKind;

typedef enum {
    OptimizationLevelKind_Debug,
    OptimizationLevelKind_Release,
    OptimizationLevelKind_Final,
}OptimizationLevelKind;

typedef enum {
    CompilerKind_Clang,
    // CompilerKind_Gcc,
    CompilerKind_Msvc
}CompilerKind;

static const char *compiler_kind_str[] = {
    [CompilerKind_Clang] = "clang",
    [CompilerKind_Msvc]  = "cl.exe",
};

#if defined(_WIN32)
#   define AUTO_PLATFORM PlatformKind_Windows
#   define CC CompilerKind_Msvc
#elif defined(__unix__) || defined(__linux__)
#   define AUTO_PLATFORM PlatformKind_LinuxX11
#   define CC CompilerKind_Clang
#else
#   error "Unsupported platform"
#endif

//
// ─────────────────────────────────────────────────────────────────────────────
//   PLATFORM CONFIG TABLE
// ─────────────────────────────────────────────────────────────────────────────
//
#define ARGS_COUNT 64
typedef struct {
    const char *defines[ARGS_COUNT];
    const char *libs[ARGS_COUNT];
    const char *inputs[ARGS_COUNT];
}PlatformConfig;

static PlatformConfig platform_cfg[] = {
    [PlatformKind_LinuxX11] =
        {
            .defines = {"-DRGFW_X11", "-DRGFW_UNIX"},
            .libs = {"-lX11", "-lXcursor", "-lXrandr", "-lXfixes"},
        },
    [PlatformKind_LinuxWayland] =
        {
            .defines = {"-DRGFW_WAYLAND"},
            .libs = {"-lwayland-cursor", "-lwayland-client", "-lxkbcommon"},
            .inputs =
                {
                    "src/gen-wayland/xdg-shell.c",
                    "src/gen-wayland/relative-pointer-unstable-v1.c",
                    "src/gen-wayland/pointer-constraints-unstable-v1.c",
                    "src/gen-wayland/xdg-toplevel-icon-v1.c",
                    "src/gen-wayland/xdg-output-unstable-v1.c",
                    "src/gen-wayland/xdg-decoration-unstable-v1.c",
                    "src/gen-wayland/pointer-warp-v1.c",
                },
        },
    [PlatformKind_Windows] = {
        .defines = {
            // "-DRGFW_WINDOWS",
            // "-DWIN32_LEAN_AND_MEAN"
        },
        .libs = {"-lgdi32", "-lshell32", "-luser32"},
    }};

//
// ─────────────────────────────────────────────────────────────────────────────
//   COMPILER BASE FLAGS
// ─────────────────────────────────────────────────────────────────────────────
//
static const char *base_flags_clang[ARGS_COUNT] = {
    "-g", "-std=c99",
    "-Wall", "-Wextra", "-Wno-override-init",
    "-Wdiv-by-zero", "-Wno-unused-function",
    "-Werror=vla",
    "-DDOT_USE_VOLK",
};

static const char *base_flags_msvc[ARGS_COUNT] = {
    "/W4", "/D", "/nologo", "DOT_USE_VOLK",
};

//
// ─────────────────────────────────────────────────────────────────────────────
//   OPTIMIZATION FLAGS
// ─────────────────────────────────────────────────────────────────────────────
//

static const char *opt_flags_clang[][8] = {
    [OptimizationLevelKind_Debug] = {"-O0", "-g", "-fsanitize=address,undefined", "-fsanitize-recover=address,undefined"},
    [OptimizationLevelKind_Release] = {"-O2", "-g", "-fsanitize=address,undefined", "-fsanitize-recover=address,undefined"},
    [OptimizationLevelKind_Final] = {"-O3", "-DNDEBUG"}
};

static const char *opt_flags_msvc[][8] = {
    [OptimizationLevelKind_Debug]   = {"/Od", "/Zi", "/MDd"},
    [OptimizationLevelKind_Release] = {"/O2", "/Zi", "/MD"},
    [OptimizationLevelKind_Final]   = {"/O2", "/GL", "/DNDEBUG", "/MD" },
};

static void append_list(Nob_Cmd *cmd, const char **list)
{
    for (int i = 0; list[i] && i < ARGS_COUNT; i++)
        nob_cmd_append(cmd, list[i]);
}

static bool compiler_exists(const char *name) {
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

static CompilerKind detect_compiler(void)
{
    if (compiler_exists("clang")) return CompilerKind_Clang;
    if (compiler_exists("cl.exe")) return CompilerKind_Msvc;
    nob_log(NOB_ERROR, "No supported compiler found (tried clang, cl.exe)");
    exit(1);
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);
    mkdir_if_not_exists(BUILD_FOLDER);

    PlatformKind platform = AUTO_PLATFORM;
    OptimizationLevelKind opt = OptimizationLevelKind_Debug;
    CompilerKind compiler = detect_compiler();
    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "--x11") == 0)     platform = PlatformKind_LinuxX11;
        else if (strcmp(argv[i], "--wayland") == 0) platform = PlatformKind_LinuxWayland;
        else if (strcmp(argv[i], "--window") == 0)  platform = PlatformKind_Windows;

        else if (strcmp(argv[i], "--clang") == 0) compiler = CompilerKind_Clang;
        else if (strcmp(argv[i], "--cl") == 0)    compiler = CompilerKind_Msvc;

        else if (strcmp(argv[i], "--debug") == 0)   opt = OptimizationLevelKind_Debug;
        else if (strcmp(argv[i], "--release") == 0) opt = OptimizationLevelKind_Release;
        else if (strcmp(argv[i], "--final") == 0)   opt = OptimizationLevelKind_Final;
    }

    bool is_msvc = compiler == CompilerKind_Msvc;
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, compiler_kind_str[compiler]);

    append_list(&cmd, is_msvc ? base_flags_msvc : base_flags_clang);
    append_list(&cmd, is_msvc ? opt_flags_msvc[opt] : opt_flags_clang[opt]);

    PlatformConfig *pc = &platform_cfg[platform];
    append_list(&cmd, pc->defines);
    append_list(&cmd, pc->libs);
    append_list(&cmd, pc->inputs);

    // System libs
    const char *vulkan_sdk = getenv("VULKAN_SDK");
    if(is_msvc){
        if (!vulkan_sdk) {
            nob_log(NOB_ERROR, "VULKAN_SDK environment variable not set");
            return 1;
        }
        nob_cmd_append(&cmd, nob_temp_sprintf("/I%s/Include", vulkan_sdk));
        nob_cmd_append(&cmd, "/I", "src/");
        nob_cmd_append(&cmd, nob_temp_sprintf("/link /LIBPATH:%s/Lib", vulkan_sdk));
        nob_cmd_append(&cmd, "vulkan-1.lib");
    }else if(platform == PlatformKind_Windows){
        // clang on Windows
        if (!vulkan_sdk) {
            nob_log(NOB_ERROR, "VULKAN_SDK environment variable not set");
            return 1;
        }
        nob_cmd_append(&cmd, nob_temp_sprintf("-I%s/Include", vulkan_sdk));
        nob_cmd_append(&cmd, nob_temp_sprintf("-L%s/Lib", vulkan_sdk));
        nob_cmd_append(&cmd, "-lvulkan-1", "-lgdi32", "-lshell32", "-luser32");
        nob_cmd_append(&cmd, "-I", "src/");
    }else{
        nob_cmd_append(&cmd, "-lm", "-lvulkan", "-ldl", "-lpthread");
        nob_cmd_append(&cmd, "-I", "src/", "-Wl,-Tscripts/plugins_section.ld");
    }

    nob_cc_inputs(&cmd, SRC_FOLDER "dot_engine.c");
    nob_cc_output(&cmd, BUILD_FOLDER "dot_engine");

    nob_log(INFO, "[*] Compiling...");
    if (!nob_cmd_run_sync(cmd)) return 1;

    return 0;
}

