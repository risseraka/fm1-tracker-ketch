/* SPDX-License-Identifier: GPL-3.0-only */
/* Dinghy's platform: everything an FM-1 firmware needs before it can be fun, taken unchanged from
 * SLOOP / Felucca / sloopDX and wired up here. You should not need to touch this file.
 *
 *   boot stub           firmware/stub, before this app: a half-written install, or OCT- held at power-on,
 *                       goes to update mode (ota-FM-1; the installer finishes it); docs/boot-stub-plan.md
 *   boot + boot guard   a firmware that crashes at boot falls back to the USB rescue, then to the ROM
 *   updater             the M-UPGRADE SysEx path: the web installer / the hub can always reach the FM-1
 *   USB-MIDI            in and out (midi_get, midi_send)
 *   USB audio           the output as a class-compliant 16-bit stereo input, 44.1 or 48 kHz
 *   keys, knobs, LEDs   fm1_in.notes / buttons, fm1_input_edges, fm1_enc_take, fm1_led_key (hal/fm1_input.h)
 *   screen              240 x 240, lcd_fill, draw_text_box with FONT_S (16 px) / FONT_L (32 px) (gfx.c)
 *   settings            one 4 KiB flash sector with a CRC (settings_load / settings_save)
 *   crash screen        the PC of a fault, then a reboot
 *
 * The app (dinghy.c) includes this file first and supplies:
 *   static void app_audio(int32_t *out, uint32_t n)   audio ISR: n stereo frames, L R L R, Q15 (+-32767), out is zero
 *   static void app_main(void)                        never returns; calls platform_poll() often (every few ms)
 */
/* the panel by name: buttons (fm1_in.buttons, fm1_input_edges, fm1_led_key: bit / id), the encoders
 * (fm1_enc_take); the 27 note keys are fm1_in.notes bit 0 (F3) .. 26 (G5), LED id 14 + key */
#ifndef BOARD_SMK37
#define BOARD_SMK37 0                           /* 1: the SMK-37 (tools/build.py BOARD=smk37; hal/fm1_cc.h) */
#endif
#if BOARD_SMK37
/* the SMK-37 (SMK37.md): the FM-1's buttons on the SMK buttons that mean the same (PATCH = HOME, PARA = EDIT,
 * CHORD = ENV, NOTE REPEAT = LFO, SCALE = SEL, GLOBE = GLO, BT = SAVE), the four extras; K1..K4 KNOB 1..4 (the page's
 * knobs), K5 the MASTER pot (hal/smk_input.h), K6 SELECT, K7 ALGORITHM, K8 PRESETS. The keys have no LEDs (KEY_LED ids lie past
 * the button LEDs and do nothing); the pads are the app's (smk_keys.c) */
enum { BTN_KNOB_BANK = 0, BTN_FADER_BANK = 1, BTN_LEFT = 2, BTN_RIGHT = 3, BTN_OCT_DOWN = 4, BTN_OCT_UP = 5,
       BTN_ARP = 6, BTN_LFO = 7, BTN_SEL = 8, BTN_ENV = 9, BTN_GLO = 10, BTN_SAVE = 11, BTN_HOME = 12, BTN_EDIT = 13,
       BTN_FX = 14, BTN_SEQ = 15, BTN_PLAY = 16, BTN_REC = 17 };
enum { ENC_KNOB1 = 0, ENC_KNOB2 = 1, ENC_KNOB3 = 2, ENC_KNOB4 = 3, ENC_MASTER = 4, ENC_SELECT = 5, ENC_ALGORITHM = 6,
       ENC_PRESETS = 7 };
#define KEY_LED(k) (32u + (k))

/* the store: 56 sectors in the stock VM and USRFLASH (src/smk_store.h), for the app's own records */
#define STORE_LEN 0x00038000u                   /* 224 KiB, 56 sectors: 4 project slots (tracker.c) */
#else
enum { BTN_OCT_DOWN = 0, BTN_OCT_UP = 1, BTN_FX = 2, BTN_SEL = 3, BTN_ENV = 4, BTN_LFO = 5, BTN_EDIT = 6,
       BTN_GLO = 7, BTN_HOME = 8, BTN_SAVE = 9, BTN_ARP = 10, BTN_SEQ = 11, BTN_PLAY = 12, BTN_REC = 13 };
