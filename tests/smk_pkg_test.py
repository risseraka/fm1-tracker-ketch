#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# SMK-37 port: Copyright (C) 2026 M. Geisler (genkigenki/makkuro)
"""An SMK-37 package as BOARD=smk37 ./build.sh writes it: its identity, flash.bin the SMK's app area (0x9D000), our
update loader; with STOCK_SMK=<the stock SMK-37_Pro_016.fwsc> also its region table
(flash 0x4060-0x411F, enciphered with the same chip key: equal bytes, equal table).

  python3 tests/smk_pkg_test.py build/ketch-smk37.fwsc "SMK-37 Pro_950"
"""
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import fm1_install as I  # noqa: E402


def main():
    path, want = sys.argv[1], sys.argv[2]
    bad = 0

    def check(what, ok):
        nonlocal bad
        print(f"{what:<72} {'ok' if ok else 'FAIL'}")
        bad += not ok

    raw = Path(path).read_bytes()
    product, image = I.load_package(path, False)
    fl = I.package_file(image, 0)
    check(f"{Path(path).name}: identity {want}", product == want)
    check("flash.bin = [0, 0x9D000): the SMK-37's app area (the stock PRCT end)", len(fl) == 0x9D000)
    check("our update loader (FELUCCA-LOADER-1 where ota.c looks)", I.LOADER_MARK in I.package_file(image, 100)[0x48:0x88])
    check("20 interleave blocks", I.block_count(raw) == 20 or I.deinterleave(raw) == I.logical_image(raw))
    stock = os.environ.get("STOCK_SMK")
    if stock and Path(stock).exists():
        sraw = Path(stock).read_bytes()
        sfl = I.package_file(I.deinterleave(sraw), 0)
        check("the region table == the stock SMK-37 package's", fl[0x4060:0x4120] == sfl[0x4060:0x4120])
    else:
        print("region table against the stock package: skipped (STOCK_SMK)")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
