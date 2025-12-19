#!/usr/bin/env bash
set -euo pipefail

OUTDIR="extra/RGFW"
echo "[*] Generating Wayland protocol sources into $OUTDIR..."

wayland-scanner client-header /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml "$OUTDIR/xdg-shell.h"
wayland-scanner public-code   /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml "$OUTDIR/xdg-shell.c"

wayland-scanner client-header /usr/share/wayland-protocols/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml "$OUTDIR/xdg-decoration-unstable-v1.h"
wayland-scanner public-code   /usr/share/wayland-protocols/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml "$OUTDIR/xdg-decoration-unstable-v1.c"

wayland-scanner client-header /usr/share/wayland-protocols/staging/xdg-toplevel-icon/xdg-toplevel-icon-v1.xml "$OUTDIR/xdg-toplevel-icon-v1.h"
wayland-scanner public-code   /usr/share/wayland-protocols/staging/xdg-toplevel-icon/xdg-toplevel-icon-v1.xml "$OUTDIR/xdg-toplevel-icon-v1.c"

wayland-scanner client-header /usr/share/wayland-protocols/unstable/relative-pointer/relative-pointer-unstable-v1.xml "$OUTDIR/relative-pointer-unstable-v1.h"
wayland-scanner public-code   /usr/share/wayland-protocols/unstable/relative-pointer/relative-pointer-unstable-v1.xml "$OUTDIR/relative-pointer-unstable-v1.c"

wayland-scanner client-header /usr/share/wayland-protocols/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml "$OUTDIR/pointer-constraints-unstable-v1.h"
wayland-scanner public-code   /usr/share/wayland-protocols/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml "$OUTDIR/pointer-constraints-unstable-v1.c"

wayland-scanner client-header /usr/share/wayland-protocols/unstable/xdg-output/xdg-output-unstable-v1.xml "$OUTDIR/xdg-output-unstable-v1.h"
wayland-scanner public-code   /usr/share/wayland-protocols/unstable/xdg-output/xdg-output-unstable-v1.xml "$OUTDIR/xdg-output-unstable-v1.c"

echo "[+] Protocol sources generated in $OUTDIR"
