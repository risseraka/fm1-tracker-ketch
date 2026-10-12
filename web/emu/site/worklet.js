// SPDX-License-Identifier: GPL-3.0-only
// The firmware runs inside the AudioWorklet: process() is the audio clock. Each 128-frame quantum it
// takes frames from the ISR's blocks (emu_audio, 256 frames at a time) and resumes the main loop until
// the firmware's milliseconds have caught up. About 60 times a second the screen, LEDs and MIDI out go
// to the page.
import { Machine } from './machine.js';

class DinghyProcessor extends AudioWorkletProcessor {
    constructor(options) {
        super();
        const o = options.processorOptions;
        this.m = new Machine(new WebAssembly.Module(o.wasm), o.settings, o.store);   // bytes: as sloop-web-emu, works everywhere
        this.buf = null;                        // the current ISR block
        this.pos = 0;
        this.frames = 0;                        // frames played since boot
        this.quanta = 0;
        this.lastGen = -1;
        this.behind = 0;
        this.port.onmessage = (ev) => {
            const d = ev.data;
            if (d.t === 'input') this.m.input(d.notes, d.buttons);
            else if (d.t === 'enc') this.m.enc(d.e, d.steps);
            else if (d.t === 'adc') this.m.adc(d.ch, d.v);
            else if (d.t === 'midi') this.m.midiIn(d.st, d.d1, d.d2);
        };
        this.m.runTo(1);                        // boot: up to the first poll (the title is drawn)
        this.post();
    }

    post() {
        const [led, dim] = this.m.leds();
        const msg = { t: 'state', leds: led.slice(), dims: dim.slice(), ms: this.m.ms(), behind: this.behind,
                      midi: this.m.midiOut(), ended: this.m.ended };
        const gen = this.m.fbGen(), tr = [];
        if (gen !== this.lastGen) {
            this.lastGen = gen;
            msg.fb = this.m.fb().slice();
            tr.push(msg.fb.buffer);
        }
        const s = this.m.settingsSaved();
        if (s) { msg.settings = s; tr.push(s.buffer); }
        const st = this.m.storeSaved();
        if (st) { msg.store = st; tr.push(st.buffer); }
        this.port.postMessage(msg, tr);
    }

    process(inputs, outputs) {
        const L = outputs[0][0], R = outputs[0][1] || L, n = L.length;
        for (let i = 0; i < n; i++) {
            if (!this.buf || this.pos >= this.m.frames) {
                this.buf = this.m.audio();
                this.pos = 0;
            }
            L[i] = this.buf[2 * this.pos];
            R[i] = this.buf[2 * this.pos + 1];
            this.pos++;
        }
        this.frames += n;
        const target = Math.floor(this.frames * 1000 / sampleRate);
        this.m.runTo(target, 40);
        this.behind = target - this.m.ms();     // > 0: the main loop is slower than real time
        if (++this.quanta % 6 === 0) this.post();
        return true;
    }
}
registerProcessor('dinghy', DinghyProcessor);