enum { ENC_SELECT = 0, ENC_ALGORITHM = 1, ENC_KNOB1 = 2, ENC_KNOB2 = 3, ENC_KNOB3 = 4, ENC_KNOB4 = 5, ENC_PRESETS = 6 };
#define KEY_LED(k) (14u + (k))

/* the store: the data region 0x97000-0xDFFFF (Felucca's store; docs/flash-map.md), for the app's own records */
#define STORE_AT 0x00097000u
#define STORE_LEN 0x00049000u                   /* 292 KiB, 73 sectors */
#endif

#ifdef DINGHY_HOST
#include "../../tests/host.c"                  /* on a computer: a fake FM-1 (tests/host.c) */
#else
#include <stdint.h>
#include "fm1_time.h"
#include "fm1_sys.h"
#include "fm1_irq.h"
#include "fm1_guard.h"
#if BOARD_SMK37
#include "smk_input.h"                          /* keys with velocity, buttons, 8 encoders, LEDs, pads, faders, wheels */
#else
#include "fm1_input.h"
#endif
#include "fm1_timer.h"
#include "fm1_lcd_hw.h"
#include "fm1_audio.h"
#if !BOARD_SMK37
#include "fm1_adc.h"                            /* (SMK: smk_input.h stands in, MASTER = K5) */
#endif
#include "bootguard.h"

#ifndef APP_NAME
#define APP_NAME "DINGHY"
#endif
#ifndef FELUCCA_ID
#if BOARD_SMK37
#define FELUCCA_ID SMK_MODEL "_990"             /* SMK-37 Pro_99N (Elite, MKE-P37: SMK_MODEL), tools/build.py */
#else
#define FELUCCA_ID "FM-1_990"                   /* package identity: 99N = Dinghy build N (tools/build.py) */
#endif
#endif
#ifndef APP_VER
#define APP_VER "0.0"
#endif

bootguard_t bootguard __attribute__((section(".noinit")));
static volatile uint32_t fm1_ms;                /* milliseconds since power-on (TIMER5) */

#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#include "libc.c"
#include "lcd.c"
#define CV_MAX (240u * 64u)                     /* the biggest text box drawn at once */
#include "gfx.c"

#define FS 44100u                               /* the sample rate */
#define HALF_FRAMES 256u                        /* I2S half buffer: 5.8 ms (as sloopDX: the USB audio band is set for it) */
#define HALF_WORDS (HALF_FRAMES * 2u)
#define FELUCCA_OTA 1
#define FELUCCA_CDC 0
#define FELUCCA_UAC 1
#define FELUCCA_UART 0
#include "usb.c"

/* ---- flash: read anywhere; erase / program only the update's staging area, the settings sector and the store */
#if BOARD_SMK37
#include "smk_store.h"                          /* the SMK-37 layout: the store, the owner tag, the settings */
#include "fm1_flash.h"
typedef char store_is_mapped[SMK_STORE_SECTORS * 0x1000u == STORE_LEN ? 1 : -1];
#else
#define SETTINGS_AT 0x000FC000u                 /* one 4 KiB sector of Felucca's globals area */
#define FL_RANGE_OK(off, n) (FL_IN(off, n, FL_OTA_LO, FL_OTA_HI) || FL_IN(off, n, SETTINGS_AT, SETTINGS_AT + 0x1000u) || \
                             FL_IN(off, n, STORE_AT, STORE_AT + STORE_LEN))
