#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# tests/cpu_bench.c in a Linux container (the kernel's instruction counter; Docker Desktop's WSL2 VM has one), then
# the device estimate from its counts (tests/cpu_budget.py, docs/research/cpu-budget.md):
#   sh fw/tests/cpu_bench.sh [ENGINE]        -> fw/build/cpu-bench.txt, the estimate on stdout (~3 min; ENGINE: that
#                                            one only, NONE: none, which the estimate needs a felucca/* line of)
set -e
cd "$(dirname "$0")/.."
mkdir -p build
W=$(pwd -W 2>/dev/null || pwd)
MSYS_NO_PATHCONV=1 docker run --rm --security-opt seccomp=unconfined -v "$W:/fw" -w /fw rust:1-bookworm sh -c \
  "cc -O2 -w -DDINGHY_HOST -DAPP_NAME='\"TRACKER\"' -DAPP_VER='\"bench\"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen \
      -Ifirmware/sound -o /tmp/cpu_bench tests/cpu_bench.c && /tmp/cpu_bench $*" | tee build/cpu-bench.txt
MAIN=$(git worktree list --porcelain | sed -n '1s/^worktree //p')     # third_party is the main checkout's
B=third_party/felucca/tests/cpu_baseline.txt
[ -f "../$B" ] && B="../$B" || B="$MAIN/$B"
${PYTHON:-python3} -I tests/cpu_budget.py build/cpu-bench.txt "$B"
