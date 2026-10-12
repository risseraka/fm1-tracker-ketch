#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""fw/tools/fm1_install.py against a simulated FM-1 (no hardware, no mido).
SLOOP's tests/install_test.py (third_party/sloop-fm1), with ours on top: the flash-head guard and the
stock firmware's 0xC... / 0xD... requests. The fake device: identity on the handshake,
then "device asks, host answers" reads of the logical image.
  python fw/tests/install_test.py
Also loads fw/build/dinghy.fwsc when it exists (skipped otherwise)."""
import hashlib
import io
import queue
import shutil
import sys
import tempfile
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import fm1_install as I  # noqa: E402

I.DELAY.update(open=0, start=0.01, reply=0, loader=0.01, reboot=0.01, retry=0.02, poll=0.05, hs=0.1,
               idle_check=0.4, idle_write=0.4, wait_loader=2, wait_reboot=2)
failed = 0


def ok(cond, what):
    global failed
    print(f"{what:<64} {'ok' if cond else 'FAIL'}")
    failed += not cond


# ---------------------------------------------------------- simulated FM-1 ---

class FakeLink:
    def __init__(self, dev, gen):
        self.dev, self.gen, self.q, self.closed = dev, gen, queue.Queue(), False

    @property
    def lost(self):
        return self.gen != self.dev.gen or not self.dev.connected

    def send(self, pkt):
        if self.lost:
            return False
        self.dev.sent.append(bytes(pkt))
        self.dev.rx(bytes(pkt))
        return True

    def read(self, timeout):
        try:
            return self.q.get(timeout=max(timeout, 0))
        except queue.Empty:
            return None

    def drain(self):
        while not self.q.empty():
            self.q.get_nowait()

    def close(self):
        self.closed = True


class FakeFM1:
    """a MIDI backend with one FM-1 on it"""

    def __init__(self, image, identity="FM-1_015", name="FM-1", unplug_after=None, after_write="FM-1_900",
                 stall_after=None, bad_addr=None, notes=()):
        self.image, self.unplug_after, self.after_write = image, unplug_after, after_write
        self.stall_after, self.bad_addr, self.notes = stall_after, bad_addr, list(notes)
        self.served = self.bad = self.upgrades = 0
        self.sent, self.links, self.gen, self.lock = [], [], 0, threading.Lock()
        self.boot(identity, name)

    def boot(self, identity, name):
        with self.lock:
            self.gen += 1
            self.identity, self.name, self.connected = identity, name, True
            self.waiting, self.queue = None, []

    def input_names(self):
        return [self.name] if self.connected else []

    output_names = input_names

    def open(self, in_name, out_name):
        if in_name != self.name or not self.connected:
            raise IOError("no such port")
        link = FakeLink(self, self.gen)
        self.links.append(link)
        return link

    def tx(self, pkt):
        for link in self.links:
            if link.gen == self.gen and not link.closed:
                link.q.put(bytes(pkt))

    def rx(self, d):
        if d == I.HS_QUERY:
            t = self.identity.encode()
            self.tx(b"\xF0" + I.pack7(bytes([0, 0x59, 0x11, 0, 0, 0]) + t + bytes(28 - len(t))) + b"\xF7")
        elif d == I.UPGRADE:
            self.upgrades += 1
            first = self.bad_addr if self.bad_addr is not None else 0
            self.queue = ([(first + k * 512, 512) for k in range(6)] + [(I.FINISH_WRITE, 8)]
                          if self.identity.startswith("ota-")
                          else [(0, 64), (0x40, 160)] + [(a, 8) for a in self.notes] +
                          [(0x1000, 512), (I.FINISH_CHECK, 8)])
            self.next()
        elif self.waiting:
            u = I.unpack7(d[1:-1])
            addr, n = self.waiting
            fin = addr >= I.NOTE                # 0xC... 0xD... 0xE... 0xF...: the host answers "success"
            want = b"success\0" if fin else self.image[addr:addr + n]
            if u[14:14 + len(want)] != want or len(u) != 15 + len(want):
                self.bad += 1
            self.waiting = None
            self.served += 1
            if self.unplug_after and self.served >= self.unplug_after:
                self.connected = False
                return
            if self.stall_after and self.served >= self.stall_after or addr & 0xF0000000 == I.REFUSED:
                return
            if addr == I.FINISH_CHECK:
                threading.Timer(0.05, self.boot, ("ota-FM-1_900", "Felucca Update")).start()
            elif addr == I.FINISH_WRITE:
                threading.Timer(0.05, self.boot, (self.after_write, "Felucca")).start()
            else:
                self.next()

    def next(self):
        if not self.queue:
            return
        self.waiting = addr, n = self.queue.pop(0)
        u = bytearray([0, 0x59, 0x30, 0, 0, 0, 0]) + addr.to_bytes(4, "little") + bytes([n & 0xFF, n >> 8, 0])
        u.append(~sum(u[6:14]) & 0xFF)
        self.tx(b"\xF0" + I.pack7(u) + b"\xF7")


# ----------------------------------------------------------------- helpers ---

FL, OTA, SIZE = 0x400, 0x4C00, 0x6000      # flash.bin (its 16 KB head first), ota.bin, the end


def package(product="FM-1_900", marker="ota", damage=False):
    """a package with a valid header and file list (flash.bin, ota.bin), as fm1pkg_make writes; the loader
    marker where ota.c looks ("ota"), elsewhere ("flash") or nowhere (None)"""
    img = bytearray((i * 7) & 0xFF for i in range(SIZE))
    img[OTA + 0x50:OTA + 0x60] = I.LOADER_MARK if marker == "ota" else bytes(16)
    img[FL + 0x4400:FL + 0x4410] = I.LOADER_MARK if marker == "flash" else bytes(16)
    lst = b""
    for typ, off, end, name in ((0, FL, OTA, b"flash.bin"), (100, OTA, SIZE, b"ota.bin")):
        data = bytes(img[off:end])
        e = bytearray(0x50)
        e[0:16] = typ.to_bytes(2, "little") + bytes(2) + I._crc16(data).to_bytes(2, "little") + bytes(2) + \
            off.to_bytes(4, "little") + len(data).to_bytes(4, "little")
        e[0x40:0x40 + len(name)] = name
        lst += I._dec(bytes(e))
    hdr = bytearray(0x40)
    hdr[4:8] = SIZE.to_bytes(4, "little")
    hdr[8:10] = (2).to_bytes(2, "little")
    hdr[2:4] = I._crc16(lst).to_bytes(2, "little")
    hdr[0:2] = I._crc16(bytes(hdr[2:0x40])).to_bytes(2, "little")
    img[0:0x40] = I._dec(bytes(hdr))
    img[0x40:0xE0] = lst
    if damage:
        img[0x1000] ^= 1                    # after the CRCs: the stored one no longer holds
    raw = bytearray()
    for i in range(I.BLOCKS):
        m = (ord(product[i]) + i + 1) & 0xFF if i < len(product) else 0x7D
        raw += img[i * I.KEEP:(i + 1) * I.KEEP] + bytes([m])
    raw += img[I.BLOCKS * I.KEEP:]
    return bytes(raw)


TMP = Path(tempfile.mkdtemp(prefix="felucca-install-"))


def pkgfile(name, raw):
    p = TMP / name
    p.write_bytes(raw)
    return str(p)


def cli(args, dev, answer=True):
    out, err = io.StringIO(), io.StringIO()
    old, sys.stderr = sys.stderr, err
    try:
        rc = I.main(args, backend=dev, out=out, ask=lambda _p: answer)
    finally:
        sys.stderr = old
    return rc, out.getvalue(), err.getvalue()


# ------------------------------------------------------------------- tests ---

def wire():
    data = bytes(range(256)) * 3
    ok(I.unpack7(I.pack7(data))[:len(data)] == data, "pack7 / unpack7 round trip")
    pkt = I.response(0x12345, b"\x01\x02\x03", fl=5)
    u = I.unpack7(pkt[1:-1])
    ok(u[:3] == b"\x00\x59\x30" and int.from_bytes(u[7:11], "little") == 0x12345 and u[14:17] == b"\x01\x02\x03"
       and ~sum(u[6:17]) & 0xFF == u[17], "response: header, address, data, checksum")
    raw = package("FM-1_906")
    ok(I.product_of(raw) == "FM-1_906" and len(I.logical_image(raw)) == len(raw) - 20, "product_of / logical_image (synthetic)")


def installs():
    raw = package()
    image = I.logical_image(raw)
    p = pkgfile("ok.fwsc", raw)

    dev = FakeFM1(image)
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and dev.upgrades == 2 and dev.served == 11 and "done: the FM-1 runs FM-1_900" in out
       and "100%" in out, f"install: running -> loader -> Felucca ({dev.served} reads)")

    dev = FakeFM1(image, identity="ota-FM-1_900", name="Felucca Update")
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and dev.upgrades == 1 and "update mode" in out, "install: device already in update mode -> finishes the write")

    dev = FakeFM1(image)
    rc, out, err = cli([p], dev, answer=False)
    ok(rc == 1 and dev.upgrades == 0 and "cancelled" in out, "install: answer no -> nothing sent but the handshake")

    dev = FakeFM1(image, identity="FM-1_905", name="Felucca")
    rc, out, err = cli(["--info"], dev)
    ok(rc == 0 and "FM-1_905" in out and "running" in out and dev.upgrades == 0, "--info: identity of the connected FM-1")

    dev = FakeFM1(image, identity="ota-FM-1_900", name="Felucca Update")
    rc, out, err = cli(["--info"], dev)
    ok(rc == 0 and "update loader" in out, "--info: device in update mode")


def errors():
    raw = package()
    image = I.logical_image(raw)
    p = pkgfile("ok.fwsc", raw)

    rc, out, err = cli([p, "--yes"], FakeFM1(image, name="IAC Driver Bus 1"))
    ok(rc == 3 and "not found" in err and "IAC Driver" in err, "no FM-1 port -> exit 3, lists the MIDI inputs")
    dev = FakeFM1(image)
    rc, out, err = cli([p, "--yes", "--port", "Felucca"], dev)
    ok(rc == 3 and dev.upgrades == 0, "--port that matches nothing -> exit 3")
    dev = FakeFM1(image, name="My Interface")
    rc, out, err = cli(["--info", "--port", "my int"], dev)
    ok(rc == 0 and "FM-1_015" in out, "--port picks a port the default match skips")

    t0 = time.monotonic()
    rc, out, err = cli([p, "--yes"], FakeFM1(image, unplug_after=2))
    ok(rc == 4 and "disconnected" in err and "nothing was written" in err and time.monotonic() - t0 < 3,
       "unplugged in step 1 -> exit 4 'lost', nothing written")
    t0 = time.monotonic()
    rc, out, err = cli([p, "--yes"], FakeFM1(image, identity="ota-FM-1_900", name="Felucca Update", unplug_after=3))
    ok(rc == 4 and "run the install again" in err and time.monotonic() - t0 < 3, "unplugged during the write -> exit 4 at once")
    rc, out, err = cli([p, "--yes"], FakeFM1(image, identity="ota-FM-1_900", name="Felucca Update", stall_after=3))
    ok(rc == 4 and "stopped answering" in err, "loader stops answering -> exit 4 after the idle time")
    rc, out, err = cli([p, "--yes"], FakeFM1(image, identity="ota-FM-1_900", name="Felucca Update", bad_addr=len(image)))
    ok(rc == 4 and "outside the package" in err, "read past the package -> exit 4")

    rc, out, err = cli([p, "--yes"], FakeFM1(image, after_write="FM-1_015"))
    ok(rc == 6 and "reports FM-1_015" in err, "another identity after the restart -> exit 6")
    dev = FakeFM1(image, identity="XY-9_001", name="usb-midi")
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 6 and dev.upgrades == 0, "another model -> exit 6, nothing sent")
    dev = FakeFM1(image, identity="ota-FM-1_015", name="FM-1 Update")
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 6 and dev.upgrades == 0 and "another firmware" in err, "another firmware's update loader -> exit 6, nothing sent")

    class Stuck(FakeFM1):          # the loader never shows up
        def boot(self, identity, name):
            super().boot(identity, name)
            if identity.startswith("ota-"):
                self.connected = False
    rc, out, err = cli([p, "--yes"], Stuck(image))
    ok(rc == 5 and "loader did not appear" in err, "no loader after step 1 -> exit 5")

    plain = pkgfile("plain.fwsc", package(marker=None))
    dev = FakeFM1(image)
    rc, out, err = cli([plain, "--yes"], dev)
    ok(rc == 2 and "no Felucca update loader" in err and dev.sent == [], "package without the loader marker -> exit 2, no MIDI")
    dev = FakeFM1(image)
    rc, out, err = cli([pkgfile("elsewhere.fwsc", package(marker="flash")), "--yes"], dev)
    ok(rc == 2 and "no Felucca update loader" in err and dev.sent == [], "the marker outside ota.bin's start -> not ours")
    dev = FakeFM1(I.logical_image(package(marker=None)))
    rc, out, err = cli([plain, "--yes", "--force"], dev)
    ok(rc == 2 and "not stock's" in err and dev.sent == [], "another loader + a non-stock flash head -> exit 2 even with --force")
    stock, I.STOCK_HEAD = I.STOCK_HEAD, hashlib.sha256(I.logical_image(package(marker=None))[FL:FL + 0x4000]).hexdigest()
    rc, out, err = cli([plain, "--yes", "--force"], dev)
    I.STOCK_HEAD = stock
    ok(rc == 0 and dev.bad == 0, "... with stock's head: installs with --force")

    # stock firmware's own requests in step 1 (M-UPGRADE func_140012740: 0xC... acked, 0xD... = refused)
    dev = FakeFM1(image, notes=[0xC1020300, 0xC2000005, 0xC0001234])
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and dev.served == 14, "0xC... state / stop / diagnostic -> 'success', the install goes on")
    for code, words in ((0x97, "already runs this firmware"), (0x83, "failed the device's check"), (0x42, "error")):
        dev = FakeFM1(image, notes=[0xD0000100 | code])
        rc, out, err = cli([p, "--yes"], dev)
        ok(rc == 7 and words in err and dev.bad == 0 and dev.served == 3 and dev.upgrades == 1,
           f"0xD00001{code:02X} -> 'success', then exit 7 ({words})")
    dev = FakeFM1(image)
    rc, out, err = cli([pkgfile("damaged.fwsc", package(damage=True)), "--yes"], dev)
    ok(rc == 2 and "damaged package" in err and dev.sent == [], "damaged package (a file's CRC) -> exit 2, no MIDI")
    rc, out, err = cli([pkgfile("short.fwsc", b"\0" * 100), "--yes"], FakeFM1(image))
    ok(rc == 2 and "too short" in err, "not a package -> exit 2")
    rc, out, err = cli([str(TMP / "missing.fwsc"), "--yes"], FakeFM1(image))
    ok(rc == 2, "missing file -> exit 2")


def ours():
    built = ROOT / "build/dinghy.fwsc"
    if not built.exists():
        print("our package: skipped (needs fw/build/dinghy.fwsc: fw/tools/build.py)")
        return
    product, image = I.load_package(str(built), False)
    ok(I.LOADER_MARK in I.package_file(image, 100)[0x48:0x88], f"{built.name} ({product}): our loader, where ota.c looks")


wire()
installs()
errors()
ours()
shutil.rmtree(TMP, ignore_errors=True)
print(f"INSTALL TESTS FAILED ({failed})" if failed else "install tests passed")
sys.exit(1 if failed else 0)
