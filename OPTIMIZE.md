# Tracker firmware — CPU and memory plan

Survey of 2026-10-12 (commit `41abae74`, device build `fw/build/dinghy.elf` of the same day). One item per session.
Written so that a session with little context can execute an item without re-surveying: read §0, your item, nothing else.

## 0. How a session works an item

1. Read `CLAUDE.md` (automatic), the head of `fw/CHECKLIST.md` (marks, the build line), this §0 and **your item only**.
2. **Baseline first.** Device build, then the size listing:
   ```
   $env:JIELI_TOOLCHAIN = "<repo>\tools\jieli-toolchain\jieli-linux-toolchains-20260730.1"; $env:AC79_SDK = "<repo>\third_party\firmwares\math0ne__fm1-chord\vendor\ac79"; $env:JIELI_DOCKER = "1"; cd fw; python tools/build.py
   python -I fw/tools/elf_sizes.py fw/build/dinghy.elf 20
   ```
   (PowerShell, Docker Desktop running; `build.py` prints `image … RAM … pool …`, the listing ranks the symbols.)
   No Docker: the browser emulator still checks behaviour, but not sizes; say so in the closing note.
3. Run the item's **Verify first** lines exactly. If a verification fails, stop and report; do not improvise a variant.
4. Make the change (the smallest that fully works, CLAUDE.md "Code style"), add the test the item names, then:
   ```
   python web/dinghy-emu/build.py --app fw/firmware/src/tracker.c --name KETCH --ver 0.1
   node fw/tests/tracker_emu.mjs | tail -5
   ```
   all pass (the count is printed), then step 2 again and compare.
5. Record: one line in `fw/CHECKLIST.md` (the item, the before/after numbers, the check); a Felucca file changed →
   a line in `fw/UPSTREAM.md`; a door closed (a feature compiled out) → one line in `docs/decisions.md`.
6. Commit by explicit paths (`git commit -- <paths>`: other sessions share the index), closing note (CLAUDE.md).

**Token rules.** Never read `tracker.c` whole (6,200 lines): `grep -n` then `sed -n a,bp`, at most ~150 lines a read.
Never read `fw/build/dinghy.dis`. The size listing: top 20, once before and once after. Test output: `| tail -5`.
Felucca's `sound/` files are vendored: change them only where the item says, by the smallest diff, logged in UPSTREAM.md.

**Marks for the items:** `[ ]` open · `[?]` needs the user's yes (closes a door or changes what is heard) · `[x]` done (commit).

## 1. Baseline (2026-10-12)

| budget | used | of | free |
|---|---|---|---|
| flash `.text` | 336,576 | 573,440 (XIP) | 41 % — **not a constraint** |
| RAM `.data + .bss` | 81,232 | 98,304 | 17,072 |
| **pool** (`.pool`) | 341,108 | 344,064 | **2,956** — the constraint |
| audio CPU | not measured on the unit | half = 5.8 ms | shedding starts at 85 % (`shed_req`) |

After M1+M2 (this commit): pool 308,724 B, RAM 80,848 B, image 338,412 B.
After M5 (this commit): pool 304,628 B (−4,096), RAM 80,848 B, image 338,420 B.
After M4 + M8 (landed 2026-10-12): pool 294,004 B of 344,064 (50,060 free), RAM 80,848 B, image 338,420 B. M3 and M6 skipped (see their items).
After M4 (this commit): pool 298,100 B (−6,528), RAM 80,848 B, image 338,420 B.

