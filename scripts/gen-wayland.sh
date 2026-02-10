#!/usr/bin/env bash
set -euo pipefail

OUTDIR="src/gen-wayland/"
mkdir -p "$OUTDIR"

generate_if_missing() {
    local mode="$1"
    local xml="$2"
    local out="$3"

    if [[ ! -f "$out" ]]; then
        wayland-scanner "$mode" "$xml" "$out"
    fi
}

generate_if_missing client-header /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml "$OUTDIR/xdg-shell.h"
generate_if_missing public-code   /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml "$OUTDIR/xdg-shell.c"

generate_if_missing client-header /usr/share/wayland-protocols/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml "$OUTDIR/xdg-decoration-unstable-v1.h"
generate_if_missing public-code   /usr/share/wayland-protocols/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml "$OUTDIR/xdg-decoration-unstable-v1.c"

generate_if_missing client-header /usr/share/wayland-protocols/staging/xdg-toplevel-icon/xdg-toplevel-icon-v1.xml "$OUTDIR/xdg-toplevel-icon-v1.h"
generate_if_missing public-code   /usr/share/wayland-protocols/staging/xdg-toplevel-icon/xdg-toplevel-icon-v1.xml "$OUTDIR/xdg-toplevel-icon-v1.c"

generate_if_missing client-header /usr/share/wayland-protocols/unstable/relative-pointer/relative-pointer-unstable-v1.xml "$OUTDIR/relative-pointer-unstable-v1.h"
generate_if_missing public-code   /usr/share/wayland-protocols/unstable/relative-pointer/relative-pointer-unstable-v1.xml "$OUTDIR/relative-pointer-unstable-v1.c"

generate_if_missing client-header /usr/share/wayland-protocols/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml "$OUTDIR/pointer-constraints-unstable-v1.h"
generate_if_missing public-code   /usr/share/wayland-protocols/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml "$OUTDIR/pointer-constraints-unstable-v1.c"

generate_if_missing client-header /usr/share/wayland-protocols/unstable/xdg-output/xdg-output-unstable-v1.xml "$OUTDIR/xdg-output-unstable-v1.h"
generate_if_missing public-code   /usr/share/wayland-protocols/unstable/xdg-output/xdg-output-unstable-v1.xml "$OUTDIR/xdg-output-unstable-v1.c"

generate_if_missing client-header /usr/share/wayland-protocols/unstable/xdg-output/xdg-output-unstable-v1.xml "$OUTDIR/xdg-output-unstable-v1.h"
generate_if_missing public-code   /usr/share/wayland-protocols/unstable/xdg-output/xdg-output-unstable-v1.xml "$OUTDIR/xdg-output-unstable-v1.c"

generate_if_missing client-header /usr/share/wayland-protocols/staging/pointer-warp/pointer-warp-v1.xml "$OUTDIR/pointer-warp-v1.h"
generate_if_missing public-code /usr/share/wayland-protocols/staging/pointer-warp/pointer-warp-v1.xml "$OUTDIR/pointer-warp-v1.c"
