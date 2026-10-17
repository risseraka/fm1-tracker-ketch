#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# SMK-37 port: Copyright (C) 2026 M. Geisler (genkigenki/makkuro)
"""The SMK-37 mapping on the host (tests/host.c with BOARD_SMK37, firmware/src/smk_keys.c): the gamepad on the pads,
every key a note with its velocity in the notes modes, OFF on a pad, the scenes on the keybed with SEL held, the
faders, the wheels, the pad and button LEDs, KNOB 1-4 named K1-K4. Builds build/host/tracker-smk37-test, runs short
scripts, checks the dumped state. Exit 1 on a failure.

  python3 tests/smk_ui_test.py
"""
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build" / "host"
BIN = OUT / "tracker-smk37-test"
fails = 0
PAD = dict(UP=1, OPTION=6, EDIT=7, LEFT=8, DOWN=9, RIGHT=10, OFF=12, SHIFT=14, PLAY=15)
BTN = dict(SEL=8, PLAY=16, REC=17)
OFF, ON, ACT = 0x6E2600, 0x2878D2, 0xBEE6FF   # smk_keys.c: off dark orange, on light blue, active bright
TO_PHRASE = ["200 pad 14 100", "230 pad 10 100", "250 pad 10 0", "280 pad 10 100", "300 pad 10 0", "320 pad 14 0"]


def check(name, ok, detail=""):
    global fails
    print(("  ok    " if ok else "  FAIL  ") + name + ("" if ok else f"  ({detail})"))
    fails += not ok


def run(lines):
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
        f.write("\n".join(lines) + "\n")
    out = subprocess.run([str(BIN), str(OUT / "smk-test"), f.name], capture_output=True, text=True, check=True).stdout
    d = {}
    for ln in out.splitlines():
        m = re.match(r"\s*(\d+) ms  (LEDS|PADS|STATE) (.*)$", ln)
        if m:
            d.setdefault(int(m[1]), {})[m[2]] = m[3].strip()
    return d


def state(d, t):
    s = d[t]["STATE"]
    st = dict(kv.split("=", 1) for kv in s.split(" msg=")[0].split())
    st["msg"] = s.split(" msg=", 1)[1]
    return st


def pads(d, t):
    return [int(x, 16) for x in d[t]["PADS"].split()]


