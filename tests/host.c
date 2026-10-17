/* SPDX-License-Identifier: GPL-3.0-only */
/* A fake FM-1 on a computer: the app (firmware/src/dinghy.c) runs unchanged on it. platform.c includes
 * this file instead of the hardware when DINGHY_HOST is set. A script presses keys over time; out come
 * the screen (PPM, at the times SHOT asks) and the sound (WAV, as the FM-1 sends it over USB).
 *
 *   mkdir -p build/host
 *   cc -O2 -Wall -DDINGHY_HOST -Ifirmware/src -Ifirmware/gen -o build/host/dinghy firmware/src/dinghy.c
 *   build/host/dinghy build/host/dinghy        (-> build/host/dinghy.wav, build/host/dinghy-<ms>.ppm)
 *   build/host/dinghy OUT script.txt [FPS]    (your own script; with FPS: OUT-NNNNN.ppm, a frame every 1/FPS s:
 *                                             a film of your firmware, e.g. ffmpeg -r 30 -i OUT-%05d.ppm ...)
 *
 * Change SCRIPT below to try your own app. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the platform's little libc (str_eq, str_cpy, fmt_fix, rng ...); its memset / memcpy / memcmp renamed: the host's are used */
#define memset libc_memset
#define memcpy libc_memcpy
#define memcmp libc_memcmp
#include "libc.c"
#undef memset
#undef memcpy
#undef memcmp

#ifndef APP_NAME
#define APP_NAME "DINGHY"
#endif
#ifndef APP_VER
#define APP_VER "0.0"
#endif
#define FS 44100u
#define HALF_FRAMES 256u

static volatile uint32_t fm1_ms;
#define FM1_TICKS_PER_US 24u
/* shortcut: 1 ms steps (the audio half reads 0 us, so the overload net never sheds here), a real clock if host CPU is ever measured */
static uint32_t fm1_ticks(void) { return fm1_ms * 1000u * FM1_TICKS_PER_US; }
enum { FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };
static int32_t fm1_adc_read(uint32_t ch) { return ch == FM1_ADC_BATT ? 600 : -1; }   /* (a battery at ~3.9 V) */
static int usb_up(void) { return 0; }
static uint32_t mem_free(int pool) { (void)pool; return 0; }   /* (no linker map here) */
#if BOARD_SMK37
/* the SMK-37 (hal/smk_input.h's API, as src/smk_keys.c uses it): keys with velocity, pads as events, 4 faders, 2 wheels,
 * the outer keys, the button and pad LEDs; 8 encoders (K1..K4 = KNOB 1..4, K5 = MASTER) */
#define FM1_NKEY 37u
#define SMK_NPAD 16u
#define SMK_NLED 19u
#define SMK_KEY_F3 5u
static struct { uint32_t notes, buttons, frames; uint8_t key_vel[FM1_NKEY], fader[4]; uint16_t wheel[2] ; } fm1_in = {
    .wheel = {540, 600}};
static const int8_t *smk_pad_map;
static int32_t host_q[2][64];                   /* pad events, outer-key events: key << 8 | velocity (0 = let go) */
static uint32_t host_qw[2], host_qr[2];
static void host_qput(uint32_t q, int32_t v) { host_q[q][host_qw[q]++ % 64u] = v; }
static int32_t host_qget(uint32_t q) { return host_qr[q] == host_qw[q] ? -1 : host_q[q][host_qr[q]++ % 64u]; }
static int32_t smk_pad_take(void) { return smk_pad_map ? host_qget(0) : -1; }
static int32_t smk_extkey_take(void) { return host_qget(1); }
static uint8_t fm1_led[SMK_NLED];
static void fm1_led_key(uint32_t id, int on) { if (id < SMK_NLED) fm1_led[id] = (uint8_t)(on != 0); }
static uint32_t padled_rgb[SMK_NPAD];           /* 0xRRGGBB, as the pads show it */
static void padled_set(uint32_t pad, uint8_t r, uint8_t g, uint8_t b)
{
    if (pad < SMK_NPAD) padled_rgb[pad] = (uint32_t)r << 16 | (uint32_t)g << 8 | b;
}
static void padled_flush(void) {}
#else
static struct { uint32_t notes, buttons; } fm1_in;
#endif
static uint32_t host_edges;
static int32_t host_enc[8];
static uint32_t fm1_input_edges(uint32_t *released) { uint32_t b = host_edges; host_edges = 0; *released = 0; return b; }
static uint32_t fm1_input_note_edges(void) { return 0; }   /* (the script sets fm1_in.notes) */
static int32_t fm1_enc_take(uint32_t e) { int32_t s = e < 8u ? host_enc[e] : 0; if (e < 8u) host_enc[e] = 0; return s; }
#if !BOARD_SMK37
static void fm1_led_key(uint32_t id, int on) { (void)id; (void)on; }
#define FM1_NCOL 11u                            /* the LED matrix as hal/fm1_input.h has it (written, never shown here) */
static const int8_t FM1_KEYMAP[6][FM1_NCOL] = {
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};
static uint8_t fm1_led[FM1_NCOL], fm1_led_dim[FM1_NCOL];
#endif

