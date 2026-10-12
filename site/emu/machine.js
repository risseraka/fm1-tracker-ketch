// SPDX-License-Identifier: GPL-3.0-only
// The emulated FM-1 around a firmware .wasm (dinghy_wasm.c): used by the AudioWorklet and by test.mjs.
// app_main() never returns; each platform_poll() calls host_yield(), where Asyncify unwinds the stack
// back to us. step() rewinds into it and runs the main loop until its next poll: one millisecond.

export class Machine {
    constructor(module, settings, store) {
        this.state = 0;                         // 0 running, 1 unwinding, 2 rewinding
        this.started = false;
        this.ended = false;
        const inst = new WebAssembly.Instance(module, { env: { host_yield: () => this.hostYield() } });
        this.e = inst.exports;
        this.async = this.e.emu_async_ptr();
        this.asyncEnd = this.async + this.e.emu_async_size();
        this.frames = this.e.emu_frames();      // per emu_audio() call (the ISR's block)
        this.settingsLen = this.e.emu_settings_len();
        this.mq = this.e.emu_mq_len();          // the MIDI out ring's length
        this.moRead = 0;
        if (settings && settings.byteLength)
            this.u8(this.e.emu_settings_ptr(), this.settingsLen).set(new Uint8Array(settings).subarray(0, this.settingsLen));
        this.settingsGen = 0;
        if (this.e.emu_store_ptr) {             // the store: the image kept from before, else erased flash (0xFF)
            const st = this.u8(this.e.emu_store_ptr(), this.e.emu_store_len());
            if (store && store.byteLength === st.length) st.set(new Uint8Array(store)); else st.fill(0xFF);
        }
        this.storeGen = 0;
    }

    u8(p, n) { return new Uint8Array(this.e.memory.buffer, p, n); }

    hostYield() {
        if (this.state === 2) {                 // arriving back where we left: carry on
            this.e.asyncify_stop_rewind();
            this.state = 0;
            return;
        }
        const h = new Uint32Array(this.e.memory.buffer, this.async, 2);
        h[0] = this.async + 8;                  // the saved stack starts after [cur, end]
        h[1] = this.asyncEnd;
        this.e.asyncify_start_unwind(this.async);
        this.state = 1;
    }

    // one pass of the main loop, up to its next platform_poll (1 ms of firmware time)
    step() {
        if (this.ended) return false;
        if (this.started) {
            this.e.asyncify_start_rewind(this.async);
            this.state = 2;
        }
        this.started = true;
        this.e.emu_main();
        if (this.state === 1) {
            this.e.asyncify_stop_unwind();
            this.state = 0;
            return true;
        }
        this.ended = true;                      // app_main returned (it should not)
        return false;
    }

    ms() { return this.e.emu_ms(); }

    // run the main loop until the firmware clock reaches ms; at most max passes (a busy app falls behind)
    runTo(ms, max = 64) {
        let n = 0;
        while (this.ms() < ms && n < max && this.step()) n++;
        return n;
    }

    // the audio interrupt: one block of this.frames stereo frames, interleaved float
    audio() {
        const p = this.e.emu_audio();
        return new Float32Array(this.e.memory.buffer, p, 2 * this.frames);
    }

    input(notes, buttons) { this.e.emu_input(notes >>> 0, buttons >>> 0); }
    enc(e, steps) { this.e.emu_enc(e, steps); }
    adc(ch, v) { this.e.emu_adc_set(ch, v); }
    midiIn(st, d1, d2) { this.e.emu_midi_in(st, d1, d2); }

    // MIDI the firmware sent since the last call: [[status, d1, d2], ...]
    midiOut() {
        const w = this.e.emu_midi_out_w(), out = [];
        const q = new Uint32Array(this.e.memory.buffer, this.e.emu_midi_out_ptr(), this.mq);
        if (w - this.moRead > this.mq) this.moRead = w - this.mq;
        for (; this.moRead !== w; this.moRead = (this.moRead + 1) >>> 0) {
            const m = q[this.moRead % this.mq];
            out.push([m & 255, m >> 8 & 127, m >> 16 & 127]);
        }
        return out;
    }

    fbGen() { return this.e.emu_fb_gen(); }
    fb() { return new Uint16Array(this.e.memory.buffer, this.e.emu_fb_ptr(), 240 * 240); }
    // per pixel: how often its colour changed since chgReset() (twice in one redraw = a flicker on the device)
    chg() { return this.u8(this.e.emu_chg_ptr(), 240 * 240); }
    chgReset() { this.e.emu_chg_reset(); }
    // per pixel: when its colour last changed (a running count): which of two things the panel got first
    at() { return new Uint32Array(this.e.memory.buffer, this.e.emu_at_ptr(), 240 * 240); }
    leds() { return [this.u8(this.e.emu_led_ptr(), 11), this.u8(this.e.emu_led_dim_ptr(), 11)]; }

    // the store's bytes if the firmware changed them since the last call, else null
    storeSaved() {
        if (!this.e.emu_store_gen) return null;
        const g = this.e.emu_store_gen();
        if (g === this.storeGen) return null;
        this.storeGen = g;
        return this.u8(this.e.emu_store_ptr(), this.e.emu_store_len()).slice();
    }

    // the settings bytes if the firmware saved since the last call, else null
    settingsSaved() {
        const g = this.e.emu_settings_gen();
        if (g === this.settingsGen) return null;
        this.settingsGen = g;
        return this.u8(this.e.emu_settings_ptr(), this.settingsLen).slice();
    }
}