#include "fm1_flash.h"
typedef char store_is_data[STORE_AT == FL_DATA_LO && STORE_AT + STORE_LEN == FL_DATA_HI ? 1 : -1];   /* = fm1_flash.h (and ldr_core.c) */
#endif
static uint8_t flash_ok;
static int st_read(uint32_t off, void *dst, uint32_t n)
{
    uint8_t *d = dst;
    while (n) {
        uint32_t k = n > 256u ? 256u : n, f = irq_save();
        int rc = FL_FAR(fl_read_ram)(off, d, k);
        irq_restore(f);
        if (rc)
            return rc;
        off += k;
        d += k;
        n -= k;
    }
    return 0;
}

static uint8_t recovery_active;
#if BOARD_SMK37
#define OTA_IDENTITY (recovery_active ? SMK_MODEL "_000" : FELUCCA_ID)
#else
#define OTA_IDENTITY (recovery_active ? "FM-1_000" : FELUCCA_ID)
#endif
#include "ota.c"
static void recovery_poll(void);
static uint32_t ota_now_ms(void) { return fm1_ms; }
static void ota_idle(void)
{
    fm1_wdt_feed();
    if (recovery_active) recovery_poll();
}
static int ota_in_area(uint32_t off, uint32_t n) { return FL_IN(off, n, OTA_AREA, OTA_AREA + OTA_AREA_LEN); }
static int ota_erase(uint32_t off)
{
    uint32_t took;
    if (!ota_in_area(off, 0x1000u) || (off & 0xFFFu))
        return -8;
    return fl_erase4k(off, &took);
}
static int ota_prog(uint32_t off, const void *p, uint32_t n)
{
    if (!ota_in_area(off, n))
        return -8;
    return fl_write(off, p, n);
}
static int ota_fread(uint32_t off, void *p, uint32_t n) { return st_read(off, p, n); }
static void ota_show(uint32_t step, int32_t code)
{
    static const char *const STEP[] = {"", "PACKAGE", "CHECK HEAD", "LOADER", "CONFIRM", "RESTART"};
    if (recovery_active) return;
    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 92, 240, &FONT_S, "UPDATE", C_WHITE, 1);
    if (step < 9u)
        draw_text_box(0, 124, 240, &FONT_S, STEP[step < 6u ? step : 0], C_WHITE, 1);
    else
        draw_text_box(0, 124, 240, &FONT_S, code == 1 ? "DRY RUN OK" : "FAILED", C_WHITE, 1);
}
static void ota_commit(const uint8_t *parm)
{
    bootguard_clear(&bootguard);
    usb_detach();
    fm1_delay_ms(30);
    fm1_enter_update(parm);
}
#include "recovery.c"

/* ---- settings: [magic][length][crc32] then the bytes; settings_save erases the sector (~50 ms with
 * the interrupts off: the sound drops out), so save when nothing plays, not on every knob step */
#define SETTINGS_MAGIC 0x59474E44u              /* "DNGY" */
static uint32_t crc32(const uint8_t *p, uint32_t n)
{
    uint32_t c = 0xFFFFFFFFu, k;
    while (n--) {
        c ^= *p++;
        for (k = 0; k < 8u; k++)
            c = c & 1u ? (c >> 1) ^ 0xEDB88320u : c >> 1;
    }
    return ~c;
}
static int settings_load(void *p, uint32_t n)   /* 0: loaded; else p is untouched */
{
    static uint8_t buf[256];
    uint32_t h[3];
    if (!flash_ok || n > sizeof buf || st_read(SETTINGS_AT, h, sizeof h) || h[0] != SETTINGS_MAGIC || h[1] != n)
        return -1;
    if (st_read(SETTINGS_AT + sizeof h, buf, n) || crc32(buf, n) != h[2])
        return -1;
    memcpy(p, buf, n);
    return 0;
}
static int settings_save(const void *p, uint32_t n)   /* p in RAM, n <= 256 */
{
    uint32_t h[3] = {SETTINGS_MAGIC, n, 0}, took;
    int rc;
    if (!flash_ok || n > 256u)
        return -1;
    h[2] = crc32(p, n);
    if ((rc = fl_erase4k(SETTINGS_AT, &took)) != 0)
        return rc;
    if ((rc = fl_write(SETTINGS_AT + sizeof h, p, n)) != 0)
        return rc;
    return fl_write(SETTINGS_AT, (const uint8_t *)h, sizeof h);   /* the header last: a cut save stays invalid */
}

