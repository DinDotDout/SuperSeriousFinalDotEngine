#!/usr/bin/env bash
set -euo pipefail

# --- Config ---
# CC=${CC:-cc}
CFLAGS_TESTS="-g -std=c99 \
    -Wall -Wextra -Wno-override-init -Wdiv-by-zero \
    -Wno-unused-function \
    -lm"

CFLAGS="-g -std=c99 \
    -fsanitize=address,undefined \
    -fsanitize-recover=address,undefined \
    -Wall -Wextra -Wno-override-init -Wdiv-by-zero \
    -Wno-unused-function -Werror=vla \
"
# --- Libraries ---
SYS_LIBS=" \
    -lm \
    -lwayland-cursor -lwayland-client -lxkbcommon \
    -lvulkan -ldl -lpthread \
"

PROTO_SRCS=" \
    src/gen-wayland/relative-pointer-unstable-v1.c \
    src/gen-wayland/pointer-constraints-unstable-v1.c \
    src/gen-wayland/xdg-toplevel-icon-v1.c \
    src/gen-wayland/xdg-output-unstable-v1.c \
    src/gen-wayland/xdg-decoration-unstable-v1.c \
    src/gen-wayland/xdg-shell.c \
"

./scripts/gen-wayland.sh

# --- Build ---
# echo "[*] Compiling..."

clang $CFLAGS -I src/ \
    -Wl,-Tscripts/plugins_section.ld \
    src/dot_engine/dot_engine.c $SYS_LIBS $PROTO_SRCS \
    -o build/dot_engine

# gcc $CFLAGS_TESTS -I src/ \
#     src/tests/arena_probe_page_fault.c -o build/arena_probe_page_fault
# echo "[+] Build complete: $OUT"
