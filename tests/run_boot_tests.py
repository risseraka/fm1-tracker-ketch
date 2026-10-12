#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The boot stub's host tests (docs/boot-stub-plan.md): makes the test packages (make_boot_pkgs.py, needs the SDK
files: AC79_SDK, default third_party/firmwares/math0ne__fm1-chord/vendor/ac79), compiles stub_test.c and
ldr_test.c with the project's zig (tools/wasm-toolchain/.venv, pip ziglang) and runs them. About 3 minutes.

  python fw/tests/run_boot_tests.py
"""
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
OUT = HERE.parent / "build" / "boottest"


def zig_python():
    for p in (ROOT / "tools/wasm-toolchain/.venv/Scripts/python.exe", ROOT / "tools/wasm-toolchain/.venv/bin/python"):
        if p.exists():
            return str(p)
    sys.exit("run_boot_tests: no tools/wasm-toolchain/.venv (see web/dinghy-emu/README.md)")


def main():
    env = dict(os.environ)
    env.setdefault("AC79_SDK", str(ROOT / "third_party/firmwares/math0ne__fm1-chord/vendor/ac79"))
    OUT.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, str(HERE / "make_boot_pkgs.py"), str(OUT)], check=True, env=env)
    exe = ".exe" if os.name == "nt" else ""
    bad = 0
    for name, args in (("stub_test", []), ("ldr_test", [str(OUT)])):
        binary = OUT / (name + exe)
        subprocess.run([zig_python(), "-m", "ziglang", "cc", "-O2", "-Wall", "-Wno-unused-function", "-o",
                        str(binary), str(HERE / f"{name}.c")], check=True)
        r = subprocess.run([str(binary), *args])
        bad |= r.returncode != 0
    print("boot tests: " + ("FAILED" if bad else "passed"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