/* ---- the store: the app's own records in the data region (store_read / store_erase / store_write; docs/API.md) */
#if BOARD_SMK37
/* SMK-37: the store's sectors lie in three runs (smk_store.h smk_store_flash): each call is split at the sectors */
static void smk_stock_release(void);
static int store_read(uint32_t off, void *p, uint32_t n)
{
    uint8_t *d = p;
    if (!flash_ok || off > STORE_LEN || n > STORE_LEN - off) return -1;
    while (n) {
        uint32_t k = 0x1000u - (off & 0xFFFu);
        if (k > n) k = n;
        if (st_read(smk_store_flash(off), d, k)) return -1;
        off += k; d += k; n -= k;
    }
    return 0;
}
static int store_erase(uint32_t off)            /* the 4 KiB sector at off; ~50 ms with the interrupts off */
{
    uint32_t took, at;
    if (!flash_ok || off & 0xFFFu || off >= STORE_LEN) return -1;
    at = smk_store_flash(off);
    if (at >= FL_SMP_LO && at < FL_SMP_HI)
        smk_stock_release();                    /* before the first erase in USRFLASH (below) */
    return fl_erase4k(at, &took);
}
static int store_write(uint32_t off, const void *p, uint32_t n)   /* p in RAM; bits only go 1 -> 0 */
{
    const uint8_t *q = p;
    if (!flash_ok || off > STORE_LEN || n > STORE_LEN - off) return -1;
    while (n) {
        uint32_t k = 0x1000u - (off & 0xFFFu), at = smk_store_flash(off);
        int rc;
        if (k > n) k = n;
        if (at >= FL_SMP_LO && at < FL_SMP_HI)
            smk_stock_release();                /* before the first program in USRFLASH too */
        if ((rc = fl_write(at, q, k)) != 0) return rc;
        off += k; q += k; n -= k;
    }
    return 0;
}

/* The stock firmware's mapping in USRFLASH (global settings at 0xC2000, magic A3 3A; 8 presets from 0xC3000 every
 * 0x5000, the rest erased; the end marker "usrdata\0" at 0xEAFF8, FL_STOCKTAG): the stock firmware trusts the area
 * while the marker is there and never rebuilds it, so after a return to stock it would read our projects as its key,
 * knob and fader mapping. So: an intact stock mapping is kept (projects 3 and 4 read as empty), and the marker is
 * released (its sector erased: the stock firmware rebuilds its factory mapping at its next start) before the first
 * erase or program in USRFLASH (store_erase, store_write) and at boot when it stands over anything else
 * (smk_owner_check). */
static int smk_stock_tagged(void)
{
    static const uint8_t TAG[8] = {'u', 's', 'r', 'd', 'a', 't', 'a', 0};
    uint8_t t[8];
    uint32_t i;
    if (st_read(FL_STOCKTAG_OFF, t, 8))
        return 0;
    for (i = 0; i < 8u; i++)
        if (t[i] != TAG[i])
            return 0;
    return 1;
}
static int smk_stock_intact(void)               /* the marker over the stock layout */
{
    uint8_t b[8];
    uint32_t n, k, i, ff;
    if (!smk_stock_tagged() || st_read(FL_SMP_LO, b, 2) || b[0] != 0xA3u || b[1] != 0x3Au)
        return 0;
    for (n = 0; n < 8u; n++)                    /* each preset: its head written, its last two sectors erased */
        for (k = 0; k < 3u; k++) {
            if (st_read(FL_SMP_LO + 0x1000u + n * 0x5000u + (k ? 0x2000u + k * 0x1000u : 0u), b, 8))
                return 0;
            for (ff = 1, i = 0; i < 8u; i++)
                ff &= b[i] == 0xFFu;
            if (ff != (k != 0u))
                return 0;
        }
    return 1;
}
static void smk_stock_release(void)             /* erase the marker's sector, once: later calls find no marker */
{
    uint32_t took;
    if (smk_stock_tagged())
        fl_erase4k(FL_STOCKTAG_LO, &took);
}
/* the store's owner (the kit's convention, smk_store.h): our tag, load as ever; anything else (the stock settings,
 * another firmware's data) is cleared once (the FL_DATA part of the store and the settings) and the tag written,
 * before the audio starts. The projects in USRFLASH check themselves (magic, slot, CRCs: tracker.c pj_head) */
