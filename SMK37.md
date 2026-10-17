# Ketch on the M-Vave SMK-37

The SMK-37 has the same chip as the FM-1 (AC791N), so Ketch runs on it as a second build target. Everything for it
sits behind `BOARD_SMK37`; the FM-1 build is unchanged.

> **Status: beta, like Ketch itself.** Tested on the host (see Tests below); it has not been tested on many units.
> Take a full flash backup of your keyboard before the first install.

## Models

The SMK-37 Pro, the SMK-37 Elite and the MKE-P37 are the same keyboard under three model names. Each has its own
package, because the stock updater only takes a unit's own model back:

| Model | Package | Identity |
| --- | --- | --- |
| SMK-37 Pro (also Starrykey 37 Play) | `ketch-smk37.fwsc` | `SMK-37 Pro_95Y` |
| SMK-37 Elite | `ketch-smk37-elite.fwsc` | `SMK-37 Elite_95Y` |
| MKE-P37 | `ketch-smk37-mke.fwsc` | `MKE-P37_95Y` |

A package installs only on exactly its model; FM-1 and SMK-37 packages are never crossed (the installers check the
model, and the firmware refuses a package whose flash layout is not the SMK-37's before it erases anything).

## Controls

The screen is the FM-1's. Where it names an FM-1 button, the SMK-37 button that means the same is listed here.

| SMK-37 | Ketch |
| --- | --- |
| **Pads** (top row 1-8, bottom row 9-16) | the gamepad, laid out as on the M8: pad 2 UP; pads 9 10 11 LEFT DOWN RIGHT; pads 7 8 OPTION EDIT; pads 15 16 SHIFT PLAY. Pad 13: OFF (a note off) in the notes modes. The other pads do nothing |
| **Keys** (37, with velocity) | in the notes modes (REC): all 37 keys play the cursor's channel, C3-C6, and write the note with its velocity (step entry stopped, live recording armed). OCT- / OCT+ move them. With SCALE held, B3-B4 pick the scenes, as the white keys on the FM-1 |
| K1-K4 | KNOB 1-4 (the screen says K1-K4) |
| K5 | MASTER (volume) |
| K6 | SELECT: the crossfader, with SCALE (SEL) held |
| K7, K8 | nothing (ALGORITHM and PRESETS on the FM-1) |
| Faders 1-4 | the mixer's levels of channels 1-4 (X LEVEL); a fader takes over once it moves |
| Pitch wheel | bend, +-2 semitones, on the cursor's channel |
| Mod wheel | the mod wheel (the instrument's MODW source), on the cursor's channel |
| SEQ PLAY / SEQ REC | PLAY / REC |
| SEQ, FX, ARP | SEQ, FX, ARP |
| CHORD, NOTE REPEAT | ENV, LFO |
| SCALE | SEL |
| PARA | EDIT (PARA + OCT- / OCT+: undo / redo) |
| GLOBE | GLO |
| PATCH | HOME |
| BT | SAVE |
| OCT- / OCT+ | OCT- / OCT+ |
| KNOB BANK, FADER BANK, `<`, `>` | nothing |

Pads light in three colours: off dark orange, on light blue, active bright. The gamepad pads are on, and active while
held; PLAY is off while stopped and active while the song plays; OFF is on in the notes modes and off outside them.
The other pads stay dark. The button LEDs light for a state: GLOBE on the G page,
PATCH while following, SEQ REC in the notes modes (blinking: armed, recording), SEQ PLAY while playing, BT with unsaved
changes, OCT- / OCT+ off octave 4, SCALE while held.

There is no TRS MIDI IN on the SMK-37.

## Projects

The four project slots are kept as on the FM-1, in the keyboard's own data areas: slots 1 and 2 in the stock VM,
slots 3 and 4 in USRFLASH. The factory calibration, Bluetooth pairing and the stock resources are never written. The
first start clears data another firmware left in the VM (the stock settings). The stock key mapping in USRFLASH is
kept until a project is first saved in slot 3 or 4; just before that write its end marker (the sector 0xEA000) is
erased, so the stock firmware rebuilds its factory mapping when you go back to it. A start that finds the marker over
anything but the intact stock mapping erases it too.

## Build

As for the FM-1 (README.md, Build), with the board and the model:

```sh
BOARD=smk37 ./build.sh --release 0.1                  # build/ketch-smk37-0.1.fwsc, SMK-37 Pro_951
BOARD=smk37 MODEL=elite ./build.sh --release 0.1      # build/ketch-smk37-elite-0.1.fwsc
BOARD=smk37 MODEL=mke ./build.sh --release 0.1        # build/ketch-smk37-mke-0.1.fwsc
```

Without `--release`: `build/ketch-smk37.fwsc` (`SMK-37 Pro_950`). Build the FM-1 last if you run the tests: both
targets write the same intermediate files.

## Install

1. Keep the keyboard on USB power.
2. `python3 tools/fm1_install.py ketch-smk37-0.1.fwsc` (the package for your model; once:
   `pip3 install mido python-rtmidi`). It asks before it writes.
3. Do not touch the keyboard or the cable until it restarts.

If an install is cut off, hold OCTAVE- while switching on: the keyboard starts in update mode and the installer
finishes the write.

## Back to the stock firmware

With M-Vave's own updater and your model's factory package, or `python3 tools/fm1_install.py SMK-37_Pro_016.fwsc`
(the unmodified SMK-37 Pro V016 file).

## Tests

`sh tests/run_smk_tests.sh` builds the three models and the FM-1 and runs: the flash map (every sector against the
allow-lists), the packages, the update entry of each model (an FM-1 package refused before anything is erased, the
region table and USR checks), the mapping on the host (`tests/smk_ui_test.py`), the installers (both), and the boot
stub and loader tests for both boards.
