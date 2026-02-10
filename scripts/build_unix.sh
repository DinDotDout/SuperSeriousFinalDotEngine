#!/usr/bin/env bash
set -euo pipefail

QUIET=0
BACKEND="x11"

# Parse args
for arg in "$@"; do
    case "$arg" in
        --quiet) QUIET=1 ;;
        --x11) BACKEND="x11" ;;
        --wayland) BACKEND="wayland" ;;
    esac
done

log() {
    if [[ $QUIET -eq 0 ]]; then
        printf "%s\n" "$*"
    fi
}

# --- Shared Config ---
CFLAGS_COMMON="-g -std=c99 \
    -Wall -Wextra -Wno-override-init -Wdiv-by-zero \
    -Wno-unused-function \
    -fno-color-diagnostics \
    -DRGFW_UNIX
"

CFLAGS_SAN="-fsanitize=address,undefined \
    -fsanitize-recover=address,undefined \
    -Werror=vla \
"

CFLAGS="$CFLAGS_COMMON $CFLAGS_SAN"

PROTO_SRCS=""
SYS_LIBS_COMMON="-lm -lvulkan -ldl -lpthread"

# --- Backend-specific config ---
if [[ "$BACKEND" == "wayland" ]]; then
    log "[*] Using Wayland backend"

    CFLAGS="$CFLAGS -DRGFW_WAYLAND"
    ./scripts/gen-wayland.sh

    SYS_LIBS="$SYS_LIBS_COMMON \
        -lwayland-cursor -lwayland-client -lxkbcommon \
    "

    PROTO_SRCS=" \
        src/gen-wayland/xdg-shell.c \
        src/gen-wayland/relative-pointer-unstable-v1.c \
        src/gen-wayland/pointer-constraints-unstable-v1.c \
        src/gen-wayland/xdg-toplevel-icon-v1.c \
        src/gen-wayland/xdg-output-unstable-v1.c \
        src/gen-wayland/xdg-decoration-unstable-v1.c \
        src/gen-wayland/pointer-warp-v1.c \
    "

elif [[ "$BACKEND" == "x11" ]]; then
    log "[*] Using X11 backend"

    CFLAGS="$CFLAGS -DRGFW_X11"
    SYS_LIBS="$SYS_LIBS_COMMON \
        -lX11 -lXcursor -lXrandr -lXfixes \
    "
else
    echo "Unknown backend: $BACKEND"
    exit 1
fi

# --- Build ---
log "[*] Compiling..."

clang $CFLAGS -I src/ \
    -Wl,-Tscripts/plugins_section.ld \
    src/dot_engine/dot_engine.c \
    $SYS_LIBS \
    $PROTO_SRCS \
    -o build/dot_engine

