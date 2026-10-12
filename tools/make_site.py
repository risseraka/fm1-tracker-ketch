#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments (Felucca's web/make_site.py); Ketch's changes:
# the emulator, the landing page, no editor or backup
"""Make the site (GitHub Pages) from a release build:

  index.html                  the landing page: install, try in the browser, releases
  install/index.html          web/install/index_pkg.html, self-contained (fm1pkg.js, fm1ota.js, metadata inlined)
  emu/                        web/emu/site/ and the tracker's tracker.wasm (python web/emu/build.py ...)
  guide/                      web/guide/index.html and docs/screens/ (tests/tracker_emu.mjs writes them: guide-*.png)
  firmware/ketch-VER.fwsc     the package (+ LICENSE, LICENSING.md, LICENSES/: the package holds
                              JieLi SDK files under Apache-2.0, see LICENSING.md)

  python tools/make_site.py build/ketch-X.Y.fwsc X.Y path/to/tracker.wasm site

The package must be one made by tools/build.py (Felucca's own loader, no vendor files).
Its identity (FM-1_95x) is read from the package; the device must report it after the install.
"""
import json
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WEB = ROOT / "web"
BLOCKS, BLK, KEEP = 20, 0x30, 0x2F


def strip_module(src):
    src = re.sub(r"^export\s+", "", src, flags=re.M)
    return re.sub(r"^import .*?;\n", "", src, flags=re.M)


def product_of(raw):
    """the package identity: one marker byte after each of the first 20 blocks (fm1pkg.js productOf)"""
    return "".join(chr((m - i - 1) & 0xFF) for i in range(BLOCKS) if (m := raw[i * BLK + KEEP]) != 0x7D)


LANDING = """<!doctype html>
<meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Ketch</title>
<meta name="description" content="{desc}">
<meta property="og:type" content="website"><meta property="og:url" content="{site}">
<meta property="og:title" content="Ketch, a tracker for the M-VAVE FM-1"><meta property="og:description" content="{desc}">
<meta property="og:image" content="{site}og.png"><meta property="og:image:width" content="720"><meta property="og:image:height" content="720">
<meta name="twitter:card" content="summary">
<style>
  :root {{ --bg: #fff; --fg: #000; --dim: #777; }}
  @media (prefers-color-scheme: dark) {{ :root:not([data-theme="light"]) {{ --bg: #000; --fg: #fff; --dim: #888; }} }}
  :root[data-theme="dark"] {{ --bg: #000; --fg: #fff; --dim: #888; }}
  body {{ margin: 0; background: var(--bg); color: var(--fg);
         font: 16px/1.6 -apple-system, BlinkMacSystemFont, "Helvetica Neue", Arial, sans-serif; }}
  main {{ max-width: 520px; margin: 0 auto; padding: 48px 16px; }}
  h1 {{ font-size: 24px; font-weight: 600; margin: 0 0 8px; }}
  a.big {{ display: block; border: 1px solid var(--fg); color: inherit; text-decoration: none; padding: 12px 16px; margin: 0 0 12px; }}
  .small {{ color: var(--dim); font-size: 13px; }}
</style>
<main>
  <h1>Ketch</h1>
  <p>A tracker firmware for the M-VAVE FM-1: song, chain and phrase screens, 4 channels on Felucca's sound engines.
     Version {version} (beta).</p>
  <a class="big" href="emu/"><b>Try it in the browser</b><br><span class="small">The firmware itself, in an emulator. Nothing is installed.</span></a>
  <a class="big" href="guide/"><b>Screens and controls</b><br><span class="small">Every screen, and what the keys do.</span></a>
  <a class="big" href="install/"><b>Install on your FM-1</b><br><span class="small">Over USB, in Chrome or Edge. Or go back to official V15.</span></a>
  <p class="small"><a href="{repo}/releases" style="color:inherit">Releases</a> ·
     <a href="{repo}" style="color:inherit">Source</a> (GPL-3.0) ·
     <a href="firmware/ketch-{fver}.fwsc" style="color:inherit">ketch-{fver}.fwsc</a></p>
</main>
"""
REPO = "https://github.com/risseraka/fm1-tracker-ketch"
SITE = "https://risseraka.github.io/fm1-tracker-ketch/"   # link previews need absolute URLs
DESC = ("Tracker firmware for the M-VAVE FM-1: song, chain and phrase screens, 4 channels. "
        "Try it in the browser, or install it over USB.")


