# Dinghy browser emulator

A Dinghy-based FM-1 firmware running in a browser: its **own C sources**, unchanged, compiled to
WebAssembly. Screen, keys, buttons, the 7 encoders, MASTER, key and button LEDs, USB-MIDI in (Web MIDI) and
out (shown on the page), the settings sector (kept in the browser), sound through an AudioWorklet.

Run: the `dinghy-emu` entry in `.claude/launch.json`, or
`python -m http.server 8767 --bind 127.0.0.1 --directory web/dinghy-emu/site`, then http://127.0.0.1:8767.
`site/dinghy.wasm` is committed; `site/tracker.wasm` is a build output, not committed (it conflicted on every merge): build it first (`build.py --app fw/firmware/src/tracker.c`); `?fw=dinghy` or `?fw=tracker` (default) picks one.
Computer keyboard: for the tracker, its gamepad on the keybed as in the prototype (A S W D arrows, Q key 1, J SHIFT, K PLAY,
L OPTION, ; EDIT; hold two at once for combos; Tab REC, then R Z X F C G V B H N T M Y , . U play A#3–C#5 and Q is a
note off; - = OCT- OCT+); for Dinghy, notes from C4. The page's KEYS box shows the current one.

## How it works

| File | Role |
|---|---|
| `dinghy_wasm.c` | The web HAL. Dinghy's `platform.c` includes `tests/host.c` when `DINGHY_HOST` is set (its own host mode); the build puts this file there instead. It defines everything `docs/API.md` gives an app: `fm1_in`, edges, encoders, LEDs, `fm1_ms` / `fm1_ticks`, the ADC, `lcd_fill` / `lcd_blit` (and so `gfx.c`), settings, MIDI, `platform_poll`. |
| `build.py` | Stages the app's source folder + `gen/` into `build/dinghy-emu/`, compiles one translation unit with `zig cc` for `wasm32-freestanding`, then Binaryen's `wasm-opt --asyncify`. |
| `site/machine.js` | The emulated FM-1 around the wasm: `app_main()` never returns, so each `platform_poll()` calls `host_yield()`, where Asyncify unwinds to JS; `step()` rewinds into the loop for one more pass (1 ms of firmware time, as in host mode). `audio()` is the ISR: `app_audio()` on a zeroed 256-frame block, then MASTER and the clip as `platform.c` does. |
| `site/worklet.js` | Runs the machine inside the AudioWorklet: the audio clock drives the firmware clock; ~60 Hz screen / LED / MIDI updates to the page. |
| `site/emu.js`, `site/index.html` | Faceplate (geometry and skin from [sloop-web-emu](https://github.com/sabliran/sloop-web-emu), GPL-3.0, traced from the FM-1 manual), computer-keyboard and Web MIDI input. |
| `test.mjs` | Headless checks with Node (boot, screen, chord, LEDs, MIDI out / in, release, settings save and reload). |

Not emulated: the USB updater / rescue, USB audio, the crash screen, CPU time (the firmware gets as many
main-loop passes as it needs; a loop that never polls hangs the page, as it would trip the watchdog on the
device). Timing is host mode's: one main-loop pass per millisecond, not the device's real loop rate.

## Build

Toolchain, project-local (once; delete `tools/wasm-toolchain/.venv` and `node_modules` to undo):

```
cd tools/wasm-toolchain
python -m venv .venv
.venv/Scripts/python -m pip install ziglang      # zig cc: clang + lld for wasm32 (bin/python on Linux/macOS)
npm install                                       # binaryen (wasm-opt), from package.json
```

Then, from the project root:

```
python web/dinghy-emu/build.py                        # Dinghy (third_party/dinghy, pinned in third_party/VERSIONS.md)
python web/dinghy-emu/build.py --app path/to/app.c    # any firmware built on Dinghy's platform.c
node web/dinghy-emu/test.mjs                          # headless checks (for Dinghy's own app)
```