static uint8_t smk_store_cleared;
static void smk_owner_check(void)
{
    uint8_t tag[16], want[16];
    uint32_t i, ours = 1, took;
    if (!flash_ok)
        return;
    if (!smk_stock_intact())
        smk_stock_release();                    /* the marker over anything but the stock mapping: let it rebuild */
    smk_owner_tag(want);
    if (st_read(OWNER_AT, tag, 16))
        return;
    for (i = 0; i < 16u; i++)
        ours &= tag[i] == want[i];
    if (ours)
        return;
    for (i = 0; i < SMK_RUN1_N; i++)
        fl_erase4k(SMK_RUN1_AT + i * 0x1000u, &took);
    for (i = 0; i < SMK_RUN2_N; i++)
        fl_erase4k(SMK_RUN2_AT + i * 0x1000u, &took);
    fl_erase4k(SETTINGS_AT, &took);
    fl_erase4k(OWNER_AT, &took);
    fl_write(OWNER_AT, want, 16);
    smk_store_cleared = 1;
}
/* the factory key / pad velocity curves (USRTRIM 0xC1000, read only; smk_input.h checks them, else its defaults) */
static void smk_trim_read(void)
{
    static uint8_t rec[0x22Cu];
    if (flash_ok && !st_read(SMK_TRIM_FLASH, rec, sizeof rec))
        smk_trim_load(rec);
}
#else
static int store_read(uint32_t off, void *p, uint32_t n)
{
    if (!flash_ok || off > STORE_LEN || n > STORE_LEN - off) return -1;
    return st_read(STORE_AT + off, p, n);
}
static int store_erase(uint32_t off)            /* the 4 KiB sector at off; ~50 ms with the interrupts off */
{
    uint32_t took;
    if (!flash_ok || off & 0xFFFu || off >= STORE_LEN) return -1;
    return fl_erase4k(STORE_AT + off, &took);
}
static int store_write(uint32_t off, const void *p, uint32_t n)   /* p in RAM; bits only go 1 -> 0 */
{
    if (!flash_ok || off > STORE_LEN || n > STORE_LEN - off) return -1;
    return fl_write(STORE_AT + off, p, n);
}
#endif

/* ---- MIDI over USB: 4-byte USB-MIDI packets, cable 0 */
static int midi_get(uint8_t *st, uint8_t *d1, uint8_t *d2)   /* 1: a message (status, data 1, data 2) */
{
    uint32_t pk;
    if (mi_r == mi_w)
        return 0;
    pk = midi_in_q[mi_r % MQ];
    mi_r++;
    *st = (uint8_t)(pk >> 8);
    *d1 = (uint8_t)(pk >> 16);
    *d2 = (uint8_t)(pk >> 24);
    return 1;
}
static void midi_send(uint32_t st, uint32_t d1, uint32_t d2)   /* from the main loop only */
{
    midi_out_event((st >> 4) | st << 8 | (d1 & 0x7Fu) << 16 | (d2 & 0x7Fu) << 24);
}

/* ---- for a status page: a USB host is there (it also powers the FM-1: there is no charge-status line), and the
 * RAM / pool left past what the build placed (app.ld: RAM 96 KiB at 0x01C08000, POOL 0x54000 at 0x01C20000) */
