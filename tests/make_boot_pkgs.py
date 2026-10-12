#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Test packages for the boot stub's host tests (fw/tests/ldr_test.c, docs/boot-stub-plan.md), made with the real
packager (tools/fm1pkg_make.py) from synthetic apps, so no JieLi toolchain is needed; the SDK files are (AC79_SDK).

  make_boot_pkgs.py OUTDIR

  old.fwsc       no stub, Dinghy's layout before the stub (an app of 300 KB from 0x4120)
  stock.fwsc     no stub, an app that reaches into the marker's sector (as the stock firmware probably does)
  other.fwsc     no stub, another app (with old.fwsc: the dark unit's half-and-half state)
  new.fwsc       stub A (version 3) + body 1
  same.fwsc      stub A + body 2 (every later install)
  oldstub.fwsc   stub B (version 2) + body 3 (a stub change)
  badmark.fwsc   stub A + body 1, the marker's body CRC wrong (the stub must refuse it)

The stubs are made up: what the packager and the loader look at is the "CRSTUB" signature at app.bin + 8 and
the bytes; the decision itself is firmware/stub/stub_core.h, compiled into the C tests. The packages carry a
made-up update loader wrapped by tools/build.py ldr_wrap ("FELUCCA-LOADER-1" where ota.c and the stub look)."""
import random
import struct
import sys
import zlib
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))
import fm1pkg_make as pk  # noqa: E402
import build  # noqa: E402  (ldr_wrap; importing it builds nothing)


def blob(seed, n):
    r = random.Random(seed)
    return bytes(r.getrandbits(8) for _ in range(n))


def stub(seed, version):
    s = bytearray(b"\xFF" * pk.STUB_LEN)
    s[0:8] = bytes.fromhex("0481800000000000")
    s[8:8 + len(pk.STUB_SIG)] = pk.STUB_SIG
    struct.pack_into("<H", s, 8 + len(pk.STUB_SIG), version)
    s[16:2300] = blob(seed, 2284)
    return bytes(s)


def loader():
    image = b"\x00\x00" + b"FELUCCA-LOADER-1" + bytes((i * 7 + (i >> 5)) & 0xFF for i in range(7000))
    return build.ldr_wrap(image)


def bad_marker(body):
    m = struct.pack("<IIII", pk.MARK_MAGIC, pk.MARK_FORMAT, len(body), zlib.crc32(body) ^ 1)
    return m + struct.pack("<I", zlib.crc32(m))


def main():
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    ota = loader()
    a, b = stub(1, 3), stub(2, 2)
    apps = {"old": blob(10, 300000), "stock": blob(11, pk.MARK_OFF + 100), "other": blob(12, 310000),
            "new": a + blob(20, 300000), "same": a + blob(21, 280000), "oldstub": b + blob(22, 290000)}
    for name, app in apps.items():
        (out / f"{name}.fwsc").write_bytes(pk.ufw(pk.flash_image(app, pk.KEY), ota, "FM-1_990"))
    (out / "badmark.fwsc").write_bytes(pk.ufw(pk.flash_image(apps["new"], pk.KEY, bad_marker), ota, "FM-1_990"))
    print(f"boot test packages in {out}")


if __name__ == "__main__":
    main()
