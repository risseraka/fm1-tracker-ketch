#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The boot stub's host tests (docs/boot-stub-plan.md): makes the test packages (make_boot_pkgs.py, needs the SDK
files: AC79_SDK, default third_party/firmwares/math0ne__fm1-chord/vendor/ac79), compiles stub_test.c and
ldr_test.c with the project's zig (tools/wasm-toolchain/.venv, pip ziglang; else CC, default cc) and runs them,
for the FM-1 and for the SMK-37 (-DBOARD_SMK37=1 on SMK-layout packages; the stub's update record for each model).
About 3 minutes.

  python fw/tests/run_boot_tests.py
"""
import os
import shlex
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
OUT = HERE.parent / "build" / "boottest"


def compiler():
    for p in (ROOT / "tools/wasm-toolchain/.venv/Scripts/python.exe", ROOT / "tools/wasm-toolchain/.venv/bin/python"):
        if p.exists():
            return [str(p), "-m", "ziglang", "cc"]
    if os.environ.get("CC") or os.name != "nt":
        return shlex.split(os.environ.get("CC", "cc"))
    sys.exit("run_boot_tests: no tools/wasm-toolchain/.venv (see web/dinghy-emu/README.md)")


def main():
    env = dict(os.environ)
    env.setdefault("AC79_SDK", str(ROOT / "third_party/firmwares/math0ne__fm1-chord/vendor/ac79"))
    exe = ".exe" if os.name == "nt" else ""
    bad = 0
    runs = [("", "fm1", [], ""), ("-smk", "smk37", ["-DBOARD_SMK37=1"], " (SMK-37)"),
            ("-smk", "smk37", ["-DBOARD_SMK37=1", '-DSMK_MODEL="SMK-37 Elite"'], " (SMK-37 Elite)"),
            ("-smk", "smk37", ["-DBOARD_SMK37=1", '-DSMK_MODEL="MKE-P37"'], " (MKE-P37)")]
    made = set()
    for sfx, board, defs, what in runs:
        out = OUT.parent / (OUT.name + sfx)
        out.mkdir(parents=True, exist_ok=True)
        if out not in made:
            subprocess.run([sys.executable, str(HERE / "make_boot_pkgs.py"), str(out),
                            *(["--board", board] if board != "fm1" else [])], check=True, env=env)
            made.add(out)
        for name, args in (("stub_test", []), ("ldr_test", [str(out)])):
            binary = out / (name + exe)
            subprocess.run([*compiler(), "-O2", "-Wall", "-Wno-unused-function", *defs, "-o", str(binary),
                            str(HERE / f"{name}.c")], check=True)
            print(f"-- {name}{what}", flush=True)
            r = subprocess.run([str(binary), *args])
            bad |= r.returncode != 0
    print("boot tests: " + ("FAILED" if bad else "passed"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