static int usb_up(void) { return usb.config && !usb.suspended; }
static uint32_t mem_free(int pool)
{
    extern uint32_t _bss_end[], _pool_end[];
    return pool ? 0x01C20000u + 0x54000u - (uint32_t)_pool_end : 0x01C08000u + 0x18000u - (uint32_t)_bss_end;
}

/* ---- the timer: 10 kHz key scan, milliseconds, USB at 2 kHz. It outranks the audio ISR; nested in a
 * render it only scans, counts and feeds the USB audio stream (as sloopDX main.c) */
static volatile uint8_t in_audio;
void fm1_timer5_irq(void)
{
    static uint32_t sub, owed, last, acc;
    uint32_t t0 = fm1_ticks(), usb_due = sub % 5u == 0u;
    fm1_timer5_ack();
    fm1_input_tick();
    acc += t0 - last;
    last = t0;
    while (acc >= 1000u * FM1_TICKS_PER_US) {
        acc -= 1000u * FM1_TICKS_PER_US;
        fm1_ms++;
    }
    if (usb_due)
        owed = 1;
    if (++sub == 10u)
        sub = 0;
    if (in_audio) {
        if (usb_due)
            uac_service();
        return;
    }
    if (owed)
        usb_poll();
    owed = 0;
}
extern void isr_timer5(void);

/* ---- the sound: ALNK0 I2S, two halves; the app renders, the MASTER knob scales, Q15 -> 24 bit (-6 dBFS ceiling) */
static int32_t abuf[2u * HALF_WORDS] __attribute__((aligned(4)));
static volatile int32_t master_q12 = 2048;
static void app_audio(int32_t *out, uint32_t n);
static void app_main(void);

void fm1_alnk0_irq(void)                       /* via isr_alnk0 (hal/fm1_isr.S) */
{
    uint8_t p;
    in_audio = 1;
    p = fm1_audio_pending();
    fm1_audio_ack_aux(p);
    if (p & FM1_AUDIO_HALF) {
        int32_t *o = &abuf[fm1_audio_free_half() * HALF_WORDS];
        uint32_t i, b;
        for (i = 0; i < HALF_WORDS; i++)
            o[i] = 0;
        uac_render_start();
        app_audio(o, HALF_FRAMES);
        for (i = 0; i < HALF_WORDS; i++) {
            int32_t v = (o[i] * master_q12) >> 12;
            o[i] = v > 32767 ? 32767 : v < -32767 ? -32767 : v;
        }
        for (b = 0; b < HALF_FRAMES; b += 32u)
            uac_tap(o + 2u * b, 32u);           /* the computer records what the speaker plays */
        for (i = 0; i < HALF_WORDS; i++)
            o[i] <<= 7;
        fm1_audio_ack_half();
    }
    in_audio = 0;
}
extern void isr_alnk0(void);

/* ---- the main loop's duties: watchdog, USB, the installer's update request, the MASTER knob */
static void platform_poll(void)
{
    static int32_t knob_avg = 512 * 16;
    int32_t a;
    fm1_wdt_feed();
    usb_retry(fm1_ms);
    ota_service();
    if (usb.ota_req) {
        usb.ota_req = 0;
        if (flash_ok)
            ota_session();                      /* returns only if nothing was committed */
    }
    if (usb.uboot_req) {
        fm1_audio_stop();
        lcd_fill(0, 0, 240, 240, C_BLACK);
        draw_text_box(0, 110, 240, &FONT_S, "UBOOT (USB)", C_WHITE, 1);
        usb_detach();
        fm1_delay_ms(30);
        bootguard.pending = 0;
        fm1_enter_uboot();
    }
    if (fm1_ms >= 30000u && bootguard.pending)
        bootguard_clear(&bootguard);            /* 30 s up: this firmware boots */
    if ((a = fm1_adc_read(FM1_ADC_MASTER)) >= 0) {
        uint32_t k10;
        knob_avg += (a * 16 - knob_avg) / 8;
        k10 = (uint32_t)(knob_avg / 16);
        master_q12 = (int32_t)((k10 * k10) >> 8);
    }
}

