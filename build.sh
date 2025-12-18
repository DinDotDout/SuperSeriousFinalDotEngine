#!/usr/bin/env bash
set -euo pipefail

# --- Config ---
# CC=${CC:-cc}

CFLAGS="-g -std=c99 \
    -fsanitize=address,undefined \
    -fsanitize-recover=address,undefined \
    -Wall -Wextra -Wno-override-init -Wdiv-by-zero \
    -Wno-unused-function \
    -lm"

# OUT=${OUT:-app}

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

gcc $CFLAGS -I src/ \
    src/dot_engine/dot_engine.c $SYS_LIBS $PROTO_SRCS \
    -o build/dot_engine

CFLAGS_TESTS="-g -std=c99 \
    -Wall -Wextra -Wno-override-init -Wdiv-by-zero \
    -Wno-unused-function \
    -lm"

gcc $CFLAGS_TESTS -I src/ \
    src/tests/arena_probe_page_fault.c -o build/arena_probe_page_fault
# echo "[+] Build complete: $OUT"
