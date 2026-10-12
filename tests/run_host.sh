#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Run firmware/src/tracker.c on this computer (tests/host.c) through tests/host_phrase.txt: build/host/tracker.wav and the
# screens as PNG. Any host C compiler: CC (default cc; a path in it is relative to fw/), e.g. on Windows the project's zig:
#   CC="../tools/wasm-toolchain/.venv/Scripts/python -m ziglang cc" sh fw/tests/run_host.sh
set -e
CC="${CC:-cc}"
cd "$(dirname "$0")/.."
mkdir -p build/host
$CC -O2 -Wall -Wno-unused-function -Wno-int-to-pointer-cast -DDINGHY_HOST -DAPP_NAME="\"TRACKER\"" -DAPP_VER="\"${APP_VER:-dev}\"" \
    -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -Ifirmware/sound -o build/host/tracker.exe firmware/src/tracker.c
build/host/tracker.exe build/host/tracker tests/host_phrase.txt
python3 - <<'PY'
from pathlib import Path
from PIL import Image
import array, wave
w = wave.open("build/host/tracker.wav")
a = array.array("h", w.readframes(w.getnframes()))[2 * 3500 * 441 // 10:]   # from 3.5 s: the phrase playing, not EDIT's previews
assert len(a) and max(map(abs, a)) > 1000, "host mode: the phrase is silent"
print("sound  -> the phrase plays (peak %d after 3.5 s)" % max(map(abs, a)))
for p in sorted(Path("build/host").glob("tracker-*.ppm")):
    Image.open(p).resize((480, 480), Image.NEAREST).save(p.with_suffix(".png"))
    p.unlink()
    print("screen ->", p.with_suffix(".png"))
PY