def leds(d, t):
    return [int(x) for x in d[t]["LEDS"].split()]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    subprocess.run(["cc", "-O1", "-w", "-DDINGHY_HOST", "-DBOARD_SMK37=1", '-DAPP_VER="test"',
                    *("-I" + str(ROOT / p) for p in ("firmware/hal", "firmware/src", "firmware/gen", "firmware/sound")),
                    "-o", str(BIN), str(ROOT / "firmware/src/tracker.c")], check=True)

    print("== the gamepad on the pads, the keys as notes (REC: notes mode, step entry)")
    d = run([*TO_PHRASE, "350 dump", "400 btn 17",
             "500 key 12 90", "550 key 12 0", "600 dump",           # C4 at 90
             "700 key 0 40", "750 key 0 0", "800 dump",             # C3, an outer key, at 40
             "900 pad 12 100", "950 pad 12 0", "1000 dump",         # OFF
             "1100 key 36 127", "1150 key 36 0", "1200 dump",       # C6, an outer key
             "1300 key 5 70", "1350 key 5 0", "1400 dump",          # F3: the FM-1's LEFT, a note here
             "1500 key 20 100", "1550 dump", "1600 key 20 0", "1650 dump", "1700 end"])
    s = state(d, 350)
    check("SHIFT + RIGHT twice on the pads: the phrase screen", s["view"] == state(d, 600)["view"] and s["kb"] == "0", s)
    s = state(d, 600)
    check("REC: notes mode", s["kb"] == "1", s)
    check("C4 written with its velocity", (s["prev_n"], s["prev_vol"], s["row"]) == ("60", "90", "1"), s)
    s = state(d, 800)
    check("C3 (an outer key) written with its velocity", (s["prev_n"], s["prev_vol"], s["row"]) == ("48", "40", "2"), s)
    s = state(d, 1000)
    check("OFF pad: a note off", (s["prev_n"], s["row"]) == ("254", "3"), s)
    s = state(d, 1200)
    check("C6 (the top key) written", (s["prev_n"], s["prev_vol"]) == ("84", "127"), s)
    s = state(d, 1400)
    check("F3 is a note, not LEFT", (s["prev_n"], s["view"]) == ("53", state(d, 600)["view"]), s)
    check("a held key sounds (key_want), let go it stops", state(d, 1550)["want"] != "0" and state(d, 1650)["want"] == "0",
          (state(d, 1550)["want"], state(d, 1650)["want"]))
    p = pads(d, 600)
    check("notes mode: the OFF pad on (light blue)", p[PAD["OFF"]] == ON, hex(p[PAD["OFF"]]))

    print("== the pads' and buttons' lights")
    d = run(["300 dump", "400 pad 7 100", "450 dump", "500 pad 7 0", "600 btn 16", "700 dump", "800 btn 16", "900 dump",
             "1000 end"])
    p = pads(d, 300)
    for name in ("UP", "LEFT", "DOWN", "RIGHT", "SHIFT", "OPTION", "EDIT"):
        check(f"{name} idle: on (light blue)", p[PAD[name]] == ON, hex(p[PAD[name]]))
    check("stopped: PLAY off (dark orange)", p[PAD["PLAY"]] == OFF, hex(p[PAD["PLAY"]]))
    check("OFF off (dark orange) outside the notes modes, unused pads dark",
          p[PAD["OFF"]] == OFF and p[0] == p[2] == p[11] == 0, p)
    check("the three colours clearly apart", all(max(c >> 16, (c >> 8) & 255, c & 255) >= 100 for c in (OFF, ON, ACT)) and
          OFF >> 16 > (OFF & 255) and (ON & 255) > ON >> 16)
    check("EDIT held: active (bright)", pads(d, 450)[PAD["EDIT"]] == ACT, hex(pads(d, 450)[7]))
    check("playing: PLAY pad active, SEQ PLAY lit", pads(d, 700)[PAD["PLAY"]] == ACT and leds(d, 700)[BTN["PLAY"]] == 1,
          (hex(pads(d, 700)[15]), leds(d, 700)))
    check("stopped: PLAY off again", pads(d, 900)[PAD["PLAY"]] == OFF and leds(d, 900)[BTN["PLAY"]] == 0)

    print("== faders, wheels")
    d = run(["300 fader 0 64", "350 dump", "400 fader 1 1", "450 dump", "500 fader 2 10", "520 fader 2 11", "550 dump",
             "600 wheel 0 700", "650 dump", "700 wheel 0 540", "750 dump", "800 wheel 1 480", "850 dump", "900 end"])
    check("fader 1: channel 1's level", state(d, 350)["lvl"].split(",")[0] == "64", state(d, 350)["lvl"])
    check("a fader's jitter (1 step) moves nothing", state(d, 450)["lvl"].split(",")[1] == "104", state(d, 450)["lvl"])
    check("a moving fader follows", state(d, 550)["lvl"].split(",")[2] == "11", state(d, 550)["lvl"])
    check("pitch wheel up: a bend up", int(state(d, 650)["bend"]) > 100, state(d, 650)["bend"])
    check("pitch wheel at rest: no bend", state(d, 750)["bend"] == "0", state(d, 750)["bend"])
    check("mod wheel up: MODW", int(state(d, 850)["mw"]) > 64, state(d, 850)["mw"])

    print("== the scenes with SEL held, the knob names")
    d = run(["300 hold 8 1", "350 key 11 100", "400 key 11 0", "450 dump", "500 hold 8 0",
             "600 pad 14 100", "620 pad 10 100", "640 pad 10 0", "660 pad 10 100", "680 pad 10 0", "700 pad 10 100",
             "720 pad 10 0", "740 pad 14 0", "800 pad 9 100", "820 pad 9 0", "850 pad 9 100", "870 pad 9 0",
             "900 pad 7 100", "950 enc 0 1", "1000 dump", "1050 pad 7 0", "1100 end"])
    check("SEL + B3: scene 1 picked (not a note)", state(d, 450)["msg"].startswith("scene 1") and state(d, 450)["want"] == "0",
          state(d, 450)["msg"])
    check("EDIT + K1 names it K1", re.match(r"K1 = \w", state(d, 1000)["msg"]) is not None and
          "KNOB" not in state(d, 1000)["msg"], state(d, 1000)["msg"])
    print("smk ui tests: " + ("FAILED" if fails else "passed"))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
