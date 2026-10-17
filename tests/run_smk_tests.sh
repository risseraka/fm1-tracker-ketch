#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# SMK-37 port: Copyright (C) 2026 M. Geisler (genkigenki/makkuro)
# The SMK-37 target's host tests (SMK37.md): the flash map, the update entry and the packages of the three models, the
# mapping on the host, the installers, the boot stub and the loader for both boards. Builds the SMK packages first
# (BOARD=smk37 MODEL=pro|elite|mke ./build.sh) and the FM-1 last, so build/ ends with the FM-1's binaries.
#   sh tests/run_smk_tests.sh            (STOCK_SMK=<SMK-37_Pro_016.fwsc>: the region table against the stock one too)
set -e
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
NODE="${NODE:-node}"          # (node 20.10 or newer: the installer modules are ES modules in .js files)
fail=0
mkdir -p build
run() { what=$1; shift; echo "== $what"; if "$@"; then :; else echo "FAILED: $what"; fail=1; fi; }
for m in pro elite mke; do
    BOARD=smk37 MODEL=$m sh build.sh > build/smk-$m.log 2>&1 || { echo "build failed (MODEL=$m): build/smk-$m.log"; exit 1; }
done
sh build.sh > build/fm1.log 2>&1 || { echo "build failed (FM-1): build/fm1.log"; exit 1; }
mkdir -p build/smktest
$CC -O1 -Wall -Wno-unused-function -DBOARD_SMK37=1 -Ifirmware/hal -Ifirmware/src -o build/smktest/smk_flash_test tests/smk_flash_test.c
run "SMK-37 flash map" build/smktest/smk_flash_test
for m in pro elite mke; do
    f=build/ketch-smk37.fwsc; name="SMK-37 Pro"
    [ $m = elite ] && f=build/ketch-smk37-elite.fwsc && name="SMK-37 Elite"
    [ $m = mke ] && f=build/ketch-smk37-mke.fwsc && name="MKE-P37"
    run "SMK package $f" python3 tests/smk_pkg_test.py $f "${name}_950"
    $CC -O1 -Wall -Wno-unused-function -DBOARD_SMK37=1 "-DSMK_MODEL=\"$name\"" -o build/smktest/smk_ota_test_$m tests/smk_ota_test.c
    run "update entry, SMK-37 build ($m)" build/smktest/smk_ota_test_$m $f build/dinghy.fwsc
done
run "the mapping on the host (pads, keys, faders, wheels, LEDs, K1-K4)" python3 tests/smk_ui_test.py
run "host mode, SMK-37 build: the phrase written on the pads plays" env BOARD=smk37 sh tests/run_host.sh
run "host mode, FM-1 build" sh tests/run_host.sh
run "installer (fm1_install.py)" python3 tests/install_test.py
if command -v "$NODE" > /dev/null; then
    run "web installer (fm1pkg.js, fm1ota.js)" "$NODE" --experimental-detect-module tests/smk_web_test.mjs build/ketch-smk37.fwsc
else
    echo "== web installer: skipped (no node; NODE=...)"
fi
run "boot stub and update loader, both boards, three models" python3 tests/run_boot_tests.py
[ $fail = 0 ] && echo "smk tests passed" || echo "SMK TESTS FAILED"
exit $fail