/* the screen: 240 x 240 RGB565, as the ST7789 gets it (gfx.c sends the bytes swapped) */
static uint16_t fb[240 * 240];
static void lcd_sync(void) {}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    for (j = y; j < y + h && j < 240u; j++)
        for (i = x; i < x + w && i < 240u; i++)
            fb[j * 240u + i] = c;
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u) {
                uint16_t c = px[j * w + i];
                fb[(y + j) * 240u + x + i] = (uint16_t)(c >> 8 | c << 8);
            }
}
#define CV_MAX (240u * 64u)
#include "gfx.c"

static uint8_t host_store[256 + 4];
static void host_load(const char *env, uint8_t *p, uint32_t n)   /* a start state from a file (web/emu/site/demo-*.bin) */
{
    const char *path = getenv(env);
    FILE *f = path ? fopen(path, "rb") : 0;
    if (!f) return;
    if (fread(p, 1, n, f)) {}
    fclose(f);
}
static int settings_load(void *p, uint32_t n)
{
    static int loaded;
    if (!loaded++) host_load("HOST_SETTINGS", host_store, sizeof host_store);
    if (host_store[0] != 1 || n > 256u) return -1;
    memcpy(p, host_store + 4, n);
    return 0;
}
static int settings_save(const void *p, uint32_t n)
{
    host_store[0] = 1;
    memcpy(host_store + 4, p, n);
    printf("%6u ms  settings saved (%u B)\n", fm1_ms, n);
    return 0;
}

static uint8_t host_flash[STORE_LEN];          /* the store: a NOR image in RAM (erased = 0xFF, writes clear bits) */
static uint8_t host_flash_ok;
static void host_flash_init(void)
{
    if (!host_flash_ok) { memset(host_flash, 0xFF, sizeof host_flash); host_load("HOST_STORE", host_flash, sizeof host_flash); host_flash_ok = 1; }
}
static int store_read(uint32_t off, void *p, uint32_t n)
{
    if (off > STORE_LEN || n > STORE_LEN - off) return -1;
    host_flash_init();
    memcpy(p, host_flash + off, n);
    return 0;
}
static int store_erase(uint32_t off)
{
    if (off & 0xFFFu || off >= STORE_LEN) return -1;
    host_flash_init();
    memset(host_flash + off, 0xFF, 0x1000u);
    return 0;
}
static int store_write(uint32_t off, const void *p, uint32_t n)
{
    const uint8_t *q = p;
    uint32_t i;
    if (off > STORE_LEN || n > STORE_LEN - off) return -1;
    host_flash_init();
    for (i = 0; i < n; i++) host_flash[off + i] &= q[i];
    printf("%6u ms  store: %u B at %05X\n", fm1_ms, n, off);
    return 0;
}

static uint8_t mq[64][3];
static uint32_t mq_w, mq_r;
static int midi_get(uint8_t *st, uint8_t *d1, uint8_t *d2)
{
    if (mq_r == mq_w) return 0;
    *st = mq[mq_r % 64u][0]; *d1 = mq[mq_r % 64u][1]; *d2 = mq[mq_r % 64u][2];
    mq_r++;
    return 1;
}
static void midi_send(uint32_t st, uint32_t d1, uint32_t d2) { printf("%6u ms  MIDI out %02X %u %u\n", fm1_ms, st, d1, d2); }

static void app_audio(int32_t *out, uint32_t n);
static void app_main(void);

/* the script: at t ms, keys down (bit k = key k, 0 = F3), a button edge, a MIDI message. A script file (the second
 * argument) replaces it, a line each: "T keys MASK" (hex or decimal), "T btn ID", "T midi ST D1 D2", "T end";
 * also "T hold ID 0|1" (a button held / let go: fm1_in.buttons), "T enc E STEPS" (encoder E, 0-based), "T shot".
 * SMK-37 builds (BOARD_SMK37): "keys" presses the gamepad's pads for its gamepad bits and the keybed for the rest;
 * "T pad P V" (hardware pad P at velocity V, 0 = let go), "T key K V" (keybed key K 0..36 at V, 0 = let go),
 * "T fader F V" (0..127), "T wheel W RAW" (0 pitch, 1 mod: raw 0..1023), "T dump" (LEDs, pads, state) */