static void fm1_fault(const fm1_crash_t *c)
{
    char b[12];
    uint32_t i, t0, v[3] = {c->vec, c->pc, c->rets};
    fm1_audio_stop();
    lcd_fill(0, 0, 240, 240, RGB(160, 0, 0));
    draw_text_box(0, 20, 240, &FONT_S, APP_NAME " CRASH", C_WHITE, 1);
    for (i = 0; i < 3u; i++) {
        uint32_t k;
        for (k = 0; k < 8u; k++) b[k] = "0123456789ABCDEF"[(v[i] >> (28u - 4u * k)) & 15u];
        b[8] = 0;
        draw_text_box(10, 60 + i * 20, 220, &FONT_S, b, C_WHITE, 0);
    }
    t0 = fm1_ticks();
    while ((uint32_t)(fm1_ticks() - t0) < 8000u * 1000u * FM1_TICKS_PER_US)
        fm1_wdt_feed();
    fm1_reboot();
}

void fm1_cstart(void)
{
    extern uint32_t _data_start[], _data_end[], _data_load[], _bss_start[], _bss_end[];
    extern uint32_t _pool_start[], _pool_end[], _rt_start[], _rt_end[], _rt_load[];
    uint32_t *s, *d, p3, boot_mode;
    fm1_time_init();
    fm1_reset_reason();
    p3 = fm1_boot.p3_rst;
    fm1_wdt_arm(0x0D);
    if ((p3 & 1u) && !(p3 & (4u | 0x40u)))
        bootguard_clear(&bootguard);
    boot_mode = bootguard_begin(&bootguard);
    if (boot_mode == BOOT_ROM)
        fm1_enter_uboot();
    fm1_irq_init();
    for (d = _bss_start; d < _bss_end; d++) *d = 0;
    for (d = _pool_start; d < _pool_end; d++) *d = 0;
    for (s = _data_load, d = _data_start; d < _data_end; s++, d++) *d = *s;
    for (s = _rt_load, d = _rt_start; d < _rt_end; s++, d++) *d = *s;
    fm1_mailbox_clear();
    fm1_guard_enable(FM1_GUARD_STACK | FM1_GUARD_WRITE | FM1_GUARD_BUS | FM1_GUARD_PC);
    fm1_boot.p3_rst = (uint8_t)p3;
    if (boot_mode == BOOT_RECOVERY)
        recovery_main();                        /* (OCT- at power-on is the boot stub's: update mode) */
    flash_ok = FL_JEDEC_OK(FL_FAR(fl_jedec_ram)());
    ota_boot_cleanup();
    lcd_init();
    lcd_fill(0, 0, 240, 240, C_BLACK);
    fm1_input_init();
    fm1_adc_init();
#if BOARD_SMK37
    smk_trim_read();                            /* (after fm1_input_init: it sets the defaults first) */
    smk_owner_check();
#endif
    fm1_audio_init(abuf, HALF_WORDS, isr_alnk0, 3);   /* (abuf is zero: .bss) */
    usb_start();
    fm1_timer5_start(isr_timer5, 4);
    fm1_guard_lock_top();
    fm1_irq_enable_all();
    app_main();
    for (;;)
        platform_poll();
}
#endif
#ifdef WASM_EXPORT
/* debug exports for tracker_emu.mjs (review C20): fmt_fix's text for (v, decimals); a store call with edge arguments
 * (op 0: write n <= 16 zero bytes at off, 1: erase the sector at off), its return code */
WASM_EXPORT("emu_fmt_fix") uint32_t emu_fmt_fix(int32_t v, int32_t d) { static char b[16]; fmt_fix(b, v, d); return (uint32_t)(uintptr_t)b; }
WASM_EXPORT("emu_store_try") int32_t emu_store_try(uint32_t op, uint32_t off, uint32_t n)
{
    static const uint8_t z[16];
    return op ? store_erase(off) : store_write(off, z, n > 16u ? 16u : n);
}
#endif
