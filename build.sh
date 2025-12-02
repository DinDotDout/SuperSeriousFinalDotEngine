#!/usr/bin/env bash
set -euo pipefail

# --- Config ---
# CC=${CC:-cc}
CFLAGS="-g -std=c99 \
    -fsanitize-recover=address,undefined \
    -Wall -Wextra -Wno-override-init -Wdiv-by-zero \
    -lm"

OUT=${OUT:-app}

# --- Libraries ---
LIBS=" \
    -D RGFW_WAYLAND \
    relative-pointer-unstable-v1.c pointer-constraints-unstable-v1.c \
    xdg-toplevel-icon-v1.c xdg-output-unstable-v1.c \
    xdg-decoration-unstable-v1.c xdg-shell.c \
    -lwayland-cursor -lwayland-client -lxkbcommon -lwayland-egl -lEGL \
    -lvulkan -ldl -lpthread \
"

SYS_LIBS=" \
    -lwayland-cursor -lwayland-client -lxkbcommon \
    -lvulkan -ldl -lpthread \
"

PROTO_SRCS=" \
    extra/RGFW/relative-pointer-unstable-v1.c \
    extra/RGFW/pointer-constraints-unstable-v1.c \
    extra/RGFW/xdg-toplevel-icon-v1.c \
    extra/RGFW/xdg-output-unstable-v1.c \
    extra/RGFW/xdg-decoration-unstable-v1.c \
    extra/RGFW/xdg-shell.c \
"

# ./gen-wayland.sh

# --- Build ---
echo "[*] Compiling..."
# gcc $CFLAGS -I. -Iextra src/dot_engine.c $SYS_LIBS $PROTO_SRCS -o build/dot_engine

# gcc $CFLAGS -Iextra \
#     src/vk10.c $SYS_LIBS $PROTO_SRCS \
#     -o build/vk
gcc $CFLAGS -Iextra \
    src/dot_engine.c $SYS_LIBS $PROTO_SRCS \
    -o build/dot_engine
echo "[+] Build complete: $OUT"
