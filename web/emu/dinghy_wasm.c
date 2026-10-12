/* SPDX-License-Identifier: GPL-3.0-only */
/* Dinghy in a browser: a fake FM-1 for WebAssembly. It plays the role of Dinghy's tests/host.c: with
 * DINGHY_HOST set, platform.c includes "../../tests/host.c" instead of the hardware, and build.py stages
 * this file there, next to the app's own unchanged sources (dinghy.c, platform.c, gfx.c, or a firmware
 * built on Dinghy). Everything docs/API.md promises an app is defined here.
 *
 *   main loop  app_main() never returns. Binaryen's Asyncify turns each platform_poll() into a yield
 *              (host_yield, imported from JS): one call = one millisecond, as in host mode. The worklet
 *              resumes the loop until the firmware's clock has caught up with the audio clock.
 *   audio      emu_audio() is the ISR: app_audio() on a zeroed block of HALF_FRAMES, then the MASTER knob
 *              and the clip as platform.c does them, out as float for the AudioWorklet.
 *   screen     lcd_fill / lcd_blit into a 240 x 240 RGB565 framebuffer (gfx.c draws through them).
 *   input      JS writes the panel state (emu_input, emu_enc, emu_adc_set, emu_midi).
 *   settings   256 bytes + header in memory; JS keeps them in the browser between visits.
 *   store      the data region (STORE_LEN) as a NOR image in memory: erased = 0xFF, a write only clears bits, as the
 *              flash; JS fills it before boot (or erases it) and keeps it after each change (emu_store_gen).
 *
 * Freestanding: no libc, no float in the firmware's path (the float conversion is ours, for WebAudio). */
#include <stdint.h>

#ifndef APP_NAME
#define APP_NAME "DINGHY"
#endif
#ifndef APP_VER
#define APP_VER "web"
#endif
#define FS 44100u
#define HALF_FRAMES 256u
#define WASM_EXPORT(n) __attribute__((export_name(n), used))
#define NULL ((void *)0)
typedef __SIZE_TYPE__ size_t;

/* ---- the platform's little libc, as on the FM-1 (platform.c includes it): memset, memcpy, memcmp, str_eq,
 * str_len, str_cpy, fmt_fix, fmt_int, rng. Plus two the compiler may call on wasm. */
#include "libc.c"
void *memmove(void *d, const void *s, size_t n)
{
    uint8_t *p = d;
    const uint8_t *q = s;
    if (p < q) while (n--) *p++ = *q++;
    else { p += n; q += n; while (n--) *--p = *--q; }
    return d;
}
size_t strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }

/* ---- the yield: JS unwinds the stack here and rewinds into it on the next step */
__attribute__((import_module("env"), import_name("host_yield"))) void host_yield(void);

/* ---- time: the firmware's clock, one millisecond per platform_poll() */
#define FM1_TICKS_PER_US 24u
static volatile uint32_t fm1_ms;
static uint32_t emu_us;                         /* microseconds within the current millisecond (delays) */
static uint32_t fm1_ticks(void) { return (fm1_ms * 1000u + emu_us) * FM1_TICKS_PER_US; }
static uint32_t fm1_micros(void) { return fm1_ms * 1000u + emu_us; }
static void fm1_wdt_feed(void) {}
static void platform_poll(void);
static void fm1_delay_ms(uint32_t ms) { while (ms--) platform_poll(); }   /* time passes, the sound goes on */
static void fm1_delay_us(uint32_t us)
{
    emu_us += us;
    while (emu_us >= 1000u) { emu_us -= 1000u; platform_poll(); }
}

/* ---- the panel: debounced state as hal/fm1_input.h keeps it (JS writes it) */
#define FM1_NCOL 11u
#define FM1_NENC 7u
static const int8_t FM1_KEYMAP[6][FM1_NCOL] = {  /* = hal/fm1_input.h and site/emu.js KEYMAP: change all three */
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};
static volatile struct {
    uint32_t notes;                             /* bit k = key k (0 = F3 .. 26 = G5) */
    uint32_t buttons;                           /* bit i = button i (BTN_...) */
    uint32_t pressed, released, notes_pressed;  /* edges since the app last took them */
    int16_t enc_steps[FM1_NENC];
} fm1_in;
static uint8_t fm1_led[FM1_NCOL], fm1_led_dim[FM1_NCOL];

