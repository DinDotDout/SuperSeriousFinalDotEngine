#define NOB_IMPLEMENTATION
#define NOB_NO_ECHO
#include "../src/third_party/nob.h/nob.h"

Nob_Log_Level nob_minimal_log_level = NOB_INFO;

#include "nob_ext.c"

#define OUTPUT_FOLDER           "build/"
#define OBJ_FOLDER              "build/obj/"

#define WAYLAND_GEN_FOLDER      "src/gen-wayland/"

#define TRACY_FOLDER            "src/third_party/tracy/"
#define TRACY_VIEWER_FOLDER     TRACY_FOLDER "viewer/"
#if defined(_WIN32)
#   define TRACY_VIEWER_ZIP     "windows-0.14.1.zip"
#   define TRACY_VIEWER_EXE     "tracy-profiler.exe"
#else
#   define TRACY_VIEWER_ZIP     "linux-0.14.1.zip"
#   define TRACY_VIEWER_EXE     "tracy-profiler-x86_64.AppImage"
#endif

static Target g_target;
static bool   g_unity;
static bool   g_no_assets;
static bool   g_profile;
static bool   g_build_cpp_deps;

static void set_x11(void)            { g_target.os = OsKind_Linux; g_target.window = WindowKind_X11; }
static void set_wayland(void)        { g_target.os = OsKind_Linux; g_target.window = WindowKind_Wayland; }
static void set_windows(void)        { g_target.os = OsKind_Windows; }

static void set_clang(void)          { g_target.compiler = CompilerKind_Clang; }
static void set_msvc(void)           { g_target.compiler = CompilerKind_Msvc; }

static void set_debug(void)          { g_target.opt = OptimizationLevelKind_Debug; }
static void set_release(void)        { g_target.opt = OptimizationLevelKind_Release; }
static void set_final(void)          { g_target.opt = OptimizationLevelKind_Final; }

static void set_use_asan(void)       { g_target.asan = true; }
static void set_analyze(void)        { g_target.analyze = true; }
static void set_unity(void)          { g_unity = true; }
static void set_no_assets(void)      { g_no_assets = true; }
static void set_profile(void)        { g_profile = true; }
static void set_build_cpp_deps(void) { g_build_cpp_deps = true; }

static void set_log_error(void)      { nob_minimal_log_level = NOB_ERROR; }
static void set_log_warn(void)       { nob_minimal_log_level = NOB_WARNING; }

static const FlagEntry flag_table[] = {
    { "-log_error",      "Log None",                                      set_log_error      },
    { "-log_warn",       "Log Warn",                                      set_log_warn       },
    { "-x11",            "Use X11 backend",                               set_x11            },
    { "-wayland",        "Use Wayland backend",                           set_wayland        },
    { "-windows",        "Use Windows backend",                           set_windows        },

    { "-clang",          "Use Clang compiler",                            set_clang          },
    { "-cl",             "Use MSVC compiler",                             set_msvc           },

    { "-debug",          "Debug build with sanitizers",                   set_debug          },
    { "-release",        "Release build with debug info",                 set_release        },
    { "-final",          "Optimized final build",                         set_final          },

    { "-asan",           "Use asan",                                      set_use_asan       },
    { "-unity",          "Build third party in the same TU",              set_unity          },
    { "-no_assets",      "Skip downloading assets",                       set_no_assets      },
    { "-analyze",        "MSVC static analysis (/analyze)",               set_analyze        },
    { "-profile",        "Link the vendored Tracy client",                set_profile        },
    { "-build_cpp_deps", "Rebuild the vendored C++ libraries, then exit", set_build_cpp_deps },
};

static const Download downloads[] = {
    { .url = "https://github.com/KhronosGroup/glTF-Sample-Assets", .rev = "edc7c9e67c639d230715049ee31f9a96a6babbbe", .dir = "assets" },
};

static const Download tracy_source = {
    .url = "https://github.com/wolfpld/tracy", .rev = "30997d5ca6bb632cc10807a1da8a6d3de0aeeb3c", .dir = "src/third_party/tracy",
};

static const Download tracy_viewer = {
    .url = "https://github.com/wolfpld/tracy/releases/download/v0.14.1/" TRACY_VIEWER_ZIP, .dir = "src/third_party/tracy/viewer",
};

static const Prebuilt tracy = {
    .name            = "Tracy client 0.14.1",
    .lib             = "TracyClient",
    .linux_dir       = TRACY_FOLDER "linux-x86_64/",
    .windows_dir     = TRACY_FOLDER "windows-x86_64/",
    .includes        = {{TRACY_FOLDER "tracy/public"}},
    .defines         = {{"TRACY_ENABLE"}},
    .windows_defines = {{"TRACY_IMPORTS"}},
    .missing_hint    = "Build it once with ./scripts/nob -build_cpp_deps (add -windows -clang for the Windows build). "
                       "This needs a C++ compiler; commit the result so nobody else has to",
};

static const CppRecipe tracy_recipe = {
    .src_dir      = TRACY_FOLDER "tracy/",
    .obj_dir      = OUTPUT_FOLDER,
    .sources      = {{"public/TracyClient.cpp"}},
    .defines      = {{"TRACY_ENABLE", "TRACY_EXPORTS"}},
    .linux_libs   = {{"pthread", "dl"}},
    .windows_libs = {{"ws2_32", "dbghelp", "advapi32", "user32"}},
    .output       = &tracy,
};

