#define NOB_IMPLEMENTATION
#include "../src/third_party/nob.h/nob.h"

#define LIB_FOLDER "src/third_party/"
#define BUILD_FOLDER "build/"
#define SRC_FOLDER   "src/"

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
    [PlatformKind_LinuxX11] = {
        .defines = {"-DRGFW_X11", "-DRGFW_UNIX"},
        .libs = {"-lX11", "-lXcursor", "-lXrandr", "-lXfixes"},
    },
    [PlatformKind_LinuxWayland] = {
        .defines = {"-DRGFW_WAYLAND"},
        .libs = {"-lwayland-cursor", "-lwayland-client", "-lxkbcommon"},
        .inputs = {
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
        .defines = {"-DRGFW_WINDOWS", "-DWIN32_LEAN_AND_MEAN"},
        .libs = {"-lgdi32", "-lshell32", "-luser32"},
    }
};

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
};

static const char *base_flags_msvc[ARGS_COUNT] = {
    "/W4", "/nologo",
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

static void append_list(Nob_Cmd *cmd, const char **list){
    for (int i = 0; list[i] && i < ARGS_COUNT; i++)
        nob_cmd_append(cmd, list[i]);
}

int main(int argc, char **argv){
    NOB_GO_REBUILD_URSELF(argc, argv);
    mkdir_if_not_exists(BUILD_FOLDER);

    PlatformKind platform = AUTO_PLATFORM;
    OptimizationLevelKind opt = OptimizationLevelKind_Debug;
    CompilerKind compiler = CC;
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
    static struct {
        const char *bin_path;
        const char *src_path;
    } targets[] = {
        { .bin_path = BUILD_FOLDER"dot_engine.o", .src_path = SRC_FOLDER"dot_engine/dot_engine.c" },
        { .bin_path = BUILD_FOLDER"nuklear.o", .src_path = LIB_FOLDER"Nuklear/nuklear.h" },
        // { .bin_path = BUILD_FOLDER"nuklear.o", .src_path = SRC_FOLDER"test.c" },
        // { .bin_path = BUILD_FOLDER"baz", .src_path = SRC_FOLDER"baz.c" },
    };

    Nob_Cmd cmd = {0};
    Procs procs = {0};
    PlatformConfig *pc = &platform_cfg[platform];
    for (size_t i = 0; i < ARRAY_LEN(targets); ++i){
        nob_cmd_append(&cmd, compiler_kind_str[compiler]);

        append_list(&cmd, is_msvc ? base_flags_msvc : base_flags_clang);
        append_list(&cmd, is_msvc ? opt_flags_msvc[opt] : opt_flags_clang[opt]);

        append_list(&cmd, pc->defines);
        // System libs
        if(i==0){
            append_list(&cmd, pc->libs);
            append_list(&cmd, pc->inputs);
            if(is_msvc){
                // nob_cmd_append(&cmd, "vulkan-1.lib");
                nob_cmd_append(&cmd, "/I", "src/");
            }else{
                // nob_cmd_append(&cmd, "-lm", "-lvulkan", "-ldl", "-lpthread");
                nob_cmd_append(&cmd, "-I", "src/", "-Wl,-Tscripts/plugins_section.ld");
            }
        }else{
            nob_cmd_append(&cmd, "-DNK_IMPLEMENTATION");
        }
        nob_cc_inputs(&cmd, "-c", targets[i].src_path);
        nob_cc_output(&cmd, targets[i].bin_path);
        if (!cmd_run(&cmd, .async = &procs)) return 1;
    }

    nob_log(INFO, "[*] Compiling libs...");
    if(!procs_flush(&procs)) return 1;
    nob_cmd_append(&cmd, compiler_kind_str[compiler]);
    append_list(&cmd, is_msvc ? base_flags_msvc : base_flags_clang);
    append_list(&cmd, is_msvc ? opt_flags_msvc[opt] : opt_flags_clang[opt]);

    append_list(&cmd, pc->defines);
    if(is_msvc){
        nob_cmd_append(&cmd, "vulkan-1.lib");
        nob_cmd_append(&cmd, "/I", "src/");
        nob_cmd_append(&cmd, "/D", "DOT_USE_VOLK");
    }else{
        nob_cmd_append(&cmd, "-lm", "-lvulkan", "-ldl", "-lpthread");
        nob_cmd_append(&cmd, "-I", "src/", "-Wl,-Tscripts/plugins_section.ld");
        nob_cmd_append(&cmd, "-DDOT_USE_VOLK");
    }
    append_list(&cmd, pc->defines);
    append_list(&cmd, pc->libs);
    append_list(&cmd, pc->inputs);
    for (size_t i = 0; i < ARRAY_LEN(targets); ++i){
        nob_cc_inputs(&cmd, targets[0].bin_path);
    }

    nob_cc_output(&cmd, BUILD_FOLDER"dot_engine");
    nob_log(INFO, "[*] Linking...");
    if (!cmd_run(&cmd)) return 1;

    return 0;
}
