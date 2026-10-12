#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The device estimate of the audio interrupt's load from tests/cpu_bench.c's counts (docs/research/cpu-budget.md).
A desk estimate: the device measurement decides (PLAN 0.5, research-todo Q5).
  tests/cpu_budget.py BENCH.txt [CPU_BASELINE.txt]      BENCH: cpu_bench's output; CPU_BASELINE: Felucca's
                                                       tests/cpu_baseline.txt (third_party/felucca, the Mac's counts)
  tests/cpu_budget.py --selftest
Two methods, each a range (% of the 5803 us half buffer, the most instructions of any half, 2 s counted):
  A  Felucca / ChoralRoot's device ratio: device us = Mac instructions / 259 (ChoralRoot docs/INTEGRATION.md
     "Measured on the host"; Felucca's 1.7 % per 100 instructions a sample, from PHYS measured on a device).
     This computer's x86 counts -> the Mac's: the ratio of cpu_bench's felucca/* lines to cpu_baseline.txt (the
     median of the whole counts and of the counts over idle: the range). Clock-free: the ratio was measured at the
     device's clock, whatever it is (Q9), as long as ours runs at Felucca's.
  B  instructions -> cycles: pi32v2 instructions = x86 x d (d from tools/emu-device's interrupt counts on our device
     build against cpu_bench's same cases: idle, one note held); cycles = x CPI 1.0 .. 2.0 (assumed: no
     cycle data for the pi32v2, XIP cache misses unknown); at 240 and at 360 MHz (Q9 open)."""
import re
import statistics
import sys

FS_DEV = 44118                              # the device's sample rate (HANDOFF 2)
HALF = 256                                  # platform.c HALF_FRAMES
HALF_US = HALF * 1e6 / FS_DEV
CHORAL = 259.0                              # Mac instructions per device us
EMU_IDLE, EMU_C4 = 119292, 170512           # tools/emu-device, 2026-10-11 (avg pi32v2 instructions per interrupt)
CPI = (1.0, 2.0)
MHZ = (240, 360)

LINE = re.compile(r"^(\S+(?: \S+)*?)\s+avg\s+(\d+) max\s+(\d+)")


def parse(text):
    out = {}
    for ln in text.splitlines():
        m = LINE.match(ln)
        if m:
            out[m.group(1)] = (int(m.group(2)), int(m.group(3)))
    return out


def mac_ratio(bench, base):
    """(low, high) x86 / Mac instruction ratio from the felucca/* scenarios"""
    idle = bench["idle"][0] / HALF
    tot, dlt = [], []
    for k, (avg, _) in bench.items():
        if not k.startswith("felucca/"):
            continue
        key = "cpu/" + k[len("felucca/"):].replace(" ", "_")
        if key in base:
            tot.append(avg / HALF / base[key])
            if base[key] > base["cpu/mix/idle"]:
                dlt.append((avg / HALF - idle) / (base[key] - base["cpu/mix/idle"]))
    a, b = statistics.median(tot), statistics.median(dlt)
    return min(a, b), max(a, b)


def isa_ratio(bench):
    """(low, high) pi32v2 / x86 instruction ratio: the note's own work, the idle interrupt"""
    i, c = bench["idle"][0], bench["anchor/C4 held"][0]
    return min(EMU_IDLE / i, (EMU_C4 - EMU_IDLE) / (c - i)), max(EMU_IDLE / i, (EMU_C4 - EMU_IDLE) / (c - i))


def load_a(x86, r):
    return x86 / r / CHORAL / HALF_US * 100


def load_b(x86, d, cpi, mhz):
    return x86 * d * cpi / (mhz * HALF_US) * 100


def report(bench, base):
    r = mac_ratio(bench, base)
    d = isa_ratio(bench)
    print(f"x86 / Mac {r[0]:.2f} .. {r[1]:.2f}; pi32v2 / x86 {d[0]:.2f} .. {d[1]:.2f}; half buffer {HALF_US:.0f} us")
    print(f"{'scenario (most instructions of a half)':42s} {'x86':>9s}  {'A: x / 259':>11s}  "
          + "  ".join(f"{'B ' + str(m) + ' MHz':>12s}" for m in MHZ))
    rows = [k for k in ("idle", "anchor/C4 held", "song/new-song-sounds 4v", "song/new-song-sounds 8v") if k in bench]
    for v in ("4v", "8v"):
        best = {}
        for k, (_, mx) in bench.items():
            if k.startswith("4ch/") and k.endswith(" " + v):
                e = k.split("/")[1]
                if e not in best or mx > bench[best[e]][1]:
                    best[e] = k
        rows += sorted(best.values(), key=lambda k: -bench[k][1])
    for k in rows:
        mx = bench[k][1]
        a = f"{load_a(mx, r[1]):3.0f}-{load_a(mx, r[0]):3.0f} %"
        b = "  ".join(f"{load_b(mx, d[0], CPI[0], m):4.0f}-{load_b(mx, d[1], CPI[1], m):3.0f} %" for m in MHZ)
        print(f"{k:42s} {mx:9d}  {a:>11s}  {b}")


def selftest():
    b = parse("idle      avg   100 max   120  voices 0.0\n"
              "anchor/C4 held avg 200 max 210  voices 1.0\n"
              "felucca/FM6/04_PAD avg 2560 max 2600  voices 6\n"
              "4ch/FM6/04_PAD 8v  avg  5000 max  6000  voices 8.0\n")
    assert b["4ch/FM6/04_PAD 8v"] == (5000, 6000) and b["anchor/C4 held"] == (200, 210)
    base = {"cpu/FM6/04_PAD": 5.0, "cpu/mix/idle": 1.0}
    lo, hi = mac_ratio(b, base)                 # whole 10 / 5 = 2.0; over idle (10 - 100/256) / 4 = 2.4
    assert abs(lo - 2.0) < 1e-9 and abs(hi - (10 - 100 / 256) / 4) < 1e-9
    lo, hi = isa_ratio(b)                       # emu idle 119292 / 100, the note (170512 - 119292) / 100
    assert abs(lo - 512.2) < 1e-9 and abs(hi - 1192.92) < 1e-9
    assert abs(load_a(259 * HALF_US, 1.0) - 100) < 1e-9          # 259 instructions a us for a whole half: 100 %
    assert abs(load_b(240 * HALF_US, 1.0, 1.0, 240) - 100) < 1e-9
    assert abs(load_b(240 * HALF_US, 1.0, 1.0, 360) - 100 * 240 / 360) < 1e-9
    print("cpu_budget: self-check ok")


def main():
    if sys.argv[1:] == ["--selftest"]:
        selftest()
        return 0
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    bench = parse(open(sys.argv[1]).read())
    path = sys.argv[2] if len(sys.argv) > 2 else "../third_party/felucca/tests/cpu_baseline.txt"
    base = {}
    for ln in open(path):
        p = ln.split()
        if len(p) == 2 and not ln.startswith("#"):
            base[p[0]] = float(p[1])
    report(bench, base)
    return 0


if __name__ == "__main__":
    sys.exit(main())