Pool: `dly_buf` 131,072 (Felucca's delay, 1/4 at 40 BPM) · `phys_slot` 51,552 · `sl_buf` 32,768 (SLICER insert) ·
`cv_px` 30,720 (the canvas) · `gr_p` 28,080 (GRAIN) · `u_shadow` 24,461 + `u_log` 8,192 (undo) · `rev_comb` 9,874 ·
`drum_kit` 7,296 · `lz4_pack.ht` 4,096 · `cho_buf` 4,096 · `q_it` 2,550 + `q_at` 2,048 (Q) · `tw` / `rc` / `tl_set` 768 each.
RAM, tracker-owned: `S` 21,037 · `entry_row` 4,096 · `skip_l` + `skip_r` + `unheard` 3,072 · `held_val` + `held_ch` 1,536 ·
`clip` 512. Felucca-owned: `fm6_note` 7,104 · `trk` 7,088 · `slc_rbuf` 4,096 · `ins` 3,424 · `drw_v` 2,944 · `slc_usr` 2,928 ·
`slc_man` 1,584. Platform: `abuf` 4,096 · `ua_ring` 2,048 · `usr_zone` 1,344 (USB audio) · `ota_*` 2,688 (never cut).
Flash: `SMP_DATA` 126,244 (samples) · `app_main` 23,862 · `FONT_S_DATA` 9,120 · `fm1_alnk0_irq` 8,514 · `drum_render` 6,862.

What already exists (reuse, do not rebuild): `cpu_pm` = the last audio half's time in ‰ of its budget, shown in G's SYSTEM
popup (`tracker.c` ~4535) with the peak; voice shedding over 85 % (`shed_voice`); incremental redraw by dirty bits
(`redraw_view`); the canvas blits by DMA (`gfx.c`); `-Os -ffunction-sections --gc-sections`, one translation unit.

## 2. Items, ranked (payoff × certainty ÷ risk)

### [x] M1 — compile the SLICER insert out (pool −32,768, CPU: one call per part per block)
- **What:** Felucca's per-track gate / stutter insert (`sound/slicer.c`, `sl_buf[NTRK][SL_LEN]`, `slicer_track` in
  `fx.c mix_part`). Its parameters `P_SLCR P_SLPAT P_SLRATE P_SLDEPTH` are reachable from nothing in the tracker.
- **Verify first:** `grep -cE 'P_SLCR|P_SLPAT|SLICER|GR_SLCR' fw/firmware/src/tracker.c fw/firmware/src/commands.c`
  → `0` and `0`; in the emulator open V (voice and sends) and the FX grid on a phrase command: no `SLCR` field / command.
  `grep -n 'slicer_\|sl_buf\|sl_lent' fw/firmware/sound/*.c fw/firmware/src/*.c | grep -v '^fw/firmware/sound/slicer.c'`
  = the callers to stub (expected: `fx.c` slicer_track / slicer_busy, a `slicer_start` at play start, `perform.c`).
- **Change:** in `src/sound.c` replace `#include "slicer.c"` by static stubs with the same signatures
  (`slicer_track` empty, `slicer_busy` → 0, `slicer_start` empty); do M2 in the same commit if it is accepted
  (perform.c borrows `sl_buf`). Leave `params.c` alone (the parameter entries cost nothing).
- **Test:** `tracker_emu.mjs`: a check that `sound.c` no longer includes `slicer.c` (a source grep in the test, as the
  "OCT- at power-on" check does) and the existing "commands while playing" section still passes (the sound path untouched).
- **Risk:** closes the door on a SLICER command (a tracker could want a gate / stutter effect later: then it comes
  back as a real feature with the pool it needs). **User's yes needed.**

### [x] M2 — compile the perform layer out (flash −≈4 KB, `.data` −220, one call per block)
- **What:** `sound/perform.c` is Felucca's keyboard FX-hold layer (`perf_begin / perf_pre / perf_master / perf_mute`,
  `pf`). Nothing in the tracker sets `pf`: `grep -nE 'perf_|pf\.' fw/firmware/src/*.c` → nothing.
- **Verify first:** that grep; the callers: `fx.c` 363, 386-396, 411-418 only.
- **Change:** in `src/sound.c` skip `#include "perform.c"`; define `static struct { uint32_t mute; } pf;` and
  `static int perf_begin(uint32_t n) { (void)n; return 0; }` plus empty `perf_pre`, `perf_block`, `perf_mute`.
  With `perf_begin` a constant, `-Os` drops `perf_master` and friends; confirm in the size listing.