static uint32_t fm1_input_edges(uint32_t *released)
{
    uint32_t p = fm1_in.pressed;
    if (released) *released = fm1_in.released;
    fm1_in.pressed = fm1_in.released = 0;
    return p;
}
static uint32_t fm1_input_note_edges(void)
{
    uint32_t p = fm1_in.notes_pressed;
    fm1_in.notes_pressed = 0;
    return p;
}
static int32_t fm1_enc_take(uint32_t e)
{
    int32_t s;
    if (e >= FM1_NENC) return 0;
    s = fm1_in.enc_steps[e];
    fm1_in.enc_steps[e] = 0;
    return s;
}
static void fm1_led_key(uint32_t id, int on)    /* a button (BTN_...) or a key (KEY_LED(k)) */
{
    uint32_t c, r;
    for (c = 0; c < FM1_NCOL; c++)
        for (r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][c] == (int8_t)id) {
                if (on) fm1_led[c] |= (uint8_t)(1u << r);
                else fm1_led[c] &= (uint8_t)~(1u << r);
            }
}

/* ---- the MASTER knob and the battery, as the ADC reads them */
enum { FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };
static volatile int32_t emu_adc[8] = {0, 0, 0, 800, 820, 0, 0, 0};
static int32_t fm1_adc_read(uint32_t ch) { return emu_adc[ch & 7u]; }
static volatile int32_t emu_usb;               /* a USB host there (platform.c usb_up): emu_usb_set */
static int usb_up(void) { return emu_usb != 0; }
static uint32_t mem_free(int pool) { (void)pool; return 0; }   /* (no linker map here) */
static volatile int32_t master_q12 = 2048;

/* ---- the screen: 240 x 240 RGB565, as the ST7789 gets it (gfx.c keeps its canvas byte-swapped) */
static uint16_t emu_fb[240 * 240];
static volatile uint32_t emu_fb_gen;
static uint8_t emu_chg[240 * 240];             /* per pixel: how often its colour changed since emu_chg_reset (a
                                                 * pixel changed twice in a redraw = a flicker on the device) */
static uint32_t emu_at[240 * 240], emu_n;     /* per pixel: when its colour last changed (a count of changes): the order
                                                 * a redraw put things on the panel */
static void emu_px(uint32_t i, uint16_t c) { if (emu_fb[i] != c) { emu_fb[i] = c; emu_at[i] = ++emu_n; if (emu_chg[i] < 255u) emu_chg[i]++; } }
static void lcd_sync(void) {}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    for (j = y; j < y + h && j < 240u; j++)
        for (i = x; i < x + w && i < 240u; i++)
            emu_px(j * 240u + i, c);
    emu_fb_gen++;
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u) {
                uint16_t c = px[j * w + i];
                emu_px((y + j) * 240u + x + i, (uint16_t)(c >> 8 | c << 8));
            }
    emu_fb_gen++;
}
#ifndef CV_MAX
#define CV_MAX (240u * 64u)                     /* as platform.c; an app may raise it before including it */
#endif
#include "gfx.c"