def main(pkg, version, wasm, out):
    pkg, wasm, out = Path(pkg), Path(wasm), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_95\d", product):
        raise SystemExit(f"{pkg}: identity {product!r} is not a Ketch package (FM-1_95x)")
    if b"FELUCCA-LOADER-1" not in raw:              # Felucca loader marker: never publish a package with vendor files
        raise SystemExit(f"{pkg}: no Felucca loader in it; the site ships only tools/build.py packages "
                         "(a package patched from an official one carries vendor files)")
    if wasm.read_bytes()[:4] != b"\0asm":
        raise SystemExit(f"{wasm}: not a WebAssembly module")
    html = (WEB / "install" / "index_pkg.html").read_text(encoding="utf-8")
    lib = strip_module((WEB / "install" / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((WEB / "install" / "fm1ota.js").read_text(encoding="utf-8"))
    fver = re.sub(r"[^A-Za-z0-9.-]", "-", version)
    name = f"ketch-{fver}.fwsc"
    meta = json.dumps({"version": version, "product": product, "pkg": "../firmware/" + name})
    for mark in ("/*LIB*/", "/*META*/"):
        if html.count(mark) != 1:
            raise SystemExit(f"index_pkg.html must contain {mark} once; update make_site.py")
    html = html.replace("/*LIB*/", lib).replace("/*META*/", meta)
    inst, emu, fw = out / "install", out / "emu", out / "firmware"
    for d in (inst, emu, fw):
        d.mkdir(parents=True, exist_ok=True)
    for old in fw.glob("ketch-*.fwsc"):             # one package: the current one
        old.unlink()
    (inst / "index.html").write_text(html, encoding="utf-8")
    shutil.copy(pkg, fw / name)
    for f in (WEB / "emu" / "site").iterdir():
        shutil.copy(f, emu / f.name)
    shutil.copy(wasm, emu / "tracker.wasm")
    lic = ROOT / "LICENSES"                         # the package holds JieLi SDK files (Apache-2.0): their
    (fw / "LICENSES").mkdir(exist_ok=True)          # licence travels next to it, with Ketch's own
    names = sorted(f.name for f in lic.glob("*.txt"))
    for n in names:
        shutil.copy(lic / n, fw / "LICENSES" / n)
    (fw / "LICENSES" / "index.html").write_text(    # the installer links this folder: Pages lists no folders
        '<!doctype html><meta charset="utf-8"><title>Ketch licences</title><h1>Licence texts</h1><ul>'
        + "".join(f'<li><a href="{n}">{n}</a></li>' for n in names)
        + '</ul><p><a href="../LICENSING.md">LICENSING.md</a> · <a href="../LICENSE">LICENSE (GPL-3.0)</a></p>\n',
        encoding="utf-8")
    for doc in ("LICENSE", "LICENSING.md"):
        shutil.copy(ROOT / doc, fw / doc)
    shutil.copy(ROOT / "assets" / "og.png", out / "og.png")
    (out / "guide" / "screens").mkdir(parents=True, exist_ok=True)
    shutil.copy(WEB / "guide" / "index.html", out / "guide" / "index.html")
    for f in (ROOT / "docs" / "screens").glob("guide-*.png"):
        shutil.copy(f, out / "guide" / "screens" / f.name)
    (out / "index.html").write_text(LANDING.format(version=version, fver=fver, repo=REPO, site=SITE, desc=DESC),
                                    encoding="utf-8")
    print(f"site: {out}: install ({len(html)} B), emu, firmware/{name} ({len(raw)} B, {product})")


if __name__ == "__main__":
    if len(sys.argv) != 5:
        sys.exit(__doc__)
    main(*sys.argv[1:5])