- **Test:** emu "commands while playing" and the mixer section still pass; the size listing shows no `perf_*`.
- **Risk:** a later "hold a key for an effect" feature. **User's yes needed** (same turn as M1 is fine: one decision,
  "Felucca's live layers are off").

### [!] M3 — `song_shape()` runs every main-loop iteration (main-core CPU, battery) — SKIPPED: `dirty` is a per-iteration local read before the edits, the ISR (921) and memcpy paths (undo, load, song_new, clones) also write; a flag needs ~8 writer sites
- **What:** `tracker.c` 5866: the loop spins (`platform_poll` never waits) and hashes 1,280 bytes each pass
  (`song_shape`, 1646) to notice a shape change for `entry_row`. The 9 `morph()` calls just below are cheap, leave them.
- **Verify first:** `grep -n 'song_shape\|entry_row' fw/firmware/src/tracker.c` — who writes `S.song[..]` and
  `S.chains[..].ph`: `grep -nE 'S\.(song|chains)\[[^]]*\](\[[^]]*\])?(\.ph)? ?= ' fw/firmware/src/tracker.c` and the
  ISR's live-record path (a new phrase on an empty chain row, "the main loop keeps a free one ready for the ISR").
- **Change:** compute the hash only when something could have changed: `if (dirty) { … }` is enough **if** every
  writer sets `dirty` before the next iteration (edits, undo, paste, load do; check the ISR writer: it must raise a
  `volatile` flag the loop folds into `dirty`). Otherwise add `static volatile uint8_t shape_dirty` set by every writer.
- **Test:** emu: a HOP in a chain still enters at the right row after an edit of the chain while playing (there is a
  HOP section: extend it with "edit the chain row above a HOP while playing, the entry row follows"); make the test's
  edit go through the ISR path too (live record onto an empty chain row), so a missed writer fails it.
- **Risk:** a stale `entry_row` after a shape change the guard misses = HOP enters at an old row. Low payoff on the
  audio budget (main core only); do it for the battery and because it is the loop's only O(song) work.

### [x] M4 — GRAIN's seek index is sized for a retired feature (pool −≈6 KB)
- **What:** `sound/eng_grain.c` 44-45: `GR_NIDX 1024` "sized for the retired PERC: 1001; it keeps user slots room",
  `GR_MAXZ 32` "a user slot <= 16; the retired PERC had 24". Per part: `ipred[1024]` int16 + `iidx[1024]` = 3,072 B,
  `zbase/zcnt/zdone[32]` = 192 B; ×4 parts.