typedef struct { uint32_t t, keys; int btn; uint8_t st, d1, d2; int kind, a, b; } host_ev_t;
enum { HE_KEYS, HE_HOLD = 1, HE_ENC, HE_SHOT, HE_PAD, HE_KEY, HE_FADER, HE_WHEEL, HE_DUMP };
static const host_ev_t SCRIPT0[] = {
    {200, 1u << 7, -1, 0, 0, 0},                                /* C4 */
    {700, 1u << 7 | 1u << 11 | 1u << 14, -1, 0, 0, 0},          /* + E4 G4: a chord */
    {1500, 0, -1, 0, 0, 0},
    {1700, 0, -1, 0x90, 81, 120},                               /* MIDI in: A5 */
    {2300, 0, -1, 0x80, 81, 0},
    {2500, 0, BTN_OCT_UP, 0, 0, 0},
    {2700, 1u << 2 | 1u << 9, -1, 0, 0, 0},                     /* G4 + D5, an octave up */
    {3400, 0, -1, 0, 0, 0},
};
static const host_ev_t *script = SCRIPT0;
static uint32_t script_n = sizeof SCRIPT0 / sizeof SCRIPT0[0], end_ms = 6500, frame_fps;
static const uint32_t SHOT[] = {1000, 3000};

static void read_script(const char *path)
{
    static host_ev_t ev[4096];
    char line[256], what[16];
    FILE *f = fopen(path, "r");
    uint32_t n = 0;
    if (!f) { perror(path); exit(1); }
    while (fgets(line, sizeof line, f) && n < 4096u) {
        host_ev_t e = {0, 0, -1, 0, 0, 0, HE_KEYS, 0, 0};
        unsigned a = 0, b = 0, c = 0;
        static const char *const KIND[] = {"", "hold", "enc", "shot", "pad", "key", "fader", "wheel", "dump"};
        int k;
        if (line[0] == '#' || sscanf(line, "%u %15s", &e.t, what) != 2) continue;
        if (!strcmp(what, "end")) { end_ms = e.t; continue; }
        for (k = 1; k < (int)(sizeof KIND / sizeof KIND[0]) && strcmp(what, KIND[k]); k++) {}
        if (k < (int)(sizeof KIND / sizeof KIND[0])) { e.kind = k; sscanf(line, "%*u %*s %i %i", &e.a, &e.b); }
        else if (!strcmp(what, "keys")) { sscanf(line, "%*u %*s %i", (int *)&e.keys); }
        else if (!strcmp(what, "btn")) { sscanf(line, "%*u %*s %d", &e.btn); }
        else if (!strcmp(what, "midi")) { sscanf(line, "%*u %*s %i %u %u", (int *)&a, &b, &c); e.st = (uint8_t)a; e.d1 = (uint8_t)b; e.d2 = (uint8_t)c; }
        else continue;
        ev[n++] = e;
    }
    fclose(f);
    script = ev;
    script_n = n;
}

static const char *host_out = "build/host/dinghy";
static int16_t *wav;
static uint32_t wav_n;

static void write_ppm_as(const char *name)
{
    FILE *f;
    uint32_t i;
    if (!(f = fopen(name, "wb"))) { perror(name); exit(1); }
    fprintf(f, "P6 240 240 255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = fb[i];
        fputc((c >> 11) * 255 / 31, f);
        fputc((c >> 5 & 63) * 255 / 63, f);
        fputc((c & 31) * 255 / 31, f);
    }
    fclose(f);
}
static void write_ppm(uint32_t t)
{
    char name[512];
    snprintf(name, sizeof name, "%s-%u.ppm", host_out, t);
    write_ppm_as(name);
    printf("%6u ms  screen -> %s\n", t, name);
}

static void write_wav(void)
{
    char name[512];
    FILE *f;
    uint32_t bytes = wav_n * 4u, v[] = {16, 0x00020001u, FS, FS * 4u, 0x00100004u};
    snprintf(name, sizeof name, "%s.wav", host_out);
    if (!(f = fopen(name, "wb"))) { perror(name); exit(1); }
    fwrite("RIFF", 1, 4, f); bytes += 36; fwrite(&bytes, 4, 1, f); bytes -= 36;
    fwrite("WAVEfmt ", 1, 8, f); fwrite(v, 4, 5, f);
    fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
    fwrite(wav, 4, wav_n, f);
    fclose(f);
    printf("sound  -> %s (%u frames)\n", name, wav_n);
}