/* ---- settings: one "sector" in memory; JS restores it before boot and keeps it after each save */
#ifdef STORE_LEN                                /* (a platform with a store: ours, fw/firmware/src/platform.c) */
static uint8_t emu_store[STORE_LEN];
static volatile uint32_t emu_store_gen;
static int store_read(uint32_t off, void *p, uint32_t n)
{
    if (off > STORE_LEN || n > STORE_LEN - off) return -1;
    memcpy(p, emu_store + off, n);
    return 0;
}
static int store_erase(uint32_t off)
{
    if (off & 0xFFFu || off >= STORE_LEN) return -1;
    memset(emu_store + off, 0xFF, 0x1000u);
    emu_store_gen++;
    return 0;
}
static int store_write(uint32_t off, const void *p, uint32_t n)
{
    const uint8_t *q = p;
    uint32_t i;
    if (off > STORE_LEN || n > STORE_LEN - off) return -1;
    for (i = 0; i < n; i++) emu_store[off + i] &= q[i];
    emu_store_gen++;
    return 0;
}
#endif
static uint8_t emu_settings[4 + 256];           /* [valid, 0, len lo, len hi] then the bytes */
static volatile uint32_t emu_settings_gen;
static int settings_load(void *p, uint32_t n)
{
    if (emu_settings[0] != 1u || n > 256u || (uint32_t)(emu_settings[2] | emu_settings[3] << 8) != n)
        return -1;
    memcpy(p, emu_settings + 4, n);
    return 0;
}
static int settings_save(const void *p, uint32_t n)
{
    if (n > 256u) return -1;
    emu_settings[0] = 1;
    emu_settings[2] = (uint8_t)n;
    emu_settings[3] = (uint8_t)(n >> 8);
    memcpy(emu_settings + 4, p, n);
    emu_settings_gen++;
    return 0;
}

/* ---- USB-MIDI: in from JS (Web MIDI or the page), out to JS */
#define EMU_MQ 256u
static uint32_t emu_mi[EMU_MQ], emu_mi_w, emu_mi_r;   /* status | d1 << 8 | d2 << 16 */
static uint32_t emu_mo[EMU_MQ], emu_mo_w;              /* JS reads [emu_mo_w - k] */
static int midi_get(uint8_t *st, uint8_t *d1, uint8_t *d2)
{
    uint32_t m;
    if (emu_mi_r == emu_mi_w) return 0;
    m = emu_mi[emu_mi_r++ % EMU_MQ];
    *st = (uint8_t)m;
    *d1 = (uint8_t)(m >> 8);
    *d2 = (uint8_t)(m >> 16);
    return 1;
}
static void midi_send(uint32_t st, uint32_t d1, uint32_t d2)
{
    emu_mo[emu_mo_w++ % EMU_MQ] = (st & 0xFFu) | (d1 & 0x7Fu) << 8 | (d2 & 0x7Fu) << 16;
}

static void app_audio(int32_t *out, uint32_t n);
static void app_main(void);

/* ---- the main loop's duty: one millisecond passes, the MASTER knob is read, the page gets its turn */
static void platform_poll(void)
{
    static int32_t knob_avg = 512 * 16;
    int32_t a;
    fm1_ms++;
    if ((a = fm1_adc_read(FM1_ADC_MASTER)) >= 0) {   /* as platform.c */
        uint32_t k10;
        knob_avg += (a * 16 - knob_avg) / 8;
        k10 = (uint32_t)(knob_avg / 16);
        master_q12 = (int32_t)((k10 * k10) >> 8);
    }
    host_yield();
}

/* ---------------------------------------------------------------- the page's side --- */
/* Asyncify: [cur, end] then the saved stack, i.e. the call chain from emu_main down to host_yield. 16 KiB is room,
 * not a measure: the deepest unwind seen was 332 B of the 16,376 (all of tracker_emu.mjs, 2026-10-11). An unwind
 * past the end traps ("unreachable"). */


static uint8_t emu_async[16384] __attribute__((aligned(8)));
static int32_t emu_blk[2u * HALF_FRAMES];
static float emu_out[2u * HALF_FRAMES];

WASM_EXPORT("emu_main") void emu_main(void) { app_main(); }     /* run, resumed by Asyncify at every poll */
WASM_EXPORT("emu_async_ptr") uint32_t emu_async_ptr(void) { return (uint32_t)(uintptr_t)emu_async; }
WASM_EXPORT("emu_async_size") uint32_t emu_async_size(void) { return sizeof emu_async; }
WASM_EXPORT("emu_ms") uint32_t emu_ms(void) { return fm1_ms; }

