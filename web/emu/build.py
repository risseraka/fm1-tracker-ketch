#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build the Dinghy browser emulator: a Dinghy-based firmware's own C sources -> site/<app>.wasm.

  python web/emu/build.py                            # Ketch (firmware/src/tracker.c) -> web/emu/site/tracker.wasm
  python web/dinghy-emu/build.py --app path/to/app.c # any firmware built on Dinghy's platform.c -> site/app.wasm
  The page loads site/<name>.wasm from ?fw=<name> (default: tracker).

Steps:
  1. stage: the app's firmware/ folder (src/ with platform.c and gfx.c, gen/, any other source folder such as
     sound/) is copied to build/dinghy-emu/firmware/, each folder on the include path; dinghy_wasm.c becomes build/dinghy-emu/tests/host.c,
     the file platform.c includes when DINGHY_HOST is set. No firmware source is changed.
  2. compile: zig cc (clang + lld) for wasm32-freestanding, one translation unit, as on the FM-1.
  3. asyncify: Binaryen's wasm-opt makes host_yield() (each platform_poll) a point where the page can pause
     app_main() and resume it later.
Toolchain (project-local, see README.md): tools/wasm-toolchain/.venv (pip ziglang) and
tools/wasm-toolchain/node_modules/binaryen (npm)."""
import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
TC = ROOT / "tools" / "wasm-toolchain"
STAGE = ROOT / "build" / "dinghy-emu"


def venv_python():
    for p in (TC / ".venv" / "Scripts" / "python.exe", TC / ".venv" / "bin" / "python"):
        if p.exists():
            return p
    sys.exit("build.py: no tools/wasm-toolchain/.venv (see web/emu/README.md)")


def wasm_opt():
    p = TC / "node_modules" / "binaryen" / "bin" / "wasm-opt"
    if not p.exists():
        sys.exit("build.py: no binaryen in tools/wasm-toolchain/node_modules (see web/emu/README.md)")
    return ["node", str(p)]


def run(cmd):
    print(" ".join(str(c) for c in cmd[:6]), "...")
    r = subprocess.run([str(c) for c in cmd], capture_output=True, text=True)
    if r.stdout.strip():
        print(r.stdout.strip())
    if r.returncode:
        sys.exit(r.stderr.strip() or f"failed: {cmd[0]}")
    if r.stderr.strip():
        print(r.stderr.strip())


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--app", default=str(ROOT / "firmware" / "src" / "tracker.c"),
                    help="the app's main .c (it includes platform.c)")
    ap.add_argument("--gen", help="the generated headers folder (default: ../gen next to the app's folder)")
    ap.add_argument("--name", default="KETCH", help="APP_NAME")
    ap.add_argument("--ver", default="web", help="APP_VER")
    ap.add_argument("--out", help="site/OUT.wasm (default: the app file's name, e.g. dinghy, tracker)")
    ap.add_argument("-D", action="append", default=[], metavar="NAME[=VALUE]", help="extra define")
    a = ap.parse_args()

    app = Path(a.app).resolve()
    src = app.parent
    gen = Path(a.gen).resolve() if a.gen else src.parent / "gen"
    if not (src / "platform.c").exists():
        sys.exit(f"build.py: no platform.c next to {app}: not a Dinghy-based firmware")

    # 1. stage
    if STAGE.exists():
        shutil.rmtree(STAGE)
    fwdir = src.parent                              # the app's firmware/ folder: src, gen, and any other
    STAGE.mkdir(parents=True)                       # source folder (e.g. sound/) next to them
    (STAGE / "tests").mkdir()
    incs = []
    for d in sorted(p for p in fwdir.iterdir() if p.is_dir()):
        shutil.copytree(d, STAGE / "firmware" / d.name)
        if d.name not in ("hal", "loader"):         # (hal/ is staged for relative includes, not searched)
            incs += ["-I", STAGE / "firmware" / d.name]
    if gen.is_dir() and gen.parent != fwdir:
        shutil.copytree(gen, STAGE / "firmware" / "gen", dirs_exist_ok=True)
        incs += ["-I", STAGE / "firmware" / "gen"]
    shutil.copy2(HERE / "dinghy_wasm.c", STAGE / "tests" / "host.c")

    # 2. compile
    raw = STAGE / "raw.wasm"
    defs = [f"-DAPP_NAME=\"{a.name}\"", f"-DAPP_VER=\"{a.ver}\"", "-DDINGHY_HOST"] + [f"-D{d}" for d in a.D]
    run([venv_python(), "-m", "ziglang", "cc", "--target=wasm32-freestanding", "-O2", "-nostdlib",
         "-fno-builtin", "-ffreestanding", "-Wall", "-Wno-unused-function", "-Wno-unused-variable",
         *defs, *incs,
         "-Wl,--no-entry", "-Wl,--export-memory", "-Wl,-z,stack-size=1048576",
         "-o", raw, STAGE / "firmware" / "src" / app.name])

    # 3. asyncify
    out = HERE / "site" / f"{a.out or app.stem}.wasm"
    run([*wasm_opt(), raw, "--asyncify", "--pass-arg=asyncify-imports@env.host_yield", "-O2", "-o", out])
    print(f"{out.relative_to(ROOT)}: {out.stat().st_size} B  ({app.relative_to(ROOT) if app.is_relative_to(ROOT) else app})")


if __name__ == "__main__":
    main()
