#!/usr/bin/env bash
LSAN_OPTIONS="suppressions=scripts/lsan.supp:detect_leaks=1:print_suppressions=0" \
./build/dot_engine