WASM_EXPORT("emu_audio") uint32_t emu_audio(void)   /* the ISR: HALF_FRAMES stereo frames -> emu_out */
{
    uint32_t i;
    for (i = 0; i < 2u * HALF_FRAMES; i++) emu_blk[i] = 0;
    app_audio(emu_blk, HALF_FRAMES);
    for (i = 0; i < 2u * HALF_FRAMES; i++) {
        int32_t v = (emu_blk[i] * master_q12) >> 12;
        v = v > 32767 ? 32767 : v < -32767 ? -32767 : v;
        emu_out[i] = (float)v * (1.0f / 32768.0f);
    }
    return (uint32_t)(uintptr_t)emu_out;
}
WASM_EXPORT("emu_frames") uint32_t emu_frames(void) { return HALF_FRAMES; }

WASM_EXPORT("emu_input") void emu_input(uint32_t notes, uint32_t buttons)
{
    uint32_t pn = fm1_in.notes, pb = fm1_in.buttons;
    notes &= 0x7FFFFFFu;
    buttons &= 0x3FFFu;
    fm1_in.notes = notes;
    fm1_in.buttons = buttons;
    fm1_in.notes_pressed |= notes & ~pn;
    fm1_in.pressed |= buttons & ~pb;
    fm1_in.released |= pb & ~buttons;
}
WASM_EXPORT("emu_enc") void emu_enc(uint32_t e, int32_t steps)
{
    if (e < FM1_NENC) fm1_in.enc_steps[e] = (int16_t)(fm1_in.enc_steps[e] + steps);
}
WASM_EXPORT("emu_adc_set") void emu_adc_set(uint32_t ch, int32_t v) { emu_adc[ch & 7u] = v; }
WASM_EXPORT("emu_usb_set") void emu_usb_set(int32_t on) { emu_usb = on; }
WASM_EXPORT("emu_midi_in") void emu_midi_in(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (emu_mi_w - emu_mi_r < EMU_MQ)
        emu_mi[emu_mi_w++ % EMU_MQ] = (st & 0xFFu) | (d1 & 0x7Fu) << 8 | (d2 & 0x7Fu) << 16;
}
WASM_EXPORT("emu_midi_out_ptr") uint32_t emu_midi_out_ptr(void) { return (uint32_t)(uintptr_t)emu_mo; }
WASM_EXPORT("emu_midi_out_w") uint32_t emu_midi_out_w(void) { return emu_mo_w; }

WASM_EXPORT("emu_fb_ptr") uint32_t emu_fb_ptr(void) { return (uint32_t)(uintptr_t)emu_fb; }
WASM_EXPORT("emu_fb_gen") uint32_t emu_fb_gen_get(void) { return emu_fb_gen; }
WASM_EXPORT("emu_chg_ptr") uint32_t emu_chg_ptr(void) { return (uint32_t)(uintptr_t)emu_chg; }
WASM_EXPORT("emu_at_ptr") uint32_t emu_at_ptr(void) { return (uint32_t)(uintptr_t)emu_at; }
WASM_EXPORT("emu_chg_reset") void emu_chg_reset(void) { uint32_t i; for (i = 0; i < sizeof emu_chg; i++) emu_chg[i] = 0; }
WASM_EXPORT("emu_led_ptr") uint32_t emu_led_ptr(void) { return (uint32_t)(uintptr_t)fm1_led; }
WASM_EXPORT("emu_led_dim_ptr") uint32_t emu_led_dim_ptr(void) { return (uint32_t)(uintptr_t)fm1_led_dim; }
WASM_EXPORT("emu_settings_ptr") uint32_t emu_settings_ptr(void) { return (uint32_t)(uintptr_t)emu_settings; }
WASM_EXPORT("emu_settings_len") uint32_t emu_settings_len(void) { return sizeof emu_settings; }
WASM_EXPORT("emu_mq_len") uint32_t emu_mq_len(void) { return EMU_MQ; }
#ifdef STORE_LEN
WASM_EXPORT("emu_store_ptr") uint32_t emu_store_ptr(void) { return (uint32_t)(uintptr_t)emu_store; }
WASM_EXPORT("emu_store_len") uint32_t emu_store_len(void) { return STORE_LEN; }
WASM_EXPORT("emu_store_gen") uint32_t emu_store_gen_get(void) { return emu_store_gen; }
#endif
WASM_EXPORT("emu_settings_gen") uint32_t emu_settings_gen_get(void) { return emu_settings_gen; }
