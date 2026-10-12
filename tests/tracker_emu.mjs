// SPDX-License-Identifier: GPL-3.0-only
// Headless checks of the tracker in the browser emulator's machine (no browser):
//   python web/dinghy-emu/build.py --app fw/firmware/src/tracker.c --name KETCH --ver 0.1
//   node fw/tests/tracker_emu.mjs          (-> build/tracker/*.png, the screen at 3x)
import { readFileSync, writeFileSync, mkdirSync, readdirSync, statSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { createRequire } from 'node:module';
import { Machine } from '../../web/dinghy-emu/site/machine.js';

const HERE = dirname(fileURLToPath(import.meta.url)), ROOT = join(HERE, '..', '..');
const OUT = join(ROOT, 'build', 'tracker');
mkdirSync(OUT, { recursive: true });
// the build must be newer than what it is built from, else the checks pass against old code
const WASM = join(ROOT, 'web', 'dinghy-emu', 'site', 'tracker.wasm');
const stale = [join(ROOT, 'fw', 'firmware', 'src'), join(ROOT, 'fw', 'firmware', 'sound')]
    .flatMap(d => readdirSync(d).map(f => join(d, f))).concat(join(ROOT, 'web', 'dinghy-emu', 'dinghy_wasm.c'))
    .filter(f => statSync(f).mtimeMs > statSync(WASM).mtimeMs);
if (stale.length) {
    console.log('rebuild first: tracker.wasm is older than', stale.map(f => f.slice(ROOT.length + 1)).join(', '),
        '\n  python web/dinghy-emu/build.py --app fw/firmware/src/tracker.c --name KETCH --ver 0.1');
    process.exit(2);
}
const PNG = createRequire(import.meta.url)(join(ROOT, 'tools', 'png.js'));
const MOD = new WebAssembly.Module(readFileSync(WASM));   // compiled once; each Machine makes its own instance
const m = new Machine(MOD, null);
let fails = 0, checks = 0;
const ok = (cond, what) => { checks++; if (!cond) { fails++; console.log('FAIL', what); } else if (process.env.DEBUG) console.log('ok  ', what); };
{   // the platform starts the audio interrupt before app_main: nothing is rendered until sound_init (Felucca's slicer
    // divides by the song's BPM, 0 before it)
    const m0 = new Machine(MOD, null);
    let trap = '';
    try { m0.audio(); } catch (e) { trap = e.message; }
    ok(!trap, `the audio interrupt before app_main renders nothing (${trap || 'no trap'})`);
}

// the gamepad (control-map.js PAD): role -> key index on the keybed, 0 = F3
createRequire(import.meta.url)(join(ROOT, 'ux-proto', 'control-map.js'));
const PAD = Object.fromEntries(Object.entries(globalThis.CONTROL_MAP.pad).map(([i, role]) => [role, +i]));

let frames = 0, held = 0;
function run(ms) {
    let peak = 0;
    const end = m.ms() + ms;
    while (m.ms() < end) {
        const a = m.audio();
        for (let i = 0; i < a.length; i++) peak = Math.max(peak, Math.abs(a[i]));
        frames += m.frames;
        m.runTo(Math.floor(frames * 1000 / 44100), 1000);
    }
    return peak;
}
const down = (...ks) => { for (const k of ks) held |= 1 << PAD[k]; m.input(held, 0); return run(30); };
const up = (...ks) => { for (const k of ks) held &= ~(1 << PAD[k]); m.input(held, 0); return run(30); };
const tap = (...ks) => { const p = down(...ks); up(...ks); return p; };

const rgb565 = h => { const v = parseInt(h.slice(1), 16); return ((v >> 16 & 255) >> 3) << 11 | ((v >> 8 & 255) >> 2) << 5 | (v & 255) >> 3; };
const ACCENT = rgb565('#ff9a2e'), BAND = rgb565('#2a2010');
const px = (x, y) => m.fb()[y * 240 + x];
const cellX = cx => 4 + cx * 6, rowY = r => 32 + r * 12, IROW_Y = r => 32 + r * 38, Q_Y = l => 85 + 12 * l;   // a phrase row;
const STRIP_Y = 226, KNOBS_Y = 190;           // an instrument page's row (I: 0..4); Q's list line; the bottom line; KNOBS
const PH_X = [2, 6, 9, 11, 15, 18, 22, 25, 29];
const cursorAt = (r, k) => px(cellX(PH_X[k]) - 1, rowY(r)) === ACCENT;
// the 2D map (tracker.c cv_map): letter -> [column, line]; G F X take the column of their anchor a (S C P = 0 1 2)
const MAP_CELL = { S: [0, 1], C: [1, 1], P: [2, 1], I: [3, 1], Q: [3, 0], E: [3, 2], L: [3, 3], V: [3, 4], G: [-1, 0], F: [-1, 2], X: [-1, 3] };
const mapLit = (p, L, a = 0) => {                // the letter drawn in the accent colour = the current screen
    const [c, l] = MAP_CELL[L], x = 212 + 7 * (c < 0 ? a : c), y = rowY(l);
    for (let j = y; j < y + 12; j++) for (let i = x; i < x + 6; i++) if (p(i, j) === ACCENT) return true;
    return false;
};
const inkIn = (x, y, w, h) => { let n = 0; for (let j = y; j < y + h; j++) for (let i = x; i < x + w; i++) if (px(i, j)) n++; return n; };

function png(name, fb = m.fb()) {               // a screen (RGB565) at 3x (tools/png.js)
    const rgba = new Uint8Array(240 * 240 * 4);
    for (let i = 0; i < 240 * 240; i++) {
        const c = fb[i];
        rgba[4 * i] = (c >> 11) * 255 / 31; rgba[4 * i + 1] = (c >> 5 & 63) * 255 / 63; rgba[4 * i + 2] = (c & 31) * 255 / 31;
    }
    writeFileSync(join(OUT, name + '.png'), PNG.png3(rgba));
}

run(50);
down('shift'); tap('right'); tap('right'); up('shift');   // boot on the song screen: S -> C -> P (chain 00, phrase 00 created)
ok(cursorAt(0, 0), 'cursor on row 0, note column');
ok(px(cellX(0) + 40, rowY(0) + 1) === BAND, 'cursor row band');
ok(inkIn(0, 0, 120, 19) > 50, 'breadcrumb drawn');
png('1-boot');

ok(tap('edit') > 0.05, 'EDIT on an empty note writes the last note (C-3) and plays it');
ok(inkIn(cellX(9), rowY(0), 6, 12) > 0, 'the row gets the last instrument (0)');
tap('down');
ok(cursorAt(1, 0) && !cursorAt(0, 0), 'down moves the cursor');
down('edit');
up('edit');
down('edit'); tap('up'); up('edit');            // EDIT + up: an octave up (C-4)
tap('down'); tap('down');
down('option'); tap('edit'); up('option');       // OPTION + EDIT on an empty note: OFF
tap('right');
ok(cursorAt(3, 1), 'right moves to the volume column');
tap('up'); tap('up'); tap('up');
tap('right'); tap('right'); tap('right');
ok(cursorAt(0, 4), 'cursor in FX1 value column');
tap('right'); tap('right'); tap('right'); tap('right'); tap('right');
ok(cursorAt(0, 8), 'stops at the last column');
for (let i = 0; i < 20; i++) tap('down');
ok(cursorAt(15, 8), 'stops at row F');
for (let i = 0; i < 8; i++) tap('left');
for (let i = 0; i < 15; i++) tap('up');
ok(cursorAt(0, 0), 'back home');
{   // a held arrow repeats (unit-feedback.md #1): 350 ms, then one every 60 ms; not with SHIFT (one screen change)
    down('down'); run(600); up('down');
    const r = [...Array(16).keys()].find(i => cursorAt(i, 0));
    ok(r >= 4 && r <= 7, `a held down repeats (row ${r})`);
    down('up'); run(1200); up('up');
    ok(cursorAt(0, 0), 'a held up stops on row 0, not on the column labels (unit-feedback.md #50)');
    tap('right'); tap('right'); down('left'); run(600); up('left');
    ok(cursorAt(0, 0), 'a held left (from the third column) stops on the first column, not on the row number');
    tap('left');
    ok(!cursorAt(0, 0), 'a tap of left goes onto the row number');
    tap('right');
    down('shift'); down('left'); run(600); up('left'); up('shift');
    ok(mapLit(px, 'C'), 'a held SHIFT + left changes screen once (P -> C)');
    down('shift'); tap('right'); up('shift');
    ok(cursorAt(0, 0), 'back on the phrase, home');
}
png('2-edited');

// ---- playback (step 2): PLAY loops the phrase, 6 ticks a row; at 120 BPM a row is 5512.5 samples = 125 ms
const PLAYC = rgb565('#ff4f6d');
const playheadAt = () => { for (let r = 0; r < 16; r++) if (px(cellX(0) - 1, rowY(r)) === PLAYC) return r; return -1; };
down('play');
const seen = [], at = [];
let peak = 0;
const t0 = m.ms();
for (let t = 0; t < 2300; t += 5) {   // (PLAY was pressed during down(): row 0 is seen late, so time row 1 to row 1)
    peak = Math.max(peak, run(5));
    const r = playheadAt();
    if (r >= 0 && r !== seen[seen.length - 1]) { seen.push(r); at.push(m.ms() - t0); }
}
up('play');
ok(seen.slice(0, 17).join() === '0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0', `playhead walks the 16 rows and loops (${seen.join(',')})`);
const loop = at[17] - at[1];
if (process.env.DEBUG) console.log(at.map((v, i) => i ? v - at[i - 1] : v).join(' '));
ok(Math.abs(loop - 2000) <= 6, `one loop of 16 rows = 2000 ms at 120 BPM (${loop} ms on screen, +-1 audio block)`);
ok(peak > 0.05, 'the phrase sounds');
ok(m.leds()[0].some(v => v), 'PLAY LED lit');
tap('play');                                     // stop: the notes are released; delay and reverb ring out
const tail = [run(250), run(1000), run(3000), run(3000)];
ok(tail[3] < 0.002 && tail[3] < tail[0] / 10, `stop: the sound dies away (peaks ${tail.map(v => v.toFixed(3)).join(' ')})`);
ok(playheadAt() === -1, 'stop: no playhead');
m.input(0, 1 << 12); run(30); m.input(0, 0); run(200);   // the panel's PLAY button
ok(playheadAt() >= 0, 'the panel PLAY button plays too');
png('3-playing');
m.input(0, 1 << 12); run(30); m.input(0, 0); run(50);

// ---- reading the screen: the firmware's own font (fw/firmware/gen/font_6x12.h), a cell at a time
const GLYPHS = (() => {
    const src = readFileSync(join(ROOT, 'fw', 'firmware', 'gen', 'font_6x12.h'), 'utf8');
    const rows = [...src.matchAll(/\{((?:0x[0-9A-F]{2}, ){11}0x[0-9A-F]{2})\}/g)].map(m => m[1].split(', ').map(h => parseInt(h, 16)));
    return rows.map((g, i) => [String.fromCharCode(32 + i), g.join()]);
})();
// the top bar's right end: the battery mark takes the last 14 px (docs/ux/system-popup.md); the tempo ends here, a tag
// ends a space before the tempo's 3-digit column (right-aligned)
const BAR_R = 240 - 4 - 14, TAG_END = BAR_R - 4 * 6;
function readText(fb, x0, y0, n, rows = 12) {   // n characters from pixel (x0, y0); '?' = no glyph matches; rows: the
                                                 // glyph rows compared (11: a frame line in the cell's blank bottom row)
    let out = '';
    for (let c = 0; c < n; c++) {
        const count = new Map();
        for (let y = 0; y < 12; y++) for (let x = 0; x < 6; x++) { const v = fb[(y0 + y) * 240 + x0 + c * 6 + x]; count.set(v, (count.get(v) || 0) + 1); }
        const bg = [...count.entries()].sort((a, b) => b[1] - a[1])[0][0];
        const bits = [];
        for (let y = 0; y < 12; y++) { let b = 0; for (let x = 0; x < 6; x++) if (fb[(y0 + y) * 240 + x0 + c * 6 + x] !== bg) b |= 1 << (5 - x); bits.push(b); }
        const g = GLYPHS.find(([, k]) => k.split(',').slice(0, rows).join() === bits.slice(0, rows).join());
        out += g ? g[0] : '?';
    }
    return out;
}
const cellAt = (fb, cx, line, n) => readText(fb, 4 + cx * 6, rowY(line), n);

// ---- a fresh machine (rig): the gamepad, the panel, the screen read back; booted, then on to the phrase screen
function rig(store, settings, song) {         // store, settings: kept from an earlier machine (a reboot); song: stay on S
    const mm = new Machine(MOD, settings || null, store);
    let fr = 0, hk = 0, bt = 0, rec = null;
    const T = {
        m: mm,
        go(ms) {
            const end = mm.ms() + ms;
            while (mm.ms() < end) { const a = mm.audio(); if (rec) for (let i = 0; i < a.length; i += 2) rec.push(a[i]); fr += mm.frames; mm.runTo(Math.floor(fr * 1000 / 44100), 1000); }
        },
        hold(k) { hk |= 1 << PAD[k]; mm.input(hk, bt); T.go(25); },
        free(k) { hk &= ~(1 << PAD[k]); mm.input(hk, bt); T.go(25); },
        button(b, on) { bt = on ? bt | 1 << b : bt & ~(1 << b); mm.input(hk, bt); T.go(25); },   // a panel button held / let go
        key(k, ms = 25) { mm.input(hk | 1 << k, bt); T.go(ms); mm.input(hk, bt); T.go(25); },   // keybed key k (0 = F3) tapped
        btn(b) { mm.input(hk, bt | 1 << b); T.go(25); mm.input(hk, bt); T.go(25); },
        xf(n) { T.button(3, true); mm.enc(0, n); T.go(30); T.button(3, false); },   // (3: BTN_SEL)   // SEL + SELECT: the crossfader (decisions.md
                                                 // "Scenes over commands": only with SEL held)   // a panel button tapped (BTN_*)
        strip: (n = 38) => readText(mm.fb(), 4, STRIP_Y, n).trimEnd(),   // the bottom line
        until(row, max = 6000) {             // playing: until the playhead is on row (a check that fails if it never is)
            for (let t = 0; t < max; t += 5) { if (T.playhead() === row) return true; T.go(5); }
            ok(false, `the playhead reaches row ${row}`);
            return false;
        },
        panel(b, n = 1) {                    // panel EDIT held + panel button b (platform.c BTN_*), n times
            mm.input(hk, 1 << 6); T.go(25);
            for (let i = 0; i < n; i++) { mm.input(hk, 1 << 6 | 1 << b); T.go(25); mm.input(hk, 1 << 6); T.go(25); }
            mm.input(hk, 0); T.go(25);
        },
        press(k, n = 1) { for (let i = 0; i < n; i++) { T.hold(k); T.free(k); } },
        with_(mod, k, n = 1) { T.hold(mod); T.press(k, n); T.free(mod); },
        cell: (cx, line, n) => cellAt(mm.fb(), cx, line, n),
        to(row, col) {                           // the phrase cursor, where the screen shows it (its cell's accent), to (row, col)
            let r = 0, c = 0;
            while (r < 16 && !(c = PH_X.findIndex(x => mm.fb()[rowY(r) * 240 + cellX(x) - 1] === ACCENT) + 1)) r++;
            if (r === 16) throw new Error(`to(${row}, ${col}): no phrase cursor on the screen`);
            for (c--; r < row; r++) T.press('down');
            for (; r > row; r--) T.press('up');
            for (; c < col; c++) T.press('right');
            for (; c > col; c--) T.press('left');
        },
        note(row) { T.to(row, 0); T.press('edit'); },
        fx(row, k, name, value) {            // FX column k of row: command `name`, value `value`
            const cx = [11, 18, 25][k], vx = [15, 22, 29][k];
            T.to(row, 3 + 2 * k);
            if (T.cell(cx, row, 3) === '---') T.press('edit');
            // the FX grid: to its first line, then each line left to right until the name shows
            // (the grid covers the rows: the bottom line names the highlighted command)
            const shown = () => readText(mm.fb(), 4, STRIP_Y, 3);
            T.hold('edit');
            T.press('up', 12); T.press('left', 7);
            for (let line = 0; line < 12 && shown() !== name; line++) {
                for (let x = 0; x < 7 && shown() !== name; x++) T.press('right');
                if (shown() !== name) { T.press('down'); T.press('left', 7); }
            }
            T.free('edit');
            T.to(row, 4 + 2 * k);
            for (let guard = 0; guard < 600; guard++) {
                const v = parseInt(T.cell(vx, row, 2), 16);
                if (v === value) break;
                const d = value - v;
                if (Math.abs(d) >= 16 && value <= 255 && v + Math.sign(d) * 16 >= 0 && v + Math.sign(d) * 16 <= 255) T.with_('edit', d > 0 ? 'up' : 'down');
                else T.with_('edit', d > 0 ? 'right' : 'left');
            }
            if (parseInt(T.cell(vx, row, 2), 16) !== value) ok(false, `fx(): ${name} ${value.toString(16)} set on row ${row}`);
            T.go(1000);                          // (EDIT on an FX cell auditions the row, #26: its release dies away)
        },
        record(ms) { rec = []; T.go(ms); const r = rec; rec = null; return r; },
        playRecord(ms) { rec = []; T.press('play'); T.go(ms); const r = rec; rec = null; return r; },   // from before PLAY
        store: () => mm.u8(mm.e.emu_store_ptr(), mm.e.emu_store_len()).slice(),   // the flash's store, settings: for a reboot
        settings: () => mm.u8(mm.e.emu_settings_ptr(), 260).slice(),
        shot: name => png(name, mm.fb()),
        playhead() { for (let r = 0; r < 16; r++) if (mm.fb()[(32 + r * 12) * 240 + cellX(0) - 1] === PLAYC) return r; return -1; },
    };
    T.go(50);
    if (!song) T.with_('shift', 'right', 2);   // S -> C -> P
    return T;
}
// a project as stored (docs/flash-map.md): payload byte i of a copy -> its offset (3,584 B after the header, then 3,840 a
// sector), and an independent LZ4 block decoder (lz4.org), to check what was stored
const pjAt = i => i < 3584 ? 256 + i : (1 + Math.floor((i - 3584) / 3840)) * 4096 + (i - 3584) % 3840;
const lz4 = s => {
    const d = []; let ip = 0;
    while (ip < s.length) {
        const t = s[ip++]; let lit = t >> 4, len = t & 15, b;
        if (lit === 15) do { b = s[ip++]; lit += b; } while (b === 255);
        for (let i = 0; i < lit; i++) d.push(s[ip++]);
        if (ip >= s.length) break;
        const off = s[ip] | s[ip + 1] << 8; ip += 2;
        if (len === 15) do { b = s[ip++]; len += b; } while (b === 255);
        for (let i = 0; i < len + 4; i++) d.push(d[d.length - off]);
    }
    return d;
};
const rms = (a, from = 0, to = a.length) => { let s = 0; for (let i = from; i < to; i++) s += a[i] * a[i]; return Math.sqrt(s / Math.max(1, to - from)); };
const onset = (a, th = 0.01) => a.findIndex(v => Math.abs(v) > th);

// ---- the instruments are Felucca's (step 3): each of the first four makes its own sound
// a fresh machine: a phrase written with the gamepad, then 4 s of it recorded to build/tracker/phrase.wav
{
    const m2 = new Machine(MOD, null);
    let fr = 0, hk = 0;
    const rec = [];
    const go = (ms, keep) => {
        const end = m2.ms() + ms;
        let pk = 0, sum = 0, cnt = 0;
        while (m2.ms() < end) {
            const a = m2.audio();
            for (let i = 0; i < a.length; i++) { pk = Math.max(pk, Math.abs(a[i])); sum += a[i] * a[i]; cnt++; if (keep) rec.push(a[i]); }
            fr += m2.frames;
            m2.runTo(Math.floor(fr * 1000 / 44100), 1000);
        }
        return { pk, rms: Math.sqrt(sum / Math.max(1, cnt)) };
    };
    const press = (...ks) => { for (const k of ks) hk |= 1 << PAD[k]; m2.input(hk, 0); go(25); for (const k of ks) hk &= ~(1 << PAD[k]); m2.input(hk, 0); go(25); };
    const editWith = (dir, times) => { hk |= 1 << PAD.edit; m2.input(hk, 0); go(25); for (let i = 0; i < times; i++) press(dir); hk &= ~(1 << PAD.edit); m2.input(hk, 0); go(25); };
    go(50);
    hk |= 1 << PAD.shift; m2.input(hk, 0); go(25); press('right'); press('right'); hk &= ~(1 << PAD.shift); m2.input(hk, 0); go(25);
    // audition each instrument: note column, instrument column set with EDIT + right
    const level = [];
    for (let i = 0; i < 4; i++) {
        editWith('right', 0);                    // row 0 note (C-3), instrument 0 on first entry
        press('right'); press('right');          // -> instrument column
        editWith('right', i ? 1 : 0);            // instrument +1 (0 -> 1 -> 2 -> 3 over the loop)
        press('left'); press('left');
        hk |= 1 << PAD.edit; m2.input(hk, 0);    // EDIT on the filled note: auditions it with the row's instrument
        level.push(go(400).rms);
        hk &= ~(1 << PAD.edit); m2.input(hk, 0); go(600);
    }
    ok(level.every(v => v > 0.003), `instruments 0-3 all sound (rms ${level.map(v => v.toFixed(3)).join(' ')})`);
    // a phrase: rows 0 4 8 C, instrument 3 (the drum kit, row 0 set above) ... play 4 s
    for (const r of [4, 8, 12]) { for (let k = 0; k < 4; k++) press('down'); editWith('up', r === 8 ? 1 : 0); }
    for (let k = 0; k < 12; k++) press('up');
    press('play');
    const played = go(4000, true);
    press('play');
    ok(played.rms > 0.003 && played.pk <= 1, `the phrase plays through Felucca (rms ${played.rms.toFixed(3)}, peak ${played.pk.toFixed(3)})`);
    const pcm = Buffer.alloc(rec.length * 2);
    rec.forEach((v, i) => pcm.writeInt16LE(Math.max(-32767, Math.min(32767, Math.round(v * 32767))), 2 * i));
    const hdr = Buffer.alloc(44);
    hdr.write('RIFF', 0); hdr.writeUInt32LE(36 + pcm.length, 4); hdr.write('WAVEfmt ', 8); hdr.writeUInt32LE(16, 16);
    hdr.writeUInt16LE(1, 20); hdr.writeUInt16LE(2, 22); hdr.writeUInt32LE(44100, 24); hdr.writeUInt32LE(44100 * 4, 28);
    hdr.writeUInt16LE(4, 32); hdr.writeUInt16LE(16, 34); hdr.write('data', 36); hdr.writeUInt32LE(pcm.length, 40);
    writeFileSync(join(OUT, 'phrase.wav'), Buffer.concat([hdr, pcm]));
}

// ---- the instrument page (step 4): SHIFT + right from the phrase, the agreed layout, edits heard live
{
    const T = rig(), p3 = (x, y) => T.m.fb()[y * 240 + x];
    const region = (x, y, w, h) => { const out = []; for (let j = y; j < y + h; j++) for (let i = x; i < x + w; i++) out.push(p3(i, j)); return out.join(); };
    T.press('edit');                             // row 0: C-3, instrument 0
    T.with_('shift', 'right');
    ok(mapLit(p3, 'I'), 'SHIFT + right: the instrument page (I lit on the map)');
    ok(p3(cellX(0) - 1, 32 + 2 + 12) === ACCENT, 'cursor on ENGINE');
    T.shot('4-instrument');
    const valuesRow3 = region(0, IROW_Y(3), 210, 38);
    T.with_('edit', 'right');                    // ENGINE: the next engine
    ok(region(0, IROW_Y(3), 210, 38) !== valuesRow3, 'a new engine relabels its parameters');
    T.shot('5-engine');
    T.with_('option', 'edit');                   // OPTION + EDIT on ENGINE: back to ANALOG (ACID's engine)
    const crumb = region(0, 0, 200, 19);
    T.with_('option', 'right');                  // instrument 1
    ok(region(0, 0, 200, 19) !== crumb, 'OPTION + right: the next instrument (crumb changes)');
    T.with_('option', 'left');
    // live: the phrase plays instrument 0; LEVEL down by three big steps makes it quieter
    T.press('down');                             // -> LEVEL
    ok(p3(cellX(0) - 1, IROW_Y(1) + 2 + 12) === ACCENT, 'down: LEVEL');
    T.press('play');
    T.go(300);
    const before = rms(T.record(2000));
    T.with_('edit', 'down', 3);
    T.go(200);
    const after = rms(T.record(2000));
    T.press('play');
    ok(after < before * 0.8, `LEVEL edited while playing: heard at once (rms ${before.toFixed(3)} -> ${after.toFixed(3)})`);
    const lvl = region(0, 32 + 38, 100, 38);
    T.m.enc(2, 4); T.go(50);                     // KNOB 1 = LEVEL (the cursor's row, first field)
    ok(region(0, 32 + 38, 100, 38) !== lvl, 'KNOB 1 turns LEVEL on its row');
    T.with_('shift', 'left');
    ok(!mapLit(p3, 'I') && mapLit(p3, 'P'), 'SHIFT + left: back to the phrase');
}

// ---- chains and song (step 5): the song screen at boot, chains and phrases made with EDIT / EDIT EDIT, 4 channels
{
    const T = rig(null, null, true), p4 = (x, y) => T.m.fb()[y * 240 + x];   // (booted: the song screen)
    const ink = (x, y, w, h) => { let n = 0; for (let j = y; j < y + h; j++) for (let i = x; i < x + w; i++) if (p4(i, j)) n++; return n; };
    const mapAt = L => mapLit(p4, L);
    const songTri = (k, line) => p4(cellX(2 + 3 * k) + 1, rowY(line) + 2 + 4) === PLAYC;
    const chainHead = r => p4(cellX(0) - 1, rowY(r)) === PLAYC;
    ok(mapAt('S'), 'boots on the song screen (S on the map)');
    ok(p4(cellX(3) - 1, 32) === ACCENT, 'cursor on song row 00, channel 1');
    T.press('edit');                             // empty song cell: the next free chain, 00
    T.press('down');
    T.press('edit'); T.press('edit');            // EDIT EDIT: a new chain, 01
    ok(ink(cellX(3), 32 + 12, 12, 12) > 10, 'EDIT EDIT on song row 01: a new chain (01)');
    T.press('up');
    T.with_('shift', 'right');                   // chain 00
    ok(mapAt('C'), 'SHIFT + right: the chain screen');
    T.press('edit');                             // phrase column: the next free phrase (00)
    T.press('right');                            // transpose
    T.hold('edit'); T.press('up'); T.free('edit');   // +12
    ok(ink(cellX(5), 32, 18, 12) > 10, 'transpose +12 written');
    T.press('left');
    T.with_('shift', 'right');                   // phrase 00
    ok(mapAt('P'), 'SHIFT + right: the phrase screen');
    T.press('edit');                             // C-3 on row 0, instrument 0
    T.press('down', 4); T.press('edit');
    T.press('down', 4); T.press('edit');
    T.press('down', 4); T.press('edit');
    T.with_('shift', 'left'); T.with_('shift', 'left');   // back to the song
    ok(mapAt('S'), 'SHIFT + left twice: back to the song');
    // song row 01 -> chain 01: a phrase of its own, made from its chain screen
    T.press('down');
    T.with_('shift', 'right');
    T.press('edit');                             // chain 01 row 0: the last phrase used (00)
    T.press('edit');                             // ... EDIT EDIT: a new phrase (01)
    T.with_('shift', 'right');
    T.press('edit'); T.press('down', 8); T.press('edit');   // two notes in phrase 01
    T.with_('shift', 'left'); T.with_('shift', 'left');
    // channel 2: chain 00 again on song row 00 (the same chain on two channels)
    T.press('up'); T.press('right');
    T.press('edit');                             // the last chain used
    T.press('left');
    // the song map (user 2026-10-11, principles-review.md #13; render.js songMap / songLit): a column per channel from x 226,
    // a song row every 2 px from y 98; the cursor's chain lit wherever it plays, others grey, empty rows dark, 16 rows framed
    const CHC = ['#4fa3ff', '#5fd36a'].map(rgb565), DIMC = rgb565('#555555'), SURFC = rgb565('#1a1f2a');
    const sm = (r, k) => p4(226 + 3 * k, 98 + 2 * r), smap = () => [sm(0, 0), sm(0, 1), sm(1, 0), sm(2, 0)];
    const same = (a, b) => a.every((v, i) => v === b[i]);
    ok(same(smap(), [CHC[0], DIMC, DIMC, SURFC]) && sm(0, 2) === SURFC && sm(0, 0) === p4(227, 99) && p4(228, 98) !== CHC[0],
        `song map: the cursor's chain 00 lit, chain 01 grey, row 02 empty (${smap().join()})`);
    ok([[224, 97], [238, 97], [224, 130], [238, 130]].every(([x, y]) => p4(x, y) === ACCENT) && p4(224, 131) !== ACCENT,
        'song map: the 16 rows on screen framed in the accent');
    T.press('down'); ok(same(smap(), [DIMC, CHC[1], CHC[0], SURFC]), `song map: on row 01, chain 01 lit on both channels it plays (${smap().join()})`);
    T.press('left'); ok(same(smap(), [DIMC, CHC[1], CHC[0], SURFC]), 'song map: on the row number of row 01, its one chain');
    T.press('up'); ok(same(smap(), [CHC[0], CHC[1], CHC[0], SURFC]), 'song map: on the row number of row 00, its chains');
    T.press('right');
    T.press('down', 63);
    ok(p4(226, 224) === SURFC && p4(224, 226) === ACCENT && p4(224, 129) !== ACCENT, 'song map: row 3F and the foot of the frame in the bottom line');
    T.press('up', 63);
    ok(p4(224, 226) !== ACCENT && p4(224, 97) === ACCENT, 'song map: back up, the bottom line cleared');
    T.shot('6-song');
    // play the song from row 00: channel 1 plays chain 00 (2 s), then chain 01 (2 s), then row 02 is empty: back to 00
    T.press('play');
    const seen = [];
    let ch2 = 0, red1 = 0;
    for (let t = 0; t < 5000; t += 50) {
        T.go(50);
        if (songTri(1, 0)) ch2++;
        if (sm(1, 0) === PLAYC) red1++;
        const r = songTri(0, 0) ? 0 : songTri(0, 1) ? 1 : -1;
        if (r >= 0 && r !== seen[seen.length - 1]) seen.push(r);
    }
    ok(seen.join() === '0,1,0', `the song: row 00, row 01, then back to 00 at the empty row 02 (${seen.join()})`);
    ok(red1 > 20 && sm(0, 2) === SURFC, `song map: row 01 red while it plays (${red1} looks)`);
    ok(ch2 > 80, `channel 2 plays too: its playhead stays on row 00, its song is one row (${ch2} of 100 looks)`);
    const loud = rms(T.record(500));
    T.press('play');
    ok(loud > 0.003, `the song sounds (rms ${loud.toFixed(3)})`);
    // the chain screen's PLAY: this chain from the cursor row, looped, this channel only
    T.press('down');                             // song row 01, channel 1
    T.with_('shift', 'right');                   // chain 01
    T.press('play');
    T.go(300);
    ok(chainHead(0), 'chain screen: PLAY loops this chain (playhead on its row 0)');
    T.go(2500);
    ok(chainHead(0), 'still chain 01 row 0 after one phrase (a one-row chain loops)');
    T.press('play');
    T.shot('7-chain');
    ok(p4(226, 224) !== SURFC && p4(226, 100) !== DIMC, 'song map: not on C (nor in its bottom line)');
    T.with_('shift', 'left');                    // song row 01: a selection of rows 00-01, channels 1-2
    T.hold('shift'); T.press('option'); T.free('shift'); T.press('up'); T.press('right');
    ok(same(smap(), [CHC[0], CHC[1], CHC[0], SURFC]), `song map: a selection lights its chains (${smap().join()})`);
}

// ---- the FX columns (step 6a): commands and values, named by the row's instrument (commands.md)
{
    const T = rig(), cell = T.cell, strip = () => T.strip(30);
    T.press('edit');                             // row 0: C-3, instrument 0 (ANALOG ACID)
    T.press('right', 3);                         // FX1 command
    T.press('edit');                             // no command used yet: nothing (picks review #13)
    ok(cell(11, 0, 3) === '---' && strip() === 'hold EDIT: pick a command', `EDIT on an empty FX command, no last command: nothing, ${strip()} (${cell(11, 0, 3)})`);
    // EDIT held on a command (or that empty one): the FX grid (commands.md 6), one group a line, on VOL; the arrows
    // move as they look and write
    T.hold('edit');
    // by screen (decisions.md "The FX grid by screen"): GLO MXR PHR INS ENV LFO VOX, RAMP last (ANALOG: INS on 2 lines)
    const titles = [36, 51, 66, 93, 120, 135, 162, 177].map(y => readText(T.m.fb(), 4 + 6, y, 4).trim()).join(' ');
    ok(titles === 'GLO MXR PHR INS ENV LFO VOX RAMP', `EDIT held on a command: the FX grid opens, its groups by screen (${titles})`);
    // the map lights the cursor's group's screen, white (on VOL: INS -> I); the current P stays in the accent
    const TEXTC = rgb565('#e8e8e8'), letter = (i, j) => { for (let y = 32 + j * 12; y < 44 + j * 12; y++) for (let x = 212 + 7 * i; x < 218 + 7 * i; x++) {
        const v = T.m.fb()[y * 240 + x]; if (v === TEXTC) return 'lit'; if (v === ACCENT) return 'accent'; } return 'grey'; };
    const onVol = [letter(3, 1), letter(2, 0), letter(2, 1)].join();
    ok(onVol === 'lit,grey,accent', `the FX grid on VOL: I lit, G grey, P in the accent (${onVol})`);
    T.shot('8-fx-grid');
    ok(strip() === 'arrows: pick a command', `... on VOL, nothing written yet: the bottom line says what to do (${strip()})`);
    T.press('down');                             // VOL (ENG's first: VOL WAV DTN MIX NOI CUT RES) -> its second line: DRV
    T.free('edit');
    ok(cell(11, 0, 3) === 'DRV', `EDIT + down in the grid: the line below, the same place (${cell(11, 0, 3)})`);
    ok(letter(3, 1) === 'grey', `the grid closed: I grey again (${letter(3, 1)})`);
    T.hold('edit'); T.press('up'); T.press('right', 4); T.free('edit');   // -> ENG: ANALOG's slots, the 4th: NOI
    ok(cell(11, 0, 3) === 'NOI', `... ENG = the engine's own names: ANALOG E3 = NOI (${cell(11, 0, 3)})`);
    T.press('left');                             // instrument column
    T.hold('edit'); T.press('down', 3); T.free('edit');   // instrument 3: DRUM KIT (the list: down one item)
    ok(cell(11, 0, 3) === 'DCY', `the row's instrument changes: the same slot is now DRUM's DCY (${cell(11, 0, 3)})`);
    T.press('down'); T.press('right');           // row 1 FX1: no instrument, the nearest above (DRUM)
    T.press('edit');                             // the last command, NOI, by name: DRUM has none
    ok(strip().startsWith('NOI: not a DRUM parameter') && cell(11, 1, 3) === '---', `pasting by name: ${strip()}`);
    T.press('up'); T.press('right');             // row 0 FX1 value
    T.hold('edit');
    ok(/^DCY [0-9A-F]{2} = /.test(strip()), `EDIT held on a value: what it sets (${strip()})`);
    T.free('edit');
    T.with_('option', 'edit');
    ok(cell(15, 0, 2) === '00' && cell(11, 0, 3) === 'DCY', 'OPTION + EDIT on a value: 00, the command stays');
    T.press('left');
    T.with_('option', 'edit');
    ok(cell(11, 0, 3) === '---' && cell(15, 0, 2) === '--', 'OPTION + EDIT on a command: the cell is empty');
    // the LOFI instrument page: SWP VIB ARP are gone (commands.md 3, 4)
    T.with_('shift', 'right');                   // I: the row's instrument (3); OPTION + left: 2 = LOFI PULSE LD
    T.with_('option', 'left');
    const row4 = readText(T.m.fb(), 4, IROW_Y(3) + 2, 32);   // (E4..E7: row 3)
    ok(/^ +TON *$/.test(row4), `LOFI's page: E4 E5 E6 (SWP VIB ARP) gone, TONE stays, labelled TON, its command (row: "${row4.trim()}")`);
}

// ---- the commands while playing (step 6b): a fresh machine per case, a phrase written with the gamepad
{
    // TPO 3C: rows of 250 ms (60 BPM) instead of 125
    const T = rig();
    T.note(0); T.fx(0, 0, 'TPO', 0x3C);
    T.press('play');
    const seen = [], at = [];
    for (let t = 0; t < 1400; t += 10) { T.go(10); const r = T.playhead(); if (r >= 0 && r !== seen[seen.length - 1]) { seen.push(r); at.push(T.m.ms()); } }
    T.press('play');
    const row = (at[4] - at[1]) / 3;
    ok(Math.abs(row - 250) < 12, `TPO 3C: a row lasts 250 ms (${row.toFixed(0)} ms)`);
}
{
    // HOP 08 on row 4: the (looped) phrase goes on at row 8
    const T = rig();
    T.note(0); T.fx(4, 0, 'HOP', 0x08);
    T.press('play');
    const seen = [];
    for (let t = 0; t < 1600; t += 20) { T.go(20); const r = T.playhead(); if (r >= 0 && r !== seen[seen.length - 1]) seen.push(r); }
    T.press('play');
    ok(seen.slice(0, 8).join() === '0,1,2,3,4,8,9,10', `HOP 08 on row 4: rows 0-4, then 8 (${seen.slice(0, 8).join()})`);
}
{
    // VOL 00 silences the channel's notes; CHA 0F: the note never plays
    const a = rig(); a.note(0); a.press('play'); a.go(100); const loud = rms(a.record(500)); a.press('play');
    const b = rig(); b.note(0); b.fx(0, 0, 'VOL', 0); b.press('play'); b.go(100); const vol0 = rms(b.record(500)); b.press('play');
    const c = rig(); c.note(0); c.fx(0, 1, 'CHA', 0x0F); c.press('play'); c.go(100); const cha = rms(c.record(500)); c.press('play');
    ok(loud > 0.01 && vol0 < loud / 20, `VOL 00: silent (rms ${loud.toFixed(3)} -> ${vol0.toFixed(4)})`);
    ok(cha < loud / 20, `CHA 0F: what is left of it never plays (rms ${cha.toFixed(4)})`);
}
{
    // DEL 05: the note starts 5 ticks (5 x 918.75 samples) later; KIL 02: cut after 2 ticks
    const a = rig(); a.note(0); const ra = a.playRecord(400); a.press('play');
    const b = rig(); b.note(0); b.fx(0, 0, 'DEL', 5); const rb = b.playRecord(400); b.press('play');
    const late = onset(rb) - onset(ra);
    // (PLAY is taken at the next audio interrupt, every 256 frames: the two runs may start up to one of them apart)
    ok(Math.abs(late - 5 * 918.75) <= 256 + 32, `DEL 05: struck 5 ticks late (${late} samples, expected 4594 +- one 256-frame block)`);
    const k = rig(); k.note(0); k.fx(0, 0, 'KIL', 2); const rk = k.playRecord(400); k.press('play');
    const o = onset(rk), after = rms(rk, o + 3 * 919, o + 5 * 919), before = rms(ra, onset(ra) + 3 * 919, onset(ra) + 5 * 919);
    ok(after < before / 10, `KIL 02: cut after 2 ticks (rms after ${after.toFixed(4)} vs ${before.toFixed(4)})`);
}
{
    // a sound command: ATK FF (a slow attack) on the note's row; dropped at stop (the instrument page shows CUT unchanged)
    const a = rig(); a.note(0); const ra = a.playRecord(300); a.press('play');
    const b = rig(); b.note(0); b.fx(0, 0, 'ATK', 0xFF); const rb = b.playRecord(300); b.press('play');
    const oa = onset(ra, 0.001), ea = rms(ra, oa, oa + 2000), eb = rms(rb, oa, oa + 2000);
    ok(eb < ea / 3, `ATK FF: the attack is slow (first 45 ms rms ${ea.toFixed(3)} -> ${eb.toFixed(3)})`);
    // CUT 00 while playing, then stop: the instrument page shows the instrument's own CUT again
    const c = rig(); c.note(0); c.fx(0, 0, 'CUT', 0);
    c.with_('shift', 'right');                 // I
    const before = readText(c.m.fb(), 4, IROW_Y(4) + 2 + 12, 8);
    c.with_('shift', 'left');
    c.press('play'); c.go(600); c.press('play'); c.go(100);
    c.with_('shift', 'right');
    const after = readText(c.m.fb(), 4, IROW_Y(4) + 2 + 12, 8);
    ok(before === after && before.trim().length > 0, `held values dropped at stop: CUT ${before.trim()} -> ${after.trim()}`);
}
{
    // a ramp: VOL 00, RTO FF under it, RTI 08 under that: the volume rises over 8 rows
    const T = rig();
    T.note(0); T.note(1); T.note(2); T.note(3); T.note(4); T.note(5); T.note(6); T.note(7); T.note(8);
    T.fx(0, 0, 'VOL', 0); T.fx(1, 0, 'RTO', 0xFF); T.fx(2, 0, 'RTI', 0x08);
    const r = T.playRecord(1300);
    T.press('play');
    const o = onset(r, 0.00001), row = 5512, lo = rms(r, o + row * 1, o + row * 2), hi = rms(r, o + row * 7, o + row * 8);
    ok(hi > lo * 3 && lo < 0.02, `VOL 00 + RTO FF + RTI 08: the volume rises (row 1 rms ${lo.toFixed(3)}, row 7 ${hi.toFixed(3)})`);
}

// ---- the FX visuals on the phrase screen (step 7, commands.md 11, 14-17)
{
    const T = rig(), fb = () => T.m.fb(), px = (x, y) => fb()[y * 240 + x];
    const LABEL = rgb565('#777777'), LINE = rgb565('#333333'), CH1 = rgb565('#4fa3ff'), X0 = cellX(2);
    // a note on row 0 with DEL 02 and KIL 08: its line starts at tick 2, carries into row 1 and is cut at tick 4
    T.note(0); T.fx(0, 0, 'DEL', 2); T.fx(0, 1, 'KIL', 8);
    T.to(5, 0);                                  // (the cursor away from those rows)
    const under = r => { const out = []; for (let x = X0; x < X0 + 20; x++) out.push(px(x, rowY(r) + 11) === LABEL ? '#' : '.'); return out.join(''); };
    ok(/^\.{5,7}#{10,12}/.test(under(0)), `DEL 02: the timeline starts at tick 2 (${under(0)})`);
    ok(/^#{10,12}/.test(under(1)) && px(X0 + 11, rowY(1) + 9) === LABEL, `KIL 08: carried into the next row, cut at tick 4 (${under(1)})`);
    // RET 04 on row 4: 4 ticks under the note, lit in the channel colour while it plays
    T.note(4); T.fx(4, 0, 'RET', 4);
    // a ramp: VOL 00 on row 8, RTO FF and RTI 04 under it: a line down FX1's left edge over rows 8..12
    T.note(8); T.fx(8, 1, 'VOL', 0); T.fx(9, 1, 'RTO', 0xFF); T.fx(10, 1, 'RTI', 4);
    T.to(15, 0);
    const rx = cellX(18) - 3;
    ok(px(rx, rowY(10) + 5) === LINE && px(rx, rowY(12) + 5) === LINE && px(rx, rowY(13) + 5) !== LINE, 'a ramp: a line over the rows it covers (8..12)');
    T.press('play');
    let retLit = false, rampLit = false;
    for (let t = 0; t < 2000 && !(retLit && rampLit); t += 10) {
        T.go(10);
        if (T.playhead() === 4 && !retLit) for (let x = X0; x < X0 + 18; x++) if (px(x, rowY(4) + 11) === CH1) {
            retLit = true;                       // the moment the ticks light: that is the screenshot
            T.shot('9-ret');
            break;
        }
        if (T.playhead() === 11 && px(rx, rowY(9) + 5) === CH1) rampLit = true;
    }
    ok(retLit, 'RET 04: its ticks light up in the channel colour as they play');
    ok(rampLit, 'the ramp line: the played part in the channel colour');
    T.shot('9-visuals');
    T.press('play');
}
{
    // HOP 08 on row 4: rows 5..F are never played where this phrase sits: dimmed
    const T = rig(), px = (x, y) => T.m.fb()[y * 240 + x], DIM = rgb565('#555555');
    T.note(0); T.fx(4, 0, 'HOP', 8); T.to(0, 0);
    const numColour = r => { const c = new Map(); for (let y = 0; y < 12; y++) for (let x = 0; x < 6; x++) { const v = px(4 + x, rowY(r) + y); if (v) c.set(v, (c.get(v) || 0) + 1); } return [...c].sort((a, b) => b[1] - a[1])[0]?.[0]; };
    ok(numColour(6) === DIM && numColour(3) !== DIM, 'HOP 08 on row 4: the rows after it are dimmed (never played here)');
    // CHA 0F with SWG 80 on row 2: while playing, the note (left of CHA) is dimmed after its roll; SW50 in the crumb
    T.note(2); T.fx(2, 1, 'CHA', 0x0F); T.fx(2, 2, 'SWG', 0x80); T.to(0, 0);
    T.press('play'); T.go(600);
    const noteDim = (() => { const c = new Map(); for (let y = 0; y < 12; y++) for (let x = 0; x < 18; x++) { const v = px(cellX(2) + x, rowY(2) + y); if (v) c.set(v, (c.get(v) || 0) + 1); } return [...c].sort((a, b) => b[1] - a[1])[0]?.[0]; })();
    ok(noteDim === DIM, 'CHA 0F: the side that lost its roll is dimmed while playing');
    const crumb = readText(T.m.fb(), 4 + 22 * 6, 4, 4);
    ok(crumb === 'SW50', `SWG 80: the channel's swing in the breadcrumb (${crumb})`);
    T.press('play');
}

// ---- the screen and the sound follow the same rules (docs/code-review.md U2-U6, F3; the prototype: the same picks)
const playRows = (T, ms) => {                  // the playhead's rows, PLAY to PLAY
    const seen = [];
    T.press('play');
    for (let t = 0; t < ms; t += 20) { T.go(20); const r = T.playhead(); if (r >= 0 && r !== seen[seen.length - 1]) seen.push(r); }
    T.press('play');
    return seen;
};
const rowColour = (T, r) => {                  // the row number's colour (the HOP dimming)
    const c = new Map();
    for (let y = 0; y < 12; y++) for (let x = 0; x < 6; x++) { const v = T.m.fb()[(32 + r * 12 + y) * 240 + 4 + x]; if (v) c.set(v, (c.get(v) || 0) + 1); }
    return [...c].sort((a, b) => b[1] - a[1])[0]?.[0];
};
{
    // U5: two HOPs on a row: the first wins, as the dimming and Q
    const T = rig();
    T.note(0); T.fx(4, 0, 'HOP', 0x08); T.fx(4, 1, 'HOP', 0x0C);
    const seen = playRows(T, 1600);
    ok(seen.slice(0, 7).join() === '0,1,2,3,4,8,9', `HOP 08 and HOP 0C on row 4: the first plays (${seen.slice(0, 7).join()})`);
}
{
    // U4: TPO below 40 = 40 BPM (rows of 375 ms), as the ramp and the text say
    const T = rig();
    T.note(0); T.fx(0, 0, 'TPO', 0x20);
    T.to(0, 4); T.hold('edit');
    const said = readText(T.m.fb(), 4, STRIP_Y, 15);
    T.free('edit');
    T.press('play');
    const at = [];
    for (let t = 0; t < 2000; t += 10) { T.go(10); const r = T.playhead(); if (r >= 0 && r !== at[at.length - 1]?.r) at.push({ r, t: T.m.ms() }); }
    T.press('play');
    const row = (at[4].t - at[1].t) / 3;
    ok(Math.abs(row - 375) < 15 && said === 'TPO 20 = 40 BPM', `TPO 20: 40 BPM, a row lasts 375 ms (${row.toFixed(0)} ms, "${said}")`);
}
{
    // U6: REP heading away from RTO: the value snaps to RTO on the first tick, so the line ends at the next row
    const T = rig(), LINE = rgb565('#333333'), px = (x, y) => T.m.fb()[y * 240 + x], rx = cellX(18) - 3;
    T.note(8); T.fx(8, 1, 'VOL', 0x80); T.fx(9, 1, 'REP', 0x10); T.fx(10, 1, 'RTO', 0x20);
    T.to(15, 0);
    ok(px(rx, rowY(9) + 5) === LINE && px(rx, rowY(12) + 5) !== LINE, 'VOL 80, REP 10, RTO 20: the ramp line ends at row 9 (not 14)');
}
{
    // U7: TPO plays at its tempo (the top bar shows it) but leaves the song's: played, stopped, saved, rebooted, G
    // still says TEMPO 120
    const T = rig(), bar = () => readText(T.m.fb(), BAR_R - 3 * 6, 4, 3).trim();
    T.note(0); T.fx(0, 0, 'TPO', 0x5A);
    T.press('play'); T.go(600);
    const playing = bar();
    T.press('play'); T.go(100);
    const stopped = bar();
    T.btn(9);   // SAVE
    const R = rig(T.store(), T.settings());
    R.with_('shift', 'up');                    // P -> G
    const g = R.cell(21, 3, 12).replace(/\s+/g, ' ').trim();
    ok(playing === '90' && stopped === '120' && g === 'TEMPO 120', `TPO 5A: 90 while playing, the song's 120 after (bar ${playing} / ${stopped}, saved: ${g})`);
}
{
    // A1 (C13): auditions on two channels within 180 ms: both end (the first one used to hang)
    const T = rig();
    T.press('edit');                           // channel 1, row 0: C-3, auditioned
    T.with_('option', 'right'); T.press('edit');   // channel 2 (a chain and phrase made), within ~150 ms
    const on = T.cell(2, 0, 3);
    T.go(2000);                                // (the release tail: ~1.5 s)
    const after = rms(T.record(300));
    ok(on === 'C-3' && after < 0.0005, `two auditions on channels 1 and 2: both silent 2 s later (rms ${after.toFixed(5)}, ch2 ${on})`);
}
{
    // H1 (C14): CUT (an E slot) held, then another engine while playing: the new engine's slot is its own, not held
    const T = rig(), CH1 = rgb565('#4fa3ff'), px = (x, y) => T.m.fb()[y * 240 + x];
    const ink = (x0, y0, w, h, col) => { let n = 0; for (let y = y0; y < y0 + h; y++) for (let x = x0; x < x0 + w; x++) if (px(x, y) === col) n++; return n; };
    const e4 = IROW_Y(3) + 2 + 12;             // the E4..E7 row's values
    T.note(0); T.fx(0, 0, 'CUT', 0x20); T.to(0, 0);
    T.press('play'); T.go(300);
    T.with_('shift', 'right'); T.go(150);      // I, the cursor on ENGINE
    const held = ink(4, e4, 42, 12, CH1);
    T.with_('edit', 'right'); T.go(150);       // the next engine
    const after = ink(4, e4, 42, 12, CH1), name = readText(T.m.fb(), 4, e4 - 12, 4);
    T.press('play');
    ok(held > 10 && after === 0, `CUT held, then another engine: its E4 (${name.trim()}) is not held (ink ${held} -> ${after})`);
}
{
    // QR1 (C15): QNT ROW, recording through a chain of two phrases: a hit late on row 15 goes to row 0 of the next
    // phrase (not of this one). RS1: a phrase made during the take does not take the one the chain's empty row gets
    const T = rig();
    T.with_('shift', 'up'); T.press('down', 3); T.press('right'); T.press('down', 6); T.with_('edit', 'left');   // G: QNT ROW
    T.with_('shift', 'down');                  // P
    T.with_('shift', 'left'); T.press('down'); T.press('edit', 2); T.press('up');   // C: row 1 = phrase 01
    T.btn(13); T.btn(13); T.press('play');     // armed, PLAY: the count-in, then the take from chain row 0
    T.with_('shift', 'right');                 // P 00 while it records (the playhead)
    T.until(15);
    T.go(70); T.key(9);                        // past the middle of row 15: D-4
    T.go(600);
    T.press('play');
    const p00 = T.cell(2, 0, 3);
    T.with_('shift', 'left'); T.press('down'); T.with_('shift', 'right');
    const p01 = T.cell(2, 0, 3);
    ok(p00 === '---' && p01 === 'D-4', `QNT ROW, late on row 15 of P00: written on P01 row 0 (P00 ${p00}, P01 ${p01})`);
}
{
    // RS1: recording through a chain, its empty row gets a spare phrase; a phrase made meanwhile is another one
    const T = rig();
    T.with_('shift', 'left');                  // C: row 0 = phrase 00, row 1 empty
    T.btn(13); T.btn(13); T.press('play');     // the take from chain row 0
    T.go(2500);                                // (count-in, into phrase 00: the spare picked)
    T.press('down', 2); T.press('edit');       // row 2: EDIT, a free phrase
    T.press('up', 2);
    T.go(2000);                                // phrase 00 ends: row 1 gets the spare
    T.press('play');
    const rows = [1, 2].map(r => T.cell(2, r, 2));
    ok(rows.join() === '01,02', `recording through a chain: row 1 gets the spare picked at the start, 01; row 2, made meanwhile, 02 (${rows.join()})`);
}
{
    // P1: SHIFT + PLAY while playing with the audio stalled: the stop is forced after 50 ms, the UI goes on
    const T = rig();
    T.note(0); T.press('play'); T.go(300);
    const main = ms => T.m.runTo(T.m.ms() + ms, 1000);   // the main loop alone: no audio interrupts
    T.m.input(1 << PAD.shift, 0); main(25); T.m.input(1 << PAD.shift | 1 << PAD.play, 0); main(25);
    T.m.input(1 << PAD.shift, 0); main(25); T.m.input(0, 0); main(200);
    T.m.input(1 << PAD.down, 0); main(25); T.m.input(0, 0); main(50);
    ok(T.m.fb()[rowY(1) * 240 + cellX(PH_X[0]) - 1] === ACCENT, 'SHIFT + PLAY with the audio stalled: the stop is forced, the cursor moves again');
    T.go(100); T.press('play');
}
{
    // U2: a row's instrument without a note: the sound commands below it are that instrument's (the screen's
    // row_inst), not the last note's. CUT 00 on row 4 under I1 on row 2 leaves instrument 0's note alone
    const make = ins1 => {
        const T = rig();
        T.note(0); T.fx(4, 0, 'CUT', 0);
        if (ins1) { T.to(2, 2); T.press('edit'); T.with_('edit', 'down'); }
        const ok1 = !ins1 || T.cell(9, 2, 1) === '1';
        const r = T.playRecord(1000); T.press('play');
        return { ok1, late: rms(r, 5512 * 5 + 2000, 5512 * 7) };   // rows 5, 6 (after PLAY's first block)
    };
    const a = make(1), b = make(0);
    ok(a.ok1 && b.late < a.late / 3 && a.late > 0.005, `I1 on row 2, CUT 00 on row 4: instrument 0's note goes on (rms ${a.late.toFixed(4)}, CUT on instrument 0: ${b.late.toFixed(4)})`);
}
{
    // U3: a song row whose chain has an empty row 0 ends that chain: the channel goes on to the next song row
    const T = rig();
    T.note(0);
    T.with_('shift', 'left', 2);              // S, song row 00 = chain 00
    T.press('left'); T.press('edit', 2);       // EDIT EDIT on the row number: an empty row 00, chain 00 to row 01
    T.press('right'); T.press('edit', 2);      // EDIT EDIT: a new empty chain on row 00
    const cells = [cellAt(T.m.fb(), 3, 0, 2), cellAt(T.m.fb(), 3, 1, 2)];
    const r = T.playRecord(1500); T.press('play');
    ok(cells[0] !== '--' && cells[1] === '00' && rms(r) > 0.005, `song 01 (empty), 00: the empty chain is passed, 00 plays (cells ${cells.join()}, rms ${rms(r).toFixed(4)})`);
}
{
    // F3: the row a phrase was entered at is kept per chain row, and forgotten when that row's phrase changes
    const T = rig(), DIM = rgb565('#555555');
    T.note(0); T.fx(4, 0, 'HOP', 8);
    T.with_('shift', 'left');                 // C: chain row 01 = a new phrase 01
    T.press('down'); T.press('edit', 2); T.press('up');
    T.press('play'); T.go(1300); T.press('play');   // (chain row 00 to its HOP, then phrase 01 from row 8)
    T.with_('shift', 'right');                // P 00: its HOP removed, so only the pass says phrase 01 starts at 8
    T.to(4, 3); T.with_('option', 'edit');
    T.with_('shift', 'left'); T.press('down'); T.with_('shift', 'right');
    const kept = rowColour(T, 3) === DIM && rowColour(T, 9) !== DIM;
    T.with_('shift', 'left'); T.with_('edit', 'right'); T.with_('shift', 'right');   // chain row 01: phrase 02 now
    const ph = readText(T.m.fb(), 4, 4, 20);
    ok(kept && rowColour(T, 3) !== DIM, `the entry row: kept from the pass (rows 0-7 dim: ${kept}), forgotten for a new phrase (crumb ${ph.trim()})`);
}

{
    // the instrument page while playing (commands.md 11, mockup inst-live): CUT held by a ramp, in the channel's
    // colour with a triangle; the bottom line says what is set and what it is now; EDIT held shows the configured value
    const T = rig(), px = (x, y) => T.m.fb()[y * 240 + x], CH1 = rgb565('#4fa3ff'), TEXT = rgb565('#e8e8e8');
    T.note(0); T.note(8); T.fx(4, 0, 'CUT', 0x20); T.fx(5, 0, 'RTO', 0xC0); T.fx(6, 0, 'RTI', 8);
    T.fx(4, 1, 'RES', 0x70); T.fx(5, 1, 'RTO', 0x20); T.fx(6, 1, 'RTI', 8);   // RES ramping down
    T.fx(4, 2, 'DRV', 0x60);                                              // DRV held, no ramp
    T.to(0, 0);
    T.press('play'); T.go(500);                  // (the ramp runs rows 4..12: 0.5 .. 1.5 s)
    T.with_('shift', 'right');                   // I, while playing; the cursor to CUT's row (E4..E7: CUT is first)
    T.press('down', 3);
    T.go(100);
    const ink = (x0, y0, w, h, col) => { let n = 0; for (let y = y0; y < y0 + h; y++) for (let x = x0; x < x0 + w; x++) if (px(x, y) === col) n++; return n; };
    const cutY = IROW_Y(3) + 2 + 12;
    const strip = T.strip(26);
    // the cell and the bottom line show the same moment (one snapshot of the held values per redraw)
    ok(/^CUT set 355Hz, now \S+\?$/.test(strip), `bottom line: what is set, what it is now, and the triangle (${strip})`);
    T.press('right');                            // the cursor off CUT: its value in the channel's colour
    T.go(200);
    for (let t = 0; t < 8; t++) {                // RES (the cursor) ramps down: compare cell and bottom line often
        T.go(37);
        const cell = readText(T.m.fb(), 4 + 8 * 6, cutY, 4).replace(/[?\s]+$/, ''), line = T.strip(26);
        const now = (line.match(/now (\S+?)\??$/) || [])[1];
        if (now !== cell) { ok(false, `RES cell ${cell} = bottom line now ${now} (${line})`); break; }
        if (t === 7) ok(true, `RES: the cell and the bottom line agree while it ramps (${cell})`);
    }
    ok(ink(4, cutY, 42, 12, CH1) > 10, 'CUT held by the ramp: drawn in the channel colour');
    ok(ink(4 + 16 * 6, cutY, 30, 12, CH1) > 10, 'DRV held without a ramp: the channel colour too');
    T.shot('10-inst-live');
    T.hold('edit'); T.go(150);
    ok(ink(4, cutY, 42, 12, CH1) === 0 && ink(4, cutY, 42, 12, TEXT) > 10, 'EDIT held: the configured value (white)');
    T.free('edit');
    T.press('play');
}

{
    // picks review #28 (user, 2026-10-11): a ramp belongs to its instrument; rows playing another instrument leave it
    // on the previous one, paused, and it resumes when the channel goes back to it
    const T = rig();
    T.note(0); T.fx(0, 0, 'CUT', 0); T.fx(1, 0, 'RTO', 0xFF); T.fx(2, 0, 'RTI', 0x0C);   // I0's CUT 00 -> FF in 12 rows
    T.note(4); T.to(4, 2); T.with_('edit', 'down');   // rows 4-7: I1
    T.note(8); T.to(8, 2); T.press('edit'); T.with_('edit', 'up');   // row 8 on: I0 again
    const ins = [0, 4, 8].map(r => T.cell(9, r, 1)).join();
    T.to(0, 0); T.with_('shift', 'right'); T.press('down', 3);   // I0's CUT, its live value in the bottom line
    const now = () => { const m = T.strip(30).match(/now ([\d.]+)(k?)/) || []; return parseFloat(m[1]) * (m[2] ? 1000 : 1); };   // Hz
    T.press('play');
    const at = {}; for (const r of [3, 5, 6, 7, 11]) { T.go(r * 125 + 60 - (at.t || 0)); at.t = r * 125 + 60; at[r] = now(); }
    T.press('play');
    ok(ins === '0,1,0' && at[3] > 0 && at[5] === at[6] && at[6] === at[7] && at[11] > at[7],
        `a CUT ramp on I0, rows 4-7 on I1: paused there, resumes on I0 (instruments ${ins}; rows 3 5 6 7 11: ${[3, 5, 6, 7, 11].map(r => at[r]).join(' ')})`);
}

{
    // review S1 / S1b (C12): instrument 0's CUT ramp plays; the audio ISR reads CUT's label through sound.c's scratch
    // track. emu_irq_due fires audio blocks inside the main loop's next scratch window (masked: after it)
    const T = rig(), eng = () => readText(T.m.fb(), 4, 34 + 12, 6).trim(), name = () => readText(T.m.fb(), 4 + 16 * 6, 34 + 12, 12).trim();
    const E = i => [...Array(8).keys()].map(k => T.m.e.emu_ins_e(i, k)).join();
    T.note(0); T.fx(1, 0, 'CUT', 0x20); T.fx(2, 0, 'RTO', 0xC0); T.fx(3, 0, 'RTI', 8);
    T.to(0, 0); T.press('play'); T.go(300);
    T.with_('shift', 'right'); const e0 = eng(), n0 = name(), E0 = E(0);
    T.with_('option', 'right'); const e1 = eng(), n1 = name();
    T.m.e.emu_irq_due(64);                       // ~90 ms of audio: more than a tick, the ramp moves
    T.with_('edit', 'right');                    // I1's ENGINE: the next engine
    const e1b = eng(), n1b = name();
    ok(e1b !== e1 && e1b !== e0, `S1 / C12: I1 gets the next engine, not I0's sound (${e1} ${n1} -> ${e1b} ${n1b}; I0 ${e0} ${n0})`);
    ok(E(0) === E0, `S1 / C12: I0's saved E values unchanged (${E(0)})`);
    // S1b: I2 (LOFI) WAV, E1, 0..4 at its top; one more step with the ISR in field_desc's window must stay in
    // LOFI's range (ANALOG's E1, DTN, goes to 127). The ISR's CUT lookup in every main-loop window of the press
    T.with_('option', 'right'); T.press('down', 2); T.press('right');
    for (let n = 0; n < 6 && T.m.e.emu_ins_e(2, 1) !== 4; n++) T.with_('edit', 'right');
    const w0 = T.m.e.emu_ins_e(2, 1);
    T.hold('edit'); T.m.e.emu_irq_look(0, 4, 100000); T.press('right'); T.m.e.emu_irq_look(0, 0, 0); T.free('edit');
    const w1 = T.m.e.emu_ins_e(2, 1);
    ok(w0 === 4 && w1 >= 0 && w1 <= 4, `S1b / C12: an E edit during the ramp stays in the edited engine's range (LOFI WAV ${w0} -> ${w1})`);
    T.press('play');
}

{
    // review S3: every engine has its E0..E7 names in commands.c ENGINE_CMD (a missing one: its slots unnamed)
    const rowless = m.e.emu_eng_rowless();
    ok(rowless === 0, `S3 / C21: every ENGINES[] name has an ENGINE_CMD row (missing: bits ${rowless.toString(2)})`);
    // review S2: cmd_encode's closed form = the search it replaced, every range 0..4095, every setting
    const bad = m.e.emu_encode_check(4095);
    ok(bad === 0, `S2: cmd_encode's inverse = the old search (${bad} mismatches)`);
}
{
    // review C20: fmt_fix's decimals and signs; the store refuses writes past its end, an unaligned erase and an
    // offset past it, and stays unchanged (a machine of its own: the store is changed by the control write)
    const f = new Machine(MOD, null).e;
    const str = p => { const b = new Uint8Array(f.memory.buffer, p, 16); return Buffer.from(b.subarray(0, b.indexOf(0))).toString(); };
    const FIX = [[0, 0, '0'], [7, 0, '7'], [-42, 0, '-42'], [0, 1, '0.0'], [5, 1, '0.5'], [-63, 1, '-6.3'], [-5, 1, '-0.5'],
        [0, 2, '0.00'], [5, 2, '0.05'], [1234, 2, '12.34'], [-7, 2, '-0.07'], [-1234, 2, '-12.34']];
    const fix = FIX.map(([v, d, want]) => [str(f.emu_fmt_fix(v, d)), want]).filter(([got, want]) => got !== want);
    ok(fix.length === 0, `C20: fmt_fix for 0, 1, 2 decimals and negatives (${fix.map(([g, w]) => g + '!=' + w).join(' ') || 'all match'})`);
    const L = f.emu_store_len(), store = () => Buffer.from(new Uint8Array(f.memory.buffer, f.emu_store_ptr(), L));
    const before = store(), gen = f.emu_store_gen();
    const rc = [f.emu_store_try(0, L - 4, 8), f.emu_store_try(1, 0x800, 0), f.emu_store_try(0, L + 1, 0), f.emu_store_try(1, L, 0)];
    ok(rc.every(r => r !== 0) && store().equals(before) && f.emu_store_gen() === gen,
        `C20: the store refuses a write past its end, an unaligned erase, an offset past it; unchanged (rc ${rc})`);
    ok(f.emu_store_try(0, L - 8, 8) === 0 && !store().equals(before), 'C20: ... while a write that ends at its end goes in');
}

// ---- FX leftovers (commands.md 9): SHIFT + right on a sound command = its parameter; EDIT tapped = copy it
{
    const T = rig(), px = (x, y) => T.m.fb()[y * 240 + x];
    const fieldCursor = (row, cx) => px(4 + cx * 6 - 1, IROW_Y(row) + 2 + 12) === ACCENT;
    const mapI = () => mapLit(px, 'I');
    T.note(0); T.fx(0, 0, 'CUT', 0x40); T.fx(1, 0, 'TV1', 0x40);
    T.to(1, 3);                                  // a player command: the usual next screen (the page's own cursor)
    T.with_('shift', 'right');
    ok(mapI() && fieldCursor(0, 0), 'SHIFT + right on TV1 (a player command): just the next screen');
    T.with_('shift', 'left');
    T.to(0, 3);
    T.with_('shift', 'right');
    ok(mapI() && fieldCursor(3, 0), 'SHIFT + right on CUT: the instrument page, the cursor on CUT');
    T.press('up', 2);                            // CUT -> WAV -> LEVEL; right: PAN
    T.press('right');
    T.press('edit');                             // tapped: PAN copied as a command, with its value
    const strip = T.strip(14);
    ok(strip === 'copied PAN 80', `EDIT tapped on PAN: ${strip}`);
    T.hold('edit'); T.press('right'); T.free('edit');   // EDIT + arrow: an edit, no copy
    ok(!readText(T.m.fb(), 4, STRIP_Y, 6).startsWith('copied'), 'EDIT + right on PAN: an edit, not a copy');
    T.with_('shift', 'left');                    // back to the phrase: EDIT on an empty FX command writes PAN 80
    T.to(2, 3);
    T.press('edit');
    ok(T.cell(11, 2, 3) === 'PAN' && T.cell(15, 2, 2) === '80', `EDIT on an empty FX command: the copied PAN 80 (${T.cell(11, 2, 3)} ${T.cell(15, 2, 2)})`);
    // in Q (step Q-3f, app.js paramJump): the cursor cell's sound command, on the instrument shown; elsewhere: I
    T.to(0, 0); T.btn(11);   // SEQ: line 0 = C-3 CUT40
    T.press('right'); T.with_('shift', 'right');
    ok(mapI() && fieldCursor(3, 0), 'SHIFT + right on CUT in Q: the instrument page, the cursor on CUT');
    T.press('up', 3); T.with_('shift', 'up'); T.press('left'); T.with_('shift', 'right');   // I's cursor up to ENGINE first
    ok(mapI() && fieldCursor(0, 0), "SHIFT + right on Q's note cell: just I, its own cursor (ENGINE)");
}

// ---- mute / solo (docs/ux/shortcuts.md): OPTION then SHIFT = mute, OPTION + PLAY = solo, OPTION + SHIFT + PLAY = clear
{
    const T = rig(), px = (x, y) => T.m.fb()[y * 240 + x];
    const LABEL = rgb565('#777777');
    const mark = k => readText(T.m.fb(), 4 + (4 + 3 * k) * 6, 20, 1);
    const inked = (k, c) => { for (let y = 20; y < 32; y++) for (let x = 0; x < 6; x++) { const v = px(4 + (4 + 3 * k) * 6 + x, y); if (v) return v === c; } return false; };
    const TEXT = rgb565('#e8e8e8');               // M label grey, S text white (user 2026-10-11, principles-review.md #4)
    for (let r = 0; r < 16; r += 2) T.note(r);   // channel 1: a note every other row (channel 2: nothing)
    T.with_('shift', 'left', 2);                 // P -> C -> S
    T.press('play'); T.go(300);
    const rec = ms => { const a = T.record(ms); return rms(a); };
    const loud = rec(500);
    T.hold('option'); T.press('shift'); T.free('option');
    // (the echoes of notes played before the mute ring out in Felucca's delay: my pick, as a desk's effect return)
    const tail = rec(500), muted = (T.go(2500), rec(500));
    ok(T.strip(24) === 'mute CH1' && mark(0) === 'M' && inked(0, LABEL), `OPTION then SHIFT: ${T.strip(24)}, M beside track 1, in the label grey`);
    ok(loud > 0.01 && muted < loud / 20 && tail < loud / 3, `muted: no new notes, the delay's echoes die away (rms ${loud.toFixed(3)} -> ${tail.toFixed(4)} -> ${muted.toFixed(4)})`);
    const chainInk = () => { const c = new Set(); for (let y = 0; y < 12; y++) for (let x = 0; x < 12; x++) c.add(px(4 + 3 * 6 + x, 32 + y)); return c; };
    T.press('right');                            // the cursor off the muted cell: its grey shows
    ok(chainInk().has(LABEL) && !chainInk().has(TEXT), "the muted channel's chain is drawn in the label grey");
    T.shot('12-mute-song'); T.press('left');   // (mockups song-mute-solo, chain-muted)
    T.hold('option'); T.press('play'); T.free('option');   // solo channel 1 (muted): its mute goes, it plays
    const solo1 = rec(600);
    ok(T.strip(24) === 'solo CH1' && mark(0) === 'S' && inked(0, TEXT) && solo1 > loud / 3, `soloing a muted channel removes its mute (${T.strip(24)}, rms ${solo1.toFixed(3)})`);
    T.press('right');                            // channel 2: solo it too -> channel 1 still soloed; unsolo 1 first
    T.press('left'); T.hold('option'); T.press('play'); T.free('option');   // unsolo 1
    const kept = (T.go(2500), rec(500));
    ok(mark(0) === ' ' && kept > loud / 3, `unsoloed: heard, the solo took the mute away (unit feedback #17; rms ${kept.toFixed(4)})`);
    T.press('right'); T.hold('option'); T.press('play'); T.free('option');  // solo 2: channel 1 soloed out
    const solo2 = (T.go(2500), rec(500));
    ok(T.strip(24) === 'solo CH2' && mark(1) === 'S' && solo2 < loud / 20, `solo CH2: channel 1 silent (rms ${solo2.toFixed(4)})`);
    T.shot('12-solo-song');
    T.press('left');                             // channel 1's chain and phrase while CH2 is soloed: grey
    T.with_('shift', 'right'); T.shot('12-solo-chain');
    T.with_('shift', 'right'); T.shot('12-solo-phrase');
    T.with_('shift', 'left', 2); T.press('right');   // back: the song, channel 2
    T.hold('option'); T.hold('shift'); T.press('play'); T.free('shift'); T.free('option');
    const back = rec(600);
    ok(T.strip(24) === 'mutes and solos cleared' && mark(1) === ' ' && back > loud / 3, `OPTION + SHIFT + PLAY: ${T.strip(24)} (rms ${back.toFixed(3)})`);
    T.press('play');
}

// ---- rows and selections (docs/ux/row-ops.md, HANDOFF §0 "Editing")
{
    const T = rig(), note = r => T.cell(2, r, 3);
    T.note(0); T.note(1); T.note(2);             // rows 0, 1, 2: C-3
    T.to(0, 0); T.hold('edit'); T.press('up'); T.free('edit');   // row 0: C-4
    T.to(1, 0);
    T.press('left');                             // the row number of row 1
    T.press('edit'); T.press('edit');            // EDIT EDIT: an empty row here
    ok(T.strip(24) === 'row inserted' && note(1) === '---' && note(2) === 'C-3' && note(3) === 'C-3', `EDIT EDIT on a row number: ${T.strip(24)} (${note(1)} ${note(2)} ${note(3)})`);
    T.hold('option'); T.press('edit'); T.free('option');
    ok(T.strip(24) === 'row deleted' && note(1) === 'C-3' && note(3) === '---', `OPTION + EDIT on a row number: ${T.strip(24)}`);
    T.press('up');                               // row 0's number: EDIT + down moves the row
    T.hold('edit'); T.press('down'); T.free('edit');
    ok(note(0) === 'C-3' && note(1) === 'C-4', `EDIT + down on a row number: the row moves (${note(0)} ${note(1)})`);
    T.press('right');                            // back on the note column, row 1
    // a selection: SHIFT + OPTION, down, OPTION copies; SHIFT + EDIT pastes at row 8
    T.to(0, 0);
    T.hold('shift'); T.press('option'); T.free('shift');
    T.press('down');
    T.press('option');
    ok(T.strip(24) === 'copied 2x1', `SHIFT + OPTION, down, OPTION: ${T.strip(24)}`);
    T.press('option');                           // a plain OPTION drops the mark
    ok(T.strip(24) === 'deselected', `a plain OPTION: ${T.strip(24)}`);
    T.with_('shift', 'option'); T.press('down'); T.with_('shift', 'option'); T.press('option');   // unit-feedback #27
    ok(!T.strip(24).startsWith('copied'), `SHIFT + OPTION again: no selection, quietly (OPTION copies nothing: ${T.strip(24)})`);
    T.to(8, 0);
    T.hold('shift'); T.press('edit'); T.free('shift');
    ok(T.strip(24) === 'pasted' && note(8) === 'C-3' && note(9) === 'C-4', `SHIFT + EDIT: ${T.strip(24)} (${note(8)} ${note(9)})`);
    // EDIT + up on a block: every note an octave up; OPTION + EDIT deletes the block
    T.to(8, 0); T.hold('shift'); T.press('option'); T.free('shift'); T.press('down');
    T.hold('edit'); T.press('up'); T.free('edit');
    ok(note(8) === 'C-4' && note(9) === 'C-5', `EDIT + up on a block: all of it (${note(8)} ${note(9)})`);
    T.hold('option'); T.press('edit'); T.free('option');
    ok(T.strip(24) === 'deleted' && note(8) === '---' && note(9) === '---', `OPTION + EDIT on a block: ${T.strip(24)}`);
    // the column label (up from row 0): OPTION + EDIT clears the column
    T.to(0, 0); T.press('up');
    T.hold('option'); T.press('edit'); T.free('option');
    ok(T.strip(24) === 'column cleared' && note(0) === '---' && note(1) === '---', `OPTION + EDIT on the N label: ${T.strip(24)}`);
    T.shot('11-column');
    // the song: EDIT EDIT on row 00's number pushes the song down
    T.press('down');
    T.with_('shift', 'left', 2);                 // P -> C -> S
    T.press('left');
    T.press('edit'); T.press('edit');
    ok(T.strip(24) === 'row inserted' && T.cell(3, 0, 2) === '--' && T.cell(3, 1, 2) === '00', `the song: a row inserted (${T.cell(3, 0, 2)} ${T.cell(3, 1, 2)})`);
}

// a block changed while EDIT stays held: one undo step, however slow (picks review #23; single cells keep 1.5 s)
{
    const T = rig(), note = r => T.cell(2, r, 3);
    T.note(0); T.note(1); T.to(0, 0);
    T.hold('shift'); T.press('option'); T.free('shift'); T.press('down');   // rows 0-1 selected
    T.hold('edit'); T.press('up'); T.go(2000); T.press('up'); T.free('edit');   // 2 s apart, EDIT held throughout
    T.panel(0);                                  // panel EDIT + OCT-: undo
    ok(note(0) === 'C-3' && note(1) === 'C-3', `a selection changed twice 2 s apart, EDIT held: one undo step (${T.strip(36)}; ${note(0)} ${note(1)})`);
}
{
    const T = rig(), note = r => T.cell(2, r, 3);
    T.note(0); T.note(1); T.to(0, 0); T.press('left');   // the row numbers: rows 0-1 moved down twice
    T.hold('shift'); T.press('option'); T.free('shift'); T.press('down');
    T.hold('edit'); T.press('down'); T.go(2000); T.press('down'); T.free('edit');
    const moved = [0, 1, 2, 3].map(note).join();
    T.panel(0);
    ok(moved === '---,---,C-3,C-3' && note(0) === 'C-3' && note(1) === 'C-3' && note(2) === '---', `a selection's rows moved twice, EDIT held: one undo step (${moved} -> ${[0, 1, 2, 3].map(note).join()})`);
}

// ---- several columns selected (unit-feedback #25, user 2026-10-12; proto-test "several columns selected"): no value
// changes; EDIT + up/down move the cells, the row landed on goes to the other end; the other columns stay
{
    const T = rig(), row = r => T.cell(2, r, 3) + T.cell(6, r, 2), ins = r => T.cell(9, r, 1);
    T.note(3); T.note(4); T.note(5); T.note(6);
    T.to(4, 0); T.with_('edit', 'right'); T.to(6, 0); T.with_('edit', 'up');
    T.to(7, 2); T.press('edit');                 // row 7: only an instrument (stays put)
    const was = [3, 4, 5, 6, 7].map(row), i7 = ins(7);
    T.to(3, 0); T.with_('shift', 'option'); T.press('down', 3); T.press('right');   // rows 3-6, N V
    T.with_('edit', 'right');
    ok([3, 4, 5, 6, 7].map(row).join() === was.join(), `two columns: EDIT + right changes nothing (${[3, 4, 5, 6, 7].map(row).join()})`);
    T.with_('edit', 'down');
    const now = [3, 4, 5, 6, 7].map(row);
    ok(now.join() === [was[4], ...was.slice(0, 4)].join() && ins(7) === i7, `EDIT + down: the cells a row down, row 7's (empty) to 3, I stays (${now.join()}; I ${ins(7)})`);
    T.with_('edit', 'up');
    ok([3, 4, 5, 6, 7].map(row).join() === was.join(), `EDIT + up: back (${[3, 4, 5, 6, 7].map(row).join()})`);
    T.press('option');                           // (copied, the mark dropped below)
    T.press('option');
    T.fx(9, 0, 'VOL', 0x20); T.fx(10, 0, 'KIL', 2);
    T.to(9, 3); T.with_('shift', 'option'); T.press('down'); T.with_('edit', 'right'); T.with_('edit', 'up');
    const cmds = T.cell(11, 9, 3) + ' ' + T.cell(11, 10, 3);
    ok(cmds === 'VOL KIL', `one column of FX command names selected: EDIT + arrows change nothing (${cmds})`);
}

// ---- a paste keeps the mark (unit-feedback #22, user 2026-10-12; proto-test "a paste keeps the mark"): it goes when a
// copied cell changes (an overlapping paste, an edit, undo); a paste onto itself is a no-op; the mark is its phrase's
{
    const T = rig(), note = r => T.cell(2, r, 3), SEL = rgb565('#16324a');
    const marked = r => T.m.fb()[(rowY(r) + 6) * 240 + cellX(PH_X[0]) - 1] === SEL;
    const copy = (r0, r1) => { T.to(r0, 0); T.with_('shift', 'option'); T.press('down', r1 - r0); T.press('option'); };
    const paste = r => { T.to(r, 0); T.go(2000); T.with_('shift', 'edit'); };
    T.with_('shift', 'left'); T.press('down'); T.press('edit'); T.press('up'); T.with_('shift', 'right');   // chain row 1: phrase 01
    T.note(0); T.note(1); T.note(4); T.note(5);
    T.to(1, 0); T.with_('edit', 'right'); T.to(5, 0); T.with_('edit', 'right');   // rows 0 1 4 5: C-3 C#3 C-3 C#3
    copy(0, 1);
    paste(8);
    ok(T.strip(24) === 'pasted' && note(8) === 'C-3' && note(9) === 'C#3' && marked(0) && marked(1), `pasted at 8, the mark stays (${note(8)} ${note(9)}; ${marked(0)})`);
    T.press('edit');                             // EDIT on the cursor's cell: row A, empty, gets a note (#21)
    ok(note(10) !== '---' && note(8) === 'C-3', `the cursor goes to the row after the block, A (#21: ${note(10)})`);
    T.with_('option', 'edit');
    paste(12); paste(0);
    ok(T.strip(24) === 'already there' && note(12) === 'C-3' && marked(1), `pasted again at C; onto itself: ${T.strip(24)}, marked ${marked(1)}`);
    T.panel(0);                                  // undo: the paste at C, not the no-op
    ok(note(12) === '---' && note(8) === 'C-3' && marked(1), `undo: the last real paste, the mark stays (${note(12)} ${note(8)}; ${marked(1)})`);
    T.with_('option', 'down'); T.go(50);
    const there = marked(1); T.to(0, 0); T.press('edit'); T.go(50);
    T.with_('option', 'up'); T.go(50);
    ok(!there && marked(1), `the mark is phrase 00's: not drawn on 01 (${there}), an edit there keeps it (${marked(1)})`);
    paste(1);
    ok(!marked(0) && !marked(1), `a paste partly over it drops it (${marked(0)} ${marked(1)})`);
    paste(15);
    ok(T.m.fb()[rowY(15) * 240 + cellX(PH_X[0]) - 1] === ACCENT, `pasted on the last row: the cursor stays on F (#21)`);
    copy(4, 5); T.to(4, 0); T.with_('edit', 'right'); T.go(50);
    ok(!marked(5), `an edit of a copied cell drops it (${marked(5)})`);
    T.to(5, 0); T.go(2000); T.with_('edit', 'right'); copy(4, 5); T.panel(0); T.go(50);
    ok(!marked(4) && note(5) === 'C#3', `an undo changing a copied cell drops it (${marked(4)} ${note(5)})`);
}

// ---- clone / interpolate (docs/ux/interpolate.md): SHIFT + EDIT on a selection
{
    const T = rig(), note = r => T.cell(2, r, 3);
    const sel = () => { T.hold('shift'); T.press('option'); T.free('shift'); };
    const notes = rs => rs.map(note).join();
    T.note(0); T.note(4);                        // C-3 on rows 0 and 4, then row 4 an octave up
    T.to(4, 0); T.hold('edit'); T.press('up'); T.free('edit');
    T.to(0, 0); sel(); T.to(4, 0); T.shot('13-interp-select');   // (mockup phrase-select)
    T.with_('shift', 'edit');
    T.shot('13-interp-notes');
    ok(T.strip(34) === 'interpolated 5 rows' && notes([0, 1, 2, 3, 4]) === 'C-3,D#3,F#3,A-3,C-4', `interpolate the notes: ${T.strip(34)} (${notes([0, 1, 2, 3, 4])})`);
    T.hold('edit'); T.press('right'); T.free('edit');
    ok(notes([0, 1, 2, 3, 4]) === 'C#3,E-3,G-3,A#3,C#4', `the ramp stays selected (unit-feedback #28): EDIT + right shifts it (${notes([0, 1, 2, 3, 4])})`);
    T.press('option');
    ok(T.strip(34) === 'copied 5x1', `still a selection: OPTION copies it (${T.strip(34)})`);
    T.press('option');                           // drop the mark
    T.to(4, 0); sel(); T.to(6, 0); T.with_('shift', 'edit');
    T.shot('13-interp-empty');
    ok(T.strip(34) === 'first and last row need a value' && note(5) === '---', `an empty last row: ${T.strip(34)}`);
    T.press('option', 2);                        // (copies the selection, then drops the mark)
    // FX values: VOL 00 on row 0, VOL 30 on row 3; rows 1 and 2 get the command and 10, 20
    T.fx(0, 0, 'VOL', 0); T.fx(3, 0, 'VOL', 0x30);
    T.to(0, 4); sel(); T.to(3, 4); T.with_('shift', 'edit');
    const fx = r => T.cell(11, r, 3) + ' ' + T.cell(15, r, 2);
    ok([1, 2].map(fx).join() === 'VOL 10,VOL 20', `interpolate an FX value column: ${T.strip(34)} (${[1, 2].map(fx).join()})`);
    T.shot('13-interp-fx');
    T.with_('shift', 'edit');                    // again right away: a ramp
    T.shot('13-rampify');                        // (mockup phrase-ramp)
    ok(T.strip(34) === 'ramp VOL 00>30 in 3 rows' && [0, 1, 2, 3].map(fx).join() === 'VOL 00,RTO 30,RTI 03,--- --',
        `SHIFT + EDIT again: a ramp: ${T.strip(34)} (${[0, 1, 2, 3].map(fx).join()})`);
    T.to(0, 4); sel(); T.to(1, 4); T.with_('shift', 'edit', 2);
    ok(T.strip(34) === 'ramp needs 3 rows' && fx(1) === 'RTO 30', `2 rows: no ramp (${T.strip(34)})`);
    T.press('option', 2);
    // the chain: SHIFT + EDIT on a selected phrase = a copy with the next free number
    T.with_('shift', 'left');
    sel(); T.with_('shift', 'edit');
    T.shot('13-clone-chain');
    ok(T.strip(34) === 'cloned as 01' && T.cell(2, 0, 2) === '01', `clone a phrase: ${T.strip(34)} (${T.cell(2, 0, 2)})`);
    T.with_('shift', 'right');
    ok(note(0) === 'C#3' && note(4) === 'C#4', `the clone has the phrase's notes (${note(0)} ${note(4)})`);
    // the song: SHIFT + EDIT twice = clone the chain, then deep clone (its phrase copied too)
    T.with_('shift', 'left', 2);
    sel(); T.with_('shift', 'edit', 2);
    T.shot('13-deep-clone-song');
    ok(T.strip(34) === 'deep cloned: 1 phrases' && T.cell(3, 0, 2) === '01', `deep clone a chain: ${T.strip(34)} (${T.cell(3, 0, 2)})`);
    T.with_('shift', 'right');
    ok(T.cell(2, 0, 2) === '02', `the new chain plays its own phrase copy (${T.cell(2, 0, 2)})`);
}

// ---- undo / redo (HANDOFF §0 "Other panel buttons"): panel EDIT + OCT- / OCT+
{
    const T = rig(), note = r => T.cell(2, r, 3);
    const white = (cx, line) => T.m.fb()[rowY(line) * 240 + cellX(cx) - 1] === rgb565('#e8e8e8');
    const UNDO = () => T.panel(0), REDO = () => T.panel(1);
    T.note(0); T.note(4);                        // two steps: a note (with its velocity and instrument) on rows 0 and 4
    T.hold('edit'); T.press('right', 3); T.free('edit');   // C-3 -> D#3: one step (one cell, within 1.5 s)
    T.to(6, 0);                                  // the cursor elsewhere: undo does not move it
    UNDO();
    T.shot('14-undo');                           // (the changed cell flashes white)
    ok(T.strip(34) === 'undo P00 4 D#3>C-3' && note(4) === 'C-3' && white(2, 4), `undo: three EDIT + right were one step (${T.strip(34)}, ${note(4)}, flash ${white(2, 4)})`);
    ok(T.m.fb()[rowY(6) * 240 + cellX(2) - 1] === ACCENT, 'undo: the cursor stays on row 6');
    UNDO();
    ok(T.strip(34) === 'undo 3 changes P00' && note(4) === '---', `undo the note: ${T.strip(34)} (${note(4)})`);
    REDO();
    ok(T.strip(34) === 'redo 3 changes P00' && note(4) === 'C-3', `redo: ${T.strip(34)} (${note(4)})`);
    T.go(1300);
    ok(!white(2, 4), 'the flash is over after 1.2 s');
    T.to(0, 0); T.hold('edit'); T.press('up'); T.free('edit');   // a new edit: nothing left to redo
    REDO();
    ok(T.strip(34) === 'nothing to redo' && note(0) === 'C-4', `a new edit drops the redo: ${T.strip(34)}`);
    T.press('edit');                             // EDIT on a filled note copies it: no change, no step
    UNDO();
    ok(note(0) === 'C-3', `an edit that changes nothing is no step: the undo takes back C-4 (${note(0)})`);
    UNDO(); UNDO(); UNDO();
    ok(note(0) === '---' && T.strip(34) === 'undo C00:0 00>--', `undo back past the phrase's creation: ${T.strip(34)}`);
    UNDO();                                      // 4 steps kept (user): the 5th, the song's chain, has dropped out
    ok(T.strip(34) === 'nothing to undo', `undo keeps 4 steps: ${T.strip(34)}`);
    // the mixer and the instruments are undone too
    T.hold('option'); T.press('shift'); T.free('option');   // mute CH1
    T.press('down');
    UNDO();
    ok(T.strip(34) === 'undo MIXER', `undo a mute: ${T.strip(34)}`);
    T.with_('shift', 'right');                   // I
    T.press('down');                             // LEVEL
    const level = () => readText(T.m.fb(), 4, IROW_Y(1) + 14, 8).trim();
    const before = level();
    T.hold('edit'); T.press('left', 4); T.free('edit');
    const after = level();
    UNDO();
    ok(T.strip(34).startsWith('undo I0 ') && level() === before && after !== before, `undo an instrument edit: ${T.strip(34)} (${after} -> ${level()})`);
}

// ---- the undo log as XOR runs (docs/ux/undo-log.md; the log is 4 KB: OPTIMIZE M8): the oldest steps drop
// out by size, redo gives back the same bytes, a step bigger than the log is kept and says so
{
    const big = () => {                          // chain 00: 16 full phrases 00..0F (every byte of every row set);
        const T = rig();                         // the song: 00, 01 (its deep clone, 10..1F), 00, 01; the cursor on S row 0
        const sel = () => { T.hold('shift'); T.press('option'); T.free('shift'); };
        for (let r = 0; r < 16; r++) T.note(r);
        [['VOL', 0x40], ['CUT', 0x20], ['KIL', 0x02]].forEach(([c, v], k) => {
            T.fx(0, k, c, v);
            for (let r = 1; r < 16; r++) { T.to(r, 3 + 2 * k); T.press('edit'); }   // (the last command and value)
        });
        T.with_('shift', 'left');                // C: phrase 00 on every row (EDIT on 00 copies it), then cloned
        T.press('edit');
        for (let r = 1; r < 16; r++) { T.press('down'); T.press('edit'); }
        T.press('up', 14); sel(); T.press('down', 14); T.with_('shift', 'edit');
        T.press('option', 2);
        T.with_('shift', 'left');                // S
        T.press('edit'); T.press('down'); T.press('edit');   // row 1: 00, then deep cloned (01: 10..1F)
        sel(); T.with_('shift', 'edit', 2); T.press('option', 2);
        T.press('up'); T.press('edit'); T.press('down', 2); T.press('edit');   // row 2: 00
        T.press('up'); T.press('edit'); T.press('down', 2); T.press('edit');   // row 3: 01
        T.press('up', 3);
        return { T, sel, rows: () => [0, 1, 2, 3].map(r => T.cell(3, r, 2)).join() };
    };
    const UNDO = T => T.panel(0), REDO = T => T.panel(1);
    {
        const { T, sel, rows } = big();
        ok(rows() === '00,01,00,01', `undo log: the song set up (${rows()})`);
        sel(); T.with_('shift', 'edit', 2); T.press('option', 2);   // row 0: 16 phrases (2.3 KB)
        const s1 = T.strip(34);
        T.press('down', 2); sel(); T.with_('shift', 'edit', 2); T.press('option', 2);   // row 2: 16 more
        const s2 = T.strip(34);
        ok(s1 === 'deep cloned: 16 phrases' && s2 === s1 && rows() === '02,01,03,01', `two 16-phrase deep clones (${s1}; ${s2}; ${rows()})`);
        UNDO(T);
        const u1 = T.strip(34);
        UNDO(T);
        const u2 = T.strip(34), r2 = rows();
        UNDO(T);
        ok(u1.startsWith('undo ') && u2.startsWith('undo ') && r2 === '02,01,00,01' && T.strip(34) === 'nothing to undo',
            `a 2.3 KB step undoes; the one before it dropped out by size, not by count (${u1}; ${u2}; ${r2}; ${T.strip(34)})`);
        REDO(T); REDO(T);
        ok(rows() === '02,01,03,01', `redo both (${rows()})`);
        T.with_('shift', 'right');   // chain 03: its phrases 30..3F
        const ph = [0, 15].map(r => T.cell(2, r, 2)).join();
        T.press('down', 15); T.with_('shift', 'right');   // phrase 3F, row 15: the redone bytes
        const row = [2, 6, 9, 11, 15, 18, 22, 25, 29].map(x => T.cell(x, 15, 3)).join('|');
        ok(ph === '30,3F' && row.endsWith('|64 |0 V|VOL|40 |CUT|20?|KIL|02 ') && !row.startsWith('---'),
            `redo writes the cloned bytes back exactly (${ph}; ${row})`);
    }
    {
        const { T, sel, rows } = big();
        sel(); T.press('down'); T.with_('shift', 'edit', 2);   // rows 0, 1: 32 distinct phrases, 4.6 KB
        const s = T.strip(38);
        T.press('option', 2);
        UNDO(T);
        ok(s === 'deep cloned: 32 phrases (no undo)' && T.strip(34) === 'nothing to undo' && rows() === '02,03,00,01',
            `a step bigger than the log is kept and says so (${s}; ${T.strip(34)}; ${rows()})`);
    }
}

// ---- names (app.js nameGet / nameSet / nameRelease): the instrument's NAME, letter by letter
{
    const T = rig();
    const name = () => readText(T.m.fb(), cellX(21), 4, 11).trimEnd();   // the top bar shows it (11 letters: no tag)
    T.with_('shift', 'right');                   // I0: ACID
    T.press('down'); T.press('right', 2);        // ENGINE -> LEVEL -> PAN -> NAME (beside them), letter 0
    ok(name() === 'ACID', `instrument 0 is named after its preset (${name()})`);
    T.press('right', 4);
    T.press('edit');                             // an empty letter: the last one (A), then on to the next
    ok(name() === 'ACIDA', `EDIT on an empty letter: ${name()}`);
    T.hold('edit'); T.press('right', 2); T.press('down');   // the grid: A -> C -> M (down = a row of 10)
    T.shot('15-name-grid');                      // (mockup inst-name)
    const hint = T.strip(36);
    ok(hint === 'name: <> letter, EDIT + arrows picks', `EDIT held on a letter: the hint (${hint})`);
    T.free('edit');
    ok(name() === 'ACIDAM', `EDIT held + arrows: the character grid (${name()})`);
    T.hold('edit'); T.press('left', 3); T.free('edit');   // M (column 2): K, then the row's start stops it
    ok(name() === 'ACIDAMK', `left stops at the grid row's start (${name()})`);
    T.press('left', 7);                          // letter 0 (A): EDIT copies it and stays
    T.press('edit'); T.press('right');
    T.hold('option'); T.press('edit'); T.free('option');   // OPTION + EDIT empties letter 1
    ok(name() === 'A IDAMK', `a tap on a filled letter stays; OPTION + EDIT empties a letter (${name()})`);
    T.press('up');                               // NAME -> PRESET (above it): a new preset keeps a changed name
    T.hold('edit'); T.press('right'); T.free('edit');
    ok(name() === 'A IDAMK', `a new preset keeps a changed name (${name()})`);
    T.press('left'); T.hold('edit'); T.press('right'); T.free('edit');   // a new engine: the preset's name again
    const n1 = name();
    T.press('right'); T.hold('edit'); T.press('right'); T.free('edit');   // ... and it follows the presets again
    ok(n1 !== 'A IDAMK' && n1 && name() !== n1, `a new engine names it after its preset (${n1}), the next preset too (${name()})`);
}

// ---- a note without an instrument (HANDOFF §0 "Playback"): moves the sounding note, no new attack; a dim ~ in its
// instrument cell; nothing sounding = nothing plays, its note and ~ dimmed while playing
{
    const ROW = 5512;                            // samples a row at 120 BPM (5512.5)
    const period = (a, from, n) => { let best = -1, bl = 0; for (let L = 150; L <= 400; L++) { let s = 0; for (let i = from; i < from + n; i++) s += a[i] * a[i + L]; if (s > best) { best = s; bl = L; } } return bl; };
    const two = legato => {                      // row 0 C-3 (instrument 0), row 1 E-3, with or without an instrument
        const T = rig();
        T.note(0); T.note(1);
        T.to(1, 0); T.press('right'); T.press('left');   // (the cursor through the note: no change)
        T.hold('edit'); T.press('right', 4); T.free('edit');   // row 1: E-3
        if (legato) { T.to(1, 2); T.hold('option'); T.press('edit'); T.free('option'); }   // ... no instrument
        return T;
    };
    const L = two(true), R = two(false);
    ok(L.cell(9, 1, 1) === '~', `a note without an instrument: a ~ in its instrument cell (${L.cell(9, 1, 1)})`);
    L.to(1, 0); L.shot('16-tie');                // (mockup phrase-legato)
    const la = L.playRecord(400), ra = R.playRecord(400);
    L.press('play'); R.press('play');
    const lo = onset(la), ro = onset(ra);
    const lAfter = rms(la, lo + ROW + 300, lo + ROW + 1300), rAfter = rms(ra, ro + ROW + 300, ro + ROW + 1300);
    const lBefore = rms(la, lo + ROW - 1000, lo + ROW);
    ok(lAfter < rAfter * 0.8 && lAfter < lBefore * 1.3, `legato: no new attack on row 1 (rms ${lBefore.toFixed(3)} -> ${lAfter.toFixed(3)}; with an instrument: ${rAfter.toFixed(3)})`);
    const p0 = period(la, lo + 1500, 1500), p1 = period(la, lo + ROW + 1500, 1500);
    ok(Math.abs(p0 - 337) < 12 && Math.abs(p1 - 268) < 12, `legato: the pitch moves C-3 -> E-3 (periods ${p0}, ${p1} samples)`);
    // a cold start: nothing sounding, so nothing plays; the note and its ~ dimmed while playing
    const C = rig();
    C.note(0);
    C.to(0, 2); C.hold('option'); C.press('edit'); C.free('option');
    C.to(4, 0); C.go(1500);                      // (the cursor off the row; the edits' auditions die away)
    const ink = (cx, line, w, col) => { let n = 0; for (let y = rowY(line); y < rowY(line) + 12; y++) for (let x = cellX(cx); x < cellX(cx) + 6 * w; x++) if (C.m.fb()[y * 240 + x] === col) n++; return n; };
    const DIM = rgb565('#555555'), TXT = rgb565('#e8e8e8');
    const lit = ink(2, 0, 3, TXT);
    const cold = C.playRecord(500);
    const dimmed = ink(2, 0, 3, DIM) > 0 && ink(2, 0, 3, TXT) === 0;
    C.shot('16-tie-unheard');   // (mockup phrase-legato, row C)
    C.press('play'); C.go(100);
    ok(rms(cold) < 0.001 && lit > 0 && dimmed, `a cold start on it: silent (rms ${rms(cold).toFixed(4)}), its note dimmed while playing (${dimmed})`);
    ok(ink(2, 0, 3, TXT) > 0, 'stopped: the note is lit again');
}

// ---- editing defaults and copy buffers (docs/ux/edit-defaults.md; app.js last, insertLast): velocity 64, a copied
// note keeps its instrument cell (a ~ too), one last value per column type, FX1-3 share one, a sound command's 00 is empty
{
    const T = rig(), c = T.cell, row = r => [c(2, r, 3), c(6, r, 2), c(9, r, 1)].join(' '), fx = (r, k) => c([11, 18, 25][k], r, 3) + ' ' + c([15, 22, 29][k], r, 2);
    T.note(0);
    ok(row(0) === 'C-3 64 0', `a new note: velocity 64, the last instrument (${row(0)})`);
    T.to(0, 1); T.with_('edit', 'down');
    T.note(1);
    ok(c(6, 0, 2) === '54' && c(6, 1, 2) === '54', `the next new note gets the edited velocity (${c(6, 0, 2)} ${c(6, 1, 2)})`);
    T.with_('edit', 'right');
    ok(row(1) === 'C#3 54 0', `editing a note keeps its velocity (${row(1)})`);
    T.to(1, 2); T.with_('option', 'edit');       // row 1: a ~ note
    T.to(1, 0); T.press('edit');                 // copies it
    ok(c(9, 1, 1) === '~', `copying a ~ note leaves it one (${c(9, 1, 1)})`);
    T.to(2, 0); T.press('edit');
    ok(row(2) === 'C#3 54 ~', `... it pastes as a ~ note (${row(2)})`);
    T.with_('edit', 'right');
    ok(row(2) === 'D-3 54 ~', `EDIT + arrows on a ~ note: it stays one (${row(2)})`);
    T.to(3, 2); T.press('edit'); T.with_('edit', 'right');   // row 3: instrument 4, now the last one
    T.to(0, 0); T.press('edit'); T.to(4, 0); T.press('edit');
    ok(row(4) === 'C-3 54 0' && c(9, 3, 1) === '4', `a copied note pastes with its own instrument, not the last (${row(4)})`);
    T.to(5, 0); T.press('edit');
    ok(row(5) === 'C-3 54 0', `... and the next paste too (${row(5)})`);
    T.to(6, 0); T.with_('option', 'edit');       // row 6: OFF; a key written on it gets the last velocity (user)
    T.btn(13); T.m.input(1 << 7, 0); T.go(25); T.m.input(0, 0); T.go(25); T.btn(13); T.btn(13);
    ok(row(6) === 'C-4 54 4', `step entry on an OFF row: the last velocity and instrument (${row(6)})`);
    // FX: the last command with the last value, one buffer for the three columns, not the velocity's
    T.fx(0, 0, 'TV1', 0);
    ok(fx(0, 0) === 'TV1 00', `a command picked in the grid on an empty cell: TV1 00 (${fx(0, 0)})`);
    T.to(0, 4); T.with_('edit', 'right');
    T.to(1, 5); T.press('edit');
    ok(fx(1, 1) === 'TV1 01', `FX2 pastes the last command and value (${fx(1, 1)})`);
    T.to(1, 1); T.with_('edit', 'up');
    T.to(2, 8); T.press('edit');
    ok(fx(2, 2) === 'TV1 01' && c(6, 1, 2) === '64', `FX3's value: the FX buffer, not the velocity (${fx(2, 2)})`);
    T.to(2, 7); T.hold('edit'); T.press('right'); T.free('edit');   // TV1 -> MUT (MXR's line)
    ok(c(25, 2, 3) !== 'TV1' && c(29, 2, 2) === '00', `another player command picked: 00 (${fx(2, 2)})`);
    T.fx(3, 0, 'HOP', 0); T.fx(4, 0, 'CUT', 0x20);
    T.to(3, 4); T.press('edit'); T.to(5, 4); T.press('edit');
    ok(fx(3, 0) === 'HOP 00' && c(15, 5, 2) === '00', `EDIT on HOP 00 copies it (${fx(3, 0)}, pasted ${fx(5, 0)})`);
    T.to(0, 4); T.with_('edit', 'up');           // VOL 11: the last value
    T.to(4, 4); T.with_('option', 'edit');
    ok(fx(4, 0) === 'CUT 00', `OPTION + EDIT on a value: 00 (${fx(4, 0)})`);
    T.to(1, 8); T.press('edit');                 // (row 1 plays instrument 0, which has CUT)
    ok(fx(1, 2) === 'CUT 00', `... stored as the last value (${fx(1, 2)})`);
    T.to(0, 4); T.with_('edit', 'up'); T.to(4, 4); T.press('edit');
    ok(fx(4, 0) === 'CUT 21', `EDIT on a sound command's 00 pastes the last value (${fx(4, 0)})`);
    T.to(7, 0); T.shot('25-edit-defaults');
    // an OFF row takes no velocity or instrument; a paste fills them only with a copied note (user, 2026-10-10)
    for (const r of [8, 9, 10]) { T.to(r, 0); T.with_('option', 'edit'); }
    T.to(8, 1); T.press('edit'); T.with_('edit', 'right'); T.to(8, 2); T.press('edit'); T.with_('edit', 'right');
    ok(row(8) === 'OFF -- -', `EDIT and EDIT + arrows write nothing in an OFF row's V or I (${row(8)})`);
    const copy = (c0, c1) => { T.to(0, c0); T.hold('shift'); T.press('option'); T.free('shift'); T.press('right', c1 - c0); T.press('option'); };
    copy(1, 2); T.to(9, 1); T.with_('shift', 'edit');
    ok(row(9) === 'OFF -- -', `a paste of V and I alone leaves an OFF row as it is (${row(9)})`);
    copy(0, 2); T.to(10, 0); T.with_('shift', 'edit');
    ok(row(10) === 'C-3 54 0', `with a copied note it pastes, V and I too (${row(10)})`);
    T.to(11, 0); T.shot('25-edit-off');
}

// ---- the popups while EDIT is held (app.js popupState, render.js popup / keyboard)
{
    const T = rig(), px = (x, y) => T.m.fb()[y * 240 + x];
    T.note(0);                                   // C-3 on row 0: the keyboard opens in the lower half
    T.hold('edit');
    const kTop = rowY(9);                        // white keys: x = 16 + 12 k, 46 high; black: 8 wide, 28 high
    ok(px(20, kTop + 40) === ACCENT && px(32, kTop + 40) === rgb565('#e8e8e8'), 'EDIT held on a note: the keyboard, C-3 lit');
    T.shot('17-keys');
    ok(readText(T.m.fb(), 16, kTop + 48, 3) === 'C-3' && readText(T.m.fb(), 16 + 7 * 12, kTop + 48, 3) === 'C-4',
        `the octaves labelled as the note cells (principles-review P2): ${readText(T.m.fb(), 16, kTop + 48, 3)} ${readText(T.m.fb(), 16 + 7 * 12, kTop + 48, 3)}`);
    T.press('right');                            // C#3: the first black key
    ok(px(27, kTop + 10) === ACCENT && px(20, kTop + 40) !== ACCENT, 'EDIT + right: C#3 lit on the black key');
    T.press('up', 2);                            // C#5: past the two octaves, the keyboard scrolls one octave
    ok(px(16 + 7 * 12 + 11, kTop + 10) === ACCENT, 'EDIT + up twice: C#5, the keyboard moved an octave up');
    T.free('edit');
    // the instrument column: the list of the 16, up / down one, left / right four
    T.to(0, 2); T.hold('edit');
    const item = n => readText(T.m.fb(), cellX(20), rowY(n), 14).trimEnd();
    ok(item(1) === '1 PAD' && px(cellX(20) - 3, rowY(0) - 2) === ACCENT, `EDIT held on the instrument: the list (${item(1)})`);
    T.shot('17-inst-list');
    {   // the instruments the song never plays dim (principles-review P1): only 0 plays here, and it is the highlighted one
        const inks = n => { const c = new Set(); for (let y = rowY(n); y < rowY(n) + 12; y++) for (let x = cellX(20); x < cellX(34); x++) c.add(px(x, y)); return c; };
        ok(inks(1).has(rgb565('#555555')) && !inks(1).has(rgb565('#e8e8e8')), 'the instrument list: 1 PAD, never played, in the dim grey');
        T.free('edit'); T.to(4, 0); T.press('edit'); T.to(4, 2); T.hold('edit'); T.press('down'); T.free('edit');   // row 4: a note of instrument 1
        T.to(0, 2); T.hold('edit');
        ok(inks(1).has(rgb565('#e8e8e8')) && inks(2).has(rgb565('#555555')), 'a note of instrument 1 in the song: 1 PAD in the text colour, 2 still dim');
    }
    T.press('down'); T.press('right');           // 0 -> 1 -> 5
    T.free('edit');
    ok(T.cell(9, 0, 1) === '5', `in the list: down one, right four (${T.cell(9, 0, 1)})`);
    ok(![...Array(240).keys()].some(x => px(x, 224) === ACCENT), 'the 16-item list closes: no frame left on the strip (y 224)');
    ok(![...Array(240).keys()].some(x => px(x, rowY(0) - 2) === ACCENT || px(x, rowY(0) - 1) === ACCENT), 'the list closes: no frame left above row 0');
    // the instrument page: ENGINE's list, its line in the bottom strip; a named list (ANALOG's WAVE)
    T.with_('shift', 'right');                   // I5, the cursor on ENGINE
    T.hold('edit');
    const help0 = T.strip();
    ok(help0 === '2 oscillators, noise, resonant LP', `EDIT held on ENGINE: the list, the engine's line (${help0})`);
    T.shot('17-engine-list');
    T.press('down');
    ok(T.strip() !== help0 && T.strip().length > 0, `down: the next engine and its line (${T.strip()})`);
    T.press('up');
    T.free('edit');
    T.press('down', 2);                          // LEVEL, E0 (WAVE)
    T.hold('edit');
    ok(px(cellX(0) - 3, IROW_Y(2) + 2 + 2 * 12 + 1) === ACCENT, 'EDIT held on WAVE: its list, right under the cell (unit feedback #13)');
    T.shot('17-wave-list');
    T.free('edit');
    ok(![...Array(240).keys()].some(x => px(x, rowY(0) - 2) === ACCENT || px(x, rowY(0) - 1) === ACCENT), 'the WAVE list closes: no frame left above row 0');
}

// ---- the map's panel (user 2026-10-11, principles-review.md #2; render.js navMap): x 209..239, y 30..92 in surf on
// every screen, so its letters don't read as more preview columns (mockups song, chain, q)
{
    const T = rig(), px = (x, y) => T.m.fb()[y * 240 + x], SURF = rgb565('#1a1f2a'), seen = [];
    const panel = () => [[209, 30], [239, 30], [209, 92], [239, 92], [225, 61]].every(([x, y]) => px(x, y) === SURF)
        && px(208, 60) !== SURF && px(225, 93) !== SURF && px(225, 29) !== SURF;
    const look = (L, a) => { if (panel() && mapLit(px, L, a)) seen.push(L); };
    look('P');
    T.with_('shift', 'up'); look('G', 2); T.with_('shift', 'down');
    T.with_('shift', 'down'); look('F', 2); T.with_('shift', 'up');
    T.with_('shift', 'left'); look('C');
    T.with_('shift', 'left'); look('S');
    T.with_('shift', 'right'); T.with_('shift', 'right'); T.with_('shift', 'right'); look('I');
    T.with_('shift', 'down'); look('E');
    T.with_('shift', 'up'); T.with_('shift', 'up'); look('Q');
    T.with_('shift', 'left'); T.with_('shift', 'left'); T.with_('shift', 'left'); T.with_('shift', 'left');   // Q -> I -> P -> C -> S
    ok(readText(T.m.fb(), 210, 96, 2) === '00' && readText(T.m.fb(), 210, 214, 2) === '3F', `the song map's 00 and 3F on S (${readText(T.m.fb(), 210, 96, 2)} ${readText(T.m.fb(), 210, 214, 2)})`);
    ok(seen.join('') === 'PGFCSIEQ', `the map on its surf panel on P G F C S I E Q (${seen.join('')})`);
}

// ---- the 2D map (HANDOFF section 5, app.js mapMove, render.js navMap): every move the firmware has screens for
{
    const T = rig(), px = (x, y) => T.m.fb()[y * 240 + x];
    const at = (L, a) => mapLit(px, L, a);
    T.note(0);                                   // on P
    T.with_('shift', 'up');
    ok(at('G', 2) && !at('P'), 'SHIFT + up from P: G, drawn over P');
    T.with_('shift', 'up');
    ok(at('G', 2), 'SHIFT + up on G: nothing');
    T.with_('shift', 'right');
    ok(at('I'), 'G over P: SHIFT + right = I');
    T.with_('shift', 'left', 2);                 // I -> P -> C
    T.with_('shift', 'up');
    ok(at('G', 1), 'SHIFT + up from C: G, drawn over C');
    T.shot('26-map-g');
    T.with_('shift', 'down');
    ok(at('C'), 'SHIFT + down from G: back to C');
    T.with_('shift', 'up'); T.with_('shift', 'right');
    ok(at('P'), 'G over C: SHIFT + right = P');
    T.with_('shift', 'left', 2); T.with_('shift', 'up');   // S, G over S
    T.with_('shift', 'left');
    ok(at('G', 0), 'G over S: SHIFT + left = nothing');
    T.with_('shift', 'right');
    ok(at('C'), 'G over S: SHIFT + right = C');
    T.with_('shift', 'left'); T.with_('shift', 'left');
    ok(at('S'), 'SHIFT + left on S: nothing');
    T.with_('shift', 'down');
    ok(at('F', 0), 'SHIFT + down from S: F, drawn under S');
    T.with_('shift', 'down');
    ok(at('X', 0), 'SHIFT + down from F: X');
    T.with_('shift', 'down');
    ok(at('X', 0), 'SHIFT + down on X: nothing');
    T.with_('shift', 'up');
    ok(at('F', 0), 'SHIFT + up from X: F');
    T.with_('shift', 'right');
    ok(at('C'), 'F under S: SHIFT + right = C');
    T.with_('shift', 'down'); T.with_('shift', 'up');
    ok(at('C'), 'F under C: SHIFT + up = back to C');
    T.with_('shift', 'down', 2); T.with_('shift', 'left');
    ok(at('S'), 'X under C: SHIFT + left = S');
    T.with_('shift', 'right', 3); T.with_('shift', 'right');
    ok(at('I'), 'SHIFT + right on I: nothing');
    T.with_('shift', 'up');
    ok(at('Q') && T.strip() === 'bar 1 S00 CH1', `SHIFT + up from I: Q (${T.strip()})`);
    T.with_('shift', 'down'); ok(at('I'), 'SHIFT + down from Q: I');
    T.with_('shift', 'down'); T.btn(7);   // E, then GLO
    ok(at('G', 2), 'GLO from E: G, drawn over P');
    T.with_('shift', 'down');
    ok(at('P'), 'SHIFT + down from that G: the phrase');
}

// ---- the E / L / V pages (render.js ENV_FIELDS / LFO_FIELDS / VOX_FIELDS, PAGES, adsrGraph, lfoGraph)
{
    const T = rig(), px = (x, y) => T.m.fb()[y * 240 + x];
    const mapAt = L => mapLit(px, L);
    const region = (x, y, w, h) => { const out = []; for (let j = y; j < y + h; j++) for (let i = x; i < x + w; i++) out.push(px(i, j)); return out.join(); };
    const graph = () => region(4, 36, 200, 64);
    // one name per parameter (user 2026-10-11, principles-review.md #3): the labels are the command names
    const names = (y, xs) => xs.map(x => readText(T.m.fb(), 4 + x * 6, y, 3)).join(' ');
    // principles-review.md #14 (user 2026-10-11): the KNOBS rows in a frame titled KNOBS on I E L V (label line y 190)
    const knobsFrame = () => readText(T.m.fb(), 14, KNOBS_Y - 13, 5) === 'KNOBS' && px(2, KNOBS_Y - 8) === px(2, KNOBS_Y + 20) && px(2, KNOBS_Y - 8) !== px(1, KNOBS_Y - 8);
    T.note(0);
    T.with_('shift', 'right');
    ok(mapAt('I') && knobsFrame(), 'I: the KNOBS frame, titled KNOBS');
    T.with_('shift', 'down');                    // P -> I -> E
    ok(knobsFrame(), 'E: the KNOBS frame');
    ok(mapAt('E') && readText(T.m.fb(), 4, 106, 3) === 'ATK' && readText(T.m.fb(), 14, 146 - 13, 9) === 'AMOUNT TO', `SHIFT + down from I: E, ATK DEC SUS REL, AMOUNT TO (${readText(T.m.fb(), 4, 106, 3)})`);
    ok(names(106, [0, 8, 16, 24]) + ' / ' + names(146, [1, 9, 17]) === 'ATK DEC SUS REL / EFL EPI ESH', `E's labels: the command names (${names(146, [1, 9, 17])})`);
    T.shot('18-env');                            // (mockup inst-env)
    // principles-review.md #5 option B (user 2026-10-11): the drawings in the text colour, the ADSR to time, TIME top right
    const TEXT = rgb565('#e8e8e8'), LABEL = rgb565('#777777');
    const ink = (x, y, w, h, c) => { for (let j = y; j < y + h; j++) for (let i = x; i < x + w; i++) if (px(i, j) === c) return true; return false; };
    const span = () => readText(T.m.fb(), 4 + 200 - 11 * 6, 34, 11).trim();
    const peakX = () => { for (let x = 4; x < 204; x++) if (px(x, 48) === TEXT) return x; return -1; };
    ok(span() === 'TIME 64ms' && ink(4 + 200 - 6, 34, 6, 12, LABEL), `the ADSR's span top right, A + D + R = 1.0 + 54 + 8.8 ms (${span()})`);
    ok(!ink(4, 36, 200, 64, ACCENT) && ink(4, 48, 200, 1, TEXT) && !ink(4, 36, 120, 12, TEXT), 'the ADSR in the text colour, its peak a line down (y 48), no accent');
    const adsr = graph(), pk0 = peakX();
    T.hold('edit'); T.press('up', 2); T.free('edit');   // ATK up two big steps: a longer attack
    ok(graph() !== adsr && peakX() > pk0 + 10 && /^TIME \d+(ms|\.\ds)$/.test(span()), `the ADSR to time follows ATK: the peak x ${pk0} -> ${peakX()}, ${span()}`);
    T.with_('shift', 'down');                    // L
    const w0 = graph(); T.go(200); const w1 = graph();
    ok(knobsFrame(), 'L: the KNOBS frame');
    ok(mapAt('L') && w0 !== w1 && !ink(4, 36, 200, 64, ACCENT) && ink(4, 36, 200, 64, TEXT), 'L: the waveform moves, in the text colour');
    ok(names(106, [0, 8, 16, 24]) + ' / ' + names(146, [1, 9, 17, 25]) === 'LRT LWV LPH LFD / LPI LFL LSH LVO', `L's labels: the command names (${names(106, [0, 8, 16, 24])} / ${names(146, [1, 9, 17, 25])})`);
    T.shot('18-lfo');                            // (mockup inst-lfo)
    T.with_('shift', 'down');                    // V
    ok(mapAt('V') && readText(T.m.fb(), 14, 48 - 13, 5) === 'VOICE' && readText(T.m.fb(), 14, 110 - 13, 5) === 'SENDS' && knobsFrame(), 'V: the VOICE, SENDS and KNOBS frames');
    ok(names(48, [1, 9, 17]) + ' / ' + names(110, [1, 9, 17, 25]) === 'VCE GLD VDT / SDS SCH SDL SRV', `V's labels: the command names (${names(48, [1, 9, 17])} / ${names(110, [1, 9, 17, 25])})`);
    T.shot('18-vox');                            // (mockup inst-vox)
    T.with_('shift', 'down');
    ok(mapAt('V'), 'SHIFT + down on V: nothing (the column ends)');
    T.with_('shift', 'right');
    ok(mapAt('I'), 'SHIFT + right from V: back to I');
    T.with_('shift', 'down', 2); T.with_('shift', 'left');   // L, then left: P
    ok(mapAt('P'), 'SHIFT + left from L: the phrase');
    // the panel's LFO button: the focus instrument's L page
    T.btn(5);
    ok(mapAt('L'), 'panel LFO: the L page');
    T.with_('shift', 'left');
    // SHIFT + right on a sound command: its parameter on its page (ATK: E)
    T.fx(0, 0, 'ATK', 0x40);
    T.to(0, 3);
    T.with_('shift', 'right');
    ok(mapAt('E') && px(cellX(0) - 1, 106 + 12) === ACCENT, 'SHIFT + right on ATK: the E page, the cursor on ATK');
}

// ---- notes mode (docs/ux/notes-mode.md option B; app.js recButton / keyDown): REC, the middle keys, step entry
{
    const T = rig(), note = r => T.cell(2, r, 3);
    const tag = () => readText(T.m.fb(), TAG_END - 4 * 6, 4, 4).trim();
    T.key(7);                                    // gamepad mode: a middle key does nothing
    ok(note(0) === '---', 'the gamepad: C4 writes nothing');
    T.btn(13);                                   // REC
    ok(T.strip() === 'keys: notes (practise)' && tag() === 'NOTE', `REC: notes mode, NOTE in the top bar (${T.strip()}, ${tag()})`);
    T.key(7); T.key(1); T.key(9);                // C4, F#3 (note off), D4: step entry, the cursor moves down
    ok([0, 1, 2].map(note).join() === 'C-4,OFF,D-4' && T.cell(9, 0, 1) === '0', `step entry: ${[0, 1, 2].map(note).join()}, instrument ${T.cell(9, 0, 1)}`);
    T.shot('19-notes');
    T.btn(1);                                    // OCT+
    ok(T.strip() === 'octave 5', `OCT+: ${T.strip()}`);
    T.key(7);
    ok(note(3) === 'C-5', `C4's key an octave up: ${note(3)}`);
    T.btn(0);
    T.press('up');                               // the gamepad keys still work
    T.go(800);
    const quiet = rms(T.record(300));
    T.m.input(1 << 7, 0); T.go(100); const held = rms(T.record(400)); T.m.input(0, 0); T.go(800);
    ok(held > 0.01 && held > quiet * 10, `a key sounds while held (rms ${quiet.toFixed(4)} -> ${held.toFixed(3)})`);
    T.press('play'); T.go(200);
    T.key(11);                                   // playing: practise, nothing written
    T.press('play');
    ok(note(3) === 'C-4' && note(4) === '---', `playing: a key writes nothing (${note(3)} ${note(4)}; the held C4 was step entry)`);
    T.btn(13);
    ok(T.strip() === 'armed: PLAY to record' && tag() === 'ARM', `REC again: armed (${T.strip()}, ${tag()})`);
    T.btn(13);
    ok(T.strip() === 'keys: gamepad' && tag() !== 'NOTE' && tag() !== 'ARM', `REC a third time: the gamepad (${T.strip()})`);
}

// ---- live recording (docs/ux/live-record.md; app.js startRecording / liveWrite): ARM, PLAY, the take
{
    const T = rig(), note = r => T.cell(2, r, 3);
    const tag = () => readText(T.m.fb(), TAG_END - 4 * 6, 4, 4).trim();
    T.btn(13); T.btn(13);                        // notes, armed
    ok(tag() === 'ARM', `REC twice: ARM in the top bar (${tag()})`);
    const cnt = T.playRecord(1500);              // PLAY: a bar of count-in (2 s at 120), the metronome, no rows yet
    ok(T.strip() === 'count-in, then recording' && tag() === 'REC' && T.playhead() === -1 && rms(cnt) > 0.005,
        `PLAY while armed: ${T.strip()}, REC, no playhead yet, clicks (rms ${rms(cnt).toFixed(3)})`);
    T.until(2); T.go(45);                        // ~2 ticks into row 2: kept as DEL 02
    T.key(7);
    T.until(6); T.key(9);                        // right at row 6's start: no DEL
    T.go(300);
    const fx = r => T.cell(11, r, 3) + ' ' + T.cell(15, r, 2);
    T.btn(13);                                   // REC: stop writing, playback goes on
    T.go(200);
    ok(T.strip() === 'RECORDING STOPPED: 2 NOTES 0 FX' && T.playhead() >= 0 && tag() === '', `REC while recording: ${T.strip()}, still playing`);
    T.press('play');                             // (read the cells stopped: the DEL timeline marks them while playing)
    // (a note with a DEL has its timeline mark under it, so it is read by its instrument and its DEL)
    ok(T.cell(9, 2, 1) === '0' && /^DEL 0[23]$/.test(fx(2)) && note(6) === 'D-4' && fx(6) === '--- --',
        `the take: row 2 with ${fx(2)} (a late hit), D-4 on time (${note(6)} ${fx(6)})`);
    T.panel(0);                                  // undo: the whole take at once
    ok(note(2) === '---' && note(6) === '---', `one undo takes the whole take back (${T.strip()})`);
    // started while playing: from the next phrase start
    T.press('play'); T.go(300);
    T.btn(13); T.btn(13);
    T.press('play');
    ok(T.strip() === 'recording from the next phrase', T.strip());
    T.until(9); T.key(7);                        // this phrase: not written
    T.until(1); T.go(10); T.key(7);              // the next pass: written
    T.go(100);
    T.shot('20-rec');   // (mockup phrase-rec)
    T.press('play');
    ok(T.strip() === 'RECORDING STOPPED: 1 NOTES 0 FX', `PLAY ends the take: ${T.strip()}`);
    T.btn(13); T.btn(13); T.press('play'); T.go(3000); T.press('play');   // a take with nothing in it
    ok(T.strip() === 'RECORDING STOPPED: NOTHING WRITTEN', `an empty take (picks review #16): ${T.strip()}`);
    ok(note(9) === '---' && T.cell(9, 1, 1) === '0' && T.cell(11, 1, 3) === 'DEL', `only from the next phrase on (row 9 ${note(9)}, row 1 written)`);
    // the chain: recording through it, an empty chain row gets a new phrase
    T.with_('shift', 'left');                    // chain 00: row 0 = phrase 00, row 1 empty
    T.btn(13); T.btn(13);
    T.press('play');
    T.go(2000 + 2000 + 300);                     // count-in, phrase 00, into row 1
    T.press('play');
    ok(T.cell(2, 1, 2) === '01', `recording through the chain: row 1 got a new phrase (${T.cell(2, 1, 2)})`);
}
// EDIT held on a plain cell, a row number, a column label, a selection: what it is in the bottom line (app.js
// CELL_HELP / helpState, via gen/help.h)
{
    const T = rig();
    const says = (what, want) => { T.hold('edit'); const s = T.strip(); T.free('edit'); ok(s === want, `EDIT held on ${what}: ${s}`); };
    says('a note', 'note played on this row');
    T.to(0, 1); says('a velocity', 'note velocity, 00 to 7F');
    T.to(0, 2); says('an instrument', 'instrument the note plays');
    T.to(0, 0); T.press('up'); says('a column label', 'column: SHIFT+OPTION selects it'); T.press('down');
    T.press('left'); says('a row number', 'row: EDIT EDIT add, EDIT+arrows move'); T.press('right');
    T.with_('shift', 'left'); says('a chain phrase', 'phrase played on this chain row');
    T.press('right'); says('a chain transpose', 'transpose the phrase, semitones'); T.press('left');
    T.with_('shift', 'left'); says('a song cell', 'chain this channel plays here');
    T.with_('shift', 'right', 2);
    T.hold('shift'); T.press('option'); T.free('shift'); T.press('down');
    says('a selection', 'selection: EDIT+arrows edit it');
}
{   // ... and on G's fields (CELL_HELP.proj)
    const T = rig();
    const says = (what, want) => { T.hold('edit'); const s = T.strip(); T.free('edit'); ok(s === want, `EDIT held on G's ${what}: ${s}`); };
    T.with_('shift', 'up'); T.press('down');     // P -> G, slot 00 -> slot 01
    says('slot', 'slot: EDIT loads; empty: EDIT EDIT new');
    T.press('down', 2); T.press('right'); says('TEMPO', 'song tempo, BPM');   // slot 03 -> TEMPO
    T.press('down', 4); says('METRO', 'metronome: off, when recording, always');
}
// a recorded note over an OFF gets the last velocity, like step entry (prototype e47f2450, user 2026-10-10)
{
    const T = rig(), row = r => [T.cell(2, r, 3), T.cell(6, r, 2), T.cell(9, r, 1)].join(' ');
    T.to(6, 0); T.with_('option', 'edit');       // row 6: OFF
    const off = row(6);
    T.btn(13); T.btn(13); T.press('play');       // armed, PLAY: the count-in, then the take
    for (let t = 0; t < 6000 && T.playhead() !== 6; t += 5) T.go(5);
    if (T.playhead() !== 6) ok(false, 'the playhead reaches row 6');
    T.m.input(1 << 9, 0); T.go(25); T.m.input(0, 0); T.go(100);
    T.press('play');
    ok(off === 'OFF -- -' && row(6) === 'D-4 64 0', `live record over an OFF: the last velocity and instrument (${off} -> ${row(6)})`);
}

// ---- levels (decisions.md "Levels: VOL and TVL"): VOL = the instrument's level (a sound command, the I page's row),
// TV1-TV4 = a track's mix level while playing (X shows it; stop: the mix again; TVL until 2026-10-12, decisions.md "TV1-TV4")
{
    const T = rig(), xv = () => T.cell(2, 11, 6).trim();
    T.note(0);
    T.with_('shift', 'right');
    const lbl = readText(T.m.fb(), 4, IROW_Y(1) + 2, 5).trim();
    T.with_('shift', 'left');
    T.fx(0, 0, 'TV1', 0);
    T.with_('shift', 'down', 2);                 // P -> F -> X
    const mix = xv();
    T.press('play'); T.go(150); const quiet = rms(T.record(400));
    const moved = xv(), label = T.cell(2, 10, 3);
    T.press('play'); T.go(50);
    ok(lbl === 'VOL' && label === 'TV1' && quiet < 0.001 && moved === 'OFF' && mix === '-4.0dB' && xv() === mix,
        `I's level row reads VOL (${lbl}); TV1 00: the track silent (rms ${quiet.toFixed(4)}), X's ${label} shows ${moved}, the mix (${mix}) again at stop (${xv()})`);
    // in Q, EDIT + PLAY hears the line's VOL (the prv_ph shortcut gone)
    const Q = rig(), w = (a, t0, t1) => rms(a, t0 * 44100, t1 * 44100);
    const q = vol => { Q.hold('edit'); Q.hold('play'); const a = Q.record(400); Q.free('play'); Q.free('edit'); Q.go(800); return w(a, .1, .3); };
    Q.note(0); Q.to(0, 0); Q.btn(11); Q.go(300);
    const plain = q();
    Q.hold('edit'); Q.free('edit'); Q.go(300); Q.fx(0, 0, 'VOL', 0); Q.to(0, 0); Q.btn(11); Q.go(300);
    const vol0 = q();
    ok(plain > 0.005 && vol0 < plain / 20, `EDIT + PLAY in Q: the line's VOL 00 heard (rms ${plain.toFixed(4)} -> ${vol0.toFixed(4)})`);
}

// ---- TV1-TV4 (decisions.md "TV1-TV4", user 2026-10-12): a track's level from any track; X's labels; a moved level in
// the colour of the track that wrote it
{
    const T = rig(), xv = k => T.cell(2 + k * 8, 11, 6).trim(), CH1 = rgb565('#4fa3ff');
    const inCH1 = k => { let n = 0; for (let y = rowY(11); y < rowY(12); y++) for (let x = cellX(2 + k * 8); x < cellX(9 + k * 8); x++) if (T.m.fb()[y * 240 + x] === CH1) n++; return n; };
    T.note(0); T.fx(0, 0, 'TV3', 0);              // on CH1: track 3's level to OFF
    T.with_('shift', 'down', 2);                 // P -> F -> X
    const labels = [0, 1, 2, 3].map(k => T.cell(2 + k * 8, 10, 3)), own = xv(0), mix3 = xv(2);
    T.press('play'); T.go(150);
    const moved = xv(2), still = xv(0), col = inCH1(2);
    T.press('play'); T.go(50);
    ok(labels.join(' ') === 'TV1 TV2 TV3 TV4' && moved === 'OFF' && still === own && col > 0 && xv(2) === mix3,
        `X's labels ${labels.join(' ')}; TV3 00 on CH1: track 3 ${moved} in CH1's colour (${col} px), CH1 stays ${still}, the mix (${mix3}) again at stop (${xv(2)})`);
}

// ---- parameter values (docs/ux/param-values.md): what plays now, the morph over commands, the size rule, the cursor
// value under scenes in the bottom line, X's TV1, G's TEMPO, A / B in list popups
{
    const T = rig(), SEL = 3, fb = () => T.m.fb(), px = (x, y) => fb()[y * 240 + x], CH1 = rgb565('#4fa3ff');
    const val = (x, y) => readText(fb(), x, y, 7).replace(/[ ?]+$/, '');
    const ink = (x, y, w, col) => { for (let j = y; j < y + 12; j++) for (let i = x; i < x + w; i++) if (px(i, j) === col) return true; return false; };
    T.note(0);
    T.with_('shift', 'right'); T.press('down', 3);   // I0's CUT (E4)
    const song = val(4, 160);
    T.button(SEL, true); T.with_('edit', 'up', 16); T.button(SEL, false);   // CUT at its top into scene 1 (on A)
    T.go(30);
    const cut = val(4, 160), line = T.strip(38), m = line.match(/^CUT A (\S+) now (\S+) B (\S+)$/);
    ok(m && m[1] === m[2] && m[3] === song && cut.length <= 6 && m[1].startsWith(cut) && cut !== m[1],
        `CUT in scene A: the bottom line A / now / B (${line}); the field drops its unit for the diamond (${cut})`);
    T.hold('edit'); const eh = T.strip(38); T.free('edit');
    T.button(SEL, true); T.go(30); const sh = T.strip(38); T.button(SEL, false);
    ok(!eh.startsWith('CUT A') && !sh.startsWith('CUT A'), `EDIT held: the help (${eh}); SEL held: the edit view (${sh})`);
    T.with_('shift', 'left');
    T.fx(0, 0, 'TV1', 0x40); T.fx(0, 1, 'TPO', 0x5A); T.fx(0, 2, 'CUT', 0x40); T.to(0, 0);   // (CUT: a command on a value scene A holds)
    T.press('play'); T.go(200);
    T.with_('shift', 'down', 2); T.press('down');   // X, the cursor on CH1's MUTE
    const xcol = ink(cellX(2), rowY(11), 42, CH1);
    T.press('up'); T.button(SEL, true); T.with_('edit', 'left'); T.button(SEL, false); T.press('down'); T.go(30);   // CH1's level into scene 1
    const xmorph = !ink(cellX(2), rowY(11), 42, CH1);   // the morph wins: white, not TV1's colour
    T.with_('shift', 'up', 2); T.with_('shift', 'right'); T.go(30);   // I0, the cursor on CUT
    const imorph = !ink(4, 160, 42, CH1) && val(4, 160) === cut;   // the morph wins over the CUT command: white, scene A's
    T.with_('shift', 'left'); T.with_('shift', 'up');   // P -> G
    T.press('down', 3); T.press('right');        // slot 03 -> TEMPO
    const tl = T.strip(30); T.press('down'); T.go(30);
    const tcol = ink(4 + 29 * 6, 32 + 3 * 12, 18, CH1), tv = readText(fb(), 4 + 29 * 6, 32 + 3 * 12, 3).trim();
    T.press('up'); T.hold('edit'); const te = T.strip(30); T.free('edit');
    T.press('play'); T.go(50);
    const ts = readText(fb(), 4 + 29 * 6, 32 + 3 * 12, 3).trim();
    ok(imorph, `I: CUT held by a command and by scene A: the morph's ${val(4, 160)}, white (${imorph})`);
    ok(xcol && xmorph && tl === 'TEMPO set 120, now 90' && tcol && tv === '90' && !te.startsWith('TEMPO set') && ts === '120',
        `X: TV1's level in CH1's colour (${xcol}), white once a scene holds it (${xmorph}); G: ${tl}, TEMPO ${tv} in CH1's colour (${tcol}); EDIT held: ${te}; stopped: ${ts}`);
    T.with_('shift', 'down'); T.with_('shift', 'down');   // G -> P -> F, the cursor on TIME
    T.button(SEL, true); T.with_('edit', 'right'); T.button(SEL, false);   // TIME into scene 1 (A)
    T.hold('edit');
    const marks = [...Array(10).keys()].map(k => readText(fb(), 4 + 1 * 6 + 13 * 6, 48 + 2 * 12 + 3 + k * 12, 1)).join('');   // (under TIME)
    T.free('edit');
    ok(/^[^A]*A[^A]*$/.test(marks), `F's TIME list: A beside the item scene A holds (${marks.replace(/ /g, '.')})`);
}

// ---- PLAY on a selection (decisions.md "PLAY on a selection"): its rows looped (HOP ignored), LOOP 4-7 in the top bar;
// SHIFT + PLAY the same place on all tracks, a gap filled from the nearest phrase above (the song map shows where)
{
    const T = rig(), px = (x, y) => T.m.fb()[y * 240 + x], tag = (n = 8) => readText(T.m.fb(), TAG_END - n * 6, 4, n);
    const smap = (k, r) => px(226 + 3 * k, 98 + 2 * r) === PLAYC;   // S's song map: track k playing song row r
    const loopRows = ms => { const seen = new Set(); for (let t = 0; t < ms; t += 10) { T.go(10); const r = T.playhead(); if (r >= 0) seen.add(r); } return [...seen].sort((a, b) => a - b).join(); };
    for (const r of [0, 4, 8, 12]) T.note(r);
    T.fx(5, 0, 'HOP', 0x0C);                     // a HOP inside the loop: ignored
    T.to(4, 0); T.hold('shift'); T.press('option'); T.free('shift'); T.press('down', 3);   // rows 4-7 selected
    T.press('play');
    const rows = loopRows(1500), t1 = tag();
    T.with_('shift', 'right'); T.go(50);         // I while the loop plays: LOOP 4-7 leaves under 4 letters, no name
    const iName = readText(T.m.fb(), 4 + 21 * 6, 4, 2), iTag = tag();   // (2: the tag starts in column 23)
    T.with_('shift', 'left'); T.go(50);
    ok(iName === '  ' && iTag === 'LOOP 4-7', `I under a LOOP tag: no name fragment ('${iName}', ${iTag})`);
    T.press('play'); T.go(50);
    ok(rows === '4,5,6,7' && t1 === 'LOOP 4-7' && T.playhead() === -1, `PLAY on rows 4-7: only they play, looped, the HOP ignored (${rows}); ${t1}; PLAY stops it`);
    T.press('option'); T.press('play'); T.go(300); const t3 = tag(); T.press('play'); T.go(50);
    ok(t3 !== 'LOOP 4-7', `PLAY on the copied rows' mark: plain PLAY, no loop (unit-feedback #28; '${t3}')`);
    T.press('option');                           // (the mark dropped)
    T.with_('shift', 'left', 2);                 // S: CH1 S01 and CH3 S00 get chain 00 too
    T.press('down'); T.press('edit'); T.press('up'); T.press('right', 2); T.press('edit');   // (EDIT: a chain each)
    const cells = [T.cell(3, 1, 2), T.cell(9, 0, 2), T.cell(9, 1, 2)].join();
    T.press('left', 2); T.press('down'); T.with_('shift', 'right', 2);   // P of S01 (P00)
    T.to(4, 0); T.hold('shift'); T.press('option'); T.free('shift'); T.press('down', 3);
    T.hold('shift'); T.press('play'); T.free('shift');
    const rows2 = loopRows(800);
    T.with_('shift', 'left', 2); T.go(200);
    const map = [smap(0, 1), smap(2, 0), smap(1, 0) || smap(1, 1), smap(3, 0) || smap(3, 1)].join();
    T.press('play');
    ok(/^\w\w,\w\w,--$/.test(cells) && rows2 === '4,5,6,7' && map === 'true,true,false,false',
        `SHIFT + PLAY: CH1 at S01, CH3 (nothing at S01) its nearest above, S00; CH2 / CH4 nothing: rest (cells ${cells}; rows ${rows2}; map ${map})`);
    // S: song rows 00-01 of CH1 selected, PLAY: those rows of CH1 only
    T.hold('shift'); T.press('option'); T.free('shift'); T.press('down');
    T.press('play'); T.go(300);
    const map2 = [smap(0, 0) || smap(0, 1), smap(2, 0)].join(), t2 = tag(10);
    T.press('play');
    ok(map2 === 'true,false' && t2 === 'LOOP 01-02', `S: song rows 01-02 of CH1 looped, CH3 not (${map2}; ${t2})`);
}

// ---- MUT / SOL (decisions.md "MUT / SOL"): the track's mute / solo while playing, X shows it, the hand wins, stop: the mix
{
    const T = rig(), TEXT = rgb565('#e8e8e8'), px = (x, y) => T.m.fb()[y * 240 + x];
    const lit = line => { for (let j = 0; j < 12; j++) for (let i = 0; i < 24; i++) if (px(cellX(2) + i, rowY(line) + j) === TEXT) return true; return false; };
    const mute = () => lit(13), solo = () => lit(14);   // X: CH1's MUTE / SOLO, white = on
    for (const r of [0, 4, 8, 12]) T.note(r);
    T.fx(0, 0, 'MUT', 1); T.fx(4, 0, 'MUT', 0); T.fx(8, 0, 'SOL', 1); T.fx(12, 0, 'MUT', 1);
    T.to(0, 4); T.hold('edit'); const text = T.strip(30); T.free('edit');
    T.fx(13, 0, 'RTO', 0);                       // a ramp under row 12's MUT: refused (on or off), no ramp line
    T.to(15, 0);
    const LINE = rgb565('#333333'), noLine = px(cellX(11) - 3, rowY(13) + 5) !== LINE;
    T.to(13, 3); T.with_('option', 'edit');      // (the RTO cleared again)
    T.with_('shift', 'down', 2);                 // X
    const at = ms => { T.go(ms - T.t); T.t = ms; };
    T.t = 0; T.press('play'); T.t = 0;
    at(60); const a = rms(T.record(300)); T.t += 300; const m1 = mute();
    at(560); const b = rms(T.record(300)); T.t += 300; const m2 = mute();
    T.with_('option', 'shift');                  // the hand mutes it before row 8's SOL 01
    at(1060); const s1 = solo(), m3 = mute();
    at(1560); const s2 = solo(), m4 = mute();
    T.with_('option', 'play'); const s3 = solo(), m5 = mute();   // the hand: solo (row 12's MUT took the solo away)
    T.with_('option', 'shift'); const m6 = mute(), s6 = solo();   // OPTION then SHIFT: mute
    T.press('play'); T.go(50);
    ok(text === 'MUT 01 = mute this track' && noLine, `the value text: ${text}; not rampable (no ramp line: ${noLine})`);
    ok(m1 && a < 0.001 && !m2 && b > 0.005, `MUT 01: muted with its row's note, X shows it (rms ${a.toFixed(4)}); MUT 00 four rows on: heard again (${b.toFixed(4)})`);
    ok(s1 && !m3 && !s2 && m4, `SOL 01: soloed, the hand's mute gone; MUT 01 on a soloed track: muted, the solo gone (unit feedback #17: ${[s1, m3, s2, m4]})`);
    ok(s3 && !m5 && m6 && !s6 && mute() && !solo(), `the hand wins (solo: the mute goes; mute: the solo goes), stop: the mixer's, the hand's mute (${[s3, m5, m6, s6, mute(), solo()]})`);
}

// ---- MUT / SOL xy target a track; G's MUTE NOTE / AUDIO (decisions.md "PLAY on a selection, audio mute, MUT / SOL on
// another track"): x = the track (0 = this one), y = on; AUDIO: a muted track plays on silently, an unmute heard at once
{
    const T = rig(), TEXT = rgb565('#e8e8e8'), px = (x, y) => T.m.fb()[y * 240 + x];
    const lit = (k, line) => { for (let j = 0; j < 12; j++) for (let i = 0; i < 24; i++) if (px(cellX(2 + 8 * k) + i, rowY(line) + j) === TEXT) return true; return false; };
    T.note(0); T.fx(0, 0, 'MUT', 0x21); T.fx(0, 1, 'SOL', 0x51);   // mute track 2; SOL 51: nothing
    T.to(0, 4); T.hold('edit'); const t1 = T.strip(30); T.free('edit');
    T.to(0, 6); T.hold('edit'); const t2 = T.strip(30); T.free('edit');
    T.with_('shift', 'down', 2);                 // X
    T.press('play'); T.go(200);
    const m = [lit(0, 13), lit(1, 13), [0, 1, 2, 3].some(k => lit(k, 14))].join();
    T.press('play');
    ok(t1 === 'MUT 21 = mute track 2' && t2 === 'SOL 51 = nothing (tracks 1-4)' && m === 'false,true,false',
        `${t1}; ${t2}; CH1 / CH2 muted, any solo: ${m}`);
}
{
    const unmuted = audio => {                   // CH1 muted before PLAY, unmuted mid-note: heard at once with AUDIO only
        const T = rig();
        T.note(0);
        T.with_('shift', 'right'); T.press('right');   // I: PRESET ACID -> SUB BASS (sustained)
        T.hold('edit'); T.press('right', 3); T.free('edit'); T.press('left'); T.with_('shift', 'left'); T.to(0, 0);
        T.with_('shift', 'up'); T.press('down', 3); T.press('right'); T.press('down', 7);   // G: TEMPO -> MUTE
        const mute = fb => readText(fb, 4 + 21 * 6, 32 + 10 * 12, 11, 11).replace(/[?\s]+/g, ' ').trim();   // (11 rows: the
                                                 // usage block's frame line, G's SYSTEM button, in the cell's bottom row)
        const row = mute(T.m.fb());
        if (audio) T.with_('edit', 'right');
        const set = mute(T.m.fb());
        T.with_('shift', 'down');                // P
        T.hold('option'); T.press('shift'); T.free('option');   // mute CH1
        T.press('play'); T.go(400);
        T.hold('option'); T.press('shift'); T.free('option');   // unmute, mid-note
        const r = rms(T.record(300)); T.press('play');
        T.btn(9); T.go(300);                     // SAVE, then a reboot: MUTE is in the project (format 8)
        const R = rig(T.store()); R.with_('shift', 'up');
        return [row, set, r, mute(R.m.fb())];
    };
    const [row, note, rn] = unmuted(false), [, audio, ra, kept] = unmuted(true);
    {   // docs/ux/system-popup.md: the battery mark, the tags right-aligned, G's usage block opens SYSTEM
        const T = rig(), e = T.m.e, px = (x, y) => T.m.fb()[y * 240 + x];
        const LABEL = rgb565('#777777'), TEXT = rgb565('#e8e8e8'), HELD = rgb565('#ffc23d'), BG = 0;
        const mark = () => [px(223, 6), px(235, 8), px(232, 9)];   // outline, nub, the fourth step
        const full = mark();
        e.emu_adc_set(3, 530); T.go(20000); const low = mark();       // 3.42 V: 14 %
        e.emu_adc_set(3, 512); T.go(20000); const red = [0, 1, 2, 3].map(() => { T.go(250); return px(223, 6); });   // 3.30 V: blinks
        e.emu_usb_set(1); T.go(1100); const usb = [px(223, 6)];               // on USB: green, no blink
        const fill = () => [228, 230, 233].map(x => px(x, 9) === TEXT ? 1 : 0).join('');
        e.emu_adc_set(3, 580); T.go(20000);      // 3.74 V: 2 steps; on USB the fill climbs 2, 3, 4, 2.. every 0.6 s
        const climb = new Set(); for (let k = 0; k < 12; k++) { T.go(150); climb.add(fill()); }
        e.emu_usb_set(0); e.emu_adc_set(3, 800); T.go(20000);
        ok(full.join() === [LABEL, LABEL, TEXT].join() && low[0] === ACCENT && low[2] === BG && red.includes(PLAYC) && red.includes(BG)
            && usb[0] === rgb565('#5fd36a') && [...climb].sort().join() === '100,110,111',
            `the battery mark: full grey, low orange, under 5 % red and blinking, on USB green with the fill climbing (${[full, low, red, usb].map(a => a.join('/')).join('; ')}; ${[...climb].sort()})`);
        T.btn(13); T.btn(13); T.go(50);          // REC twice: ARM
        const armEnd = readText(T.m.fb(), TAG_END - 6, 4, 1); T.btn(13); T.go(50);
        ok(armEnd === 'M', `ARM ends a space before the tempo, as NOTE does (${armEnd})`);
        T.with_('shift', 'up'); T.press('down', 3); T.press('right'); T.press('down', 8);   // G: slot 3, TEMPO .. the usage block
        const btnLit = px(127, 32 + 11 * 12 - 1) === ACCENT;
        T.hold('edit'); const help = T.strip(38); T.free('edit'); T.go(300);
        const SY = 20 + ((32 + 16 * 12 - 20 - (11 * 12 + 4)) >> 1), line = i => readText(T.m.fb(), 4, SY + 2 + i * 12, 34);
        const pop = [line(0).trim(), line(1).slice(0, 3), line(4).slice(0, 9).replace(/ +/g, ' '), line(10).trim()];
        T.press('up'); T.go(50);
        const shut = [line(0).trim() === 'SYSTEM', px(127, 32 + 11 * 12 - 1) === ACCENT];
        T.hold('edit'); T.free('edit'); T.go(300); T.btn(12); T.go(300);   // open again; PLAY on the panel closes it, does not play
        const shut2 = [line(0).trim() === 'SYSTEM', T.playhead()];
        ok(btnLit && help.startsWith('EDIT: system') && pop.join('|') === 'SYSTEM|CPU|USB off|FW    EMU' && shut.join() === 'false,true'
            && shut2.join() === 'false,-1',
            `G: the usage block a button, EDIT opens SYSTEM, any key or button closes it and does nothing else (${btnLit}; ${help}; ${pop.join('|')}; ${shut}; ${shut2})`);
    }
    ok(row === 'MUTE NOTE' && audio === 'MUTE AUDIO' && kept === 'MUTE AUDIO' && ra > 0.005 && rn < ra / 10,
        `G: ${row} -> ${audio}, saved and loaded (${kept}); unmuted mid-note: AUDIO heard at once (rms ${ra.toFixed(4)}), NOTE from the next note (${rn.toFixed(4)})`);
}

{
    // MUTE AUDIO: the muted track's sends are silent too (Felucca's sends come after the part's level, fx.c mix_part):
    // SRV FF + SDL FF on a muted track, nothing comes out; the same unmuted, the effects are heard
    const sends = mute => {
        const T = rig();
        T.note(0); T.fx(0, 0, 'SRV', 0xFF); T.fx(0, 1, 'SDL', 0xFF); T.to(0, 0);
        T.with_('shift', 'up'); T.press('down', 3); T.press('right'); T.press('down', 7); T.with_('edit', 'right');   // MUTE AUDIO
        T.with_('shift', 'down');
        if (mute) { T.hold('option'); T.press('shift'); T.free('option'); }
        const r = rms(T.playRecord(1500)); T.press('play');
        return r;
    };
    const on = sends(false), off = sends(true);
    ok(on > 0.005 && off < 0.0001, `MUTE AUDIO, SRV / SDL FF: heard unmuted (rms ${on.toFixed(4)}), nothing muted, sends included (${off.toFixed(6)})`);
}

// ---- the overload net (Felucca's audio.c): a half over 85 % of its time sheds a voice before the next one: the quietest
// releasing voice first; the only held voice never (the emulator renders in no time: emu_overload stands for a late half)
{
    const shed = off => {                        // row 0: a note, REL FF (a long release); OFF 01: released at tick 1
        const T = rig(), e = T.m.e;
        T.note(0); T.fx(0, 0, 'REL', 0xFF); if (off) T.fx(0, 1, 'OFF', 1); T.to(0, 0);
        T.press('play'); T.go(60);
        const v0 = e.emu_voices(); e.emu_overload(); T.go(30);
        const r = [v0, e.emu_voices(), e.emu_sheds()]; T.press('play'); return r;
    };
    const held = shed(false), rel = shed(true);
    ok(held[2] === 0 && held[1] === held[0] && rel[2] === 1 && rel[1] !== rel[0],
        `overload: the only held voice kept (voices ${held[0]} -> ${held[1]}, ${held[2]} shed); a releasing one shed (${rel[0]} -> ${rel[1]}, ${rel[2]} shed)`);
}

// ---- the crossfader turns only with SEL held; a scene's morph wins over TV1 (decisions.md "Scenes over commands")
{
    const T = rig(), SEL = 3, xv = () => T.cell(2, 11, 6).trim();
    T.m.enc(0, 32); T.go(30);                    // no SEL: nothing moves
    ok(T.strip() === 'hold SEL to crossfade', `SELECT without SEL: ${T.strip()}`);
    T.note(0); T.fx(0, 0, 'TV1', 0);
    T.with_('shift', 'down', 2);                 // X; SEL held: CH1's level into scene 1 (A), at -8.0dB
    T.button(SEL, true); T.with_('edit', 'left', 8); T.button(SEL, false);
    const inScene = xv();
    T.press('play'); T.go(150); const a = rms(T.record(400)); const shown = xv(); T.press('play');
    ok(inScene !== '-4.0dB' && a > 0.001 && shown === inScene, `TV1 00 under a scene holding CH1's level: the morph's ${shown} plays (rms ${a.toFixed(4)})`);
    T.xf(32);                                    // to B, which lacks it: the song's own mix level there, not TV1's 00
    T.press('play'); T.go(150); const b = rms(T.record(400)); const atB = xv(); T.press('play');
    ok(atB === '-4.0dB' && b > a, `... at B (scene 2 lacks it): the mix (${atB}), TV1 ignored at that end too (rms ${b.toFixed(4)})`);
}

// ---- EDIT's audition lasts while EDIT is held, released on let go; none in a selection (picks review #27)
{
    const T = rig(), w = (a, t0, t1) => rms(a, t0 * 44100, t1 * 44100);
    T.note(0); T.note(1);
    T.with_('shift', 'right'); T.press('right');   // I: PRESET ACID -> SUB BASS (sustained)
    T.hold('edit'); T.press('right', 3); T.free('edit'); T.press('left'); T.with_('shift', 'left'); T.to(0, 0); T.go(1000);
    T.hold('edit'); const on = T.record(800); T.free('edit'); const off = T.record(300);
    ok(w(on, .6, .8) > w(on, .05, .15) / 2 && w(off, .2, .3) < w(on, .6, .8) / 4,
        `EDIT held on a note: heard while held (rms at 0.6 s ${w(on, .6, .8).toFixed(4)}), released on let go (${w(off, .2, .3).toFixed(4)})`);
    T.go(1000);
    T.hold('shift'); T.press('option'); T.free('shift'); T.press('down');   // rows 0-1 selected
    T.hold('edit'); T.press('up'); const blk = T.record(400); T.free('edit');
    ok(rms(blk) < 0.001 && T.cell(2, 0, 3) === 'C-4', `EDIT + up on a selection: no audition (rms ${rms(blk).toFixed(4)}, ${T.cell(2, 0, 3)})`);
    // unit-feedback #28: once OPTION copied, the mark is drawn only: EDIT writes, edits and auditions the cursor's cell
    const r1 = T.cell(2, 1, 3); T.press('option'); T.to(5, 0); T.press('edit'); T.go(1000);
    T.hold('edit'); T.press('up'); const mk = T.record(400); T.free('edit');
    ok(rms(mk) > 0.001 && T.cell(2, 5, 3) !== '---' && T.cell(2, 0, 3) === 'C-4' && T.cell(2, 1, 3) === r1,
        `after a copy, EDIT + up edits and auditions the cursor's cell, not the marked rows (rms ${rms(mk).toFixed(4)}; ${T.cell(2, 5, 3)}; ${T.cell(2, 0, 3)})`);
}

// ---- EDIT on an FX cell auditions the row (unit-feedback #26, user 2026-10-12; proto-test "EDIT on an FX value
// auditions the row with it"): the row's note, else the nearest above, with the row's sound commands, while EDIT is held
{
    const T = rig();
    T.note(4); T.fx(4, 0, 'ATK', 0xFF); T.note(8); T.fx(8, 0, 'ATK', 1); T.fx(10, 0, 'ATK', 1); T.fx(1, 0, 'ATK', 1);   // (01: EDIT on a sound command's 00 pastes)
    const held = (r, k = null) => { T.to(r, 4); T.go(1500); T.hold('edit'); if (k) T.press(k); const a = T.record(300); T.free('edit'); return a; };
    const slow = held(4), fast = held(8), below = held(10), none = held(1), step = held(8, 'right');
    const e = a => rms(a, 0, 4410);             // (the first 100 ms of the hold: the note started with it)
    ok(rms(fast) > 0.001 && e(slow) < e(fast) / 3, `EDIT on an FX value: the row heard, with its ATK (first 100 ms: ATK FF ${e(slow).toFixed(4)}, ATK 01 ${e(fast).toFixed(4)})`);
    ok(rms(below) > 0.001 && rms(none) < 0.0005, `a row without a note: the note above (rms ${rms(below).toFixed(4)}); none above: silent (${rms(none).toFixed(4)})`);
    ok(rms(step) > 0.001, `EDIT + right on an FX value: heard (rms ${rms(step).toFixed(4)})`);
}

// ---- EDIT + PLAY: the preview while PLAY is held (app.js previewRow / auditionStart, audio.js audition)
{
    const T = rig();
    const preview = (ms, after = 300) => { T.hold('edit'); T.hold('play'); const on = T.record(ms); T.free('play'); T.free('edit'); return [on, T.record(after)]; };
    T.note(0);                                   // row 0: C-3, instrument 0
    T.with_('shift', 'right'); T.press('right');   // I: PRESET ACID -> SUB BASS (dry, sustained: a held note stays)
    T.hold('edit'); T.press('right', 3); T.free('edit'); T.press('left'); T.with_('shift', 'left'); T.to(0, 0);
    const w = (a, t0, t1) => rms(a, t0 * 44100, t1 * 44100);
    T.hold('edit'); T.hold('play'); T.free('edit'); const p = T.record(800); T.free('play');   // P: no EDIT + PLAY (picks review
    // #4): EDIT's audition only, it ends with EDIT though PLAY stays held
    T.with_('shift', 'right');                   // I: the instrument, the last note, held while PLAY is
    const [on, off] = preview(800);
    T.hold('edit'); T.free('edit'); const blip = T.record(800);   // EDIT alone (the I page: no audition)
    T.with_('shift', 'left'); T.to(0, 0);
    ok(w(p, .3, .5) < w(on, .6, .8) / 4 && T.playhead() === -1,
        `EDIT + PLAY in P: no preview, the song does not start (rms at 0.3 s ${w(p, .3, .5).toFixed(4)} vs I's ${w(on, .6, .8).toFixed(4)})`);
    ok(w(on, .6, .8) > w(on, .1, .3) / 2 && w(on, .6, .8) > 3 * w(blip, .6, .8),
        `EDIT + PLAY on I: held while PLAY is (rms at 0.6 s ${w(on, .6, .8).toFixed(4)}, EDIT alone ${w(blip, .6, .8).toFixed(4)})`);
    ok(w(off, .2, .3) < w(on, .6, .8) / 4, `PLAY let go: it ends (${w(off, .2, .3).toFixed(4)})`);
    const tap = () => { T.hold('edit'); T.free('edit'); return T.record(300); };   // EDIT alone: the plain audition
    const before = tap();
    T.fx(0, 0, 'PAN', 0); T.to(0, 4); T.hold('edit'); T.press('up', 16); T.free('edit'); T.to(0, 0);   // PAN FF: right
    T.press('play'); T.go(300); T.with_('shift', 'right');
    T.hold('edit'); T.hold('play'); T.go(200); T.free('play'); T.free('edit'); T.go(300);
    T.with_('shift', 'left');
    ok(T.playhead() >= 0, 'while playing: EDIT + PLAY on I previews, playback goes on');
    T.press('play'); T.to(0, 0);
    // in Q (step Q-3e, app.js previewQ): the line, its note and sound commands, the instrument shown; EDIT no jump
    const px = (x, y) => T.m.fb()[y * 240 + x];
    T.fx(2, 0, 'VOL', 0x40); T.to(0, 0);         // row 2: a line with no note
    T.btn(11);   // SEQ
    const [q, qoff] = preview(800);
    ok(mapLit(px, 'Q') && w(q, .6, .8) > w(on, .6, .8) / 4 && w(q, .1, .3) < w(on, .1, .3) / 2 && w(qoff, .2, .3) < w(q, .6, .8) / 2,   // (PAN FF: the left channel is quiet, its tail weighs more)
        `EDIT + PLAY in Q: the line's note held while PLAY is, its PAN FF heard (left rms ${w(q, .1, .3).toFixed(4)} vs ${w(on, .1, .3).toFixed(4)}), it ends, still in Q`);
    T.hold('edit'); T.free('edit'); T.go(300); T.to(0, 0);   // EDIT in Q: to the line's row in P
    const after = tap();                         // the part has the instrument's own PAN again
    ok(Math.abs(rms(after) / rms(before) - 1) < 0.2, `then the instrument's own values (audition rms ${rms(after).toFixed(4)}, before ${rms(before).toFixed(4)})`);
    T.btn(11); T.press('down'); T.go(1000);     // Q again, the next line (the audition's tail gone)
    const [none] = preview(400);
    ok(mapLit(px, 'Q') && rms(none, 0.05 * 44100) < 0.001, `on a line with no note: nothing (rms ${rms(none, 0.05 * 44100).toFixed(4)})`);
}

// ---- HOME: follow (app.js followState; HANDOFF §0 "Playback"): S / C / P track the cursor's channel while playing
{
    const T = rig(), px = (x, y) => T.m.fb()[y * 240 + x];
    const curRow = () => { for (let r = 0; r < 16; r++) if (px(cellX(PH_X[0]) - 1, rowY(r)) === ACCENT) return r; return -1; };
    T.note(0); T.note(4); T.note(8); T.to(0, 0);
    T.btn(8);
    ok(T.strip() === 'follow on', `HOME: ${T.strip()}`);
    T.press('play'); T.go(700);
    const r1 = curRow(), p1 = T.playhead(); T.go(400); const r2 = curRow(), p2 = T.playhead();
    ok(r1 === p1 && r2 === p2 && r2 !== r1, `the phrase cursor rides the playhead (${r1}=${p1}, ${r2}=${p2})`);
    // the wrap, 15 -> 0: row 15 is drawn plain again, nothing of the cursor or the playhead left (unit feedback #45)
    T.until(15); T.until(0);
    const when = T.m.at(), x0 = cellX(0) - 1;   // the panel gets row 15's playhead cleared before row 0's is drawn: never both lit
    ok(when[rowY(15) * 240 + x0] < when[rowY(0) * 240 + x0], `the wrap: row 15 goes out before row 0 (${when[rowY(15) * 240 + x0]} < ${when[rowY(0) * 240 + x0]})`);
    const lit15 = [...Array(6 * 30).keys()].filter(i => [ACCENT, PLAYC].includes(px(cellX(0) - 2 + i % 30, rowY(15) + (i / 30 | 0) * 2))).length;
    ok(lit15 === 0 && curRow() === 0, `the wrap: row 15 drawn plain (${lit15} cursor / playhead pixels), the cursor on row 0 (${curRow()})`);
    T.press('right'); T.press('left'); T.go(200);
    ok(curRow() === T.playhead(), `left / right keep following (${curRow()} = ${T.playhead()})`);
    T.press('up');
    ok(T.strip() === 'follow off', `up: ${T.strip()}`);
    const r3 = curRow(); T.go(500);
    ok(curRow() === r3 && T.playhead() !== r3, `then the cursor stays (${curRow()}, playhead ${T.playhead()})`);
    T.press('play');
    // the song view: two song rows, the cursor goes from one to the other
    T.with_('shift', 'left', 2); T.press('edit'); T.press('down'); T.press('edit');   // EDIT copies chain 00, then pastes it on row 01
    T.press('up'); T.btn(8);
    T.hold('shift'); T.press('play'); T.free('shift');               // the song, from row 00
    const songCur = r => px(cellX(3) - 1, rowY(r)) === ACCENT;
    T.go(1000); const s0 = songCur(0); T.go(2200); const s1 = songCur(1);
    ok(s0 && s1, `the song cursor follows the song row (row 00 ${s0}, then row 01 ${s1})`);
    T.press('play');
}

// ---- the LEDs (app.js leds; HANDOFF §0 "Keybed", §5 review 10)
const KEYMAP = (() => {                          // id -> [column, row bit], from the HAL's FM1_KEYMAP
    const src = readFileSync(join(ROOT, 'fw', 'firmware', 'hal', 'fm1_input.h'), 'utf8');
    const rows = [...src.match(/FM1_KEYMAP\[6\]\[FM1_NCOL\] = \{([\s\S]*?)\};/)[1].matchAll(/\{([^}]*)\}/g)].map(m => m[1].split(',').map(Number));
    const at = {};
    rows.forEach((row, r) => row.forEach((id, c) => { if (id >= 0) at[id] = [c, r]; }));
    return at;
})();
const ledOf = (mm, id) => { const [on, dim] = mm.leds(), [c, r] = KEYMAP[id]; return on[c] >> r & 1 ? 'on' : dim[c] >> r & 1 ? 'dim' : 'off'; };
{
    const T = rig();
    const led = id => ledOf(T.m, id);
    const key = k => led(14 + k);                // keybed key k (0 = F3)
    T.note(0); T.hold('edit'); T.press('right'); T.free('edit');   // row 0: C#3 (its key, C#4 = key 8, is no step)
    T.note(4);                                   // row 4: C#3 too (the last note)
    T.to(2, 0);
    ok(key(0) === 'dim' && key(26) === 'dim' && key(7) === 'off' && led(12) === 'dim' && led(13) === 'dim' && led(3) === 'dim',
        'the gamepad: its keys dim, the others off; PLAY, REC and SEL (scenes) dim');
    ok(led(11) === 'dim', 'SEQ dim (it opens Q; picks review #8)');
    ok(led(8) === 'dim', 'HOME dim'); T.btn(8); ok(led(8) === 'on', 'HOME lit while following'); T.btn(8);
    T.btn(13);                                   // notes mode: the white keys are the steps
    ok(led(13) === 'on' && key(0) === 'on' && key(7) === 'on' && key(4) === 'dim' && key(6) === 'off' && key(26) === 'off',
        `notes: steps 0 and 4 lit, the cursor's row 2 dim, row 3 off (${[0, 7, 4, 6].map(key).join()})`);
    T.press('play');
    T.until(1); T.go(20);
    ok(key(2) === 'on' && led(12) === 'on', `playing: the playhead's step (row 1) the opposite: ${key(2)}; PLAY lit`);
    T.until(4); T.go(30);
    ok(key(7) === 'off' && key(8) === 'on', `row 4: its lit step goes dark under the playhead, C#3 flashes C#4's key (${key(7)}, ${key(8)})`);
    T.go(150);
    ok(key(8) === 'off', 'the flash lasts 0.1 s');
    T.press('play');
    T.m.input(1 << 10, 0); T.go(50);
    ok(key(10) === 'on', 'a note key held: lit');
    T.m.input(0, 0); T.go(25);
    T.btn(1);
    ok(led(1) === 'on' && led(0) === 'dim', 'OCT+: lit off octave 4');
    T.btn(0);
    T.btn(13);                                   // armed: REC blinks
    const seen = new Set(); for (let t = 0; t < 600; t += 50) { seen.add(led(13)); T.go(50); }
    ok(seen.has('on') && seen.has('off'), `armed: REC blinks (${[...seen].join()})`);
    T.btn(13);
    ok(led(13) === 'dim' && key(0) === 'dim', 'the gamepad again: REC dim, the gamepad keys dim');
}

// ---- saving (docs/flash-map.md): SAVE, STOP TO SAVE, the A / B copies, a reboot, a damaged copy
{
    const head = (st, copy) => {                 // slot 0's copy: its header (magic "KRPJ", seq, len, raw)
        const v = new DataView(st.buffer, st.byteOffset + copy * 7 * 4096, 32);
        return { magic: v.getUint32(0, true) === 0x4A50524B, seq: v.getUint32(8, true), len: v.getUint32(12, true), raw: v.getUint32(16, true) };
    };
    const payload = (st, copy, len) => [...Array(len).keys()].map(i => st[copy * 7 * 4096 + pjAt(i)]);   // the sectors' pieces
    const T = rig(), note = r => T.cell(2, r, 3);
    const store = T.store, led = id => ledOf(T.m, id);
    T.note(0); T.note(4); T.to(4, 0); T.hold('edit'); T.press('up'); T.free('edit'); T.to(8, 0);   // C-3, C-4
    T.with_('shift', 'right'); T.press('down');  // I0's LEVEL, three steps down
    const lvl = () => readText(T.m.fb(), 4, IROW_Y(1) + 14, 8).trim();
    T.hold('edit'); T.press('left', 3); T.free('edit');
    const level = lvl();
    T.with_('shift', 'left');
    ok(led(9) === 'on', `unsaved changes: SAVE lit (${led(9)})`);
    T.press('play'); T.btn(9);
    ok(T.strip() === 'stop to save', `SAVE while playing: ${T.strip()}`);
    T.press('play'); T.go(100);
    T.btn(9);
    const st1 = store(), h1 = head(st1, 0);
    ok(T.strip() === 'saved to 00' && led(9) === 'dim' && h1.magic && h1.seq === 1 && h1.len < 3584 && !head(st1, 1).magic,
        `SAVE: ${T.strip()}, SAVE dim; copy A, seq 1, ${h1.len} B packed from ${h1.raw} (one sector)`);
    ok(lz4(payload(st1, 0, h1.len)).length === h1.raw, 'what was stored is a valid LZ4 block of the project');
    // a reboot with that flash: the song and the instrument come back, nothing to undo
    const R = rig(st1), rnote = r => R.cell(2, r, 3);
    ok(rnote(0) === 'C-3' && rnote(4) === 'C-4', `after a reboot: the phrase is back (${rnote(0)} ${rnote(4)})`);
    R.with_('shift', 'right'); R.press('down');
    ok(readText(R.m.fb(), 4, IROW_Y(1) + 14, 8).trim() === level, `... and I0's LEVEL (${level})`);
    R.panel(0);
    ok(R.strip() === 'nothing to undo', 'a loaded project has no undo history');
    // a second save goes to copy B; a damaged B: the older A is loaded
    T.to(8, 0); T.hold('edit'); T.press('down'); T.free('edit');   // (an edit: row 8 gets a note)
    T.press('edit'); T.to(9, 0);
    T.btn(9);
    const st2 = store(), h2 = head(st2, 1);
    ok(h2.magic && h2.seq === 2 && head(st2, 0).seq === 1, `the second save: copy B, seq 2 (A kept, seq ${head(st2, 0).seq})`);
    const R2 = rig(st2);
    ok(R2.cell(2, 8, 3) !== '---', `a reboot: the newer copy (B), row 8 = ${R2.cell(2, 8, 3)}`);
    const bad = st2.slice(); bad[7 * 4096 + 256 + 10] ^= 0xFF;   // one payload byte of B damaged
    const R3 = rig(bad);
    ok(R3.cell(2, 8, 3) === '---' && R3.cell(2, 0, 3) === 'C-3', 'B damaged: its CRC fails, the older copy A is loaded');
    const R4 = rig(new Uint8Array(st2.length).fill(0xFF));
    ok(R4.cell(2, 0, 3) === '---', 'an erased flash: a new song');
}

// ---- the G page (render.js viewProj; app.js slotEdit / slotOption / confirmThen; HANDOFF §0 "Projects")
{
    const G = T => {                             // reading the G page of machine T
        const fb = () => T.m.fb();
        return {
            strip: () => T.strip(),
            top: () => readText(fb(), 4, 4, 24).trimEnd(),
            slot: r => T.cell(3, r, 13).trimEnd(),
            // the open slot = the title's number; no star in the list (user 2026-10-11, principles-review.md #7)
            open: r => readText(fb(), 4, 4, 10) === 'PROJECT ' + r.toString(16).toUpperCase().padStart(2, '0') && [...Array(16).keys()].every(k => T.cell(17, k, 1) !== '*'),
            right: line => T.cell(21, line, 12).replace(/\s+/g, ' ').trim(),
            btn: T.btn,
            key: T.key,
        };
    };
    const HELD = rgb565('#ffc23d');
    const T = rig(), g = G(T);
    T.note(0);                                   // phrase 00, row 0: C-3
    T.with_('shift', 'up');                      // P -> G
    ok(g.top() === 'PROJECT 00  UNTITLED*' && g.slot(1) === '--' && g.open(0) && g.right(3) === 'TEMPO 120' && g.right(9) === 'QNT FEEL'
        && g.right(11) === 'PHR 1/128', `SHIFT + up from P: G (${g.top()}; slot 01 ${g.slot(1)}; ${g.right(3)}, ${g.right(9)}, ${g.right(11)})`);
    T.shot('21-proj');   // (mockup proj)
    T.press('down'); T.press('right');           // slot 01 -> NAME (left / right keep the line)
    T.press('edit');                             // its first letter: the last one, A
    g.btn(9);                                    // SAVE: into the open slot
    ok(g.strip() === 'saved to 00' && g.slot(0) === 'A' && g.top() === 'PROJECT 00  A', `SAVE: ${g.strip()}, slot 00 = ${g.slot(0)}, ${g.top()}`);
    T.press('left', 2); T.press('down');         // (the name's letters first) slot 01, slot 02
    T.press('edit');
    ok(g.strip() === 'empty slot: EDIT EDIT = new', `EDIT on an empty slot: ${g.strip()}`);
    T.go(400); T.press('edit', 2);               // EDIT EDIT: a new project there
    ok(g.strip() === 'new project 02' && g.open(2) && !g.open(0) && g.top() === 'PROJECT 02  UNTITLED' && g.right(11) === 'PHR 0/128',
        `EDIT EDIT: ${g.strip()}, open 02, ${g.right(11)}`);
    T.press('up', 2); T.press('edit');           // load 00 (nothing unsaved: at once)
    ok(g.strip() === 'loaded 00' && g.right(11) === 'PHR 1/128' && g.top() === 'PROJECT 00  A', `EDIT on slot 00: ${g.strip()}, ${g.right(11)}`);
    T.press('down', 3); T.press('right');        // slot 03 -> TEMPO
    T.with_('edit', 'right');                    // 121: unsaved
    T.press('left'); T.press('up'); T.press('edit');   // slot 02
    const amber = (() => { const f = T.m.fb(); for (let i = STRIP_Y * 240; i < 238 * 240; i++) if (f[i] === HELD) return true; return false; })();
    T.shot('21-proj-confirm');   // (mockup proj-confirm)
    ok(g.strip() === 'unsaved changes: EDIT to load 02' && amber && g.top() === 'PROJECT 00  A*', `unsaved changes: ${g.strip()} (amber ${amber})`);
    T.press('down');
    ok(g.strip() === 'cancelled' && g.open(0), `any other key cancels: ${g.strip()}`);
    // copy the open project (as it is now: TEMPO 121, unsaved) into 03, then over 02
    T.press('up', 3); T.with_('shift', 'option');
    ok(g.strip() === 'select: OPTION copies', g.strip());
    T.with_('shift', 'option'); T.press('option');
    ok(g.strip() === 'select: OPTION copies', `SHIFT + OPTION again: the slot unmarked, quietly; OPTION copies nothing (${g.strip()})`);
    T.with_('shift', 'option');
    T.press('option');
    ok(g.strip() === 'copied project 00', g.strip());
    T.press('down', 3); T.with_('shift', 'edit');
    ok(g.strip() === 'pasted to 03' && g.top() === 'PROJECT 00  A*', `SHIFT + EDIT on an empty slot: ${g.strip()} (00 still open, unsaved)`);
    T.press('up'); T.with_('shift', 'edit');
    ok(g.strip() === 'replace 02? EDIT to confirm', `over a project: ${g.strip()}`);
    T.press('edit');
    ok(g.strip() === 'pasted to 02', g.strip());
    // copy a saved slot (02: its record sector by sector) into 01, then open 01
    T.with_('shift', 'option'); T.press('option');
    T.press('up'); T.with_('shift', 'edit');
    T.press('down');
    ok(g.slot(1) === 'A' && g.slot(2) === 'A' && g.slot(3) === 'A', `pasted: slots ${[1, 2, 3].map(g.slot).join(' / ')}`);
    T.press('up'); T.press('edit'); T.press('edit');   // load 01: asks (unsaved), EDIT confirms
    ok(g.strip() === 'loaded 01' && g.right(3) === 'TEMPO 121' && g.right(11) === 'PHR 1/128' && g.top() === 'PROJECT 01  A',
        `the copy of a copy loads: ${g.strip()}, ${g.right(3)}, ${g.right(11)}`);
    T.press('down', 2); T.with_('option', 'edit');
    ok(g.strip() === 'delete 03? EDIT to confirm', g.strip());
    T.press('edit');
    const del = g.strip();
    T.press('up');
    ok(g.slot(3) === '--' && del === 'deleted 03', `deleted: ${del}, slot 03 ${g.slot(3)}`);
    // a reboot reopens the last slot; GLO: the G page and back
    const R = rig(T.store(), T.settings()), h = G(R);
    R.with_('shift', 'left', 2);                 // P -> C -> S
    h.btn(7);
    ok(h.top() === 'PROJECT 01  A' && h.open(1), `a reboot opens slot 01 again; GLO: G (${h.top()})`);
    h.btn(7);
    ok(h.top().startsWith('CH1>S00'), `GLO again: back to S (${h.top()})`);
    h.btn(7);
    // while playing: asks, then waits for the phrase end (2 s at 120), stops and loads; PLAY: at once
    R.press('play'); R.go(300);
    R.press('edit');                             // slot 00
    ok(h.strip() === 'EDIT: load 00 at phrase end', `EDIT while playing: ${h.strip()}`);
    R.press('edit');
    R.go(500);
    ok(h.strip() === 'load 00 at phrase end (PLAY: now)', `confirmed: ${h.strip()}`);
    R.go(1800);
    ok(h.strip() === 'loaded 00' && h.open(0), `at the phrase end: ${h.strip()}`);
    R.press('play'); R.go(300);
    R.press('down', 2); R.press('edit'); R.press('edit');   // slot 02
    R.go(200); R.press('play'); R.go(50);
    ok(h.strip() === 'loaded 02' && h.open(2), `PLAY while it waits: stops, loads at once (${h.strip()})`);
}
{
    // the settings at work: STEP 2 in step entry; METRO ON; COUNT 2BAR and QNT ROW when recording; SWING
    const T = rig();
    const toG = () => T.with_('shift', 'up'), toP = () => T.with_('shift', 'down');   // P <-> G
    const quiet = rms(T.playRecord(800)); T.press('play');   // an empty phrase: nothing (METRO REC: only recording)
    toG(); T.press('down', 3); T.press('right'); T.press('down', 4);   // slot 03, TEMPO, .. METRO
    T.with_('edit', 'right');                    // METRO ON
    toP();
    const clicks = rms(T.playRecord(800)); T.press('play');
    ok(quiet < 0.001 && clicks > 0.005, `METRO ON: the metronome while playing (rms ${quiet.toFixed(3)} -> ${clicks.toFixed(3)})`);
    toG(); T.press('up', 3);                     // METRO -> TUNE SWING STEP
    T.with_('edit', 'right');                    // STEP 2
    toP();
    T.btn(13); T.key(7); T.key(7); T.btn(13); T.btn(13);   // notes mode: two notes, step entry moves 2 rows; back to the gamepad
    ok(T.cell(2, 0, 3) !== '---' && T.cell(2, 1, 3) === '---' && T.cell(2, 2, 3) !== '---'
        && T.m.fb()[rowY(4) * 240 + cellX(PH_X[0]) - 1] === ACCENT, `STEP 2: rows 0 and 2 written, the cursor on row 4 (${[0, 1, 2].map(r => T.cell(2, r, 3)).join(' ')})`);
    toG(); T.press('down', 4); T.with_('edit', 'right');   // STEP -> .. COUNT: 2BAR
    T.press('down'); T.with_('edit', 'left');    // QNT ROW
    T.press('up', 4); T.with_('edit', 'up', 5);   // SWING 50 %
    toP();
    T.btn(13); T.btn(13); T.press('play');       // armed, PLAY: 2 bars of count-in (4 s at 120)
    T.go(3000);
    const early = T.playhead();
    T.until(4); T.go(85);                        // past the middle of row 4: QNT ROW writes row 5
    T.key(9);
    T.btn(13); T.press('play');
    ok(early === -1 && T.cell(2, 5, 3) === 'D-4' && T.cell(11, 5, 3) === '---' && T.cell(2, 4, 3) === '---',
        `COUNT 2BAR: no row after 3 s (${early}); QNT ROW: a late hit on row 4 is written on row 5, no DEL (${T.cell(2, 5, 3)} ${T.cell(11, 5, 3)})`);
    T.press('play'); T.until(0); T.go(150);      // (the finder looks unshifted: even rows only)
    const swung = T.m.fb()[rowY(1) * 240 + cellX(0) - 1] !== PLAYC && T.m.fb()[rowY(1) * 240 + cellX(0) + 2] === PLAYC;
    const bar = readText(T.m.fb(), 4 + 22 * 6, 4, 4).trim();
    T.press('play');
    ok(swung && bar === '', `SWING: an odd row's playhead shifted right, no SWxx in the bar (song swing, not SWG: '${bar}')`);
}
for (const ver of [2, 3, 4, 5, 6, 7]) {
    // a version 2 project (before the G page), a version 3 one (before the knob slots) and a version 4 one (before the
    // mixer's levels) still load: what they lack gets its defaults (the G page's settings; the knob slots: the engine's
    // HOME knobs; the levels: -4.0dB; version 5, before the effects: Felucca's defaults; version 6, before the scenes:
    // empty scenes, the fader at A; version 7, before G's MUTE: NOTE)
    const T = rig();
    T.note(0);
    T.btn(9);   // SAVE (version 8, slot 00 copy A)
    const st = T.store(), len = new DataView(st.buffer).getUint32(12, true);
    const raw = lz4([...Array(len).keys()].map(i => st[pjAt(i)]));
    const SZ = 1 + 64 * 4 + 64 * 16 * 2 + 128 * 16 * 9 + 8, PJ = 13 + 6, KN = 16 * 4, LV = 4, FX = 9, SC = 8 * 8 * 3 + 3, MM = 1;   // S up to the mixer, S.pj, S.knob, S.lvl, S.fx, S.scn, S.mute_mode
    const old = raw.slice(0, [SZ, SZ + PJ, SZ + PJ + KN, SZ + PJ + KN + LV, SZ + PJ + KN + LV + FX, SZ + PJ + KN + LV + FX + SC][ver - 2]).concat(raw.slice(SZ + PJ + KN + LV + FX + SC + MM));
    const lz = [0xF0]; for (let n = old.length - 15; ; n -= 255) { if (n < 255) { lz.push(n); break; } lz.push(255); }
    lz.push(...old);                             // one literal run: a valid LZ4 block
    const out = st.slice(); out.fill(0xFF, 0, 14 * 4096);
    const hd = new DataView(out.buffer);
    hd.setUint32(0, 0x4A50524B, true); out[4] = ver; out[5] = 0; out[6] = st[6]; out[7] = st[7];
    hd.setUint32(8, 1, true); hd.setUint32(12, lz.length, true); hd.setUint32(16, old.length, true); hd.setUint32(20, PNG.crc32(Buffer.from(lz)), true); hd.setUint32(24, 0, true);
    if (ver === 2) hd.setUint32(28, PNG.crc32(out.slice(0, 28)), true);
    else { out.fill(0, 28, 44); hd.setUint32(44, PNG.crc32(out.slice(0, 44)), true); }   // (no name)
    lz.forEach((v, i) => { out[pjAt(i)] = v; });
    const R = rig(out);
    ok(R.cell(2, 0, 3) === 'C-3', `a version ${ver} project loads (row 0: ${R.cell(2, 0, 3)})`);
    if (ver === 2) {
        R.with_('shift', 'up');
        const right = line => R.cell(21, line, 12).replace(/\s+/g, ' ').trim();
        ok(readText(R.m.fb(), 4, 4, 24).trimEnd() === 'PROJECT 00  UNTITLED' && right(4) === 'STEP 1' && right(7) === 'METRO REC' && right(8) === 'COUNT 1BAR',
            `... with the G page's defaults (${right(4)}, ${right(7)}, ${right(8)})`);
    }
    const K = rig(out);
    K.with_('shift', 'right');                   // I0 (ANALOG): the KNOBS row
    const names = [0, 1, 2, 3].map(n => readText(K.m.fb(), 4 + n * 48 + 16, KNOBS_Y, 3)).join(' ');
    ok(names === 'CUT RES ATK REL', `... with the engine's knob slots (${names})`);
    K.with_('shift', 'left'); K.with_('shift', 'down');   // F: the defaults
    const fxv = [0, 1, 2].map(r => readText(K.m.fb(), 4 + 9 * 6, [48, 110, 172][r] + 12, 6).trim()).join();
    ok(fxv === '47%,71%,47%', `... with the effects' defaults (FDBK, SIZE, DEPTH: ${fxv})`);
    K.with_('shift', 'down'); K.press('down');   // X, the cursor off the levels
    const lv = [0, 1, 2, 3].map(k => K.cell(2 + 8 * k, 11, 7).trim()).join();
    ok(lv === '-4.0dB,-4.0dB,-4.0dB,-4.0dB', `... with the mixer's levels (${lv})`);
}

// ---- X, the mixer (render.js viewMix, app.js grids.mix, mockup mix): a level bar, LEVEL, MUTE, SOLO per channel
{
    const T = rig(), px = (x, y) => T.m.fb()[y * 240 + x];
    const top = () => readText(T.m.fb(), 4, 4, 24).trimEnd();
    const lvl = k => T.cell(2 + 8 * k, 11, 7).trim();
    // MUTE / SOLO always upper case, on = white, off = dim (user 2026-10-11, principles-review.md #10): read as 'MUTE' / 'mute'
    const fld = (k, j) => { const t = T.cell(2 + 8 * k, 13 + j, 4); let on = false;
        for (let y = 0; y < 12; y++) for (let x = 0; x < 24; x++) if (px(4 + (2 + 8 * k) * 6 + x, 32 + (13 + j) * 12 + y) === rgb565('#e8e8e8')) on = true;
        return t === (j ? 'SOLO' : 'MUTE') ? (on ? t : t.toLowerCase()) : t; };
    const bar = k => { let n = 0; for (let y = 32; y < 32 + 118; y++) if (px(4 + (3 + 8 * k) * 6 + 2, y) === rgb565('#4fa3ff')) n++; return n; };
    T.note(0);                                   // phrase 00 row 0, channel 1
    T.with_('shift', 'down', 2);
    ok(top() === 'MIXER' && mapLit(px, 'X', 2), `SHIFT + down twice from P: F, then X, drawn under P (${top()})`);
    ok([1, 2, 3].map(lvl).join() === '-4.0dB,-4.0dB,-4.0dB' && fld(1, 0) === 'mute' && fld(1, 1) === 'solo',
        `levels at -4.0dB, mute / solo off (${[1, 2, 3].map(lvl).join()} ${fld(1, 0)} ${fld(1, 1)})`);
    ok(bar(0) === 97, `channel 1's bar: 104 / 127 of 118 px (${bar(0)})`);
    T.shot('27-mix');   // (mockup mix)
    // 0 dB marked on each bar (user 2026-10-11, principles-review.md #9): a text-colour tick at level 112, 2 px past each side
    ok([0, 1, 2, 3].every(k => [-2, 9].every(d => px(4 + (3 + 8 * k) * 6 + 2 + d, 32 + 118 - 104) === rgb565('#e8e8e8'))), 'X: a 0 dB tick across each bar (y 46)');
    const loud = rms(T.playRecord(600)); T.press('play');
    T.hold('edit'); T.press('down', 2); T.free('edit');   // 104 - 16 = 88: -12.0dB
    T.press('right');
    ok(lvl(0) === '-12.0dB' && bar(0) === 82, `EDIT + down twice: -8 each (${lvl(0)}, bar ${bar(0)} px); right: channel 2`);
    T.panel(0);
    ok(T.strip() === 'undo MIXER' && lvl(0) === '-4.0dB', `undo: one step (${T.strip()}, ${lvl(0)})`);
    T.panel(1); T.press('left');
    const soft = rms(T.playRecord(600)); T.press('play');
    ok(soft > loud * 0.3 && soft < loud * 0.5, `heard 8 dB lower (rms ${loud.toFixed(3)} -> ${soft.toFixed(3)})`);
    T.hold('option'); T.press('edit'); T.free('option');   // OPTION + EDIT: the default
    T.press('right'); ok(lvl(0) === '-4.0dB', `OPTION + EDIT: -4.0dB again (${lvl(0)})`); T.press('left');
    T.press('down'); T.press('edit');            // MUTE
    T.press('right');
    ok(fld(0, 0) === 'MUTE' && bar(0) === 0, `EDIT on mute: MUTE, the bar grey (${fld(0, 0)})`);
    T.press('left');
    const muted = rms(T.playRecord(600)); T.press('play');
    ok(muted < loud * 0.05, `muted: silent (rms ${muted.toFixed(4)})`);
    T.press('down'); T.press('edit');            // SOLO: removes the mute (unit feedback #17)
    T.press('right');
    ok(fld(0, 0) === 'mute' && fld(0, 1) === 'SOLO', `SOLO removes the mute (${fld(0, 0)} ${fld(0, 1)})`);
    T.press('left'); T.press('up'); T.go(400); T.press('edit');   // MUTE: removes the solo
    T.press('right');
    ok(fld(0, 0) === 'MUTE' && fld(0, 1) === 'solo', `MUTE removes the solo (${fld(0, 0)} ${fld(0, 1)})`);
    T.press('left'); T.go(400); T.press('edit');  // mute off
    T.press('up');
    T.hold('edit'); ok(T.strip() === 'channel level, 0dB = full', `EDIT held on a level: ${T.strip()}`); T.free('edit'); T.hold('edit'); T.press('up'); T.free('edit');   // 112: 0.0dB, then saved
    T.btn(9);
    const R = rig(T.store(), T.settings());
    R.with_('shift', 'down', 2); R.press('right');
    ok(R.cell(2, 11, 7).trim() === '0.0dB', `saved and reloaded: ${R.cell(2, 11, 7).trim()}`);
    R.with_('shift', 'up', 2);
    ok(readText(R.m.fb(), 4, 4, 3) === 'CH2' && mapLit((x, y) => R.m.fb()[y * 240 + x], 'P'), `SHIFT + up from X: P again, on channel 2 (${readText(R.m.fb(), 4, 4, 8)})`);
}

// ---- F, the effects (render.js viewFx / FX_FIELDS, app.js grids.fx; mockup fx): Felucca's delay, reverb, chorus
{
    const T = rig(), px = (x, y) => T.m.fb()[y * 240 + x];
    const top = () => readText(T.m.fb(), 4, 4, 24).trimEnd();
    const FY = [48, 110, 172];                   // each frame's label line (render.js FX_PAGE rowY)
    const fld = (x, r) => readText(T.m.fb(), 4 + x * 6, FY[r] + 12, 7).trim(), lbl = (x, r) => readText(T.m.fb(), 4 + x * 6, FY[r], 6).trim();
    T.note(0);
    T.with_('shift', 'down');
    ok(top() === 'EFFECTS' && mapLit(px, 'F', 2), `SHIFT + down from P: F, drawn under P (${top()})`);
    const labels = [[1, 0], [9, 0], [17, 0], [25, 0], [1, 1], [9, 1], [17, 1], [1, 2], [9, 2]].map(([x, r]) => lbl(x, r)).join(' ');
    ok(labels === 'TIME FDBK COLOUR MIX TYPE SIZE DAMP RATE DEPTH', `the fields (${labels})`);
    const vals = [[9, 0], [17, 0], [25, 0], [1, 1], [9, 1], [17, 1], [1, 2], [9, 2]].map(([x, r]) => fld(x, r)).join(' ');
    ok(vals === '47% 55% 71% ROOM 71% 47% 0.41Hz 47%', `Felucca's defaults (${vals})`);
    T.shot('28-fx');   // (mockup fx)
    T.press('right', 3); T.press('down');
    T.press('left'); ok(fld(17, 1) === '47%', 'MIX, down: DAMP (the nearest), left: SIZE');
    T.press('right'); T.press('down'); T.press('up', 2); T.press('left', 3);   // DEPTH, then TIME
    const top0 = readText(T.m.fb(), 4 + 20 * 6, 32, 4);   // the top right before the list (where it used to open)
    T.hold('edit');
    T.shot('28-fx-list');
    const under = readText(T.m.fb(), 4 + 6, FY[0] + 2 * 12 + 3, 4), atTop = readText(T.m.fb(), 4 + 20 * 6, 32, 4) === top0;
    ok(T.strip() === 'delay time, in steps of the tempo' && under === '4BAR' && atTop,
        `EDIT held on TIME: the list right under the cell (unit feedback #13: ${under}; the top right untouched: ${atTop}) and the help (${T.strip()})`);
    T.press('down'); T.free('edit');
    T.press('right'); ok(fld(1, 0) === '8T', `EDIT + down in the list: the next longer-to-shorter, 8T (${fld(1, 0)})`);
    T.hold('edit'); T.press('up'); T.free('edit');   // FDBK + 15 (an eighth of 0..120)
    T.press('left'); ok(fld(9, 0) === '59%', `EDIT + up on FDBK: +15 (${fld(9, 0)})`);
    T.panel(0);
    ok(T.strip() === 'undo EFFECTS' && fld(9, 0) === '47%', `undo: ${T.strip()} (${fld(9, 0)})`);
    T.press('right'); T.hold('option'); T.press('edit'); T.free('option'); T.press('left');
    ok(fld(9, 0) === '47%', `OPTION + EDIT: the default (${fld(9, 0)})`);
    T.press('right'); T.hold('edit'); T.press('down', 2); T.free('edit');   // FDBK 30: 24%
    T.btn(9);
    const R = rig(T.store(), T.settings());
    R.with_('shift', 'down');
    ok(readText(R.m.fb(), 4, 4, 7) === 'EFFECTS', 'after a reboot: F');
    R.press('right', 2); const fb9 = readText(R.m.fb(), 4 + 9 * 6, 48 + 12, 7).trim();
    ok(fb9 === '24%', `saved and reloaded: FDBK ${fb9}`);
    // the panel FX on V: F (app.js 'jump')
    R.with_('shift', 'up'); R.with_('shift', 'right'); R.with_('shift', 'down', 3);   // P, I, E L V
    R.btn(2);
    ok(mapLit((x, y) => R.m.fb()[y * 240 + x], 'F', 2), 'panel FX on V: F, drawn under P');
}

// ---- a boost reads +dB (user 2026-10-11, principles-review.md #9; felucca.js fmt 'db'): X and the I page's LEVEL
{
    const T = rig();
    T.with_('shift', 'down', 2);                 // P -> F -> X
    T.hold('edit'); T.press('up', 2); T.free('edit');   // 104 + 16 = 120: +4.0dB
    T.press('right');
    const x = T.cell(2, 11, 7).trim();
    T.with_('shift', 'up', 2); T.with_('shift', 'right');   // X -> F -> P -> I0
    T.press('down');                             // LEVEL (instrument 0's own, at -4.0dB)
    T.hold('edit'); T.press('up', 2); T.free('edit');   // (its own steps: past 0 dB)
    const i = readText(T.m.fb(), 4, 72 + 12, 6);
    ok(x === '+4.0dB' && /^\+\d\.\ddB$/.test(i), `a boost reads +: X ${x}, I's LEVEL ${i}`);
}

// ---- scenes (docs/ux/scenes.md; model.js morph, app.js crossfade / sceneKey / sceneSet): SEL held = edits go into the
// scene on the fader's near side; the crossfader on SELECT morphs A -> B (absolute: a side without the value plays the
// song's); SCN xx moves it from a phrase; SEL + the middle white keys pick the scenes; saved (format 7)
{
    const T = rig(), SEL = 3;
    const xfade = n => T.xf(n), heard = () => { T.go(1500); const r = rms(T.playRecord(600)); T.press('play'); return r; };
    T.note(0);
    T.with_('shift', 'down', 2);                 // X, channel 1's level
    const loud = heard();
    T.button(SEL, true);
    ok(T.strip() === 'scene 1 (A): edits go into the scene', `SEL: the bottom line names the near scene (${T.strip()})`);
    T.hold('edit'); T.press('down', 13); T.free('edit');   // into scene 1: CH1's level 104 -> 0 (off)
    T.button(SEL, false);
    const atA = heard();
    ok(atA < loud * 0.05, `scene 1 on A holds CH1 off: silent at A (rms ${loud.toFixed(3)} -> ${atA.toFixed(4)})`);
    T.panel(0);
    ok(T.strip() === 'undo SCENES', `undo: ${T.strip()}`);
    ok(heard() > loud * 0.9, 'undone: heard again');
    T.panel(1);
    xfade(32);                                   // 32 detents of 4: B
    const atB = heard();
    ok(Math.abs(atB - loud) < loud * 0.1, `at B: scene 2 holds nothing, the song's level plays (rms ${atB.toFixed(3)})`);
    xfade(-8);                                   // 127 - 32 = 95: three quarters to B
    const mid = heard();
    ok(mid > loud * 0.1 && mid < loud * 0.5, `in between: the level blended (rms ${mid.toFixed(3)})`);
    T.hold('shift'); xfade(-1); T.free('shift');   // SHIFT + a turn: snaps to A
    ok(heard() < loud * 0.05, 'SHIFT + a turn: at A again at once');
    // SCN FF on the phrase: the fader to B where the row plays (model.js 'S', audio.js)
    T.with_('shift', 'up', 2);
    T.fx(0, 0, 'SCN', 0xFF);
    ok(T.cell(11, 0, 3) === 'SCN', `SCN in the FX grid (${T.cell(11, 0, 3)})`);
    T.to(0, 4); T.hold('edit');
    ok(T.strip() === 'SCN FF = scene B', `EDIT held on its value: ${T.strip()}`);
    T.free('edit');
    T.with_('option', 'edit'); T.hold('edit');
    ok(T.strip() === 'SCN 00 = scene A', `... 00: ${T.strip()}`);
    T.free('edit'); T.with_('edit', 'up', 4);
    T.hold('edit'); ok(T.strip() === 'SCN 40 = 25% toward B', `... 40: ${T.strip()}`); T.free('edit');
    T.fx(0, 0, 'SCN', 0xFF);
    ok(heard() > loud * 0.8, 'SCN FF plays: the fader at B, heard');
    // saved: the scenes, the fader where SCN left it (B)
    T.btn(9);
    const R = rig(T.store(), T.settings());
    const rHeard = () => { R.go(1500); const r = rms(R.playRecord(600)); R.press('play'); return r; };
    R.xf(-32); R.go(30);                   // to A: scene 1's CH1 off, kept
    R.to(0, 3); R.with_('option', 'edit'); R.to(0, 0);   // (no SCN: the fader stays where it is)
    ok(rHeard() < loud * 0.05, 'saved and reloaded: scene 1 still holds CH1 off (silent at A)');
    // SEL + a scene key: the far side gets it (the fader at A: B), SHIFT + key: the near side
    R.button(SEL, true);
    R.key(9);
    ok(R.strip(14) === 'scene 3 on B', `SEL + D4: ${R.strip(14)}`);
    ok(ledOf(R.m, 3) === 'on' && ledOf(R.m, 14 + 6) === 'on' && ledOf(R.m, 14 + 9) !== 'dim' && ledOf(R.m, 14 + 7) === 'dim' && ledOf(R.m, 14 + 18) === 'dim',
        'SEL held: SEL lit, scene 1 (A, near) lit, scene 3 (B) blinking, the others dim');
    R.hold('shift'); R.key(11); R.free('shift');
    ok(R.strip(14) === 'scene 4 on A', `SEL + SHIFT + E4: ${R.strip(14)}`);
    R.button(SEL, false);
    ok(rHeard() > loud * 0.9, 'scene 4 on A holds nothing: heard at A');
    R.button(SEL, true); R.key(6); R.button(SEL, false);   // (far side: B) scene 1 on B, the fader to B
    R.xf(32); R.go(30);
    ok(rHeard() < loud * 0.05, 'scene 1 on B: silent at B');
    R.with_('shift', 'down', 2);                 // X: SEL + OPTION + EDIT takes the level out of the scene
    R.button(SEL, true); R.with_('option', 'edit'); R.button(SEL, false);
    ok(rHeard() > loud * 0.9, 'SEL + OPTION + EDIT: out of the scene, the song\'s level again');
}

// ---- the scene strip (docs/ux/scenes.md 6, render.js sceneStrip / faderMark, app.js sceneStripKey / sceneJump; mockups
// scene-strip, scene-strip-stuck, scene-strip-full): turning the crossfader shows A's and B's values per parameter;
// up / down give it the focus (a line cursor, the screen's cursor dimmed), EDIT goes to the value's page, OPTION closes
{
    const T = rig(), fb = () => T.m.fb(), px = (x, y) => fb()[y * 240 + x], SEL = 3;
    const line = (l, n = 33) => readText(fb(), 4, rowY(l), n);
    const TEXT = rgb565('#e8e8e8'), WHITE = rgb565('#ffffff'), LABEL = rgb565('#777777'), DIM = rgb565('#555555'), LINE = rgb565('#333333');
    const CUROFF = rgb565('#7a4a18'), CTX = rgb565('#d99bd9');
    const inkOf = (x, y, w, col) => { for (let j = y; j < y + 12; j++) for (let i = x; i < x + w; i++) if (px(i, j) === col) return true; return false; };
    T.note(0); T.note(4); T.note(8); T.to(0, 0);
    T.with_('shift', 'right');                   // I0: CUT and RES into scene 1 (A, the near side)
    T.press('down', 3);
    T.button(SEL, true); T.with_('edit', 'down'); T.press('right'); T.with_('edit', 'down'); T.button(SEL, false);
    T.with_('shift', 'left'); T.with_('shift', 'down', 2);   // X: CH1's level into scene 1
    T.button(SEL, true); T.with_('edit', 'down'); T.button(SEL, false);
    T.xf(32); T.go(30);                    // to B; X: the strip at the top
    ok(line(0, 1) === 'A' && line(0, 33).includes('SC1') && line(1, 33).includes('I0 CUT'), `on X the strip takes the top (${line(0, 33)})`);
    T.m.enc(5, 1); T.go(30);                     // KNOB 4 on X: track 4's level, no knob strip (decisions.md "Knobs, take 3")
    ok(line(0, 7) !== 'I0 ACID' && !line(0, 33).includes('SC1'), `... a knob turn closes it, no knob strip on X (${line(0, 8)})`);
    T.m.enc(5, -1); T.go(1600);
    T.with_('shift', 'up');                      // F: MIX into scene 2 (B, the near side now)
    T.press('right', 3);
    T.button(SEL, true); T.with_('edit', 'up'); T.button(SEL, false);
    T.with_('shift', 'up');                      // back to P, the cursor on row 0
    T.go(1600);
    ok(line(10, 3) !== 'A S', 'the strip goes after 1.5 s');
    T.xf(-19); T.go(30);                   // 127 - 76 = 51, as the mockup
    const top = line(10, 33);
    ok(top === 'A SC1 --------o------------ SC2 B', `the top line: the scenes on A and B, the fader between (${top})`);
    ok(inkOf(4, 152, 6, WHITE) && inkOf(4 + 32 * 6, 152, 6, LABEL) && inkOf(4 + 2 * 6, 152, 18, CTX),
        'the near side\'s letter white (A), the far one grey, the scenes in mauve (principles-review #16)');
    ok([11, 12, 13, 14].map(l => line(l, 33).replace(/ +/g, ' ').trim()).join('|').match(/I0 CUT.*\|.*I0 RES.*\|.*CH1.*\|.*F MIX/),
        `one line per value, A's first: ${[11, 12, 13, 14].map(l => line(l, 33).replace(/ +/g, ' ').trim()).join(' | ')}`);
    ok(line(13, 6) === '-8.0dB' && line(13, 33).endsWith('-4.0dB') && inkOf(4, rowY(13), 36, TEXT) && inkOf(4 + 27 * 6, rowY(13), 36, DIM),
        `CH1: A's value under A, B (none) dim with the song's (${line(13, 33)})`);
    ok(px(0, 150) === LINE && px(208, 150) === LINE && px(209, 150) !== LINE, 'a line frame while passing (principles-review #15), 209 px wide');
    const fx = Math.round(51 * 235 / 127) + 1;
    ok([18, 19, 20].every(y => px(fx, y) === TEXT && px(fx + 2, y) === TEXT) && px(fx + 3, 19) !== TEXT, `the fader mark on the top bar's line at x ${fx} (render.js faderMark)`);
    T.shot('30-scene-strip');                    // (mockup scene-strip)
    // up / down: the focus
    T.press('down');
    ok(px(0, 150) === ACCENT && line(11, 33).includes('I0 CUT') && px(4 + 5 * 6, rowY(11) + 1) === BAND, 'down: it keeps the focus, accent frame, the first line banded');
    ok(px(cellX(2) - 1, rowY(0)) === CUROFF, 'the phrase cursor dimmed where it is (cursorOff)');
    ok(T.strip() === 'EDIT: go to I0 CUT, OPTION: close', `the bottom line: ${T.strip()}`);
    T.press('down'); T.go(1600);
    ok(T.strip() === 'EDIT: go to I0 RES, OPTION: close' && line(10, 1) === 'A', `down: the next line; it stays (${T.strip()})`);
    T.shot('30-scene-strip-stuck');              // (mockup scene-strip-stuck)
    T.press('play'); T.hold('shift'); T.free('shift');
    ok(line(10, 1) === 'A' && px(0, 150) === ACCENT, 'PLAY and SHIFT leave it'); T.press('play');
    T.press('edit');
    ok(readText(fb(), 4 + 8 * 6, IROW_Y(3) + 2, 3) === 'RES' && px(4 + 8 * 6 - 1, IROW_Y(3) + 2 + 12) === ACCENT, 'EDIT: I0\'s page, the cursor on RES');
    ok(!T.strip().includes('OPTION: close'), `the strip closed: no line of its own (${T.strip()}: SEL's own message, from the crossfade)`);
    T.with_('shift', 'left');                    // P; the strip, then OPTION closes it and does nothing else
    T.xf(1); T.go(30); T.press('down'); T.press('option');
    ok(line(10, 3) !== 'A S' && px(cellX(2) - 1, rowY(0)) === ACCENT, 'OPTION: closed, the cursor back in the accent');
    T.xf(-1); T.go(30); T.press('down'); T.press('right');
    ok(line(10, 3) !== 'A S' && px(cellX(6) - 1, rowY(0)) === ACCENT, 'another key: closed, and it acts (right: the volume column)');
    T.press('left');
    T.xf(1); T.go(30); T.button(SEL, true); T.press('down'); T.button(SEL, false);
    ok(px(0, 150) !== ACCENT && px(cellX(2) - 1, rowY(1)) === ACCENT, 'SEL held: the d-pad stays the page\'s');
    T.press('up');
    T.xf(-1); T.go(30); T.m.enc(2, 1); T.go(30);
    ok(line(12, 14) === 'SCENE 1 (A) I0' && line(10, 3) !== 'A S', `a knob turn: the knob strip takes its place (CUT, in the near scene: ${line(12, 14)})`);
    T.go(1600);
    T.press('down', 12);                         // the cursor low: the strip at the top
    T.xf(1); T.go(30);
    ok(line(0, 3) === 'A S', `the cursor low: at the top (${line(0, 12)})`);
}
{   // a full scene: 8 values; a ninth is refused; A + B over 7 lines: the whole page (mockup scene-strip-full)
    const T = rig(), fb = () => T.m.fb(), SEL = 3;
    T.xf(1); T.go(30);
    ok(readText(fb(), 4, rowY(14), 23) === 'NOTHING IN THESE SCENES', `empty scenes: ${readText(fb(), 4, rowY(14), 23)}`);
    T.xf(-1); T.go(1600);
    T.note(0); T.with_('shift', 'right'); T.press('down', 2);   // I0: E0..E7
    T.button(SEL, true);
    for (let k = 0; k < 8; k++) { T.with_('edit', 'right'); if (k === 3) { T.press('down'); T.press('left', 3); } else if (k < 7) T.press('right'); }
    T.press('left', 3); T.press('up', 2); T.with_('edit', 'right');   // LEVEL: a ninth
    ok(T.strip() === 'scene full: 8 parameters max', `a ninth value: ${T.strip()}`);
    T.button(SEL, false);
    T.with_('shift', 'left'); T.xf(1); T.go(30);
    ok(readText(fb(), 4, 20, 1) === 'A' && readText(fb(), 4, rowY(7), 33).includes('I0 KTR'), `8 lines: the whole page from the label line (${readText(fb(), 4, 20, 12)})`);
    T.shot('30-scene-strip-full');
}

// ---- what the pages show of the scenes (render.js sceneVal / valueCell / slotCell / knobStrip / viewMix / viewFx; mockups
// scene-edit, scene-morph, scene-mix): SEL held, the near scene's values bright, the others dim; otherwise a value the
// morph moves shows what plays, a diamond after it; without SEL an edit of a value the near scene holds goes there;
// SEL + OPTION + a scene key empties it (asks first)
{
    const T = rig(), fb = () => T.m.fb(), px = (x, y) => fb()[y * 240 + x], SEL = 3;
    const TEXT = rgb565('#e8e8e8'), DIM = rgb565('#555555'), WHITE = rgb565('#ffffff');
    const ink = (x, y, w, col) => { for (let j = y; j < y + 12; j++) for (let i = x; i < x + w; i++) if (px(i, j) === col) return true; return false; };
    const val = (x, y) => readText(fb(), x, y, 7).replace(/[ ?]+$/, '');
    const diamond = (x, y, col = TEXT) => px(x + 2, y + 4) === col && px(x, y + 6) === col && px(x + 4, y + 6) === col && px(x + 2, y + 8) === col;
    const after = (x, y) => x + val(x, y).length * 6 + 1;   // where a mark after the value starts
    T.note(0);
    T.with_('shift', 'right'); T.press('down', 3);   // I0's CUT (E4)
    const song = val(4, 160);
    T.button(SEL, true);
    T.with_('edit', 'down');                     // CUT into scene 1
    const inScene = val(4, 160);
    T.press('right');                            // the cursor onto RES
    ok(inScene !== song && ink(4, 160, 30, TEXT) && ink(100, 160, 30, DIM) && !ink(100, 160, 30, TEXT) && ink(4, 84, 30, DIM),
        `SEL held: CUT (in scene 1, ${inScene}) bright, DRV and LEVEL dim`);
    ok(ink(20, 202, 30, TEXT) && ink(68, 202, 30, DIM) && !ink(68, 202, 30, TEXT), 'the KNOBS row too: CUT bright, RES dim');
    T.shot('31-scene-edit');                     // (mockup scene-edit)
    T.button(SEL, false); T.press('left');       // the cursor back on CUT
    T.press('down');                             // (onto the KNOBS names: CUT's own cell plain)
    ok(val(4, 160) === inScene && diamond(after(4, 160), 160) && !ink(52, 160, 30, DIM), `at A: CUT plays scene 1's ${val(4, 160)}, a diamond after it; RES plain`);
    ok(diamond(after(20, 202), 202), 'the KNOBS row\'s CUT: the diamond too');
    T.shot('31-scene-morph');                    // (mockup scene-morph)
    T.xf(32); T.go(30);
    ok(val(4, 160) === song && !diamond(after(4, 160), 160), `at B (scene 2 holds nothing): the song's CUT ${val(4, 160)}, no diamond`);
    T.xf(-16); T.go(30);
    const half = val(4, 160);
    ok(half !== song && half !== inScene && diamond(after(4, 160), 160), `halfway: blended (${half}), a diamond`);
    T.xf(-16); T.go(1600);                 // A
    T.press('up');                               // CUT: EDIT + up without SEL goes into scene 1 (it holds CUT)
    T.with_('edit', 'up');
    const up = val(4, 160);
    T.xf(32); T.go(30);
    ok(up !== inScene && val(4, 160) === song, `without SEL: the near scene's CUT edited (${inScene} -> ${up}), the song's kept (${val(4, 160)})`);
    T.xf(-32); T.go(1600);
    T.with_('shift', 'left'); T.m.enc(2, 1); T.go(30);   // P: KNOB 1 = CUT, in the near scene
    ok(readText(fb(), 4, 176, 19) === 'SCENE 1 (A) I0 ACID' && diamond(after(20, 202), 202, WHITE), `the knob strip: ${readText(fb(), 4, 176, 19)}, the value with a diamond`);
    T.go(1600);
    T.with_('shift', 'down');                    // F: MIX into scene 1
    T.press('right', 3); T.button(SEL, true); T.with_('edit', 'up'); T.button(SEL, false); T.press('left');
    ok(diamond(after(154, 60), 60), `F: MIX plays scene 1's ${val(154, 60)}, a diamond`);
    T.with_('shift', 'down');                    // X: CH1's level into scene 1
    T.button(SEL, true); T.with_('edit', 'down');
    ok(ink(64, 164, 36, DIM) && !ink(64, 164, 36, TEXT) && !ink(4 + 2 * 6, rowY(13), 24, TEXT), 'X, SEL held: CH2\'s level dim (not in the scene), MUTE / SOLO dim');
    T.button(SEL, false); T.press('right');
    ok(diamond(58, 164), `X: CH1 plays scene 1's ${readText(fb(), 16, 164, 7).trim()}, a diamond`);
    T.shot('31-scene-mix');                      // (mockup scene-mix)
    // SEL + OPTION + a scene key: empty it, asks first; EDIT confirms (one undo step), another key cancels
    T.button(SEL, true); T.hold('option'); T.key(6); T.free('option');
    ok(T.strip() === 'empty scene 1? EDIT to confirm', `SEL + OPTION + B3: ${T.strip()}`);
    T.press('down');
    ok(T.strip() === 'cancelled', `another key: ${T.strip()}`);
    T.press('up');
    T.hold('option'); T.key(6); T.free('option'); T.press('edit');
    T.button(SEL, false);
    ok(T.strip() === 'scene 1 emptied' && !diamond(58, 164), `EDIT: ${T.strip()}, no diamond`);
    T.panel(0);
    ok(T.strip() === 'undo SCENES' && diamond(58, 164), `undo: ${T.strip()}, back`);
    // OPTION + EDIT without SEL on a value the near scene holds: its default, kept in the scene
    T.press('left'); T.press('up', 2);           // CH1's level
    T.with_('option', 'edit'); T.press('right');
    ok(readText(fb(), 16, 164, 7).trim() === '-4.0dB' && !diamond(58, 164), `OPTION + EDIT: the scene's level at the default (${readText(fb(), 16, 164, 7).trim()}, as the song's: no diamond)`);
    T.button(SEL, true);
    ok(ink(16, 164, 36, TEXT) && ink(112, 164, 36, DIM), '... still in scene 1 (bright under SEL, CH3 dim)');
    T.button(SEL, false);
}

// ---- knob recording (docs/ux/knob-record.md; app.js recordKnob / recSlot / recordScn / touch / touchState; mockup
// phrase-knob-rec): in REC a knob turn is written as its command on the recorded channel's row nearest it, only on rows
// playing the instrument, in the slot holding it, else the first free one (not above a ramp command); the instrument
// stays as it was; the take's cells red; touch: a second pass replaces the first where the hand moves
{
    const T = rig(), fb = () => T.m.fb(), px = (x, y) => fb()[y * 240 + x];
    const title = () => readText(fb(), 4, 176, 30).trimEnd();
    const cmdAt = (row, k) => T.cell([11, 18, 25][k], row, 3), valAt = (row, k) => T.cell([15, 22, 29][k], row, 2);
    const red = (row, k) => { for (let j = 0; j < 12; j++) for (let i = 0; i < 18; i++) if (px(cellX([11, 18, 25][k]) + i, rowY(row) + j) === PLAYC) return true; return false; };
    T.note(0);
    T.note(8); T.to(8, 2); T.with_('edit', 'down');    // row 8: instrument 1 (the list: one down)
    T.fx(4, 0, 'VOL', 1); T.fx(4, 1, 'VOL', 2); T.fx(4, 2, 'VOL', 3);   // row 4: no free FX
    T.fx(3, 0, 'RTO', 0x80);                     // row 3 FX1: a ramp command, so row 2's FX1 is not free
    T.to(0, 0);
    T.with_('shift', 'right'); T.press('down', 3);
    const cut = readText(fb(), 4, 160, 6).replace(/[?\s]+$/, '');   // I0's own CUT
    T.with_('shift', 'left');
    T.btn(13); T.btn(13);                        // REC: notes, armed
    T.press('play'); T.go(2100);                 // a bar of count-in, then recording
    T.until(6); T.m.enc(2, 4); T.go(10);         // KNOB 1 (CUT) early in row 6
    ok(cmdAt(6, 0) === 'CUT' && cmdAt(6, 1) === '---', `in REC: written as CUT on row 6, FX1 (${cmdAt(6, 0)} ${valAt(6, 0)})`);
    ok(title() === 'REC I0 ACID' && px(4 + 3, 176 + 6) !== PLAYC && [...Array(30)].some((_, i) => px(4 + i, 176 + 5) === PLAYC), `the knob strip: ${title()} in the REC red`);
    ok(red(6, 0), 'the written cell drawn red (the take)');
    T.shot('32-knob-rec');   // (mockup phrase-knob-rec)
    T.m.enc(2, 1); T.go(10);
    ok(cmdAt(6, 0) === 'CUT' && cmdAt(6, 1) !== 'CUT' && cmdAt(6, 2) !== 'CUT', 'the same row again: overwritten where it is, no second slot');
    T.until(2); T.m.enc(2, 1); T.go(10);
    ok(cmdAt(2, 0) === '---' && cmdAt(2, 1) === 'CUT', `row 2: FX1 sits above an RTO, FX2 is used (${cmdAt(2, 0)} ${cmdAt(2, 1)})`);
    T.until(4); T.m.enc(2, 1); T.go(10);
    ok(title() === 'not recorded: no free FX', `row 4, three commands: ${title()}`);
    T.until(10); T.m.enc(2, 1); T.go(10);
    ok(title() === 'not recorded: row plays I1', `row 10 (instrument 1 from row 8): ${title()}`);
    T.until(0); T.with_('shift', 'right'); T.press('down', 4);   // on I, the KNOBS values: KNOB 1 = CUT (rows 1..3 by now)
    T.m.enc(2, 1); T.go(10);
    ok(T.strip() === 'REC I0 ACID', `on I: ${T.strip()} in the bottom line`);
    T.m.enc(5, 1); T.go(10);                     // KNOB 4 = REL: a sound command, recorded too
    T.with_('shift', 'left');
    T.btn(13);                                   // REC: the take ends
    ok(/^RECORDING STOPPED: 0 NOTES [1-9]\d* FX$/.test(T.strip()), `a knob take's end counts its FX cells (picks review #16): ${T.strip()}`);
    const cr = [...Array(12).keys()].find(r => cmdAt(r, 1) === 'CUT');   // (row 2's FX2, or the I-page turn's)
    ok(cr !== undefined && !red(cr, 1), `the take ended: its cells drawn as usual (row ${cr})`);
    T.press('play'); T.go(100);
    T.with_('shift', 'right'); T.press('up', 4);
    ok(readText(fb(), 4, 160, 6).replace(/[?\s]+$/, '') === cut, `the instrument's own CUT unchanged (${cut}, ${readText(fb(), 4, 160, 6)})`);
    T.with_('shift', 'left');
    T.panel(0);
    ok(cmdAt(6, 0) === '---' && cmdAt(2, 1) === '---', `undo: the whole take at once (${T.strip()})`);
}
{   // touch: pass 1 turns CUT up, pass 2 down over the same rows: what is left is pass 2's line; the crossfader: SCN
    const T = rig();
    const cmdAt = (row, k) => T.cell([11, 18, 25][k], row, 3), valAt = (row, k) => parseInt(T.cell([15, 22, 29][k], row, 2), 16);
    T.note(0);
    T.btn(13); T.btn(13); T.press('play'); T.go(2100);
    for (let r = 4; r < 10; r++) { T.until(r); T.go(10); T.m.enc(2, 2); T.go(10); }   // pass 1: up, a turn early in each row
                                                 // (2 detents a row: slower than the acceleration's 50 ms)
    const pass1 = [4, 5, 6, 7, 8, 9].map(r => valAt(r, 0));
    for (let r = 4; r < 10; r++) { T.until(r); T.go(10); T.m.enc(2, -2); T.go(10); }   // pass 2 (after > 0.6 s off the knob): down
    T.until(15); T.go(30);
    const vals = [], rows = [];
    for (let r = 0; r < 16; r++) if (cmdAt(r, 0) === 'CUT') { vals.push(valAt(r, 0)); rows.push(r); }
    ok(pass1.every((v, i) => !i || v > pass1[i - 1]), `pass 1 wrote a rising line (${pass1.map(v => v.toString(16)).join(' ')})`);
    ok(vals.length >= 6 && vals.every((v, i) => !i || (v <= vals[i - 1] && vals[i - 1] - v <= 12)),
        `pass 2 started from what played and replaced pass 1 in its path: a falling line (rows ${rows.join()}: ${vals.map(v => v.toString(16)).join(' ')})`);
    // the crossfader in REC: SCN xx (the fader's position) on the row nearest the move
    T.until(2); T.go(10); T.xf(8); T.go(1600);   // (rows 12..15 are under the scene strip while it shows)
    const r = [2, 3].find(r => cmdAt(r, 0) === 'SCN' || cmdAt(r, 1) === 'SCN');
    const k = r === undefined ? -1 : cmdAt(r, 0) === 'SCN' ? 0 : 1;
    ok(r !== undefined && valAt(r, k) === Math.round(32 * 255 / 127), `the crossfader in REC: SCN ${r === undefined ? '-' : valAt(r, k).toString(16)} on row ${r} (the fader at 32 = 40)`);
    T.btn(13); T.go(100);
    const before = [...Array(16).keys()].filter(r => [0, 1, 2].some(k => cmdAt(r, k) === 'SCN')).length;
    T.xf(4); T.go(200);
    ok([...Array(16).keys()].filter(r => [0, 1, 2].some(k => cmdAt(r, k) === 'SCN')).length === before, 'not recording: a move writes nothing');
    T.press('play');
}

// ---- the KNOBS row (step 15b-1; knob-slots.md "Review", mockup inst-knobs): the I page's last row = the 4 knob slots;
// step 15b-3 ("Split in two rows", mockup inst-knob-values): the names pick, the values edit, a knob always turns a value
{
    const T = rig(), px = (x, y) => T.m.fb()[y * 240 + x];
    const slot = n => readText(T.m.fb(), 4 + n * 48 + 16, KNOBS_Y, 3);
    const names = () => [0, 1, 2, 3].map(slot).join(' ');
    const KC = ['#4fa3ff', '#5fd36a', '#f2d14b', '#47d7e0'].map(rgb565), DIM = rgb565('#555555');
    // principles-review.md #1: KNOB n turns the cursor's row, so a 2 px bar in knob n's colour under its nth field;
    // the KNOBS rings grey unless the cursor is on them (mockups inst, inst-knob-values, env)
    const bars = (y, xs) => xs.every((x, n) => [0, 1].every(d => px(4 + x * 6 + 1, y + 25 + d) === KC[n]));
    T.with_('shift', 'right');                   // I0: ANALOG ACID
    ok(bars(34, [0, 16]) && px(4 + 33 * 6, 34 + 25) !== ACCENT && [0, 1, 2, 3].every(n => px(4 + n * 48 + 7, KNOBS_Y + 11 - 7) === DIM),
       'I on ENGINE: bars under ENGINE and PRESET in knobs 1 and 2, no accent line, the KNOBS rings grey');
    T.press('down');                             // LEVEL PAN NAME: two bars, none under NAME
    ok(bars(72, [0, 8]) && !KC.includes(px(4 + 16 * 6 + 1, 72 + 25)),
       'LEVEL PAN NAME: bars under LEVEL and PAN, none under NAME');
    T.press('down', 3);                          // LEVEL -> E0 -> E4 -> KNOB 1's name
    ok(names() === 'CUT RES ATK REL', `the KNOBS row: ANALOG's HOME knobs (${names()})`);
    ok([0, 1, 2, 3].every(n => px(4 + n * 48 + 7, KNOBS_Y + 11 - 7) === KC[n]), "each slot a ring in knob n's colour (blue green yellow cyan)");
    const val = n => readText(T.m.fb(), 4 + n * 48 + 16, KNOBS_Y + 12, 5).trim();
    const under = () => bars(KNOBS_Y, [0, 8, 16, 24]) && px(4 + 7, KNOBS_Y + 11 - 7) === KC[0];
    ok(px(4 + 16 - 1, KNOBS_Y + 1) === ACCENT && px(4 + 16 - 1, KNOBS_Y + 12) !== ACCENT && under(), "the cursor on slot 1's name, the knob bars under the values, the rings coloured");
    T.shot('22-inst-knobs');
    const cut0 = val(0);
    T.m.enc(2, 1); T.go(50);                     // KNOB 1 on the names: slot 1's value, never the choice
    ok(slot(0) === 'CUT' && val(0) !== cut0, `KNOB 1 on the names turns slot 1's value: CUT ${cut0} -> ${val(0)}`);
    T.with_('edit', 'right');                    // EDIT + right on the names: the next parameter
    ok(slot(0) === 'RES', `EDIT + right picks slot 1's parameter: CUT -> ${slot(0)}`);
    T.hold('edit');
    ok(px(4 + 6 - 4, 32 + 1) === ACCENT && T.strip(36).startsWith('RES: '), `EDIT held: the FX grid, the command's help (${T.strip(36)})`);
    T.shot('22-inst-knobs-grid');   // (mockup inst-knobs)
    T.press('down');                             // ENG's second line: DRV KTR PAN (RES's place: its last, PAN)
    T.press('up');
    ok(slot(0) === 'RES', `up again: back to RES, the place remembered (app.js gridCol): ${slot(0)}`);
    T.press('down');
    T.free('edit');
    ok(slot(0) === 'PAN' && !T.strip(36).startsWith('copied'), `EDIT + down in the grid: ${slot(0)}, no copy`);
    T.press('right'); T.press('edit'); T.free('edit');   // (EDIT tapped on slot 2: nothing copied)
    T.with_('option', 'edit');
    ok(slot(1) === 'RES', 'OPTION + EDIT: the engine default');
    T.press('left');
    T.hold('edit'); T.press('up'); T.free('edit');   // PAN -> the line above, the same place: DTN
    T.panel(0);                                  // panel EDIT + OCT-: undo
    ok(T.strip(36) === 'undo I0 KNOB 1 DTN>PAN' && slot(0) === 'PAN', `undo a slot: ${T.strip(36)} (${slot(0)})`);
    T.btn(9);   // SAVE, then a reboot: the slot is in the project
    const R = rig(T.store());
    R.with_('shift', 'right');
    ok(readText(R.m.fb(), 4 + 16, KNOBS_Y, 3) === 'PAN', 'saved and loaded with the project');
    T.press('down');                             // the values row
    ok(px(4 + 16 - 1, KNOBS_Y + 12) === ACCENT && px(4 + 16 - 1, KNOBS_Y + 1) !== ACCENT && under(), "down: the cursor on slot 1's value, drawn in the same row");
    const pan0 = val(0);
    T.with_('edit', 'right');                    // EDIT + right on the value: PAN itself, the slot stays
    T.shot('22-inst-knob-values');
    const pan1 = val(0);
    ok(slot(0) === 'PAN' && pan1 !== pan0, `EDIT + right on the value edits PAN: ${pan0} -> ${pan1}`);
    T.with_('option', 'edit');
    const panRow = readText(T.m.fb(), 4 + 8 * 6, 72 + 12, 5).trim();
    ok(slot(0) === 'PAN' && val(0) !== pan1 && val(0).startsWith(panRow),   // (the highlight's edge reads as one more glyph)
         `OPTION + EDIT on the value: PAN's default, as its own field shows it (${val(0)} / ${panRow})`);
    const res0 = val(1);
    T.m.enc(3, 2); T.go(50);                     // KNOB 2 on the values: slot 2's value
    ok(slot(1) === 'RES' && val(1) !== res0, `KNOB 2 on the values turns RES: ${res0} -> ${val(1)}`);
    T.press('up', 5);                            // ENGINE: a new engine, then back: the engine's knobs again
    T.hold('edit'); T.press('right'); T.press('left'); T.free('edit');
    ok(names() === 'CUT RES ATK REL', `a new engine resets the slots (${names()})`);
}

// ---- the KNOBS rows on E / L / V too (step 15b-4; knob-slots.md "On all four instrument pages", mockups env lfo vox):
// the same two rows as on I, in the same place (y 190 / 198), working the same way
{
    const T = rig(), px = (x, y) => T.m.fb()[y * 240 + x];
    const slot = n => readText(T.m.fb(), 4 + n * 48 + 16, KNOBS_Y, 3);
    const names = () => [0, 1, 2, 3].map(slot).join(' ');
    const val = n => readText(T.m.fb(), 4 + n * 48 + 16, KNOBS_Y + 12, 5).trim();
    const under = () => bars(KNOBS_Y, [0, 8, 16, 24]) && px(4 + 7, KNOBS_Y + 11 - 7) === KC[0];
    const KC = ['#4fa3ff', '#5fd36a', '#f2d14b', '#47d7e0'].map(rgb565), DIM = rgb565('#555555');
    const bars = (y, xs) => xs.every((x, n) => [0, 1].every(d => px(4 + x * 6 + 1, y + 25 + d) === KC[n]));
    T.with_('shift', 'right'); T.with_('shift', 'down');   // P -> I0 -> E
    ok(names() === 'CUT RES ATK REL', `E: the KNOBS rows at the bottom (${names()})`);
    ok(bars(106, [0, 8, 16, 24]) && px(4 + 7, KNOBS_Y + 11 - 7) === DIM, 'E on ATK: bars under ATK DEC SUS REL in the knob colours, the rings grey');
    T.press('down', 2);                          // ATK -> FILTER -> KNOB 1's name
    ok(px(4 + 16 - 1, KNOBS_Y + 1) === ACCENT && under(), "E: down twice, the cursor on slot 1's name, the knob bars under the values, the rings coloured");
    const cut0 = val(0);
    T.m.enc(2, 1); T.go(50);
    ok(slot(0) === 'CUT' && val(0) !== cut0, `E: KNOB 1 on the names turns CUT: ${cut0} -> ${val(0)}`);
    T.hold('edit');
    ok(px(4 + 6 - 4, 32 + 1) === ACCENT && T.strip(36).startsWith('CUT: '), `E: EDIT held on a name, the FX grid and its help (${T.strip(36)})`);
    T.press('right'); T.free('edit');
    ok(slot(0) === 'RES' && !T.strip(36).startsWith('copied'), `E: EDIT + right in the grid picks: ${slot(0)}`);
    T.with_('option', 'edit');
    ok(slot(0) === 'CUT', "E: OPTION + EDIT on a name, the engine's knob");
    T.press('down');                             // the values row
    ok(px(4 + 16 - 1, KNOBS_Y + 12) === ACCENT && under(), "E: down, the cursor on slot 1's value, the bars in the same place");
    const atk = () => readText(T.m.fb(), 4, 106 + 12, 6).trim(), atk0 = atk();
    T.m.enc(4, 3); T.go(50);                     // KNOB 3 on the values: slot 3 = ATK, the page's own ATK follows
    ok(slot(2) === 'ATK' && atk() !== atk0 && val(2).startsWith(atk().slice(0, 4)), `E: KNOB 3 turns ATK, its own field too: ${atk0} -> ${atk()} / ${val(2)}`);
    const cut1 = val(0);
    T.with_('edit', 'right');
    ok(slot(0) === 'CUT' && val(0) !== cut1, `E: EDIT + right on a value edits CUT: ${cut1} -> ${val(0)}`);
    T.with_('shift', 'down');                    // L, V: the same rows
    ok(mapLit(px, 'L') && names() === 'CUT RES ATK REL', `L: the KNOBS rows (${names()})`);
    T.with_('shift', 'down');
    ok(mapLit(px, 'V') && names() === 'CUT RES ATK REL', `V: the KNOBS rows (${names()})`);
    T.shot('24-vox-knobs');   // (mockup vox)
}

// ---- the knob strip (step 15b-2; knob-slots.md, mockup phrase-knobs): away from the instrument pages KNOB 1-4 turn the
// focus instrument's slots, the strip shows for 1.5 s away from the cursor
{
    const T = rig(), px = (x, y) => T.m.fb()[y * 240 + x], LINE = rgb565('#333333');
    const ink = (x0, y0, w, h, col) => { let n = 0; for (let y = y0; y < y0 + h; y++) for (let x = x0; x < x0 + w; x++) if (px(x, y) === col) n++; return n; };
    const turn = (k, n) => { T.m.enc(2 + k, n); T.go(50); };
    T.note(0);
    T.note(8); T.to(8, 2); T.with_('edit', 'right');   // row 8: instrument 4 (EDIT held: the list, right = 4 on)
    T.to(2, 0);
    const Y = rowY(12) - 2;                  // the cursor high: the strip at the bottom (line 12)
    turn(1, 2);
    const names = [0, 1, 2, 3].map(n => readText(T.m.fb(), 4 + n * 48 + 16, Y + 16, 3)).join(' ');
    ok(px(0, Y + 5) === LINE && px(0, Y) === LINE && readText(T.m.fb(), 4, Y + 2, 7) === 'I0 ACID' && names === 'CUT RES ATK REL',
        `KNOB 2 on the phrase: the strip at the bottom in a line frame (principles-review.md #15), I0's knobs (${readText(T.m.fb(), 4, Y + 2, 7)}: ${names})`);
    ok(ink(4 + 48 + 16, Y + 16, 18, 12, ACCENT) > 10 && ink(4 + 16, Y + 16, 18, 12, ACCENT) === 0, "the turned knob's name in the accent");
    T.shot('23-knob-strip');
    const res1 = readText(T.m.fb(), 4 + 48 + 16, Y + 28, 5).trim();
    turn(1, 3);
    const res2 = readText(T.m.fb(), 4 + 48 + 16, Y + 28, 5).trim();
    ok(res1 !== res2, `the value follows the turn (RES ${res1} -> ${res2})`);
    T.panel(0);
    ok(T.strip(36).startsWith(`undo I0 RES ${res2}>`) && T.strip(36) !== `undo I0 RES ${res2}>${res1}`, `turns within 1.5 s = one undo step: ${T.strip(36)}`);
    T.go(1600);
    ok(px(0, Y + 5) !== LINE, 'gone 1.5 s after the last turn');
    T.to(10, 0);                                 // the cursor low, below row 8: the strip at the top, instrument 1
    turn(0, 1);
    T.shot('23-knob-strip-top');
    ok(px(0, 32 + 3) === LINE && readText(T.m.fb(), 4, 32, 3) === 'I4 ', `the cursor low: the strip at the top, the nearest instrument above (${readText(T.m.fb(), 4, 32, 12)})`);
    T.go(1600);
    T.with_('shift', 'right');                   // the instrument page: the turns stayed where they were made
    const eng = readText(T.m.fb(), 4, 34 + 12, 8).trim();
    T.with_('shift', 'left'); T.to(0, 0); turn(0, 2); T.go(1600); T.to(10, 0);
    T.with_('shift', 'right');
    ok(readText(T.m.fb(), 4, 34 + 12, 8).trim() === eng, `knob turns on the phrase do not reach the page's knobs (ENGINE ${eng})`);
}

// ---- knobs, take 3 (docs/decisions.md "Knobs, take 3"; app.js knobFields / recordTvl / assignKnob / accel, proto-test
// "knobs take 3"): F's knobs turn the cursor's frame row, X's KNOB n track n's TVn (bars in the knob colours, no strip;
// in REC X records TVn, F nothing); the keybed EDIT held + a knob assigns on I E L V; acceleration x1 / x2 / x4
{
    const T = rig(), fb = () => T.m.fb(), px = (x, y) => fb()[y * 240 + x];
    const KC = ['#4fa3ff', '#5fd36a', '#f2d14b', '#47d7e0'].map(rgb565), FY = [48, 110, 172];
    const turn = (k, n) => { T.go(100); T.m.enc(2 + k, n); T.go(30); };   // (100 ms apart: x1)
    const fast = (k, n) => { T.m.enc(2 + k, n); T.go(10); };
    const fld = (x, r) => readText(fb(), 4 + x * 6, FY[r] + 12, 7).replace(/[?\s]+$/, '');   // (? : the cursor's edge)
    const bar = (x, y, n) => [0, 1].every(d => px(4 + x * 6 + 1, y + d) === KC[n]);
    const lvl = k => T.cell(2 + 8 * k, 11, 7).trim();
    const cmdAt = (row, k) => T.cell([11, 18, 25][k], row, 3);
    T.note(0);
    T.with_('shift', 'down');                    // F, the cursor on TIME (DELAY)
    turn(1, 2); turn(2, -1); turn(3, 1);
    ok([fld(9, 0), fld(17, 0), fld(25, 0)].join() === '49%,54%,72%', `F, DELAY: KNOB 2-4 = FDBK COLOUR MIX (${[fld(9, 0), fld(17, 0), fld(25, 0)].join()})`);
    ok([1, 9, 17, 25].every((x, n) => bar(x, FY[0] + 25, n)), 'bars in the knob colours under the DELAY row');
    fast(0, 1); fast(0, 1); fast(0, 1);          // 10 ms apart: a list stays x1
    ok(fld(1, 0) === '16T', `TIME, a list, 3 fast detents: 3 steps (1/8 -> ${fld(1, 0)})`);
    T.press('down');                             // REVERB (TYPE SIZE DAMP)
    turn(3, 1); turn(2, 1);
    ok([fld(1, 1), fld(9, 1), fld(17, 1)].join() === 'ROOM,71%,48%', `REVERB: KNOB 4 nothing, KNOB 3 = DAMP (${[fld(1, 1), fld(9, 1), fld(17, 1)].join()})`);
    ok([1, 9, 17].every((x, n) => bar(x, FY[1] + 25, n)) && !KC.includes(px(4 + 25 * 6 + 1, FY[1] + 25)), 'three bars, none past DAMP');
    T.with_('shift', 'down'); T.press('down');   // X, the cursor on CH1's MUTE
    turn(2, 1);
    ok(lvl(2) === '-3.5dB' && lvl(1) === '-4.0dB' && T.cell(2, 13, 4) === 'MUTE', `X: KNOB 3 = track 3's level from MUTE (${lvl(2)})`);
    ok([0, 1, 2, 3].every(k => bar(2 + 8 * k, 32 + 12 * 12 + 1, k)), "a bar in track n's colour under each level");
    T.shot('35-knobs-take3-mix');
    // REC: X records TVn on the knob's track; F writes nothing
    T.with_('shift', 'up', 2);                   // P
    T.btn(13); T.btn(13); T.press('play'); T.go(2100);   // REC armed, PLAY: a bar of count-in, then recording
    T.with_('shift', 'down'); turn(1, 1);        // F: FDBK
    T.with_('shift', 'down'); turn(0, 1);        // X: KNOB 1, track 1 (playing)
    ok(T.strip() === 'REC TV1 105', `X in REC: ${T.strip()}`);
    turn(3, 1);
    ok(T.strip() === 'TV4 not recorded: track 4 not playing', `track 4 is not playing: ${T.strip()}`);
    T.btn(13); T.press('play'); T.go(100);
    ok(lvl(0) === '-4.0dB', `the mix level unchanged (${lvl(0)})`);
    T.with_('shift', 'up', 2);
    const cmds = [...Array(16).keys()].flatMap(r => [0, 1, 2].map(k => cmdAt(r, k))).filter(c => c !== '---');
    ok(cmds.length >= 1 && cmds.every(c => c === 'TV1'), `the take wrote TV1 only, none from F (${cmds.join(' ')})`);
    // quick assign
    T.with_('shift', 'right'); T.press('down'); T.press('right');   // I0, PAN
    const slot = n => readText(fb(), 4 + n * 48 + 16, KNOBS_Y, 3), pan = readText(fb(), 4 + 8 * 6, 72 + 12, 5);
    T.hold('edit'); turn(2, 1); turn(2, 1); T.free('edit');
    ok(slot(2) === 'PAN' && readText(fb(), 4 + 8 * 6, 72 + 12, 5) === pan && T.strip() === 'KNOB 3 = PAN',
        `EDIT held + KNOB 3 on PAN: slot 3 = ${slot(2)}, PAN unchanged, nothing copied (${T.strip()})`);
    T.press('up');                               // ENGINE
    T.hold('edit'); turn(1, 1); T.free('edit');
    ok(slot(1) === 'RES' && T.strip() === 'ENGINE takes no knob', `on ENGINE: nothing (${slot(1)}, ${T.strip()})`);
    T.press('down', 4);                          // the KNOBS names, slot 1 (CUT)
    T.hold('edit'); turn(3, 1); T.free('edit');
    ok(slot(3) === 'CUT', `on the KNOBS names: slot 1's parameter into slot 4 (${slot(3)})`);
    T.with_('shift', 'left');
    T.hold('edit'); turn(0, 1); T.free('edit');
    ok(T.strip() === 'EDIT + KNOB assigns on I E L V', `on P: nothing (${T.strip()})`);
    // acceleration on I's LEVEL: 104 -> +1 (x1) -> +4 (10 ms: x4) -> +2 (35 ms: x2) -> -1 (a direction change: x1)
    T.with_('shift', 'right'); T.press('up', 4); T.press('down'); T.press('left');   // LEVEL
    const level = () => parseFloat(readText(fb(), 4, 72 + 12, 7)), seen = [];
    let was = level();
    T.go(200);
    for (const [ms, d] of [[0, 1], [10, 1], [35, 1], [10, -1]]) { T.go(ms); T.m.enc(2, d); T.go(1); seen.push((level() - was) * 2); was = level(); }
    ok(seen.join() === '1,4,2,-1', `LEVEL steps: x1, x4 (10 ms), x2 (35 ms), x1 on a direction change (${seen.join()})`);
}

// ---- Q, the event view (step Q-1; docs/ux/event-view.md, events.js qItems / drawQ, app.js GRID.q / qJump / seqButton):
// every phrase row the instrument plays, once, with its plays and channels; edited in place; an EDIT tap goes there
{
    const T = rig(), px = (x, y) => T.m.fb()[y * 240 + x];
    const line = l => [16, 40, 52, 76].map((x, j) => readText(T.m.fb(), x, Q_Y(l), [3, 1, 3, 5][j]).trim()).join(' ').trimEnd();
    const count = l => readText(T.m.fb(), 215 - 12 - 2 * 6, Q_Y(l), 2);   // (x2: right-aligned, MAP_X - 12)
    T.note(0); T.note(4); T.fx(8, 0, 'VOL', 0x40);   // phrase 00: two notes and a VOL on row 8 (instrument 0 from row 0)
    T.with_('shift', 'left'); T.press('edit'); T.press('down'); T.press('edit'); T.with_('shift', 'right');   // chain row 1: phrase 00 again (copied, pasted)
    T.btn(11);                                   // SEQ
    ok(readText(T.m.fb(), 4, 4, 3) === 'CH1' && mapLit(px, 'Q'), `SEQ: Q of instrument 0 (${readText(T.m.fb(), 4, 4, 12)})`);
    {   // the top bar = I's breadcrumb (principles-review.md #11): I0 in the accent, the name in grey at column 21, the tempo
        const top = readText(T.m.fb(), 4, 4, 38), lit = (c0, n) => [...Array(n * 6 * 8).keys()].some(i => px(4 + c0 * 6 + i % (n * 6), 4 + (i / (n * 6) | 0)) === ACCENT);
        ok(top.startsWith('CH1>S00>C00:1>P00>I0') && top.slice(21).startsWith('ACID') && readText(T.m.fb(), BAR_R - 18, 4, 3) === '120' && lit(18, 2) && !lit(0, 17) && !lit(21, 4),
            `Q's top bar: the breadcrumb, I0 lit (${top.trimEnd()})`);
    }
    ok([0, 1, 2].map(l => line(l).slice(0, 9)).join(' | ') === 'P00 0 C-3 | P00 4 C-3 | P00 8 ---' && line(2).endsWith('VOL40') && [0, 1, 2].map(count).join() === 'x2,x2,x2',
        `every phrase row once, its plays counted: ${[0, 1, 2].map(l => line(l) + ' ' + count(l)).join(' | ')}`);
    ok(px(4, Q_Y(0) + 5) === rgb565('#4fa3ff'), "channel 1's dot");
    ok(T.strip() === 'bar 1 S00 CH1, plays 2 times', `the bottom line: where it plays first (${T.strip()})`);
    T.shot('28-q');
    T.press('down', 2); T.press('right');
    ok(px(76 - 1, Q_Y(2)) === ACCENT, 'down twice, right: the VOL cell');
    T.press('up'); ok(px(52 - 1, Q_Y(1)) === ACCENT, 'up to a line without commands: its note');
    T.press('down'); ok(px(76 - 1, Q_Y(2)) === ACCENT, 'down again: back on the command picked with right');
    T.hold('edit'); ok(/^SEQ: only VOL, -[\d.]+dB$/.test(T.strip()), `EDIT held: the SEQ cue, the value explained (${T.strip()})`); T.press('right'); T.free('edit');
    ok(line(2).endsWith('VOL41'), `EDIT + right: VOL41 (${line(2)})`);
    T.press('up', 2); T.with_('edit', 'up');
    ok(line(0).startsWith('P00 0 C-4'), `EDIT + up on a note: an octave (${line(0)})`);
    T.press('down'); T.with_('option', 'edit');
    ok(line(1) === 'P00 8 --- VOL41', `OPTION + EDIT on a note alone: the row is empty, its line goes (${line(1)})`);
    T.panel(0); ok(line(1) === 'P00 4 C-3', `undo: back (${line(1)}; ${T.strip()})`);
    T.press('down'); T.press('right'); T.press('edit');   // an EDIT tap on VOL41: the phrase, row 8, its value
    ok(mapLit(px, 'P') && px(cellX(15) - 1, rowY(8)) === ACCENT, 'EDIT tap: P, the cursor on row 8, its value');
    T.to(9, 4);
    ok(T.cell(11, 8, 3) === 'VOL' && T.cell(15, 8, 2) === '41', `the row edited in Q: ${T.cell(11, 8, 5)}`);
    T.press('edit');
    ok(T.cell(11, 9, 3) === 'VOL' && T.cell(15, 9, 2) === '41', `Q's edit stored the last value: EDIT pastes VOL 41 (${T.cell(11, 9, 5)})`);
    T.with_('shift', 'right'); T.with_('shift', 'up');
    ok(mapLit(px, 'Q') && px(76 - 1, Q_Y(2)) === ACCENT, 'SHIFT + up from I: Q again, the cursor where it was');
    T.with_('shift', 'left'); ok(mapLit(px, 'P'), 'SHIFT + left from Q: P');
}
{   // a list longer than the screen: it scrolls within a line of its edges, a bar shows where
    const T = rig(), px = (x, y) => T.m.fb()[y * 240 + x];
    const row0 = () => readText(T.m.fb(), 16, Q_Y(0), 5);
    for (let r = 0; r < 14; r++) T.note(r);
    T.btn(11);   // SEQ
    ok(row0() === 'P00 0' && px(215 - 10, 85) === rgb565('#777777'), `14 lines: from the top, the scroll bar's thumb at the top (${row0()})`);
    T.press('down', 9); ok(row0() === 'P00 0', 'down to the 10th line: no scroll yet');
    T.press('down'); ok(row0() === 'P00 1', `the 11th: one line up (${row0()})`);
    T.press('down', 5); ok(row0() === 'P00 3' && px(215 - 10, 85 + 11 * 12 - 1) === rgb565('#777777'), `to the end: the last 11 lines, the thumb at the bottom (${row0()})`);
}
{   // SEQ in Q: the filter (step Q-3, app.js seqButton / openQ; its tag: principles-review.md #11, its cue in the
    // EDIT-held help: #22); followed by name to another instrument
    const T = rig(), px = (x, y) => T.m.fb()[y * 240 + x];
    const seq = () => T.btn(11);
    const line = l => [16, 40, 52, 76].map((x, j) => readText(T.m.fb(), x, Q_Y(l), [3, 1, 3, 5][j]).trim()).join(' ').trimEnd();
    const lines = () => [0, 1, 2].map(line).filter(x => x).join(' | '), tag = () => readText(T.m.fb(), 4 + 21 * 6, 4, 9).trimEnd();
    const held = () => { T.hold('edit'); const h = T.strip(); T.free('edit'); seq(); return h; };   // (the tap's jump: SEQ comes back)
    T.note(0); T.note(4); T.fx(8, 0, 'VOL', 0x40); T.note(12); T.to(12, 2); T.with_('edit', 'right');   // row 12: instrument 1
    T.to(0, 0); seq();
    ok(lines() === 'P00 0 C-3 | P00 4 C-3 | P00 8 --- VOL40', `Q of instrument 0: ${lines()}`);
    ok(held() === 'SEQ: only notes, note of P00 row 0', `EDIT held on a note: the SEQ cue first (${T.strip()})`);
    T.press('down', 2); T.press('right');
    ok(/^SEQ: only VOL, -[\d.]+dB$/.test(held()), 'EDIT held on VOL: SEQ keeps the VOL lines');
    seq();
    ok(lines() === 'P00 8 --- VOL40' && tag() === 'VOL ONLY' && px(76 - 1, Q_Y(0)) === ACCENT, `SEQ: only the VOL lines, VOL ONLY in the name's place, the cursor on its line (${lines()}; ${tag()})`);
    T.shot('28-q-filter');
    ok(/^SEQ: all again, -[\d.]+dB$/.test(held()), `EDIT held with the filter on: how to leave it (${T.strip()})`);
    seq();
    ok(lines() === 'P00 0 C-3 | P00 4 C-3 | P00 8 --- VOL40' && tag() === 'ACID' && px(76 - 1, Q_Y(2)) === ACCENT, `SEQ again: every line, the cursor still on VOL (${tag()})`);
    T.press('up', 2); seq();
    ok(lines() === 'P00 0 C-3 | P00 4 C-3' && tag() === 'NOTE ONLY', `SEQ on a note: the lines with a note (${lines()}; ${tag()})`);
    T.with_('option', 'edit', 2);                   // both notes emptied: the filter leaves nothing
    ok(readText(T.m.fb(), 4, Q_Y(0), 22).trimEnd() === 'no NOTE here', `the filter left no line: no NOTE here (${readText(T.m.fb(), 4, Q_Y(0), 22)})`);
    T.panel(0); T.panel(0); ok(lines() === 'P00 0 C-3 | P00 4 C-3', `undo twice: back (${lines()})`);
    seq(); T.press('down', 2); seq();                // VOL again
    T.with_('shift', 'left'); T.to(12, 0); seq();   // instrument 1: no VOL lines
    ok(T.strip() === 'no VOL here: every line' && tag() !== 'VOL ONLY' && line(0).startsWith('P00 C'), `instrument 1 has no VOL: every line, and the bottom line says so (${T.strip()}; ${line(0)})`);
    T.with_('shift', 'left'); T.to(0, 0); seq();
    ok(tag() === 'VOL ONLY' && lines() === 'P00 8 --- VOL40', `back on instrument 0: the filter is back, by name (${tag()})`);
    seq(); ok(tag() === 'ACID', 'SEQ: off for good');
    T.with_('shift', 'left'); T.to(12, 0); seq(); T.with_('shift', 'left'); T.to(0, 0); seq();
    ok(tag() === 'ACID' && !T.strip().startsWith('no VOL'), `and it stays off across instruments (${tag()}; ${T.strip()})`);
}
{   // the list in place (unit feedback #43, events.js listItems): the first row, a command without a note or an instrument,
    // only gets the instrument on the phrase's second pass; it still comes first
    const T = rig(), seq = () => T.btn(11);
    const line = l => [16, 40, 52, 76].map((x, j) => readText(T.m.fb(), x, Q_Y(l), [3, 1, 3, 5][j]).trim()).join(' ').trimEnd();
    const lines = () => [0, 1, 2].map(line).filter(x => x).join(' | ');
    T.fx(0, 0, 'VOL', 0x40); T.to(0, 2); T.with_('option', 'edit');   // row 0: VOL only, its instrument emptied
    T.note(4); T.note(8);
    T.with_('shift', 'left', 2); T.press('edit'); T.press('down'); T.press('edit');   // S: chain 00 on song row 01 too
    T.with_('shift', 'right', 2); T.to(4, 0); seq();
    ok(lines() === 'P00 0 --- VOL40 | P00 4 C-3 | P00 8 C-3', `Q: the first row first (${lines()})`);
}

{   // OPTION + arrows in Q (step Q-3, app.js optionMove): left / right = the previous / next instrument the song plays
    // (unused ones skipped), up / down = the previous / next phrase in the list
    const T = rig(), seq = () => { T.btn(11); };
    const line = l => readText(T.m.fb(), 16, Q_Y(l), 9).trimEnd(), top = () => readText(T.m.fb(), 4, 4, 38);
    const cur = () => [0, 1, 2, 3].find(l => T.m.fb()[Q_Y(l) * 240 + 52 - 1] === ACCENT);   // the line whose note is lit
    T.note(0); T.note(2);                        // phrase 00: rows 0 and 2 (instrument 0)
    T.with_('shift', 'left'); T.press('down'); T.press('edit'); T.press('edit'); T.with_('shift', 'right');   // chain row 1: a new phrase
    T.note(0); T.note(4); T.to(4, 2); T.with_('edit', 'down', 2);   // the new phrase: row 0 (instrument 0), row 4 (instrument 2: its list, down twice)
    T.to(0, 0); seq();
    const p1 = line(2).slice(0, 3);              // (the line under the cursor's lit note reads '?' there: its first 5 only)
    ok(line(0).slice(0, 5) === 'P00 0' && line(1) === 'P00 2 C-3' && /^P0[1-9] 0 C-3$/.test(line(2)), `Q of instrument 0: ${[0, 1, 2].map(line).join(' | ')}`);
    T.press('down'); T.with_('option', 'down');
    ok(cur() === 2, `OPTION + down: the next phrase's first line (line ${cur()})`);
    T.with_('option', 'down');
    ok(cur() === 2, `OPTION + down on the last phrase: stays (line ${cur()})`);
    T.with_('option', 'up');
    ok(cur() === 0, `OPTION + up: the previous phrase's first line (line ${cur()})`);
    T.with_('option', 'right');
    ok(top().includes('>I2') && line(0).slice(0, 5) === p1 + ' 4', `OPTION + right: instrument 2, 1 skipped (the song never plays it): ${top().trimEnd()}; ${line(0)}`);
    T.with_('option', 'right');
    ok(top().includes('>I2'), `OPTION + right on the last one played: stays (${top().trimEnd()})`);
    T.with_('option', 'left');
    ok(top().includes('>I0') && line(0).slice(0, 5) === 'P00 0', `OPTION + left: instrument 0 again (${top().trimEnd()})`);
}

{   // selected lines in Q (step Q-3, app.js qChangeBlock, proto-test "Q: selected lines change together"): SHIFT +
    // OPTION selects lines, EDIT + arrows / OPTION + EDIT change the cursor cell's command on every one, wherever it sits
    const T = rig(), px = (x, y) => T.m.fb()[y * 240 + x];
    const seq = () => { T.btn(11); };
    const vols = () => [0, 1, 2].map(l => readText(T.m.fb(), 76, Q_Y(l), 5)).join(' ');
    const SEL = rgb565('#16324a'), selected = () => [0, 1, 2, 3].map(l => px(3, Q_Y(l) + 1) === SEL ? 1 : 0).join('');
    T.note(0); T.fx(0, 0, 'VOL', 0x20); T.fx(4, 1, 'VOL', 0x21); T.fx(8, 2, 'VOL', 0x22); T.note(12);   // VOL in FX1, FX2, FX3
    T.to(0, 0); seq(); T.press('right');
    ok(vols() === 'VOL20 VOL21 VOL22', `Q: three VOL lines (${vols()})`);
    T.hold('shift'); T.press('option'); T.free('shift');
    ok(T.strip() === 'select lines: EDIT+arrows change them' && selected() === '1000', `SHIFT + OPTION: the line selected (${T.strip()}; ${selected()})`);
    T.with_('option', 'down');
    ok(selected() === '1100', `OPTION + down with lines selected: the cursor moves, the selection grows (${selected()})`);
    T.press('down');
    ok(selected() === '1110', `down: three lines selected (${selected()})`);
    T.hold('edit');
    ok(T.strip() === 'selection: EDIT+arrows edit it', `EDIT held: the selection's help (${T.strip()})`);
    T.press('up'); T.press('right'); T.free('edit');
    ok(vols() === 'VOL31 VOL32 VOL33' && mapLit(px, 'Q'), `EDIT + up, + right: every VOL +16, +1, wherever it sits; EDIT no jump (${vols()})`);
    T.panel(0);
    ok(vols() === 'VOL20 VOL21 VOL22', `undo: both changes, one step (${vols()})`);
    T.with_('option', 'edit');
    ok(vols() === 'VOL00 VOL00 VOL00' && selected() === '1110', `OPTION + EDIT: all back to 00, still selected (${vols()}; ${selected()})`);
    T.press('option');
    ok(selected() === '0000' && T.strip() === 'deselected', `a plain OPTION drops the selection (${selected()}; ${T.strip()})`);
    T.hold('shift'); T.press('option'); T.press('option'); T.free('shift');
    ok(selected() === '0000' && T.strip() === 'select lines: EDIT+arrows change them', `SHIFT + OPTION again: no selection, quietly (${selected()})`);
    T.hold('shift'); T.press('option'); T.free('shift'); seq();
    ok(selected() === '0000', `SEQ drops it too (${selected()})`);
}

// ---- Q-2, Q's timeline (events.js draw, final): the SONG strip, the BARS page, NOTE / the focus curve / REST. The same
// song is built here with the gamepad and in the prototype's model, and the timeline (y 21..81) compared pixel for pixel
{
    const require = createRequire(import.meta.url);
    for (const f of ['fonts.js', 'felucca.js', 'model.js', 'control-map.js', 'render.js', 'events.js'])
        require(join(ROOT, 'ux-proto', f));
    const { Model: PM, Render: PR, Events: PE } = globalThis;
    const T = rig(), px = (x, y) => T.m.fb()[y * 240 + x];
    // phrase 00 (channel 1, chain 00 rows 0 and 1): notes, CUT ramped up by REP to RTO, VOL, KIL, CHA, CUT again
    T.note(0); T.fx(2, 0, 'CUT', 0x20); T.fx(3, 0, 'REP', 0x10); T.fx(4, 0, 'RTO', 0xC0); T.note(4);
    T.fx(6, 1, 'VOL', 0x40); T.note(8); T.to(8, 0); T.with_('edit', 'up'); T.fx(8, 1, 'KIL', 3);
    T.fx(10, 2, 'CHA', 0x88); T.fx(12, 0, 'CUT', 0xF0);
    T.with_('shift', 'left'); T.press('edit'); T.press('down'); T.press('edit'); T.press('up');   // chain row 1: phrase 00 again
    // channel 2: chain 01, phrase 01: a note on row 0 and CUT 80 on row 5
    T.with_('shift', 'left'); T.press('right'); T.press('edit'); T.with_('shift', 'right'); T.press('edit'); T.press('edit');
    T.with_('shift', 'right');
    T.note(0); T.fx(5, 0, 'CUT', 0x80);
    const n2 = T.cell(2, 0, 3);                  // (the last note: C-4, edited on row 8)
    T.to(0, 0); T.with_('shift', 'left'); T.with_('shift', 'left'); T.press('left'); T.with_('shift', 'right'); T.with_('shift', 'right');
    T.btn(11);   // SEQ: Q of instrument 0
    const song = PM.newSong(), row = (n, fx) => Object.assign(PM.emptyRow(), { n, i: n === PM.EMPTY ? PM.EMPTY : 0, vol: n === PM.EMPTY ? PM.EMPTY : 64,
        fx: [0, 1, 2].map(k => fx && fx[k] ? { c: fx[k][0], v: fx[k][1] } : { c: '', v: 0 }) });
    const E = PM.EMPTY;
    song.song[0][0] = 0; song.chains[0][0].ph = 0; song.chains[0][1].ph = 0;
    song.song[0][1] = 1; song.chains[1][0].ph = 1;
    Object.assign(song.phrases[0], { 0: row(48), 2: row(E, [['e4', 0x20]]), 3: row(E, [['R', 0x10]]), 4: row(48, [['O', 0xC0]]),
        6: row(E, [, ['V1', 0x40]]), 8: row(60, [, ['K', 3]]), 10: row(E, [, , ['X', 0x88]]), 12: row(E, [['e4', 0xF0]]) });
    Object.assign(song.phrases[1], { 0: row(n2 === 'C-4' ? 60 : 48), 5: row(E, [['e4', 0x80]]) });
    const same = (qRow, qCol, qPlayT) => {
        const fb = PR.makeFB();
        PE.drawQ(fb, song, { view: 'q', instSel: 0, qRow, qCol, qTop: 0, songRow: 0, ch: 0, chainRow: 0, qPlayT });
        const bad = [];
        for (let y = 21; y < 82; y++) for (let x = 4; x < 207; x++) {
            const o = (y * 240 + x) * 4, want = (fb[o] >> 3) << 11 | (fb[o + 1] >> 2) << 5 | fb[o + 2] >> 3;
            if (px(x, y) !== want) bad.push(`${x},${y} ${px(x, y).toString(16)}/${want.toString(16)}`);
        }
        return bad;
    };
    let bad = same(0, 0);
    ok(!bad.length, `Q's timeline as the prototype draws it, the first line's note (${bad.length} pixels differ: ${bad.slice(0, 4).join(' ')})`);
    T.shot('29-q-timeline');
    const lines = PE.qItems(song, 0), cut = lines.findIndex(it => it.cmds.some(q => q.code === 'e4' && q.v === 0xF0));
    T.press('down', cut); T.press('right');
    bad = same(cut, 1);
    ok(!bad.length, `on CUT F0: its plays at full colour, the curve (${bad.length} pixels differ: ${bad.slice(0, 4).join(' ')})`);
    T.press('right'); T.press('up', cut);
    ok(readText(T.m.fb(), 4, 21, 9) === 'SONG 1-2 ' && readText(T.m.fb(), 4, 33, 8) === 'BARS 1-2' && readText(T.m.fb(), 4, 57, 3) === 'CUT',
        `the scales labelled: ${readText(T.m.fb(), 4, 21, 8)} / ${readText(T.m.fb(), 4, 33, 8)} / ${readText(T.m.fb(), 4, 57, 4)}`);
    // a 9-bar song: chain 00 plays phrase 00 eight times, then song row 01 -> chain 02 -> phrase 02, CUT 60 on its row 0;
    // the cursor on that line: the second page (bars 9-16), the curve coming in from CUT F0 on the first page
    T.with_('shift', 'left'); T.with_('shift', 'left');
    T.press('edit'); T.press('down');            // chain rows 2-7: phrase 00 (an EDIT tap copies it, EDIT on an empty cell pastes)
    for (let r = 2; r < 8; r++) { T.press('down'); T.press('edit'); }
    T.press('up', 7); T.with_('shift', 'left'); T.press('down'); T.press('edit'); T.press('edit');   // EDIT EDIT: chain 02
    T.with_('shift', 'right'); T.press('edit'); T.press('edit'); T.with_('shift', 'right');
    T.fx(0, 0, 'CUT', 0x60);
    T.btn(11);   // SEQ
    song.song[1][0] = 2; for (let r = 2; r < 8; r++) song.chains[0][r].ph = 0;
    song.chains[2][0].ph = 2; song.phrases[2][0] = row(E, [['e4', 0x60]]);
    const late = PE.qItems(song, 0).findIndex(it => it.ph === 2);
    T.press('up', 20); T.press('left', 3); T.press('down', late); T.press('right');
    bad = same(late, 1);
    ok(!bad.length && readText(T.m.fb(), 4, 21, 8) === 'SONG 1-9' && readText(T.m.fb(), 4, 33, 9) === 'BARS 9-9 ',
        `the last page (one bar: the song ends there), as the prototype (${readText(T.m.fb(), 4, 21, 8)} / ${readText(T.m.fb(), 4, 33, 9)}; ${bad.length} pixels differ: ${bad.slice(0, 4).join(' ')})`);
    T.shot('29-q-page2');
    // PLAY in Q (step Q-3d, app.js play / liveState qPlayT, events.js draw): the song from the song row where the line
    // plays first; the playhead a red tick on SONG and a line through the page from its ruler; the page follows it
    const PLAYC = rgb565('#ff4f6d'), TLX = 215 - 8 - 128, bars = () => readText(T.m.fb(), 4, 33, 9);
    const head = () => {                         // the playhead's row on the page: a red column from y 33 to 80
        for (let x = TLX; x < TLX + 128; x++) { let y = 33; while (y < 81 && px(x, y) === PLAYC) y++; if (y === 81) return x - TLX; }
        return -1;
    };
    T.press('play'); T.go(300);
    let h = head();
    ok(bars() === 'BARS 9-9 ' && h >= 0 && h <= 4 && px(TLX + Math.floor((128 + h) * 128 / 144), 26) === PLAYC,
        `PLAY on the bar-9 line: the song from S01, the playhead on its page and on SONG (${bars()}, row ${128 + h})`);
    bad = same(late, 1, 128 + h);
    ok(!bad.length, `the playhead as the prototype draws it (${bad.length} pixels differ: ${bad.slice(0, 4).join(' ')})`);
    T.shot('30-q-play');
    T.go(2000); h = head();
    ok(bars() === 'BARS 1-8 ' && h >= 0 && h <= 4 && px(76 - 1, Q_Y(late)) === ACCENT,
        `the song comes round to bar 1: the page follows the playhead, the cursor stays (${bars()}, row ${h})`);
    bad = same(late, 1, h);
    ok(!bad.length, `page 1 with the playhead, as the prototype (${bad.length} pixels differ: ${bad.slice(0, 4).join(' ')})`);
    T.press('play'); T.go(50);
    ok(head() < 0 && bars() === 'BARS 9-9 ', `PLAY again stops: no playhead, the cursor line's page (${bars()})`);
    T.press('up', 20); T.press('left', 3);       // line 0: P00 row 0, 8 plays (chain 00 rows 0-7)
    ok(T.strip() === 'bar 1 S00 CH1, plays 8 times', `stopped: where it plays first (${T.strip()})`);
    T.press('play'); T.go(2500);                 // (about 20 rows: the second pass of P00)
    h = head();
    ok(h > 16 && h < 32 && T.strip() === 'bar 3 S00 CH1, plays 8 times', `playing at row ${h}: the bottom line says the next play (${T.strip()})`);
    T.press('edit');
    ok(mapLit(px, 'P') && readText(T.m.fb(), 4, 4, 13) === 'CH1>S00>C00:2', `the EDIT tap: the next play, chain row 2 (${readText(T.m.fb(), 4, 4, 17)})`);
    T.press('play');
}

// ---- the review's marks on the real screen (docs/ux/hardware-check.md 1.8-1.15, HANDOFF open item 10): the same state
// built here with the gamepad and in the prototype's model, each screen compared pixel for pixel with render.js (freshly
// loaded: nothing from Q-2's renders), so the unit's LCD can be held against the prototype's 1:1 mockups. The bottom
// line is left out where it holds a message (y 222 on)
{
    const require = createRequire(import.meta.url);
    for (const f of ['fonts.js', 'felucca.js', 'model.js', 'control-map.js', 'render.js', 'events.js']) {
        const p = join(ROOT, 'ux-proto', f);
        delete require.cache[require.resolve(p)]; require(p);
    }
    const { Model: PM, Render: PR, Events: PE } = globalThis;
    const T = rig(), song = PM.newSong(), fb = () => T.m.fb();
    const ui = o => Object.assign({
        view: 'song', ch: 0, songRow: 0, chainRow: 0, phraseRow: 0, instSel: 0, colChain: 0, colPhrase: 0, instField: 0, envField: 0,
        lfoField: 0, voxField: 0, fxField: 0, mixField: 0, projRow: 0, charPos: 0, sel: null, kb: 'pad', oct: 0, popup: null, help: '',
        confirm: null, playing: false, msg: '', mute: [false, false, false, false], solo: [false, false, false, false],
        pos: [0, 1, 2, 3].map(() => ({ active: false })), slot: 0, dirty: false, slots: [null, null, null, null], batt: { v: 4.2, pct: 1, usb: false }, blink: true,
    }, o);
    const same = (what, u, y0 = 0, y1 = 222) => {
        const p = PR.makeFB(), bad = [];
        PR.render(p, song, ui(u));
        for (let y = y0; y < y1; y++) for (let x = 0; x < 240; x++) {
            const o = (y * 240 + x) * 4, want = (p[o] >> 3) << 11 | (p[o + 1] >> 2) << 5 | p[o + 2] >> 3;
            if (fb()[y * 240 + x] !== want) bad.push(`${x},${y} ${fb()[y * 240 + x].toString(16)}/${want.toString(16)}`);
        }
        ok(!bad.length, `${what}, as the prototype draws it (${bad.length} pixels differ: ${bad.slice(0, 4).join(' ')})`);
    };
    // rig: S row 00 = chain 00 = phrase 00; here S row 01 = chain 01 (EDIT EDIT), a C-3 on phrase row 0
    song.song[0][0] = 0; song.chains[0][0].ph = 0;
    T.press('edit'); Object.assign(song.phrases[0][0], { n: 48, i: 0, vol: 0x64 });
    T.with_('shift', 'left', 2); T.press('down'); T.press('edit', 2); T.press('up'); song.song[1][0] = 2;   // (the EDIT tap pastes a fresh chain, 01: EDIT EDIT = 02)
    same('1.9 / 1.10 S: the map panel, the song map (chain 00 lit, 02 grey, empty rows), its frame, 00 / 3F', { view: 'song' });
    T.press('right'); T.hold('option'); T.press('shift'); T.free('option');   // mute CH2, solo CH3
    T.press('right'); T.hold('option'); T.press('play'); T.free('option'); T.press('left', 2);
    same("1.12 S: CH2's M grey, CH3's S white, the cells they silence", { view: 'song', mute: [false, true, false, false], solo: [false, false, true, false] });
    T.hold('option'); T.hold('shift'); T.press('play'); T.free('shift'); T.free('option');
    T.go(2000); T.press('down', 63);
    same("1.10 S on row 3F: the song map's frame, its foot in the bottom line", { view: 'song', songRow: 63 }, 0, 240);
    T.press('up', 63);
    T.with_('shift', 'right', 2); T.press('right', 2); T.hold('edit');
    same('1.13 / 1.15 P: the instrument list, the unused ones dimmed, in the accent frame of an overlay with the focus', { view: 'phrase', colPhrase: 2,
        help: 'instrument the note plays', popup: { items: song.inst.map((ins, i) => PM.hex(i, 1) + ' ' + ins.name),
            unused: song.inst.map((x, i) => !PE.instEvents(song, i).ev.length), cur: 0, cursorX: 9 } }, 0, 240);
    T.free('edit'); T.press('left', 2);
    T.m.enc(3, 2); T.go(50); T.m.enc(3, -2); T.go(50);   // KNOB 2 there and back: the strip, the values unchanged
    same('1.15 P: the knob strip in the grey frame of a passing overlay', { view: 'phrase', knobs: { title: 'I0 ' + song.inst[0].name, hot: 1, inst: 0,
        items: [['e4', 'CUT'], ['e5', 'RES'], ['atk', 'ATK'], ['rel', 'REL']].map(([key, label]) => ({ key, label })) } });
    T.go(2000); T.with_('shift', 'right');
    same('1.8 / 1.14 I on ENGINE: the knob-colour bars, the grey KNOBS rings, the KNOBS frame', { view: 'inst' });
    T.press('down', 4);
    same('1.8 I on the KNOBS names: the rings in the knob colours', { view: 'inst', instField: 12 });
    T.with_('shift', 'down');
    same('1.8 / 1.13 / 1.14 E on ATK: the bars, the dotted sustain, TIME, the KNOBS frame', { view: 'env' });
    T.press('down', 3);
    same('1.8 E on the KNOBS values', { view: 'env', envField: 11 });
    T.with_('shift', 'down');
    same('1.14 L: the KNOBS frame (under the moving waveform)', { view: 'lfo' }, 100);
    T.with_('shift', 'down');
    same('1.14 V: the KNOBS frame', { view: 'vox' });
    T.with_('shift', 'left'); T.with_('shift', 'down', 2);   // P, F, X
    T.hold('edit'); T.press('up', 4); T.free('edit'); song.mix[0].lvl = 127;
    same('1.11 X: the 0 dB ticks, CH1 at +7.5dB', { view: 'mix' });
}

// ---- the phrase timeline and ramp lines follow the player (code-review.md §8c U37-U44, U48; C16 / C17): the same
// phrases built here with the gamepad and in the prototype's model (which applies the same rules), compared pixel for
// pixel with render.js, stopped; then the rules that need playback, as point checks
{
    const require = createRequire(import.meta.url);
    for (const f of ['fonts.js', 'felucca.js', 'model.js', 'control-map.js', 'render.js', 'events.js']) {
        const p = join(ROOT, 'ux-proto', f);
        delete require.cache[require.resolve(p)]; require(p);
    }
    const { Model: PM, Render: PR } = globalThis;
    const T = rig(), song = PM.newSong(), E = PM.EMPTY;
    const row = (n, fx, i = 0) => Object.assign(PM.emptyRow(), { n, i: n === E ? E : i, vol: n === E ? E : 0x64,
        fx: [0, 1, 2].map(k => fx && fx[k] ? { c: fx[k][0], v: fx[k][1] } : { c: '', v: 0 }) });
    const same = (what, u) => {
        const p = PR.makeFB(), bad = [];
        PR.render(p, song, Object.assign({ view: 'phrase', ch: 0, songRow: 0, chainRow: 0, phraseRow: 14, colPhrase: 0, instSel: 0,   // (the cursor: T.to(14, 0))
            sel: null, kb: 'pad', oct: 0, popup: null, help: '', confirm: null, playing: false, msg: '', mute: [false, false, false, false],
            solo: [false, false, false, false], pos: [0, 1, 2, 3].map(() => ({ active: false })), slot: 0, dirty: false, slots: [null, null, null, null],
            batt: { v: 4.2, pct: 1, usb: false }, blink: true }, u));
        for (let y = 32; y < 224; y++) for (let x = 0; x < 207; x++) {
            const o = (y * 240 + x) * 4, want = (p[o] >> 3) << 11 | (p[o + 1] >> 2) << 5 | p[o + 2] >> 3;
            if (T.m.fb()[y * 240 + x] !== want) bad.push(`${x},${y} ${T.m.fb()[y * 240 + x].toString(16)}/${want.toString(16)}`);
        }
        ok(!bad.length, `${what}, as the prototype draws it (${bad.length} pixels differ: ${bad.slice(0, 4).join(' ')})`);
    };
    // SWING 50 % (G), so odd rows strike 2 ticks late (U42)
    T.with_('shift', 'up'); T.press('down', 3); T.press('right'); T.press('down', 2); T.with_('edit', 'up', 5);
    const swing = T.cell(21, 5, 12).replace(/\s+/g, ' ').trim();
    T.with_('shift', 'down');
    song.swing = 50;
    // phrase 00: U38 a legato note under a running KIL; U39 a KIL on a row without a note; U40 RET + KIL; U42 an odd row;
    // U48 two KIL / OFF; U37 a KIL carried into the next phrase; U43 a ramp stopped by VOL; U44 no ramp under KIL
    T.note(0); T.fx(0, 0, 'KIL', 0x0C);
    T.note(1); T.to(1, 2); T.with_('option', 'edit');                   // C-3 without an instrument: legato
    T.note(4); T.fx(4, 0, 'KIL', 0x18); T.fx(5, 0, 'KIL', 0x01);
    T.note(8); T.fx(8, 0, 'RET', 0x04); T.fx(8, 1, 'KIL', 0x02);
    T.note(11); T.fx(11, 0, 'KIL', 0x02);
    T.note(13); T.fx(13, 0, 'KIL', 0x02); T.fx(13, 1, 'OFF', 0x04);
    T.note(15); T.fx(15, 0, 'KIL', 0x0C);
    T.fx(2, 2, 'TV1', 0); T.fx(3, 2, 'RTO', 0xFF); T.fx(4, 2, 'RTI', 0x08); T.fx(6, 2, 'TV1', 0x80);
    T.fx(9, 2, 'KIL', 0x03); T.fx(10, 2, 'RTO', 0x20);
    T.to(14, 0);
    Object.assign(song.phrases[0], { 0: row(48, [['K', 0x0C]]), 1: row(48, null, E), 2: row(E, [, , ['V1', 0]]), 3: row(E, [, , ['O', 0xFF]]),
        4: row(48, [['K', 0x18], , ['I', 8]]), 5: row(E, [['K', 1]]), 6: row(E, [, , ['V1', 0x80]]), 8: row(48, [['G', 4], ['K', 2]]),
        9: row(E, [, , ['K', 3]]), 10: row(E, [, , ['O', 0x20]]), 11: row(48, [['K', 2]]), 13: row(48, [['K', 2], ['F', 4]]), 15: row(48, [['K', 0x0C]]) });
    song.song[0][0] = 0; song.chains[0][0].ph = 0;
    ok(/50%/.test(swing) && T.cell(9, 1, 1) === '~', `the set-up: ${swing}, row 1 without an instrument (${T.cell(9, 1, 1)})`);
    same('P00: the timeline (legato under a KIL, a KIL alone, RET + KIL, a swung row, KIL + OFF) and the ramp lines', {});
    // phrase 01 after it (chain row 1): the KIL carried in from P00's row F (U37, stopped: P00's own timeline)
    T.with_('shift', 'left'); T.press('down'); T.press('edit', 2); T.with_('shift', 'right');
    song.chains[0][1].ph = 1;
    same('P01: the KIL carried in from P00', { chainRow: 1 });
}
{
    // the rules that need playback: U37 the carried KIL from what the player has; U41 RET's count from its ramp; U46 a
    // KIL on the side CHA dropped is not drawn
    const T = rig(), px = (x, y) => T.m.fb()[y * 240 + x], CH1 = rgb565('#4fa3ff'), TEXT = rgb565('#e8e8e8'), LABEL = rgb565('#777777');
    const under = r => { let n = 0; for (let x = cellX(2); x < cellX(2) + 18; x++) { const c = px(x, 32 + r * 12 + 11); if (c === LABEL || c === TEXT || c === CH1) n++; } return n; };
    T.note(0); T.fx(0, 0, 'RET', 0x02); T.fx(1, 0, 'RTO', 0x08); T.fx(2, 0, 'RTI', 0x04);   // RET ramps 2 -> 8 over rows 0-4
    T.note(8); T.fx(8, 0, 'CHA', 0xF0); T.fx(8, 1, 'KIL', 0x02);       // the right side always loses its roll
    T.to(14, 0);
    const still = [under(3), under(8)];
    T.press('play');
    let ret3 = 0, kil8 = -1, carry0 = 0;
    for (let t = 0; t < 1400; t += 10) {
        T.go(10);
        const h = T.playhead();
        if (h === 3) ret3 = Math.max(ret3, under(3));
        if (h === 9 && kil8 < 0) kil8 = under(8);
    }
    T.press('play');
    const L = rig();                            // a phrase with only C-3 KIL 0C on row F: on the next pass (it loops) the
    L.note(15); L.fx(15, 0, 'KIL', 0x0C); L.to(14, 0);   // cut runs on through row 0
    const lpx = (x, y) => L.m.fb()[y * 240 + x], lunder = () => { let n = 0; for (let x = cellX(2); x < cellX(2) + 18; x++) if (lpx(x, 32 + 11) === LABEL) n++; return n; };
    const stopped0 = lunder();
    L.press('play');
    for (let t = 0; t < 2600; t += 10) { L.go(10); if (L.playhead() === 0 && t > 1500) carry0 = Math.max(carry0, lunder()); }
    L.press('play');
    ok(still[0] === 0 && ret3 >= 8, `U41: RET ramped, row 3 (no RET cell) shows the player's hits while it plays (stopped ${still[0]}, playing ${ret3} px)`);
    ok(still[1] > 0 && kil8 === 0, `U46: CHA F0, the right side's KIL 02 dropped: drawn stopped (${still[1]} px), not after the roll (${kil8})`);
    ok(stopped0 === 0 && carry0 >= 15, `U37: the KIL from row F, carried into the next pass, drawn on row 0 while playing (${stopped0} -> ${carry0} px)`);
}
{
    // U49: P00 HOP 08 -> P01 entered at 8; P01 HOP 02 on row 1, HOP 05 on row A: P02 entered at 5 (rows 0-4 dimmed)
    const T = rig(), DIM = rgb565('#555555');
    const rowColour = r => { const c = new Map(); for (let y = 0; y < 12; y++) for (let x = 0; x < 6; x++) { const v = T.m.fb()[(32 + r * 12 + y) * 240 + 4 + x]; if (v) c.set(v, (c.get(v) || 0) + 1); } return [...c].sort((a, b) => b[1] - a[1])[0]?.[0]; };
    T.fx(4, 0, 'HOP', 8);
    T.with_('shift', 'left'); T.press('down'); T.press('edit', 2); T.with_('shift', 'right');
    T.fx(1, 0, 'HOP', 2); T.fx(10, 0, 'HOP', 5);
    T.with_('shift', 'left'); T.press('down'); T.press('edit', 2); T.with_('shift', 'right');
    T.to(15, 0);
    ok(rowColour(3) === DIM && rowColour(4) === DIM && rowColour(5) !== DIM, 'U49: P02 entered at row 5 (where P01, entered at 8, really hops): rows 0-4 dimmed');
}
{
    // U45: Q's NOTE dots at the pitch played: chain 00 plays P00 (one C-3) at +00 and at +0C: two heights
    const T = rig(), px = (x, y) => T.m.fb()[y * 240 + x];
    T.note(0);
    T.with_('shift', 'left'); T.press('down'); T.press('edit'); T.go(400); T.with_('edit', 'left');   // chain row 1: P00 again
    T.press('right'); T.with_('edit', 'right', 12); T.press('left');   // its transpose +0C
    const tr = T.cell(5, 1, 3), c1 = T.cell(2, 1, 2);
    T.with_('shift', 'right'); T.m.input(0, 1 << 11); T.go(25); T.m.input(0, 0); T.go(25);   // SEQ: Q of instrument 0
    const ys = new Set();
    for (let y = 45; y < 57; y++) for (let x = 79; x < 207; x++) if (px(x, y) && px(x, y) !== rgb565('#777777')) ys.add(y);
    ok(c1 === '00' && Math.max(...ys) - Math.min(...ys) >= 6, `U45: Q's NOTE dots for P00 at +00 and ${tr}: two heights (rows ${[...ys].join(',')})`);
}

{
    // U43 in Q: the focus curve (CUT) ramps 20 -> FF over 8 rows, but VOL in its column on row 4 stops it there (as the
    // player); compared with events.js (same rule)
    const require = createRequire(import.meta.url);
    for (const f of ['fonts.js', 'felucca.js', 'model.js', 'control-map.js', 'render.js', 'events.js']) {
        const p = join(ROOT, 'ux-proto', f);
        delete require.cache[require.resolve(p)]; require(p);
    }
    const { Model: PM, Render: PR, Events: PE } = globalThis;
    const T = rig(), px = (x, y) => T.m.fb()[y * 240 + x], E = PM.EMPTY;
    T.note(0); T.fx(0, 0, 'CUT', 0x20); T.fx(1, 0, 'RTO', 0xFF); T.fx(2, 0, 'RTI', 0x08); T.fx(4, 0, 'VOL', 0x40);
    T.to(0, 0);
    T.m.input(0, 1 << 11); T.go(25); T.m.input(0, 0); T.go(25);   // SEQ: Q of instrument 0
    T.press('right');                            // the line's CUT: the focus
    const song = PM.newSong(), row = (n, fx) => Object.assign(PM.emptyRow(), { n, i: n === E ? E : 0, vol: n === E ? E : 0x64,
        fx: [0, 1, 2].map(k => fx && fx[k] ? { c: fx[k][0], v: fx[k][1] } : { c: '', v: 0 }) });
    song.song[0][0] = 0; song.chains[0][0].ph = 0;
    Object.assign(song.phrases[0], { 0: row(48, [['e4', 0x20]]), 1: row(E, [['O', 0xFF]]), 2: row(E, [['I', 8]]), 4: row(E, [['V1', 0x40]]) });
    const fb = PR.makeFB(), bad = [];
    PE.drawQ(fb, song, { view: 'q', instSel: 0, qRow: 0, qCol: 1, qTop: 0, songRow: 0, ch: 0, chainRow: 0 });
    for (let y = 21; y < 82; y++) for (let x = 4; x < 207; x++) {
        const o = (y * 240 + x) * 4, want = (fb[o] >> 3) << 11 | (fb[o + 1] >> 2) << 5 | fb[o + 2] >> 3;
        if (px(x, y) !== want) bad.push(`${x},${y} ${px(x, y).toString(16)}/${want.toString(16)}`);
    }
    ok(!bad.length, `U43 in Q: the CUT curve stops where VOL in its column stops the ramp, as the prototype (${bad.length} pixels differ: ${bad.slice(0, 4).join(' ')})`);
}

{
    // U48 in the player: KIL 01 and KIL 05 on one row: the first cuts (after 1 tick), as the screen draws it
    const a = rig(); a.note(0); const ra = a.playRecord(400); a.press('play');
    const k = rig(); k.note(0); k.fx(0, 0, 'KIL', 1); k.fx(0, 1, 'KIL', 5); const rk = k.playRecord(400); k.press('play');
    const o = onset(rk), cut = rms(rk, o + 2 * 919, o + 4 * 919), full = rms(ra, onset(ra) + 2 * 919, onset(ra) + 4 * 919);
    ok(cut < full / 10, `KIL 01 and KIL 05 on a row: cut after 1 tick (ticks 2-4 rms ${cut.toFixed(4)} vs ${full.toFixed(4)})`);
}

// ---- coverage of the player's commands (code-review.md §8d C1-C8, C11), measured on the WAV: the pitch by
// autocorrelation, the level by rms, onsets by a threshold. A tick is 918.75 samples at 120 BPM; windows skip the
// first ~300 samples of a tick (the bend's ~6 ms smoothing)
const TK = 918.75;
function freq(a, i0, n) {                      // the fundamental of a[i0 .. i0 + n), Hz (normalised autocorrelation)
    const lag = l => { let s = 0, e1 = 0, e2 = 0; for (let i = i0; i < i0 + n; i++) { s += a[i] * a[i + l]; e1 += a[i] * a[i]; e2 += a[i + l] * a[i + l]; } return s / Math.sqrt(e1 * e2 + 1e-12); };
    const r = []; for (let l = 0; l < 700; l++) r.push(l < 30 ? -1 : lag(l));
    const best = Math.max(...r);
    let l = 30; while (r[l] < 0.92 * best) l++;  // the first lag near the best: no octave errors
    while (r[l + 1] > r[l]) l++;
    const y0 = r[l - 1], y1 = r[l], y2 = r[l + 1], d = (y0 - y2) / (2 * (y0 - 2 * y1 + y2));   // parabolic peak
    return 44100 / (l + (isFinite(d) ? d : 0));
}
const st = (f, f0) => 12 * Math.log2(f / f0);  // semitones from f0
{
    // unit-feedback #30 (user, 2026-10-12; proto-test "EDIT on an instrument value auditions it"): EDIT pressed on an
    // instrument page value plays the instrument while held, the last note used; not on NAME nor a KNOBS name
    const T = rig(), px = (x, y) => T.m.fb()[y * 240 + x];
    T.to(0, 0); T.press('edit'); T.with_('edit', 'right', 2); T.go(1000);   // D-3: the last note used
    T.with_('shift', 'right'); T.press('down');   // I: LEVEL
    const held = () => { T.go(1500); T.hold('edit'); const a = T.record(400); T.free('edit'); return a; };
    const at = (cx, row) => px(cellX(cx) - 1, IROW_Y(row) + 2 + 12) === ACCENT;
    const lv = held(), f = freq(lv, 4410, 4096);
    T.go(1500); T.hold('edit'); T.press('left'); const ch = T.record(400); T.free('edit'); const fc = freq(ch, 4410, 4096);
    T.press('down', 3); const onKnob = px(21, 195) === ACCENT, kn = held();   // (slot 1's name, right of its ring)
    T.press('down'); const kv = held();
    T.press('up', 4); T.press('right', 2); const onName = at(16, 1), nm = held();
    ok(rms(lv) > 0.001 && Math.abs(st(f, 146.83)) < 0.5, `EDIT on LEVEL: heard, D-3 the last note (rms ${rms(lv).toFixed(4)}, ${f.toFixed(1)} Hz)`);
    ok(onName && rms(nm) < 0.0005 && onKnob && rms(kn) < 0.0005, `EDIT on NAME, on a KNOBS name: silent (${onName} ${rms(nm).toFixed(4)}; ${onKnob} ${rms(kn).toFixed(4)})`);
    ok(rms(kv) > 0.001, `EDIT on a KNOBS value: heard (rms ${rms(kv).toFixed(4)})`);
    ok(rms(ch) > 0.001 && Math.abs(st(fc, 146.83)) < 0.5, `EDIT + left on LEVEL: the last note too, not C-4 (${fc.toFixed(1)} Hz)`);
}
{
    // C1: ARP 37: +3 / +7 semitones on ticks 1 / 2 (and 4 / 5), held on the row after (decisions.md "Rampable ARP and
    // VIB"), ARP 00 on row 2 stops it
    const T = rig(); T.to(0, 0); T.hold('edit'); T.go(155); T.free('edit'); T.fx(0, 0, 'ARP', 0x37); T.fx(2, 0, 'ARP', 0);   // (C-3 auditioned 180 ms,
    // as when EDIT's audition was that long: the measured pitches depend on the engine's state)
    const a = T.playRecord(600); T.press('play');
    const o = onset(a), at = (row, tick) => st(freq(a, Math.round(o + (row * 6 + tick) * TK + 350), 520), freq(a, Math.round(o + 3 * 6 * TK), 2000));
    const got = [at(0, 1), at(0, 2), at(0, 3), at(1, 1), at(1, 2), at(2, 1), at(2, 2)].map(v => v.toFixed(1));
    ok(Math.abs(at(0, 1) - 3) < 0.3 && Math.abs(at(0, 2) - 7) < 0.3 && Math.abs(at(0, 3)) < 0.3 && Math.abs(at(1, 1) - 3) < 0.3 && Math.abs(at(1, 2) - 7) < 0.3
        && Math.abs(at(2, 1)) < 0.3 && Math.abs(at(2, 2)) < 0.3,
        `C1 ARP 37: +3 / +7 on ticks 1 / 2, 0 on tick 3; held on the row after; ARP 00 stops it (${got.join(' ')} st)`);
}
{
    // C2: PIT 10 on row 0, held on row 1: the bend grows a semitone a tick while the note lasts; a new note on row 2 with
    // PIT 00 starts at 0
    const T = rig(); T.note(0); T.fx(0, 0, 'PIT', 0x10); T.note(2); T.fx(2, 0, 'PIT', 0);
    const a = T.playRecord(600); T.press('play');
    const o = onset(a), f0 = freq(a, Math.round(o + 2 * 6 * TK + 2 * TK), 1500), at = (row, tick) => st(freq(a, Math.round(o + (row * 6 + tick) * TK + 350), 520), f0);
    const want = [[0, 0, 1], [0, 2, 3], [0, 4, 5], [1, 1, 8], [1, 2, 9]], got = want.map(([r, k]) => at(r, k));   // (+1 at tick 0: the
    ok(want.every(([, , v], i) => Math.abs(got[i] - v) < 0.4),                // first tick's step; row 2's new note is the 0)
        `C2 PIT 10, held over two rows: +1 st a tick from the note, row 2's new note (PIT 00) back at 0 (${got.map(v => v.toFixed(1)).join(' ')} st)`);
}
{
    // C3: VIB F8 (speed F, depth 8: +-1 st) swings the pitch from its row on, held on the next (its wobble goes on);
    // VIB 00 on row 2: steady
    const T = rig(); T.note(0); T.fx(0, 0, 'VIB', 0xF8); T.fx(2, 0, 'VIB', 0);
    const a = T.playRecord(700); T.press('play');
    const o = onset(a), f0 = freq(a, Math.round(o + 3 * 6 * TK), 2000);
    const sw = []; for (let k = 0; k < 12; k++) sw.push(st(freq(a, Math.round(o + k * TK + 350), 520), f0));
    const next = st(freq(a, Math.round(o + 2 * 6 * TK + 350), 2000), f0), r1 = sw.slice(6);
    ok(Math.max(...sw) > 0.6 && Math.min(...sw) < -0.6 && Math.max(...sw) < 1.3 && Math.max(...r1) > 0.6 && Math.min(...r1) < -0.6 && Math.abs(next) < 0.2,
        `C3 VIB F8: +-1 st on its row and the next (${sw.map(v => v.toFixed(1)).join(' ')}), steady after VIB 00 (${next.toFixed(2)})`);
}
{
    // decisions.md "Rampable ARP and VIB": a ramp under ARP moves each digit on its own, once a row: ARP 37 + RTO 7C (one
    // row): row 1 plays +7 / +C; the ramp line under VIB 24 + RTO 4F + RTI 04 (VIB was unrampable)
    const T = rig(); T.to(0, 0); T.hold('edit'); T.go(155); T.free('edit');
    T.fx(0, 0, 'ARP', 0x37); T.fx(1, 0, 'RTO', 0x7C); T.fx(3, 0, 'ARP', 0);
    T.note(8); T.fx(8, 1, 'VIB', 0x24); T.fx(9, 1, 'RTO', 0x4F); T.fx(10, 1, 'RTI', 4); T.to(15, 0);
    const LINE = rgb565('#333333'), rx = cellX(18) - 3, line = [9, 10, 12].every(r => T.m.fb()[(rowY(r) + 5) * 240 + rx] === LINE);
    const a = T.playRecord(700); T.press('play');
    const o = onset(a), at = (row, tick) => st(freq(a, Math.round(o + (row * 6 + tick) * TK + 350), 520), freq(a, Math.round(o + 4 * 6 * TK), 2000));
    const got = [at(0, 1), at(0, 2), at(1, 1), at(1, 2)].map(v => v.toFixed(1));
    ok(Math.abs(at(0, 1) - 3) < 0.3 && Math.abs(at(0, 2) - 7) < 0.3 && Math.abs(at(1, 1) - 7) < 0.3 && Math.abs(at(1, 2) - 12) < 0.3 && line,
        `ARP 37 + RTO 7C: +3 / +7, then +7 / +C, each digit on its own (${got.join(' ')} st); a ramp line under VIB 24 + RTO 4F (${line})`);
}
{
    // the ghost: an ARP held from row 0 drawn on the playhead row in its first empty FX cell, at half CH1's brightness;
    // on a row whose FX1 is taken, in FX2; none stopped
    const T = rig(), GHOST = rgb565('#27517f'), px = (x, y) => T.m.fb()[y * 240 + x];
    T.note(0); T.fx(0, 0, 'ARP', 0x37); T.fx(2, 0, 'TV1', 0x80); T.to(15, 0);
    const lit = (cx, r) => { for (let j = 0; j < 12; j++) for (let i = 0; i < 18; i++) if (px(cellX(cx) + i, rowY(r) + j) === GHOST) return true; return false; };
    T.press('play');
    T.until(2); T.go(30); const r2 = T.cell(18, 2, 3) + ' ' + T.cell(22, 2, 2), l2 = lit(18, 2);
    T.until(3); T.go(30); const r3 = T.cell(11, 3, 3) + ' ' + T.cell(15, 3, 2), l3 = lit(11, 3);
    T.press('play'); T.go(50);
    const off = T.cell(11, 3, 3);
    ok(r3 === 'ARP 37' && l3 && r2 === 'ARP 37' && l2 && off === '---',
        `the ghost: row 3 FX1 ${r3} (${l3}), row 2 (FX1 taken) FX2 ${r2} (${l2}); stopped: ${off}`);
}
{
    // (the FX first: EDIT on an FX cell auditions, #26; the note's audition last, as before)
    // C4: OFF 02 releases at tick 2 (the instrument's REL, not a cut like KIL 02); OFF 0C on row 0 releases at row 2 tick 0
    const n = rig(); n.note(0); const rn = n.playRecord(700); n.press('play');
    const k = rig(); k.fx(0, 0, 'KIL', 2); k.note(0); const rk = k.playRecord(700); k.press('play');
    const f = rig(); f.fx(0, 0, 'OFF', 2); f.note(0); const rf = f.playRecord(700); f.press('play');
    const c = rig(); c.fx(0, 0, 'OFF', 0x0C); c.note(0); const rc = c.playRecord(700); c.press('play');
    const w = (a, t0, t1) => { const o = onset(a); return rms(a, Math.round(o + t0 * TK), Math.round(o + t1 * TK)); };
    const full = w(rn, 2.05, 2.35), cut = w(rk, 2.05, 2.35), rel = w(rf, 2.05, 2.35);   // (the first ~7 ms after tick 2: ACID's REL is short)
    ok(rel > 3 * cut && rel < full, `C4 OFF 02: a release from tick 2, not a cut (just after: rms ${rel.toFixed(4)}; KIL 02 ${cut.toFixed(4)}, none ${full.toFixed(4)})`);
    ok(w(rc, 7, 11) > 0.8 * w(rn, 7, 11) && w(rc, 14, 18) < 0.8 * w(rn, 14, 18),
        `C4 OFF 0C: held through row 1, released from row 2 (row 1 ${w(rc, 7, 11).toFixed(4)} / ${w(rn, 7, 11).toFixed(4)}, row 2 ${w(rc, 14, 18).toFixed(4)} / ${w(rn, 14, 18).toFixed(4)})`);
}
{
    // C5: VOL 00 + REP 40 alone (no RTO): the velocity climbs 64 a row and stops at the range end (FF from row 4 on)
    const T = rig(); for (let r = 0; r < 8; r++) T.note(r);
    T.fx(0, 0, 'VOL', 0); T.fx(1, 0, 'REP', 0x40);
    const a = T.playRecord(1200); T.press('play');
    const o = onset(a, 0.00001), lv = r => rms(a, Math.round(o + r * 6 * TK + 200), Math.round(o + r * 6 * TK + 2000));
    const v = [1, 2, 3, 4, 5, 6].map(lv);
    ok(v[0] < v[1] && v[1] < v[2] && v[2] < v[3] && Math.abs(v[4] - v[3]) < 0.2 * v[3] && Math.abs(v[5] - v[3]) < 0.2 * v[3],
        `C5 REP 40 alone: louder row by row to the range end, then held (rows 1-6 rms ${v.map(x => x.toFixed(3)).join(' ')})`);
}
{
    // C6: TV1 80 / RTO FF / RTI 04, RCU LIN / EXP / LOG: at row 2 of 4 the track level is 1/2, CURVE_EXP[8] (20 %) or
    // CURVE_LOG[8] (81 %) of the way (from 80: heard from the start, so the onset is the note's)
    const lvl = curve => {
        const T = rig(); for (let r = 0; r < 4; r++) T.note(r);
        T.fx(0, 0, 'TV1', 0x80); T.fx(1, 0, 'RTO', 0xFF); T.fx(2, 0, 'RTI', 4); if (curve) T.fx(3, 0, 'RCU', curve);
        const a = T.playRecord(600); T.press('play');
        const o = onset(a, 0.00001);
        return rms(a, Math.round(o + 12 * TK + 200), Math.round(o + 12 * TK + 2000));
    };
    const lin = lvl(0), exp = lvl(1), log = lvl(2);
    ok(exp < 0.7 * lin && log > 1.2 * lin, `C6 RCU: at row 2 of 4, EXP below LIN below LOG (rms ${exp.toFixed(4)} / ${lin.toFixed(4)} / ${log.toFixed(4)})`);
}
{
    // C7: SWG 00 / RTO FF / RTI 04: the swing rises over 4 rows, so odd rows strike later row by row (row 1: 1 tick,
    // row 3: 2 ticks); notes cut after a tick so each onset stands alone
    const T = rig(); for (const r of [0, 1, 3]) { T.note(r); T.fx(r, 1, 'KIL', 1); }
    T.fx(0, 0, 'SWG', 0); T.fx(1, 0, 'RTO', 0xFF); T.fx(2, 0, 'RTI', 4);
    const a = T.playRecord(800); T.press('play');
    const pk = Math.max(...a.slice(0, 20000).map(Math.abs)), th = 0.3 * pk;   // (above the effects' tails)
    const o = onset(a, th), next = from => from + onset(a.slice(from), th);
    const o1 = next(Math.round(o + 3 * TK)), o3 = next(Math.round(o + 14 * TK));
    const late1 = (o1 - o) / TK - 6, late3 = (o3 - o) / TK - 18;
    ok(Math.abs(late1 - 1) < 0.35 && Math.abs(late3 - 2) < 0.35, `C7 a SWG ramp: row 1 ${late1.toFixed(2)} ticks late, row 3 ${late3.toFixed(2)} (1, then 2)`);
}
{
    // C8: RET 04: 3 more hits in the row, (6 - DEL) / 4 ticks apart; RET 44: each hit softer
    const hits = (ret, del) => {
        const T = rig(); T.note(0); T.with_('edit', 'down'); T.with_('edit', 'right');   // C#2: the DRUM KIT's rim, short hits
        T.to(0, 2); T.hold('edit'); T.press('down', 3); T.free('edit');   // instrument 3: DRUM KIT
        T.fx(0, 0, 'RET', ret); if (del) T.fx(0, 1, 'DEL', del); T.fx(1, 0, 'KIL', 0);
        const a = T.playRecord(500); T.press('play');
        const o = onset(a, 0.002), W = 64, env = [];
        for (let i = o; i < o + 6 * TK; i += W) env.push(rms(a, i, i + W));
        const at = [], peak = [];
        for (let j = 2; j < env.length; j++) if (env[j] > 1.6 * env[j - 2] && env[j] > 0.01 && (!at.length || j - at[at.length - 1] > 6)) at.push(j);
        for (const j of at) peak.push(Math.max(...env.slice(j, j + 4)));
        return { n: at.length + 1, gaps: at.map((j, i) => (j - (i ? at[i - 1] : 0)) * W / TK), peak };
    };
    const r4 = hits(0x04, 0), r44 = hits(0x44, 0);
    ok(r4.n === 4 && r4.gaps.slice(1).every(g => Math.abs(g - 1.5) < 0.3), `C8 RET 04: 4 hits, 1.5 ticks apart (${r4.n} hits, gaps ${r4.gaps.map(g => g.toFixed(2)).join(' ')})`);
    const fall = r => r.peak[2] / r.peak[0];    // (the rim's own hits overlap: compared with RET 04's)
    ok(r44.n === 4 && fall(r44) < 0.8 * fall(r4), `C8 RET 44: each hit softer than RET 04's (3rd / 1st retrigger ${fall(r44).toFixed(2)} vs ${fall(r4).toFixed(2)})`);
}
{
    // C11: the pitch the player sends is reset at stop: PIT bends the note up, stop mid-row, then the EDIT audition of
    // that row is at the note's own pitch
    const base = rig(); base.to(0, 0); base.hold('edit'); base.go(30); const b = base.record(200); base.free('edit');   // (EDIT's
    // audition lasts while it is held)
    const T = rig(); T.note(0); T.fx(0, 0, 'PIT', 0x10); T.to(0, 0);
    T.press('play'); T.go(400); T.press('play'); T.go(100);
    T.hold('edit'); T.go(30); const a = T.record(200); T.free('edit');
    const d = st(freq(a, 2000, 3000), freq(b, 2000, 3000));
    ok(Math.abs(d) < 0.2, `C11 after PIT and stop, the audition is at the note's pitch (${d.toFixed(2)} st off)`);
}

// ---- the audio ISR never touches ins_desc's shared scratch track (code-review.md S1, S1b). The emulator runs the ISR only
// between main-loop passes, so the race cannot happen here: the source is checked instead. cmd_target is the ISR's only
// way to a descriptor (a strike, a ramp's tick, the preview); it takes the engine's own one, which equals the engines'
// mode-dependent ones (desc hooks) in range and list size because none of those is a list
{
    const src = f => readFileSync(join(ROOT, 'fw', 'firmware', f), 'utf8');
    const body = src('src/commands.c').match(/static int32_t cmd_target\([^)]*\)[^{]*\{([\s\S]*?)\r?\n\}/)[1];
    ok(!/ins_desc|cmd_desc|scratch/.test(body), "cmd_target (the ISR's) reads no shared scratch track: no ins_desc / cmd_desc / scratch");
    const hooks = [];
    for (const e of ['lofi', 'noise', 'phys']) for (const m of src(`sound/eng_${e}.c`).matchAll(/static const param_desc_t (\w+) = \{"[^"]*", (F_\w+)/g)) hooks.push(m[1] + ' ' + m[2]);
    ok(hooks.length >= 8 && hooks.every(h => !/F_ENUM|F_ONOFF/.test(h)), `the engines' mode-dependent descriptors are no lists (${hooks.join(', ')})`);
}

// ---- the drum kit at its loudest (drum_voice.c dv_rim_run's drive, dv_bell_run's strike): KIT 80, TONE, SNAP and
// ACC at full; in 32 bits these products wrap (Felucca widened them; fm1-firmwares.md section 10)
{
    const T = rig();
    T.note(0);                                   // C-3 ... C#2 = 37, GM side stick: the RIM lane
    T.with_('edit', 'down'); T.with_('edit', 'right');
    T.to(0, 2); T.hold('edit'); T.press('down', 3); T.free('edit');   // instrument 3: DRUM KIT
    T.fx(0, 0, 'KIT', 4); T.to(0, 4); T.hold('edit'); const kit = T.strip(30); T.free('edit');
    T.fx(0, 1, 'TON', 0xFF); T.fx(0, 2, 'SNP', 0xFF); T.fx(1, 0, 'ACC', 0xFF);   // (ACC from row 1: the 2nd pass)
    T.note(8); T.with_('edit', 'up'); T.with_('edit', 'right', 7);    // G#3 = 56, GM cowbell: the BELL lane
    ok(kit.startsWith('KIT 04 = 80') && T.cell(2, 0, 3) === 'C#2' && T.cell(2, 8, 3) === 'G#3', `the kit and the notes (${kit}, ${T.cell(2, 0, 3)}, ${T.cell(2, 8, 3)})`);
    const a = T.playRecord(4100);              // the 2nd pass (ACC held): the rim at 2 s, the bell at 3 s
    T.press('play');
    const hit = s => { const i = Math.round(s * 44100); return a.slice(i + onset(a.slice(i))); };
    // the rim, driven into the soft clip: a square at the body's pitch, each half-cycle 12+ samples. A wrapped
    // product flips the sign in the middle of a half-cycle (runs of 3..8 samples)
    const r = hit(2);
    let run = 1, shortest = 99;
    for (let i = 1; i < 882; i++) {
        if (Math.sign(r[i]) === Math.sign(r[i - 1]) || Math.abs(r[i]) < 0.05) { run++; continue; }
        if (Math.abs(r[i - 1]) > 0.05) shortest = Math.min(shortest, run);
        run = 1;
    }
    ok(shortest >= 10, `KIT 80 RIM at full TONE SNAP ACC: no half-cycle of the drive split by a wrap (shortest ${shortest} samples)`);
    // the bell: the strike is loudest at the hit; wrapped, its envelope term goes negative and the first ms drop out
    const b = hit(3), b0 = rms(b, 0, 88), b1 = rms(b, 88, 176);
    ok(b0 > b1, `KIT 80 BELL at full SNAP ACC: the strike's first 2 ms are its loudest (${b0.toFixed(3)} > ${b1.toFixed(3)})`);
}

// ---- OCT- at power-on (the boot stub, fw/firmware/stub/stub.c): PA2 low powers the key matrix (HANDOFF section 2),
// so the rail comes up before the key scan; the app no longer scans for OCT- itself. Device-only code: the source is checked
{
    const src = f => readFileSync(join(ROOT, 'fw', 'firmware', f), 'utf8');
    const stub = src('stub/stub.c'), rail = stub.match(/static void shw_rail\(uint32_t on\)\s*\{([\s\S]*?)\r?\n\}/);
    ok(rail && /R32\(0x50000u\) &= ~\(1u << 2\)/.test(rail[1]) && /R32\(0x50008u\) &= ~\(1u << 2\)/.test(rail[1]), 'the stub drives PA2 low as an output (shw_rail(1))');
    const on = stub.indexOf('shw_rail(1);'), scan = stub.indexOf('key = stub_key();');
    ok(on > 0 && scan > on, 'the stub powers the key matrix (PA2 low) before it scans for OCT-');
    ok(!/recovery_key/.test(src('src/recovery.c') + src('src/platform.c')), 'the app has no power-on OCT- scan of its own (the stub owns it)');
}

// ---- Felucca's live layers are off (decisions.md 2026-10-12, fw/OPTIMIZE.md M1 + M2): the SLICER insert and the FX hold
// layer are not compiled in; fx.c's calls land on stubs in sound.c. A source check (the pool they freed is device-only)
{
    const src = f => readFileSync(join(ROOT, 'fw', 'firmware', f), 'utf8'), sound = src('src/sound.c');
    ok(!/#include "slicer\.c"/.test(sound) && !/#include "perform\.c"/.test(sound + src('sound/fx.c')), 'slicer.c and perform.c are not included');
    ok(/static int perf_begin\(uint32_t n\) \{ \(void\)n; return 0; \}/.test(sound) && /static int slicer_busy\(const track_t \*t\) \{ \(void\)t; return 0; \}/.test(sound),
       'the stubs say nothing plays through them (perf_begin 0, slicer_busy 0)');
}

// ---- no flicker (unit-feedback.md #2): a pixel changes colour at most once per redraw (twice = it flashed). A screen
// change paints the new screen over the old one (no clear first); rows redrawn under a strip are not blitted over it
{
    const T = rig(), flick = (x0, y0, x1, y1) => { const c = T.m.chg(); let n = 0; for (let y = y0; y < y1; y++) for (let x = x0; x < x1; x++) if (c[y * 240 + x] > 1) n++; return n; };
    T.note(0); T.note(4); T.note(12);
    const bad = [];                              // P I E P G P F X F P C S C P I Q I P (L animates: not on the way)
    for (const d of ['right', 'down', 'left', 'up', 'down', 'down', 'down', 'up', 'up', 'left', 'left', 'right', 'right', 'right', 'up', 'down', 'left']) {
        T.m.chgReset(); T.with_('shift', d);
        const n = flick(0, 20, 240, STRIP_Y);
        if (n) bad.push(`${d}: ${n}`);
    }
    ok(!bad.length, `a screen change flashes no pixel (${bad.join(', ') || 'none'})`);
    T.xf(1); T.m.chgReset(); T.xf(1);            // the scene strip: shown, then a detent
    ok(flick(0, rowY(0), 240, STRIP_Y) === 0, `a crossfader detent: the rows are not drawn over the scene strip (${flick(0, rowY(0), 240, STRIP_Y)} px flashed)`);
    T.go(1600);
    T.to(0, 0); T.press('play'); T.until(9);
    T.m.enc(2, 1); T.go(30);                     // the knob strip, at the bottom (the cursor high): lines 12-15 under it
    T.m.chgReset(); T.go(700);                   // the playhead passes rows 12-15
    ok(flick(0, rowY(12) - 2, 209, rowY(12) + 40) === 0, `playing: the playhead row is not drawn over the knob strip (${flick(0, rowY(12) - 2, 209, rowY(12) + 40)} px flashed)`);
    T.press('play');
}

// ---- the LCD's SPI clock (unit-feedback.md #24, #38): 60 MHz as fm1-ctl on the device; the emulator does not model the
// SPI, so the source is checked
ok(/#define LCD_BAUD 0u/.test(readFileSync(join(ROOT, 'fw', 'firmware', 'src', 'lcd.c'), 'utf8')), 'the LCD runs SPI1 at 60 MHz (LCD_BAUD 0)');

// C19: the run is complete: a block that stops early (an exception caught, a loop cut short) leaves fewer checks. Raise
// MIN_CHECKS with each new check
const MIN_CHECKS = 714;
// ---- OPTIMIZE M9 (preparation): the three test projects of the CPU measurement on the unit, built by key presses into
// slots 01..03 (the 4th is the user's own song). Four channels of 16th notes, one engine each, RET on every 4th row so the
// tails stack. The store goes to build/tracker/m9-store.bin; each slot is reloaded after a reboot and must sound.
{
    const SET = [['FM6', 'FM6', 'FM6', 'FM6'], ['PHYS', 'PHYS', 'GRAIN', 'GRAIN'], ['DRUM', 'TRIO', 'SLICE', 'WHEEL']];
    const T = rig(), top = () => readText(T.m.fb(), 4, 4, 24).trimEnd();
    let built = 0;
    SET.forEach((engines, n) => {
        T.with_('shift', 'up'); T.press('down', n + 1); T.go(400); T.press('edit', 2);   // G: a new project in slot n + 1
        T.with_('shift', 'down'); T.with_('shift', 'left', 2);                          // P -> S
        T.press('left', 3);                      // CH1
        engines.forEach((eng, k) => {
            if (k) T.press('right');              // S: the next channel
            T.press('edit', k ? 2 : 1); T.with_('shift', 'right'); T.press('edit', k ? 2 : 1); T.with_('shift', 'right');   // its chain, its phrase (EDIT EDIT = a new one)
            for (let r = 0; r < 16; r++) {            // the notes, each on the channel's instrument (EDIT + down / up: one step; the last one carries over, not always)
                T.note(r); T.to(r, 2);
                for (let g = 0, v; g < 40 && (v = parseInt(T.cell(9, r, 2), 16)) !== k; g++) T.with_('edit', v < k ? 'down' : 'up');
            }
            for (let r = 0; r < 16; r += 4) T.fx(r, 0, 'RET', 0x23);
            T.with_('shift', 'right');           // I: the engine of that instrument
            for (let g = 0; g < 14 && !readText(T.m.fb(), 4, 46, 6).startsWith(eng); g++) T.with_('edit', 'down');
            for (let g = 0; g < 14 && !readText(T.m.fb(), 4, 46, 6).startsWith(eng); g++) T.with_('edit', 'up');   // (the list does not wrap)
            if (readText(T.m.fb(), 4, 46, 6).startsWith(eng)) built++; if (!top().includes('>I' + k)) built--;   // (the instrument shown is the channel's)
            T.with_('shift', 'left'); T.with_('shift', 'left', 2);
        });
        T.btn(9); T.go(300);
    });
    ok(built === 12 && T.strip() === 'saved to 03', `the 3 projects built: ${built} of 12 engines set (${T.strip()})`);
    const st = T.store();
    mkdirSync(OUT, { recursive: true }); writeFileSync(join(OUT, 'm9-store.bin'), st);
    for (let n = 1; n <= 3; n++) {
        const R = rig(st);
        R.with_('shift', 'up'); R.press('down', n); R.press('edit');
        R.with_('shift', 'down'); R.with_('shift', 'left', 2);
        const a = R.playRecord(2000);
        ok(top() !== '' && R.strip() !== 'load failed 0' + n && rms(a, 44100) > 0.002, `slot 0${n}: loads and sounds (rms ${rms(a, 44100).toFixed(4)}, ${R.strip()})`);
    }
}

// ---- the demo song for the browser emulator (web/dinghy-emu/site/demo-store.bin + demo-settings.bin, which the site serves):
// project DEMO in slot 01, four channels on four engines, each phrase played in with REC (notes mode) and fixed to its
// channel's instrument. Song rows 00-04: INTRO A B A OUTRO, each a chain of 4 phrases = 4 bars on every channel (they
// stay bar for bar together). Jungle, TEMPO 168: CH1 an Amen-style chopped break (DRUM engine: kicks on 0 and 10, snares
// on 4 and 12 with ghost snares, 16th hats; the intro thins in, A ends in a fill, B reorders the hits and ends in a snare
// roll), CH2 FM6 reese bass (long notes, an MLV ramp; A's third bar and all of B are the same phrase on the chain's TRN),
// CH3 TRIO minor pad (Am Am F G; its CUT ramp opens over the whole intro), CH4 WHEEL ragga stabs (call and response).
// The outro dies over its 4 bars: the break stops, the pad and bass ring out to note offs (the bass fading, a VOL ramp).
// The commands: SWG, RET (rolls), DEL (ghost snares, a late stab), CHA (hats that sometimes drop), CUT / MLV / VOL ramps
// (RTO, RTI under them), TRN. (The SLICE engine's built-in BREAK is one bar at 120 BPM: it would need a PTCH at 168.)
// Where a channel is quiet it plays a chain of an empty phrase: an empty song cell would end that channel's song and it
// would never come back (walk()), and a shorter chain would put the channel out of step.
{
    const T = rig(), top = () => readText(T.m.fb(), 4, 4, 24).trimEnd();
    let oct = 4, built = 0, row = 0, ch = 0, crow = 0, fxN = 0, fxOk = 0;
    const setOct = o => { for (; oct < o; oct++) T.btn(1); for (; oct > o; oct--) T.btn(0); };
    const OFF = 99;                              // a phrase row: the keybed key 7 + n (n semitones above the octave's C), -1 a rest, OFF a note off (F#3)
    const phrase = (k, o, rows, fxs = [], pre = () => {}) => {
        T.to(0, 0); setOct(o); T.btn(13);
        for (const n of rows) { if (n === -1) T.press('down'); else T.key(n === OFF ? 1 : 7 + n); }
        T.btn(13); T.btn(13);                    // REC, REC, REC: back to the gamepad
        rows.forEach((n, r) => {
            if (n === -1 || n === OFF) return;
            T.to(r, 2);
            for (let g = 0, v; g < 40 && (v = parseInt(T.cell(9, r, 2), 16)) !== k; g++) T.with_('edit', v < k ? 'down' : 'up');
        });
        pre();                                   // (the engine first: a sound command is named by it)
        for (const [r, c, name, val] of fxs) T.fx(r, c, name, val);
        for (const [r, c, name, val] of fxs) {   // read back, the cursor off that row
            T.to(r < 8 ? 15 : 0, 0); fxN++;
            if (T.cell([11, 18, 25][c], r, 3) === name && parseInt(T.cell([15, 22, 29][c], r, 2), 16) === val) fxOk++;
        }
    };
    const goRow = r => { T.press(r > row ? 'down' : 'up', Math.abs(r - row)); row = r; };   // S: the song row
    const goCh = k => { T.press(k > ch ? 'right' : 'left', Math.abs(k - ch)); ch = k; };     // S: the channel
    const cgo = r => { T.press(r > crow ? 'down' : 'up', Math.abs(r - crow)); crow = r; };   // C: the chain row
    T.with_('shift', 'up'); T.press('down'); T.go(400); T.press('edit', 3);       // G: slot 01 (a new project)
    T.press('right'); let chr = 0;
    for (const c of 'DEMO') {                    // NAME: the character grid (EDIT held; it starts on the last letter)
        const t = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'.indexOf(c);
        T.hold('edit');
        for (let d = Math.floor(t / 10) - Math.floor(chr / 10); d; d += d < 0 ? 1 : -1) T.press(d < 0 ? 'up' : 'down');
        T.press(t % 10 < chr % 10 ? 'left' : 'right', Math.abs(t % 10 - chr % 10));
        T.free('edit'); chr = t;
    }
    T.press('left', 5); T.press('down', 2); T.press('right');   // TEMPO 168 (40..255, EDIT + up / down: 10)
    for (let g = 0, v, d; g < 40 && (d = 168 - (v = +T.cell(21, 3, 12).match(/\d+/)[0])); g++) T.with_('edit', Math.abs(d) >= 10 ? (d > 0 ? 'up' : 'down') : d > 0 ? 'right' : 'left');
    const tempo = T.cell(21, 3, 12);
    T.with_('shift', 'down'); T.with_('shift', 'left', 2);   // P -> S
    const K = 0, SN = 2, HC = 6, R = -1;         // the DRUM engine's GM notes at octave 2: C KICK, D SNARE, F# HAT CL
    const H = o => { const r = Array(16).fill(R); for (const i in o) r[i] = o[i]; return r; };   // a sparse phrase {row: note}
    const CS = (r, c, v) => [r, c, 'CHA', v];
    const RAMP = (r, name, from, to, rti = 0x10) => [[r, 0, name, from], [r + 1, 0, 'RTO', to], [r + 2, 0, 'RTI', rti]];
    const P = (rows, fx = []) => ({ rows, fx });
    // per channel: engine, octave, then INTRO, A, B, OUTRO as { ch: the phrase labels of the 4 chain rows, tr: their
    // transposes, ph: { label: the phrase when it is new here (null: an empty one) } }
    const E4 = l => ({ ch: [l, l, l, l], ph: { [l]: null } });
    const SPEC = [['DRUM', 2,
        { ch: ['i1', 'i1', 'i2', 'i3'], ph: {
            i1: P([K, R, R, R, R, R, HC, R, R, R, K, R, R, R, HC, R], [[14, 0, 'RET', 2]]),
            i2: P([K, R, HC, R, R, R, HC, R, HC, R, K, R, HC, R, HC, R]),
            i3: P([K, R, HC, R, SN, R, HC, R, K, R, HC, SN, HC, R, SN, SN], [[14, 0, 'RET', 3], [15, 0, 'RET', 2]]) } },
        { ch: ['a', 'a', 'a', 'af'], ph: {
            a: P([K, HC, K, HC, SN, HC, HC, SN, HC, SN, K, HC, SN, HC, K, HC],
                 [[0, 0, 'SWG', 0x20], [7, 0, 'DEL', 3], [9, 0, 'DEL', 2], [12, 0, 'RET', 4], CS(3, 1, 0x8F), CS(11, 1, 0x8F), CS(15, 1, 0x8F)]),
            af: P([K, HC, K, HC, SN, HC, HC, SN, HC, SN, K, HC, SN, SN, SN, SN], [[7, 0, 'DEL', 3], [12, 0, 'RET', 4], [13, 0, 'RET', 3], [15, 0, 'RET', 2]]) } },
        { ch: ['b', 'b', 'b', 'bf'], ph: {
            b: P([K, K, HC, SN, HC, K, SN, HC, SN, HC, K, HC, SN, HC, K, HC], [[3, 0, 'DEL', 2], [6, 0, 'RET', 4], CS(2, 1, 0x8F), CS(9, 1, 0x8F)]),
            bf: P([K, K, HC, SN, HC, K, SN, HC, SN, HC, K, HC, SN, SN, SN, SN], [[3, 0, 'DEL', 2], [6, 0, 'RET', 4], [13, 0, 'RET', 3], [15, 0, 'RET', 2]]) } },
        { ch: ['o1', 'o3', 'Ed', 'Ed'], ph: { o1: P([K, R, HC, R, K, R, HC, R, K, R, R, R, R, R, R, R]), o3: P(H({ 0: K })), Ed: null } }],
     ['FM6', 2,
        E4('Ei'),
        { ch: ['a1', 'a1', 'a1', 'a1'], tr: [0, 0, -2, 0], ph: { a1: P(H({ 0: 9, 10: 7 }), RAMP(0, 'MLV', 0x10, 0x50)) } },     // a reese: A held, then G, the modulator opening
        { ch: ['a1', 'a1', 'a1', 'a1'], tr: [-2, -2, -5, -2], ph: {} },                                                          // the same, down: G F, G F, E, G F
        { ch: ['bo', 'Eo', 'Eo', 'Eo'], ph: { bo: P(H({ 0: 9, 12: OFF }), [[0, 0, 'VOL', 0xC0], [1, 0, 'RTO', 0x30], [2, 0, 'RTI', 0x10]]), Eo: null } }],
     ['TRIO', 3,
        { ch: ['p1', 'p2', 'p2', 'p2'], ph: { p1: P(H({ 0: 9 }), RAMP(0, 'CUT', 0x10, 0x60, 0x40)), p2: P(H({ 0: 9, 8: 9 })) } },   // the filter opens over the 4 bars
        { ch: ['pAm', 'pAm', 'pF', 'pG'], ph: { pAm: P(H({ 0: 9, 8: 12 })), pF: P(H({ 0: 5, 8: 9 })), pG: P(H({ 0: 7, 8: 11 })) } },
        { ch: ['bF', 'bF', 'bG', 'bGc'], ph: { bF: P(H({ 0: 5, 8: 5 })), bG: P(H({ 0: 7, 8: 7 })), bGc: P(H({ 0: 7, 8: 7 }), RAMP(0, 'CUT', 0x60, 0x20)) } },
        { ch: ['po1', 'po2', 'Eo', 'Eo'], ph: { po1: P(H({ 0: 9 })), po2: P(H({ 0: 7, 8: OFF })), Eo: null } }],
     ['WHEEL', 4,
        E4('Ei'),
        { ch: ['s1', 's2', 's3', 's4'], ph: {
            s1: P([R, R, R, R, R, R, 9, OFF, R, R, R, 12, OFF, R, 9, OFF], [[11, 0, 'DEL', 2]]),          // call
            s2: P([R, R, 12, OFF, R, R, 9, OFF, R, R, R, R, 5, OFF, R, R]),                                // response
            s3: P([R, R, R, 12, OFF, R, 9, OFF, R, R, R, 11, OFF, R, 9, OFF]),
            s4: P([R, R, 12, OFF, R, R, 11, OFF, R, R, 9, R, R, R, R, OFF]) } },
        { ch: ['t1', 't1', 't2', 't2'], ph: {
            t1: P([R, R, 12, OFF, R, R, 9, OFF, R, R, 12, OFF, R, R, 5, OFF]),
            t2: P([12, OFF, R, R, 12, OFF, R, R, 11, OFF, R, R, 7, R, R, OFF]) } },
        E4('Eo')]];
    const fxWant = SPEC.reduce((n, c) => n + c.slice(2).reduce((m, p) => m + Object.values(p.ph).reduce((q, f) => q + (f ? f.fx.length : 0), 0), 0), 0);
    const SECT = [0, 1, 2, 4];                   // song rows INTRO, A, B, OUTRO (row 03 = A again, below)
    SPEC.forEach(([eng, o, ...parts], k) => {
        if (k) { T.press('right'); ch = k; }
        let first = true, lastLabel = null;
        const done = new Set();
        parts.forEach((p, i) => {
            goRow(SECT[i]); T.press('edit', 2); T.with_('shift', 'right'); T.press('up', 15); crow = 0;   // a new chain, C, its row 00
            p.ch.forEach((label, q) => {
                cgo(q);
                if (!done.has(label)) {
                    T.press('edit', 2);                                           // a new phrase on that row
                    const def = p.ph[label];
                    if (def) {
                        T.with_('shift', 'right');                                // P
                        phrase(k, o, def.rows, def.fx, first ? () => {
                            T.to(0, 0); T.with_('shift', 'right');                // I: the engine of that instrument
                            for (let g = 0; g < 14 && !readText(T.m.fb(), 4, 46, 6).startsWith(eng); g++) T.with_('edit', 'down');
                            for (let g = 0; g < 14 && !readText(T.m.fb(), 4, 46, 6).startsWith(eng); g++) T.with_('edit', 'up');   // (the list does not wrap)
                            if (readText(T.m.fb(), 4, 46, 6).startsWith(eng) && top().includes('>I' + k)) built++;
                            T.with_('shift', 'left');
                        } : undefined);
                        first = false;
                        T.with_('shift', 'left');                                 // C
                    }
                    done.add(label); T.press('edit');                             // (copy it: the next row takes it)
                } else {
                    if (label !== lastLabel) throw new Error(`demo: ${label} is not the phrase last copied (${lastLabel})`);
                    T.press('edit');                                              // the last phrase
                }
                lastLabel = label;
                const t = p.tr ? p.tr[q] : 0;
                if (t) { T.press('right'); T.with_('edit', t < 0 ? 'left' : 'right', Math.abs(t)); T.press('left'); }   // its transpose
            });
            T.with_('shift', 'left');                                             // S
        });
        goRow(1); T.press('edit'); goRow(3); T.press('edit'); goRow(0);           // row 03 = row 01's chain
    });
    const song = [0, 1, 2, 3, 4].map(r => [0, 1, 2, 3].map(k => T.cell(3 + 3 * k, r, 2)));
    const part = (k, r) => SPEC[k].slice(2)[[0, 1, 2, 1, 3][r]];
    const chains = [];                           // every chain the song uses, opened: rows 00-07 (the cursor off them), read back
    for (let r = 0; r < 5; r++) for (let k = 0; k < 4; k++) {
        goCh(k); goRow(r);
        T.with_('shift', 'right'); T.press('down', 15);
        const rows = [...Array(8).keys()];
        chains.push({ k, r, ph: rows.map(q => T.cell(2, q, 2)), tr: rows.map(q => T.cell(5, q, 3)) });
        T.press('up', 15); T.with_('shift', 'left');
    }
    const fmt = t => (t < 0 ? '-' : '+') + String(Math.abs(t)).padStart(2, '0');
    const ids = [0, 1, 2, 3].map(() => new Map());   // per channel: label -> the phrase number read back
    const chainOk = chains.every(c => {
        const sp = part(c.k, c.r), seen = ids[c.k];
        return c.ph.slice(0, 4).every(x => x !== '--') && c.ph.slice(4).every(x => x === '--')
            && sp.ch.every((l, q) => c.tr[q] === fmt(sp.tr ? sp.tr[q] : 0) && (seen.has(l) ? seen.get(l) === c.ph[q] : ![...seen.values()].includes(c.ph[q]) && seen.set(l, c.ph[q])));
    });
    const apart = [0, 1, 2, 3].every(k => [0, 1, 2, 3].every(j => j === k || [...ids[k].values()].every(x => ![...ids[j].values()].includes(x))));
    const lens = [0, 1, 2, 3, 4].map(r => chains.filter(c => c.r === r).map(c => c.ph.filter(x => x !== '--').length));
    T.with_('shift', 'up'); T.btn(9); T.go(300);
    ok(built === 4 && T.strip() === 'saved to 01' && top() === 'PROJECT 01  DEMO' && tempo.includes('168'),
        `the demo: ${built} of 4 engines set, ${T.strip()}, ${top()}, ${tempo.trim()}`);
    ok([0, 1, 2, 3].every(k => [0, 1, 2, 3, 4].every(r => song[r][k] !== '--') && song[1][k] === song[3][k]
        && [0, 2, 4].every(r => [0, 2, 4].every(q => q === r || song[r][k] !== song[q][k]) && song[r][k] !== song[1][k])),
        `the demo's song rows 00-04 are INTRO A B A OUTRO (${song.map(r => r.join(' ')).join(' / ')})`);
    ok(chains.length === 20 && chainOk && apart,
        `every chain read back holds its 4 phrases from row 00 (rows 04-07 empty), as designed, a phrase per label, none shared between channels, TRN as set (CH song row: rows 00-03 / TRN: ${chains.map(c => `${c.k + 1}.${c.r} ${c.ph.slice(0, 5).join(' ')}${c.tr.slice(0, 4).some(t => t !== '+00') ? ' ' + c.tr.slice(0, 4).join(' ') : ''}`).join('; ')})`);
    ok(lens.every(l => l.every(n => n === 4)), `the chains of a song row are the same length on every channel (phrases per chain, song rows 00-04: ${lens.map(l => l.join('/')).join(' ')})`);
    ok(fxN === fxWant && fxOk === fxN, `the demo's commands: ${fxOk} of ${fxN} cells read back (SWG RET DEL CHA, CUT / MLV / VOL ramps)`);
    const st = T.store(), se = T.settings();
    const SITE = join(ROOT, 'web', 'dinghy-emu', 'site');
    writeFileSync(join(SITE, 'demo-store.bin'), st); writeFileSync(join(SITE, 'demo-settings.bin'), se);
    const Rg = rig(st, se, true);                // a reboot: the tracker opens the last project (the settings), on S
    const bar = Math.round(44100 * 16 * 60 / (168 * 4)), sec = 4 * bar, LEN = 20 * bar / 44.1 + 1500;
    const a = Rg.playRecord(LEN);
    ok(rms(a, 44100) > 0.002, `the demo opens at boot and sounds (rms ${rms(a, 44100).toFixed(4)})`);
    Rg.press('play'); Rg.go(1500);                // (stopped)
    const soloed = [];                           // each channel soloed (OPTION + PLAY) and the song played: heard in every bar that has notes in it
    const has = (k, r, q) => { const sp = part(k, r), d = SPEC[k].slice(2).map(x => x.ph[sp.ch[q]]).find(Boolean); return !!d && d.rows.some(n => n !== R && n !== OFF); };
    for (let k = 0; k < 4; k++) {
        if (k) Rg.press('right');
        Rg.hold('option'); Rg.press('play'); Rg.free('option');
        const x = Rg.playRecord(LEN); Rg.press('play'); Rg.go(1500);
        Rg.hold('option'); Rg.press('play'); Rg.free('option');
        soloed.push([0, 1, 2, 3, 4].flatMap(r => [0, 1, 2, 3].map(q => [r * 4 + q, has(k, r, q)])).filter(b => b[1]).map(b => rms(x, b[0] * bar + 4000, (b[0] + 1) * bar)));
    }
    ok(soloed.every(l => l.every(v => v > 0.003)), `each channel plays through every bar that has notes when soloed (min rms per channel: ${soloed.map((l, k) => `CH${k + 1} ${Math.min(...l).toFixed(3)} of ${l.length} bars`).join('; ')})`);
    const hi = s => { const d = []; for (let i = s * sec + 1; i < (s + 1) * sec; i++) d.push(a[i] - a[i - 1]); return rms(d); };   // (first difference: the highs)
    const env = s => { const e = [], st = Math.round(bar / 16); for (let i = 0; i < 48; i++) { const d = []; for (let j = s * sec + i * st + 1; j < s * sec + (i + 1) * st; j++) d.push(a[j] - a[j - 1]); e.push(rms(d)); } return e; };   // the highs per 16th, A's first 3 bars
    const cor = (p, q) => { const m = x => x.reduce((u, v) => u + v, 0) / x.length, mp = m(p), mq = m(q); let n = 0, dp = 0, dq = 0; p.forEach((x, i) => { n += (x - mp) * (q[i] - mq); dp += (x - mp) ** 2; dq += (q[i] - mq) ** 2; }); return n / Math.sqrt(dp * dq); };
    const eA = env(1), simAA = cor(eA, env(3)), simAB = cor(eA, env(2));
    ok(simAA > simAB + 0.1, `section A sounds like the second A and not like B: 16th-note envelope correlation ${simAA.toFixed(2)} against ${simAB.toFixed(2)}`);
    ok(hi(0) < hi(1) * 0.7 && rms(a, 4 * sec, 5 * sec) < rms(a, sec, 2 * sec) / 2,
        `INTRO has no lead, OUTRO dies: highs ${hi(0).toFixed(4)} against ${hi(1).toFixed(4)}, outro rms ${rms(a, 4 * sec, 5 * sec).toFixed(3)} against ${rms(a, sec, 2 * sec).toFixed(3)}`);
    {   // every screen on the demo song, playing (build/tracker/guide-*.png: the Ketch README and site guide)
        const G = rig(st, se, true), seen = [];
        const shot = L => { G.go(400); seen.push(mapLit((x, y) => G.m.fb()[y * 240 + x], L, 2) ? L : L.toLowerCase()); G.shot('guide-' + L); };
        G.press('down'); G.press('play'); G.go(bar * 1000 / 44100 * 4);   // song row 01 (section A), from its bar 2
        shot('S'); G.with_('shift', 'right'); shot('C'); G.with_('shift', 'right'); shot('P');
        G.with_('shift', 'up'); shot('G'); G.with_('shift', 'down');
        G.with_('shift', 'down'); shot('F'); G.with_('shift', 'down'); shot('X'); G.with_('shift', 'up'); G.with_('shift', 'up');
        G.with_('shift', 'right'); shot('I'); G.with_('shift', 'up'); shot('Q'); G.with_('shift', 'down');
        G.with_('shift', 'down'); shot('E'); G.with_('shift', 'down'); shot('L'); G.with_('shift', 'down'); shot('V');
        ok(seen.join('') === 'SCPGFXIQELV', `the guide's screens, each on its letter of the map (${seen.join('')})`);
    }
}

ok(checks + 1 >= MIN_CHECKS, `the whole run: ${checks + 1} checks, at least ${MIN_CHECKS}`);
// ---- the save's LZ4 hash shares the pool block with Q's tables (OPTIMIZE M5): SAVE in Q leaves the list as it was
{
    const T = rig();
    T.note(0); T.note(4); T.with_('shift', 'left'); T.press('edit'); T.press('down'); T.press('edit'); T.with_('shift', 'right');   // two notes, the phrase twice in the chain
    T.btn(11); T.go(30);
    const qpix = () => T.m.fb().slice(0, STRIP_Y * 240);
    const q0 = qpix();
    T.btn(9); T.go(100);
    const saved = T.strip();
    T.press('down'); T.press('up'); T.go(30);   // a redraw from the tables
    const q1 = qpix();
    ok(readText(T.m.fb(), 16, Q_Y(0), 3) === 'P00' && saved === 'saved to 00' && q0.every((v, i) => v === q1[i]), `SAVE in Q: the list is as it was after a redraw (${saved})`);
    {   // the aliasing itself: one union holds the hash and Q's tables, and SAVE in Q rebuilds them (a source check: 2 lines are too few for the clobber to show)
        const src = readFileSync(join(ROOT, 'fw', 'firmware', 'src', 'tracker.c'), 'utf8');
        ok(/union \{ uint16_t ht\[2048\]; struct \{ qitem_t it\[Q_MAX\]/.test(src) && /proj_save\(pj_slot\)\)[^\n]*\r?\n\s*if \(ui\.view == V_Q\) q_build\(\);/.test(src),
            'LZ4 hash and Q tables share a union; save_button rebuilds Q after the save');
    }
}

// ---- GRAIN's seek index is sized for our sample sets (OPTIMIZE M4): every built-in set fits, or the engine goes silent on it
{
    const rd = f => readFileSync(join(ROOT, 'fw', 'firmware', f), 'utf8');
    const hdr = rd('gen/felucca_samples.h'), grain = rd('sound/eng_grain.c');
    const zones = [...hdr.match(/SMP_ZONES\[\] = \{([\s\S]*?)\n\};/)[1].matchAll(/\{(\d+), (\d+),/g)].map(m => Number(m[2]));
    const sets = [...hdr.match(/SMP_SETS\[\] = \{([\s\S]*?)\n\};/)[1].matchAll(/\{"\w+", (\d+), (\d+)\}/g)].map(m => [Number(m[1]), Number(m[2])]);
    const need = Math.max(...sets.map(([z0, nz]) => zones.slice(z0, z0 + nz).reduce((a, n) => a + Math.ceil(n / 256), 0)));
    const nidx = Number(grain.match(/#define GR_NIDX (\d+)/)[1]), maxz = Number(grain.match(/#define GR_MAXZ (\d+)/)[1]);
    ok(sets.length === 5 && need <= nidx && Math.max(...sets.map(s => s[1])) <= maxz && nidx <= 512 && maxz <= 16,
        `GRAIN's index: the biggest built-in set needs ${need} entries, GR_NIDX ${nidx} (at most 512), GR_MAXZ ${maxz} (at most 16)`);
}

console.log(`${checks - fails}/${checks} checks passed  (screens in ${OUT})`);
process.exit(fails ? 1 : 0);