- **Verify first:** the largest source the index must cover: `grep -nE 'GR_NIDX|GR_SEG|SMP_USER|user_len|smp_user' fw/firmware/sound/eng_grain.c fw/firmware/sound/core.h fw/firmware/gen/felucca_samples.h | head -20`;
  entries needed = max source samples / `GR_SEG` (256) + 1 over every reachable source (the sample bank's zones and a
  user slot's maximum). If that is ≤ 512: `GR_NIDX 512`; `GR_MAXZ 16` if no reachable set has more than 16 zones.
- **Change:** the two defines (Felucca file: UPSTREAM.md line). Nothing else.
- **Test:** emu: a GRAIN instrument on each sample source plays (extend the engine section: every SRC value); read how
  the engine guards the index (`grep -n 'GR_NIDX' eng_grain.c`) and assert that guard is not hit for any source.
- **Risk:** a source longer than the index → the engine clamps (check what it does: silence or a repeat), heard as a
  truncated grain source. Felucca's own user slots may be longer than ours: use **our** `SMP_USER_SLOTS` length.

### [x] M5 — share the LZ4 hash table with Q's tables (pool −4,096)
- **What:** `tracker.c` 4286: `lz4_pack.ht[2048]` uint16 lives only during a save; `q_it` (2,550) + `q_at` (2,048) are
  Q's view tables, rebuilt by `q_build()` whenever Q is entered (callers: 3343, 5062, 5074, 5152, 5385).
- **Verify first:** a save never runs while Q is shown and Q is always rebuilt on entry: `grep -n 'q_build()' fw/firmware/src/tracker.c`
  and the save path (`lz4_pack(u_shadow, U_SIZE)` at 4469, called from G's save) — "STOP TO SAVE while playing". Q is
  not reachable from G without passing `q_build()`: read the view switch (`grep -n 'ui.view = V_Q' tracker.c`).
- **Change:** one pool union: `static union { uint16_t ht[2048]; struct { qitem_t it[Q_MAX]; uint8_t at[NPHR * 16u]; } q; } qh __attribute__((section(".pool")));`
  and the two names become `qh.ht`, `qh.q.it`, `qh.q.at` (replace_all; no other change).
- **Test:** emu: save from G, then open Q: the lines are right (the existing Q section run after the save section);
  save, load, Q again. With the union reverted the test still passes (it is a memory-aliasing change): say so.
- **Risk:** low; a future "save from Q" would corrupt Q's tables until the next `q_build()` (a comment at the union).

### [!] M6 — `entry_row[4][64][16]` (RAM −2,048 or −3,840) — SKIPPED: the cache holds the entry actually played per chain row; `walk_entry` walks the whole channel, so option (b) does not hold
- **What:** `tracker.c` 339: one byte per channel × song row × chain row (HOP's entry row). RAM is the second
  constraint (17 KB free); do this after the pool items.
- **Verify first:** `grep -n 'entry_row' tracker.c` — every read / write; the value range (0..15 and EMPTY).
- **Change:** nibbles with 0xF as EMPTY are impossible (16 rows + EMPTY = 17 values). Options, pick the one the reads
  allow: (a) one bit "entered" + 4 bits of row in a 5-bit packed array — fiddly; (b) **per channel × song row only** if
  `walk_entry` (1655) recomputes the chain-row entries from the song-row entry cheaply: then
  `uint8_t entry_row[NCH][NSONG]` (−3,840). Read `walk_entry` before choosing; if (b) does not hold, skip the item and
  write why in CHECKLIST.
- **Test:** the HOP sections of the emu test unchanged and passing.

### [?] deferred (user, 2026-10-12: after M9's numbers) M7 — PHYS polyphony 3 → 2 per part (pool −17,184)
- **What:** `eng_phys.c` 45 `PHYS_POLY 3`; `phys_slot[NPART][PHYS_POLY]` = 4,296 B a slot (the waveguide line).
  A tracker channel plays one note at a time; overlap comes from release tails.
- **Verify first:** how a 3rd note on a part is handled with 2 slots (voice stealing in `eng_phys.c` / `voice.c`:
  `grep -n 'PHYS_POLY' eng_phys.c`): a steal, not a crash.
- **Change:** the define (UPSTREAM.md line). **User's yes needed** and a listen: a PHYS instrument with long release on
  fast 16ths — the second-oldest tail is cut.
- **Risk:** audible; the emulator cannot judge it. Rank below M1 / M4 / M5, which cost nothing audible.

### [x] M8 — undo log 8 → 4 KB (pool −4,096)
- **What:** `tracker.c` 3405 `U_LOG 8192`: the byte log of the last 4 undo steps (`U_STEPS`). A step larger than the
  log cannot be undone (check what `undo_step` does when the log is full: `grep -n 'U_LOG' tracker.c`).
- **Verify first:** the largest step today: a paste of the clip (64 × 4 cells) or a deep clone of a song block (phrases
  too, 9 B × 16 rows each) — compute its log size from `undo_step`'s record format.
- **Change:** the define, only if 4 steps of the largest paste still fit; otherwise reject and note it.
- **Risk:** "UNDO" silently unavailable after a big clone. **User's yes needed** (the 4-step undo is a decided feature).

### [~] M9 — audio CPU: measure before any engine work (device, user needed) — preparation done, the measurement is the user's (CHECKLIST)
- **What:** nothing is known about the unit's load; the emulator "renders in no time" (`emu_overload` exists only to
  test shedding). `cpu_pm` and its peak are on G's SYSTEM popup.
- **Protocol (one session prepares, the user reads the popup):** 4 test projects saved in the slots, each 4 channels
  of 16th notes with DEL 02 + RET so release tails stack: (1) FM6 on all four, (2) PHYS ×2 + GRAIN ×2, (3) DRUM + TRIO
  + SLICE + WHEEL, (4) the user's own song. For each: open G → SYSTEM while it plays, note `xx% pk yy%`, with F's
  reverb / delay / chorus at their defaults and then at their maxima. Record the table in CHECKLIST.
- **Then:** only an engine whose peak passes 85 % (shedding audible) gets work, and that work is in Felucca's
  file for that engine, measured by the same protocol. Candidate order by code size / per-sample work:
  `fm6_render` (6-op, `fm6_core.c`), `grain_render` + `grain_block`, `phys_render` (`phys_dsp.c`), `drum_render`.
  Do **not** touch `fx.c`'s mixer loops or the reverb (fixed cost, shared) before the engines are measured.
- **Tracker-side ISR work is cheap** (`events_block`: per block 4 × `scene_apply` + `ch_level` + `player_retrig`;
  `player_tick` per tick): leave it unless the measurement says the load is high with the simplest engine.

### [ ] M10 — idle the main loop (battery; needs a hardware fact)
- **What:** the main loop spins between redraws (`platform_poll` returns at once; the key scan runs in the timer IRQ).
  A wait-for-interrupt when nothing is dirty and no key / knob changed would cut the core's idle power.
- **Verify first:** HANDOFF §2 / `docs/research/` for the pi32v2 idle instruction or the SDK's low-power call; the
  AC79 SDK (`third_party/firmwares/math0ne__fm1-chord/vendor/ac79`) for `wfi` / `idle` / `power_` — one grep, note the
  finding in `docs/research-todo.md`. Without a verified instruction: no change.
- **Risk:** the audio ISR and the 10 kHz timer must wake it (they do if it is a true WFI); a wrong opcode faults
  (the device emulator `tools/emu-device/run.sh` shows a fault). Device-only verification; low priority.

## 3. Rejected (do not retry; the reason is the test)

- Deferred (user, 2026-10-12): **`dly_buf` 128 KB → smaller:** `fx.c` 5 "1/4 at 40 BPM fits"; TEMPO's floor is 40 (`PR` table, 4505). Only if the
  user raises the floor to 80 BPM: `DLY_LEN 32768` (the index is masked: a power of two) = pool −65,536. A user decision,
  listed here so the trade is visible, not an item.
- **`cv_px` (the canvas) smaller:** the pages draw in bands the canvas holds; a smaller canvas = more DMA blits per
  redraw (slower, visible tearing on the instrument pages). No.
- **`u_shadow`:** = `sizeof S + sizeof ins` by design (the save packs from it; `docs/flash-map.md`). No.
- **`S` / `row_t` packing:** a saved-format change touching every editor path, the undo log and the Q tables for
  ~3 KB. No.
- **`-flto`, `-O3`, float:** JieLi clang 4.0.1; one translation unit already gives global inlining; `-Os` is the
  right flag for flash; Dinghy's build forbids float. No.
- **Splitting `tracker.c` / `app_main` (24 KB):** a readability smell, zero bytes and zero cycles; a large diff for
  no measured gain, against "shortest diff wins". No, until a feature needs it.
- **USB audio buffers (`abuf`, `ua_ring`, `usr_zone`), OTA staging, `settings_load.buf`:** platform and safety;
  CLAUDE.md "Never cut".
- **`SMP_DATA` (126 KB of flash):** flash is 41 % free; dropping sample sets saves nothing that is short.

## 4. Order for the next sessions

M1 + M2 (one user decision, −32 KB pool, the largest cheap win) → M5 (−4 KB, no decision) → M4 (−6 KB, verify the
index) → M3 (loop) → M9's preparation (the test projects; the user reads the numbers) → M6 → M7 / M8 (decisions) → M10.
After M1 + M4 + M5 the pool has ≈ 45 KB free: enough for the open UX items without further squeezing.