static bool
tracy_viewer_fetch(void)
{
    if(!download_all(&tracy_viewer, 1)) return(false);
    const char *viewer = TRACY_VIEWER_FOLDER TRACY_VIEWER_EXE;
    if(!nob_file_exists(viewer)){
        if(!extract_zip_member(TRACY_VIEWER_FOLDER TRACY_VIEWER_ZIP, TRACY_VIEWER_EXE, TRACY_VIEWER_FOLDER)) return(false);
#if !defined(_WIN32)
        if(chmod(viewer, 0755) != 0){
            nob_log(NOB_ERROR, "Could not make %s executable", viewer);
            return(false);
        }
#endif
    }
    nob_log(NOB_INFO, "Tracy viewer: %s", viewer);
    return(true);
}

static const WaylandProtocol g_wayland_protocols[] = {
    {"/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml",                                   "xdg-shell"},
    {"/usr/share/wayland-protocols/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml",           "xdg-decoration-unstable-v1"},
    {"/usr/share/wayland-protocols/staging/xdg-toplevel-icon/xdg-toplevel-icon-v1.xml",               "xdg-toplevel-icon-v1"},
    {"/usr/share/wayland-protocols/unstable/relative-pointer/relative-pointer-unstable-v1.xml",       "relative-pointer-unstable-v1"},
    {"/usr/share/wayland-protocols/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml", "pointer-constraints-unstable-v1"},
    {"/usr/share/wayland-protocols/unstable/xdg-output/xdg-output-unstable-v1.xml",                   "xdg-output-unstable-v1"},
    {"/usr/share/wayland-protocols/staging/pointer-warp/pointer-warp-v1.xml",                         "pointer-warp-v1"},
};

int
main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF_PLUS(argc, argv, "scripts/nob_ext.c");
    g_target = target_host();
    if(!parse_flags(argc, argv, flag_table, NOB_ARRAY_LEN(flag_table))) return 1;
    if(!target_check(&g_target)) return 1;

    if(g_profile || g_build_cpp_deps){
        if(!download_all(&tracy_source, 1)) return 1;
    }
    if(g_build_cpp_deps){
        return(cpp_recipe_build(&g_target, &tracy_recipe) ? 0 : 1);
    }
    if(g_profile && !tracy_viewer_fetch()){
        return 1;
    }

    if(!g_no_assets){
        download_all(downloads, NOB_ARRAY_LEN(downloads));
    }

    if(g_target.opt == OptimizationLevelKind_Final){
        g_unity = true;
    }

    Exe exe = {
        .name            = "dot_engine",
        .out_dir         = OUTPUT_FOLDER,
        .obj_dir         = OBJ_FOLDER,
        .sources         = {{"src/dot_engine/dot_engine.c"}},
        .includes        = {{"src/", "src/third_party/vulkan-headers/include", "src/third_party/vulkan-utility-libraries/include"}},
        .clang_flags     = {{
            "-std=c11",
            "-fvisibility=hidden", // symbols are not exported unless marked DOT_ENGINE_API
        }},
        .clang_warnings  = {{
            "-Wall", "-Wextra", "-Wno-override-init", "-Wno-incompatible-pointer-types-discards-qualifiers",
            "-Wdiv-by-zero", "-Wno-unused-function", "-Werror=vla", "-Werror=implicit-fallthrough", "-Werror=switch-enum",
            "-Werror=conversion", "-Werror=implicit-int-conversion",
        }},
        .msvc_flags      = {{
            "/std:c11", // also enables the conforming preprocessor
            "/wd4116",  // unnamed type definition in parentheses
            "/wd4324",  // structure was padded due to alignment specifier
        }},
        .msvc_warnings   = {{"/W4"}},
        .msvc_link_flags = {{
            "/INCREMENTAL:NO", // incremental linking pads the SECTION_DECLARE "$a/$m/$z" sections and breaks iteration
        }},
        .linux_libs      = {{"m", "pthread"}},
        .windows_libs    = {{"dbghelp"}},
    };

    if(g_target.os == OsKind_Windows){
        str_list_push(&exe.defines, "DOT_PLATFORM_WINDOWS");
        str_list_push(&exe.defines, "_CRT_SECURE_NO_WARNINGS"); // allow fopen/sprintf/... without the "use _s variant" deprecation warnings
    }else if(g_target.window == WindowKind_X11){
        str_list_push(&exe.defines, "DOT_PLATFORM_LINUX");
        str_list_push(&exe.linux_libs, "X11");
        str_list_push(&exe.linux_libs, "Xcursor");
        str_list_push(&exe.linux_libs, "Xrandr");
        str_list_push(&exe.linux_libs, "Xfixes");
    }else if(g_target.window == WindowKind_Wayland){
        if(!wayland_protocols_add(&exe, WAYLAND_GEN_FOLDER, g_wayland_protocols, NOB_ARRAY_LEN(g_wayland_protocols))) return 1;
        str_list_push(&exe.defines, "DOT_PLATFORM_LINUX");
        str_list_push(&exe.defines, "RGFW_WAYLAND");
        str_list_push(&exe.defines, "RGFW_OPENGL");
        str_list_push(&exe.linux_libs, "wayland-cursor");
        str_list_push(&exe.linux_libs, "wayland-client");
        str_list_push(&exe.linux_libs, "wayland-egl");
        str_list_push(&exe.linux_libs, "xkbcommon");
    }else{
        nob_log(NOB_ERROR, "Invalid target");
    }

    if(!g_unity){
        str_list_push(&exe.quiet_sources, "src/dot_engine/third_party.c");
        str_list_push(&exe.defines, "DOT_SPLIT_THIRD_PARTY=1");
    }

    if(g_profile){
        exe.prebuilts[0] = &tracy;
    }

    return(exe_build(&g_target, &exe) ? 0 : 1);
}