#if BOARD_SMK37
static uint32_t smk_pad_of(uint32_t k);         /* src/smk_keys.c: the pad of gamepad key k (16: none) */
static void smk_host_dump(void);                /* src/smk_keys.c: LEDs, pads, state (DINGHY_HOST) */
static void host_key(uint32_t key, uint32_t vel)   /* a keybed key: the middle 27 in fm1_in.notes, the outer ones queued */
{
    if (key >= SMK_KEY_F3 && key < SMK_KEY_F3 + 27u) {
        if (vel) { fm1_in.key_vel[key] = (uint8_t)vel; fm1_in.notes |= 1u << (key - SMK_KEY_F3); }
        else fm1_in.notes &= ~(1u << (key - SMK_KEY_F3));
    } else if (key < FM1_NKEY)
        host_qput(1, (int32_t)(key << 8 | vel));
}
static void host_keys(uint32_t mask)            /* "keys": the gamepad bits on their pads, the rest on the keybed */
{
    static uint32_t was;
    uint32_t k, p, ch = mask ^ was;
    for (k = 0; k < 27u; k++) {
        if (!(ch >> k & 1u)) continue;
        if ((p = smk_pad_of(k)) < 16u) host_qput(0, (int32_t)(p << 8 | (mask >> k & 1u ? 100u : 0u)));
        else host_key(k + SMK_KEY_F3, mask >> k & 1u ? 100u : 0u);
    }
    was = mask;
}
#endif

static void platform_poll(void)                 /* one call = one millisecond */
{
    static uint32_t step, shot, owed, frame;
    static int32_t blk[2u * HALF_FRAMES];
    fm1_ms++;
#if BOARD_SMK37
    fm1_in.frames++;                            /* (the SMK scans ~625 frames a second; enough for "settled") */
#endif
    while (step < script_n && script[step].t <= fm1_ms) {
        if (script[step].st) {
            mq[mq_w % 64u][0] = script[step].st; mq[mq_w % 64u][1] = script[step].d1; mq[mq_w % 64u][2] = script[step].d2;
            mq_w++;
        } else if (script[step].btn >= 0)
            host_edges |= 1u << script[step].btn;
        else if (script[step].kind == HE_HOLD)
            fm1_in.buttons = script[step].b ? fm1_in.buttons | 1u << script[step].a : fm1_in.buttons & ~(1u << script[step].a);
        else if (script[step].kind == HE_ENC && script[step].a >= 0 && script[step].a < 8)
            host_enc[script[step].a] += script[step].b;
        else if (script[step].kind == HE_SHOT)
            write_ppm(fm1_ms);
#if BOARD_SMK37
        else if (script[step].kind == HE_PAD)
            host_qput(0, (int32_t)((uint32_t)script[step].a << 8 | (uint32_t)script[step].b));
        else if (script[step].kind == HE_KEY)
            host_key((uint32_t)script[step].a, (uint32_t)script[step].b);
        else if (script[step].kind == HE_FADER && script[step].a >= 0 && script[step].a < 4)
            fm1_in.fader[script[step].a] = (uint8_t)script[step].b;
        else if (script[step].kind == HE_WHEEL && script[step].a >= 0 && script[step].a < 2)
            fm1_in.wheel[script[step].a] = (uint16_t)script[step].b;
        else if (script[step].kind == HE_DUMP)
            smk_host_dump();
        else
            host_keys(script[step].keys);
#else
        else if (script[step].kind != HE_KEYS)
            fprintf(stderr, "script: an SMK-37 event in an FM-1 build, ignored\n");
        else
            fm1_in.notes = script[step].keys;
#endif
        step++;
    }
    owed += FS;                                 /* the frames this millisecond is worth, in blocks as the ISR renders */
    while (owed >= 1000u * HALF_FRAMES) {
        uint32_t i;
        owed -= 1000u * HALF_FRAMES;
        memset(blk, 0, sizeof blk);
        app_audio(blk, HALF_FRAMES);
        wav = realloc(wav, (wav_n + HALF_FRAMES) * 4u);
        for (i = 0; i < 2u * HALF_FRAMES; i++) {
            int32_t v = blk[i];                 /* (USB audio at the MASTER knob all the way up) */
            wav[2u * wav_n + i] = (int16_t)(v > 32767 ? 32767 : v < -32767 ? -32767 : v);
        }
        wav_n += HALF_FRAMES;
    }
    if (frame_fps) {                            /* a film: a frame every 1/fps s, as the screen is then */
        if ((uint64_t)frame * 1000u <= (uint64_t)fm1_ms * frame_fps) {
            char name[512];
            snprintf(name, sizeof name, "%s-%05u.ppm", host_out, frame++);
            write_ppm_as(name);
        }
    } else if (shot < sizeof SHOT / sizeof SHOT[0] && fm1_ms == SHOT[shot])
        write_ppm(SHOT[shot++]);
    if (fm1_ms >= end_ms) {
        write_wav();
        exit(0);
    }
}

int main(int argc, char **argv)
{
    if (argc > 1) host_out = argv[1];
    if (argc > 2) read_script(argv[2]);
    if (argc > 3) frame_fps = (uint32_t)atoi(argv[3]);
    app_main();
    return 0;
}
