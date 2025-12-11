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
SYS_LIBS=" \
    -lwayland-cursor -lwayland-client -lxkbcommon \
    -lvulkan -ldl -lpthread \
"

PROTO_SRCS=" \
    src/third_party/RGFW/relative-pointer-unstable-v1.c \
    src/third_party/RGFW/pointer-constraints-unstable-v1.c \
    src/third_party/RGFW/xdg-toplevel-icon-v1.c \
    src/third_party/RGFW/xdg-output-unstable-v1.c \
    src/third_party/RGFW/xdg-decoration-unstable-v1.c \
    src/third_party/RGFW/xdg-shell.c \
"

# ./gen-wayland.sh

# --- Build ---
echo "[*] Compiling..."
# gcc $CFLAGS -I. -Iextra src/dot_engine.c $SYS_LIBS $PROTO_SRCS -o build/dot_engine

# gcc $CFLAGS -Iextra \
#     src/vk10.c $SYS_LIBS $PROTO_SRCS \
#     -o build/vk
gcc $CFLAGS -I src/ \
    src/dot_engine/dot_engine.c $SYS_LIBS $PROTO_SRCS \
    -o build/dot_engine
echo "[+] Build complete: $OUT"
