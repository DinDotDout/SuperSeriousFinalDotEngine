#!/usr/bin/env bash
export LSAN_OPTIONS="suppressions=scripts/lsan.supp:detect_leaks=1:print_suppressions=0"
# export ENABLE_VULKAN_RENDERDOC_CAPTURE=1
./build/dot_engine
