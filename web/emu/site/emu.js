// SPDX-License-Identifier: GPL-3.0-only
// Dinghy browser emulator, main thread: the faceplate, input, screen, LEDs, MIDI.
// The firmware runs in worklet.js. The faceplate geometry is sloop-web-emu's (sabliran, GPL-3.0), traced
// from the FM-1 user manual's panel drawing: drawing units, origin at the case's top-left, case 963 x 588.
'use strict';

// the firmware: site/<FW>.wasm (?fw=dinghy for Dinghy's demo app)
const FW = (new URLSearchParams(location.search).get('fw') || '').replace(/[^a-z0-9_-]/gi, '') || 'tracker';
const DEMO = FW === 'tracker' && new URLSearchParams(location.search).has('demo');   // the demo song: loaded fresh, never kept
const S = 1.0;                                   // px per drawing unit
const DEV = { w: 963, h: 588 };
const BTN = { 'OCT-': 0, 'OCT+': 1, FX: 2, SEL: 3, ENV: 4, LFO: 5, EDIT: 6, GLO: 7,
              HOME: 8, SAVE: 9, ARP: 10, SEQ: 11, PLAY: 12, REC: 13 };   // Dinghy's BTN_... ids
const KEYMAP = [                                 // = hal/fm1_input.h and dinghy_wasm.c: change all three; ids 0..13 buttons, 14..40 note keys
    [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
    [ 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1],
    [34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12],
    [23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33],
    [ 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22],
    [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
];
const ENCODERS = [                               // [label, ENC_... id, cx, cy, pointer colour]
    ['SELECT', 0, 172, 100], ['PRESETS', 6, 67, 203], ['ALGORITHM', 1, 170, 203],
    ['KNOB1', 2, 538, 100, '#4f8fe8'], ['KNOB2', 3, 647, 100, '#46b96b'],
    ['KNOB3', 4, 757, 100, '#e0c23e'], ['KNOB4', 5, 866, 100, '#e08a3e'],
];
const MASTER = { cx: 70, cy: 100 };
const KNOB_D = 46;
const SCREEN = { x: 241, y: 48, w: 226, h: 233 };
const BTN_PANE = { x: 508, y: 168, w: 387, h: 135 };
const BTN_COLS = [550, 611, 672, 733, 793, 854];
const BTN_ROWS = [
    { y: 207, labels: [['FX', 'FX'], ['SEL', 'SEL'], ['ENV', 'ENV'], ['LFO', 'LFO'], ['EDIT', 'EDIT'], ['GLO', 'GLO']] },
    { y: 268, labels: [['HOME', 'HOME'], ['SAVE', 'SAVE'], ['ARP', 'ARP'], ['SEQ', 'SEQ'], ['PLAY|STOP', 'PLAY'], ['REC', 'REC']] },
];
const OCT_PANE = { x: 43, y: 258, w: 160, h: 45 };
const KEY_PANE = { x: 20, y: 340, w: 885, h: 228 };
const WKEY = { y: 497, w: 44, h: 98, x0: 63, pitch: 54.53 };
const BKEY = { y: 396.5, w: 38, h: 86, xs: [92, 146, 201, 310, 365, 474, 529, 583, 692, 747, 856],
               labels: ['OP1', 'OP2', 'OP3', 'OP4', 'OP5', 'OP6', 'PIT', 'GLO', 'MONO', 'POLY', ''] };
const WHITE_IDS = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26];
const BLACK_IDS = [1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25];
const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

let node = null, notes = 0, buttons = 0, master = 820;
const btnEls = {}, keyEls = [];
const device = document.getElementById('device');
device.style.width = DEV.w * S + 'px';
device.style.height = DEV.h * S + 'px';
const px = u => u * S + 'px';

const send = m => { if (node) node.port.postMessage(m); };
const sendInput = () => send({ t: 'input', notes, buttons });
const noteDown = k => { notes |= 1 << k; keyEls[k]?.classList.add('down'); sendInput(); };
const noteUp = k => { notes &= ~(1 << k); keyEls[k]?.classList.remove('down'); sendInput(); };
const btnDown = b => { buttons |= 1 << b; btnEls[b]?.classList.add('down'); sendInput(); };
const btnUp = b => { buttons &= ~(1 << b); btnEls[b]?.classList.remove('down'); sendInput(); };
const enc = (e, steps) => { if (steps) send({ t: 'enc', e, steps }); };
const sendMaster = () => send({ t: 'adc', ch: 4, v: master });

function el(cls, x, y, w, h) {                   // centred at (x, y)
    const d = document.createElement('div');
    d.className = cls;
    d.style.left = px(x);
    d.style.top = px(y);
    if (w) { d.style.width = px(w); d.style.height = px(h); }
    device.appendChild(d);
    return d;
}
function label(text, x, y) { el('lab', x, y).textContent = text; }

function knobDrag(cap, turn) {
    let last = null, acc = 0;
    cap.addEventListener('wheel', ev => { ev.preventDefault(); turn(ev.deltaY < 0 ? 1 : -1); }, { passive: false });
    cap.addEventListener('pointerdown', ev => {
        ev.preventDefault();
        last = ev.clientY;
        acc = 0;
        try { cap.setPointerCapture(ev.pointerId); } catch (e) {}
    });
    cap.addEventListener('pointermove', ev => {
        if (last === null) return;
        acc += last - ev.clientY;
        last = ev.clientY;
        const s = Math.trunc(acc / 9);
        if (s) { acc -= s * 9; turn(s); }
    });
    const up = () => { last = null; };
    cap.addEventListener('pointerup', up);
    cap.addEventListener('pointercancel', up);
}

function pressable(elm, down, up) {
    elm.addEventListener('pointerdown', ev => { ev.preventDefault(); try { elm.setPointerCapture(ev.pointerId); } catch (e) {} down(); });
    elm.addEventListener('pointerup', up);
    elm.addEventListener('pointercancel', up);
}

function build() {
    for (const [name, e, cx, cy, col] of ENCODERS) {
        label(name, cx, cy - 36);
        const cap = el('cap', cx, cy, KNOB_D, KNOB_D);
        if (col) cap.style.setProperty('--pcol', col);
        let rot = 0;
        knobDrag(cap, s => { rot += s * 18; cap.style.setProperty('--rot', rot + 'deg'); enc(e, s); });
    }
    label('MASTER', MASTER.cx, MASTER.cy - 36);
    const mcap = el('cap', MASTER.cx, MASTER.cy, KNOB_D, KNOB_D);
    mcap.style.setProperty('--pcol', '#e8b84b');
    const showMaster = () => mcap.style.setProperty('--rot', (-150 + 300 * master / 1023) + 'deg');
    showMaster();
    knobDrag(mcap, s => { master = Math.max(0, Math.min(1023, master + s * 32)); showMaster(); sendMaster(); });

    const hint = { FX: '1', SEL: '2', ENV: '3', LFO: '4', EDIT: '5', GLO: '6', HOME: '7', SAVE: '8',
                   ARP: '9', SEQ: '0', PLAY: '␣', REC: 'R', 'OCT-': 'Z', 'OCT+': 'X' };
    const makeBtn = (print, fw, cx, cy, w, h) => {
        const id = BTN[fw], b = el('btn', cx, cy, w, h);
        b.innerHTML = print.split('|').join('<br>') + '<kbd>' + hint[fw] + '</kbd>';
        pressable(b, () => btnDown(id), () => btnUp(id));
        btnEls[id] = b;
    };
    el('pane', BTN_PANE.x + BTN_PANE.w / 2, BTN_PANE.y + BTN_PANE.h / 2, BTN_PANE.w, BTN_PANE.h);
    for (const row of BTN_ROWS)
        row.labels.forEach(([print, fw], i) => makeBtn(print, fw, BTN_COLS[i], row.y, 50, 46));
    el('pane', OCT_PANE.x + OCT_PANE.w / 2, OCT_PANE.y + OCT_PANE.h / 2, OCT_PANE.w, OCT_PANE.h);
    makeBtn('OCT-', 'OCT-', 85, 280.5, 64, 33);
    makeBtn('OCT+', 'OCT+', 161, 280.5, 64, 33);

    el('pane', KEY_PANE.x + KEY_PANE.w / 2, KEY_PANE.y + KEY_PANE.h / 2, KEY_PANE.w, KEY_PANE.h);
    const makeKey = (k, cls, cx, cy, w, h, print) => {
        const d = el('pill ' + cls, cx, cy, w, h);
        d.style.borderRadius = px(w / 2);
        d.innerHTML = '<div class="slot"></div>' + (print ? '<i>' + print + '</i>' : '');
        d.title = NOTE_NAMES[(53 + k) % 12] + (Math.floor((53 + k) / 12) - 1) + ' (key ' + k + ')';
        pressable(d, () => noteDown(k), () => noteUp(k));
        keyEls[k] = d;
    };
    WHITE_IDS.forEach((k, i) => makeKey(k, 'wk', WKEY.x0 + WKEY.pitch * i, WKEY.y, WKEY.w, WKEY.h, ''));
    BLACK_IDS.forEach((k, i) => makeKey(k, 'bk', BKEY.xs[i], BKEY.y, BKEY.w, BKEY.h, BKEY.labels[i]));

    const bz = el('', SCREEN.x + SCREEN.w / 2, SCREEN.y + SCREEN.h / 2, SCREEN.w, SCREEN.h);
    bz.id = 'bezel';
    bz.style.transform = 'translate(-50%, -50%)';
    const cv = document.createElement('canvas');
    cv.id = 'screen';
    cv.width = cv.height = 240;
    cv.style.width = cv.style.height = px(Math.min(SCREEN.w, SCREEN.h) - 8);
    bz.appendChild(cv);
    return cv;
}

const canvas = build();
const c2d = canvas.getContext('2d');
const img = c2d.createImageData(240, 240);

function drawFB(fb) {
    const d = img.data;
    for (let k = 0; k < 240 * 240; k++) {
        const v = fb[k];
        d[4 * k] = (v >> 11 & 31) * 255 / 31 | 0;
        d[4 * k + 1] = (v >> 5 & 63) * 255 / 63 | 0;
        d[4 * k + 2] = (v & 31) * 255 / 31 | 0;
        d[4 * k + 3] = 255;
    }
    c2d.putImageData(img, 0, 0);
}

function applyLeds(leds, dims) {
    const lit = new Set(), glow = new Set();
    for (let c = 0; c < 11; c++)
        for (let r = 1; r < 5; r++) {
            const id = KEYMAP[r][c];
            if (id < 0) continue;
            if (leds[c] >> r & 1) lit.add(id);
            else if (dims[c] >> r & 1) glow.add(id);
        }
    for (let b = 0; b < 14; b++) {
        btnEls[b]?.classList.toggle('lit', lit.has(b));
        btnEls[b]?.classList.toggle('glow', !lit.has(b) && glow.has(b));
    }
    for (let k = 0; k < 27; k++) {
        keyEls[k]?.classList.toggle('lit', lit.has(14 + k));
        keyEls[k]?.classList.toggle('glow', !lit.has(14 + k) && glow.has(14 + k));
    }
}

// ---- computer keyboard: middle row = white keys from C4 (key 7), top row = black keys
const KEYNOTES = { a: 7, s: 9, d: 11, f: 12, g: 14, h: 16, j: 18, k: 19, l: 21, ';': 23, "'": 24,
                   w: 8, e: 10, t: 13, y: 15, u: 17, o: 20, p: 22, ']': 25 };
// the tracker's gamepad on the keybed (tracker.c PAD_*), laid out as the prototype's (ux-proto/control-map.js pad)
// and its notes-mode keys A#3..C#5 (the firmware ignores them outside notes mode); these win over Z X R below
const KEYPAD = { a: 0, q: 1, s: 2, w: 3, d: 4, j: 21, k: 23, l: 24, ';': 26,
                 r: 5, z: 6, x: 7, f: 8, c: 9, g: 10, v: 11, b: 12, h: 13, n: 14, t: 15, m: 16, y: 17, ',': 18, '.': 19, u: 20 };
const keyNotes = () => FW === 'tracker' ? KEYPAD : KEYNOTES;
const KEYBTNS = { 1: 'FX', 2: 'SEL', 3: 'ENV', 4: 'LFO', 5: 'EDIT', 6: 'GLO', 7: 'HOME', 8: 'SAVE',
                  9: 'ARP', 0: 'SEQ', z: 'OCT-', x: 'OCT+', ' ': 'PLAY', r: 'REC', tab: 'REC', '-': 'OCT-', '=': 'OCT+' };
const KEYENC = { ArrowUp: [0, 1], ArrowDown: [0, -1], ArrowRight: [1, 1], ArrowLeft: [1, -1], '[': [6, -1], '\\': [6, 1] };
const held = new Set();
window.addEventListener('keydown', ev => {
    if (ev.ctrlKey || ev.metaKey || ev.altKey) return;
    if (ev.key in KEYENC) { enc(...KEYENC[ev.key]); ev.preventDefault(); return; }
    if (ev.repeat) return;
    const k = ev.key.toLowerCase();
    if (k in keyNotes()) { held.add(k); noteDown(keyNotes()[k]); ev.preventDefault(); }
    else if (k in KEYBTNS) { held.add(k); btnDown(BTN[KEYBTNS[k]]); ev.preventDefault(); }
});
window.addEventListener('keyup', ev => {
    const k = ev.key.toLowerCase();
    if (!held.delete(k)) return;
    if (k in keyNotes()) noteUp(keyNotes()[k]);
    else if (k in KEYBTNS) btnUp(BTN[KEYBTNS[k]]);
});
window.addEventListener('blur', () => {
    held.clear();
    notes = buttons = 0;
    keyEls.forEach(e => e?.classList.remove('down'));
    Object.values(btnEls).forEach(e => e.classList.remove('down'));
    sendInput();
});

// ---- settings: the firmware's 256-byte sector, kept in this browser per firmware (best effort)
const SKEY = 'dinghy-emu-settings-' + FW;
function loadSettings() {
    try {
        const s = localStorage.getItem(SKEY);
        return s ? Uint8Array.from(JSON.parse(s)).buffer : null;
    } catch (e) { return null; }
}
function saveSettings(u8) {
    try { localStorage.setItem(SKEY, JSON.stringify(Array.from(u8))); } catch (e) {}
}

// ---- the store: the firmware's flash data region (its saved projects), kept in this browser per firmware (best effort)
const TKEY = 'dinghy-emu-store-' + FW;
let storeErr = '';
function loadStore() {
    try {
        const s = localStorage.getItem(TKEY);
        return s ? Uint8Array.from(atob(s), c => c.charCodeAt(0)).buffer : null;
    } catch (e) { return null; }
}
function saveStore(u8) {
    try {
        let b = '';
        for (let i = 0; i < u8.length; i += 0x8000) b += String.fromCharCode.apply(null, u8.subarray(i, i + 0x8000));
        localStorage.setItem(TKEY, btoa(b));
        storeErr = '';
    } catch (e) { storeErr = 'projects NOT kept in this browser (' + e.name + ')'; }
}


// ---- MIDI: Web MIDI in goes to the firmware's USB-MIDI in; what it sends is shown
const midiLog = document.getElementById('midilog');
function logMidi(events) {
    for (const [st, d1, d2] of events) {
        const kind = (st & 0xF0) === 0x90 && d2 ? 'on ' : (st & 0xF0) === 0x80 || (st & 0xF0) === 0x90 ? 'off' : st.toString(16);
        const line = document.createElement('div');
        line.textContent = `${kind} ${NOTE_NAMES[d1 % 12]}${Math.floor(d1 / 12) - 1} ${d2}`;
        midiLog.prepend(line);
    }
    while (midiLog.childElementCount > 12) midiLog.lastChild.remove();
}
function initMidi() {
    if (!navigator.requestMIDIAccess) return;
    navigator.requestMIDIAccess({ sysex: false }).then(acc => {
        const hook = () => acc.inputs.forEach(inp => {
            inp.onmidimessage = ev => {
                const d = ev.data;
                if (d && d.length >= 2 && d[0] < 0xF0) send({ t: 'midi', st: d[0], d1: d[1], d2: d[2] || 0 });
            };
        });
        hook();
        acc.onstatechange = hook;
    }).catch(() => {});
}

// ---- power on (audio needs a click)
const status = document.getElementById('status');
const NAME = FW === 'tracker' ? 'Ketch' : FW[0].toUpperCase() + FW.slice(1);
document.querySelector('h1 b').textContent = NAME.toUpperCase();
document.title = NAME + ' emulator' + (DEMO ? ' · demo song' : '');
if (FW === 'tracker') {
    document.getElementById('keyhelp').textContent = 'A S W D = ← ↓ ↑ →\nQ key 1 · J SHIFT · K PLAY\nL OPTION · ; EDIT\n(keybed keys, hold two at once)\nTab REC, then:\nR Z X F C G V B H N T M Y , . U\n= A#3…C#5\nQ = note off · − = OCT− OCT+';
    const out = document.getElementById('midilog').parentElement;   // the tracker sends no MIDI: its place is the demo song's
    out.innerHTML = DEMO
        ? '<h2>DEMO SONG</h2><div>loaded fresh on each visit; changes are not kept</div><a href="?">back to your own songs</a>'
        : '<h2>DEMO SONG</h2><a href="?demo">load the demo song</a><div>your own songs stay as they are</div>';
}
async function powerOn() {
    document.getElementById('power').remove();
    const ctx = new AudioContext({ sampleRate: 44100, latencyHint: 'interactive' });
    await ctx.audioWorklet.addModule('worklet.js');
    const wasm = await (await fetch(FW + '.wasm', { cache: 'no-store' })).arrayBuffer();
    const bin = async f => (await fetch(f, { cache: 'no-store' })).arrayBuffer();
    node = new AudioWorkletNode(ctx, 'dinghy', {
        numberOfInputs: 0, outputChannelCount: [2],
        processorOptions: DEMO ? { wasm, settings: await bin('demo-settings.bin'), store: await bin('demo-store.bin') }
                               : { wasm, settings: loadSettings(), store: loadStore() },
    });
    node.port.onmessage = ev => {
        const m = ev.data;
        if (m.fb) drawFB(m.fb);
        applyLeds(m.leds, m.dims);
        if (m.midi.length && midiLog.isConnected) logMidi(m.midi);
        if (m.settings && !DEMO) saveSettings(m.settings);
        if (m.store && !DEMO) saveStore(m.store);
        status.textContent = m.ended ? 'app_main returned (it should not): stopped'
            : `${(m.ms / 1000).toFixed(1)} s` + (m.behind > 20 ? `  ·  main loop ${m.behind} ms behind` : '')
            + (storeErr ? '  ·  ' + storeErr : '');

    };
    node.connect(ctx.destination);
    await ctx.resume();
    sendMaster();
    sendInput();
    initMidi();
}
document.getElementById('power').addEventListener('click', powerOn, { once: true });
