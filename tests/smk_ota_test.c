/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 port: Copyright (C) 2026 M. Geisler (genkigenki/makkuro) */
/* The update entry (firmware/src/ota.c) of the SMK-37 build, on the host: a fake host serves a package over the
 * SysEx protocol, the "device" flash holds the SMK package's flash.bin.
 *   cc -DBOARD_SMK37=1 [-DSMK_MODEL='"SMK-37 Elite"'] -o smk_ota_test tests/smk_ota_test.c
 *   smk_ota_test SMK.fwsc FM1.fwsc        (build/ketch-smk37*.fwsc and an FM-1 build's package)
 * Checks: an FM-1 package (flash.bin 0x93000) is refused before anything is erased (-15); our SMK package is staged
 * at 0xBB000 and the record carries the model's ota-<model>_016; the board check of an official loader's package:
 * the region table must be this keyboard's (-16) and a USR image must go to 0xF4000 / 0xA000 (-17). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !BOARD_SMK37
#error "the SMK-37 build's update entry: -DBOARD_SMK37=1"
#endif
#define FELUCCA_ID "SMK-37 Pro_950"
#define FELUCCA_OTA_DRYRUN 0
#include "../firmware/hal/fm1_cc.h"

static uint8_t nor[0x100000], *logical, rx[1024], committed[112];
static size_t logical_len;
static uint32_t rx_len, rx_full, now_ms, erases, progs, bad_range, commits;

static uint32_t pack7(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 8;
        while (nb >= 7) { out[o++] = acc & 0x7F; acc >>= 7; nb -= 7; }
    }
    if (nb) out[o++] = acc & 0x7F;
    return o;
}
static uint32_t unpack7h(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 7;
        if (nb >= 8) { out[o++] = (uint8_t)acc; acc >>= 8; nb -= 8; }
    }
    return o;
}
static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    uint8_t u[64], m[600];
    uint32_t addr, len, i, s = 0;
    unpack7h(p + 1, n - 2, u);
    if (u[2] == 0x11) return 0;
    addr = u[7] | u[8] << 8 | u[9] << 16 | (uint32_t)u[10] << 24;
    len = u[11] | u[12] << 8 | u[13] << 16;
    memcpy(m, "\x00\x59\x30", 3);
    m[3] = (uint8_t)(len + 8); m[4] = (uint8_t)((len + 8) >> 8); m[5] = 0; m[6] = 0;
    memcpy(m + 7, &addr, 4);
    m[11] = (uint8_t)len; m[12] = (uint8_t)(len >> 8); m[13] = 0;
    if (addr >= 0xE0000000u) { memset(m + 14, 0, len); memcpy(m + 14, "success", 8); }
    else if (addr + len > logical_len) { printf("read past the package %#x\n", addr); exit(1); }
    else memcpy(m + 14, logical + addr, len);
    for (i = 6; i < 14 + len; i++) s += m[i];
    m[14 + len] = (uint8_t)~s;
    rx_len = pack7(m, 15 + len, rx);
    rx_full = 1;
    return 0;
}
static int ota_frame_get(const uint8_t **p, uint32_t *n) { if (!rx_full) return 0; *p = rx; *n = rx_len; return 1; }
static void ota_frame_done(void) { rx_full = 0; }
static uint32_t ota_now_ms(void) { return now_ms; }
static void ota_idle(void) { now_ms++; }
static int ota_erase(uint32_t off)              /* as platform.c's: the staging area only */
{
    if (off < 0xBB000u || off >= 0xC0000u || (off & 0xFFFu)) { bad_range++; return -8; }
    erases++;
    memset(nor + off, 0xFF, 0x1000);
    return 0;
}
static int ota_prog(uint32_t off, const void *p, uint32_t n)
{
    const uint8_t *s = p;
    uint32_t i;
    if (off < 0xBB000u || off + n > 0xC0000u) { bad_range++; return -8; }
    progs++;
    for (i = 0; i < n; i++) nor[off + i] &= s[i];
    return 0;
}
static int ota_fread(uint32_t off, void *p, uint32_t n) { memcpy(p, nor + off, n); return 0; }
static void ota_show(uint32_t step, int32_t code) { (void)step; (void)code; }
static void ota_commit(const uint8_t *parm) { memcpy(committed, parm, sizeof committed); commits++; }
#include "../firmware/src/ota.c"

static uint8_t *load_logical(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    uint8_t *raw = malloc(0x200000), *lg;
    size_t n, i;
    if (!f) { perror(path); exit(2); }
    n = fread(raw, 1, 0x200000, f);
    fclose(f);
    lg = malloc(n);
    for (i = 0; i < 20; i++) memcpy(lg + i * 47, raw + i * 48, 47);
    memcpy(lg + 20 * 47, raw + 20 * 48, n - 20 * 48);
    *len = n - 20;
    free(raw);
    return lg;
}
static int check(const char *what, int ok) { printf("%-72s %s\n", what, ok ? "ok" : "FAIL"); return ok ? 0 : 1; }

int main(int argc, char **argv)
{
    static uint8_t hdr[0x400];
    uint8_t *smk, *fm1;
    size_t slen, flen;
    uint32_t fo, fl, oo, ol, fc;
    int bad = 0, rc;
    if (argc < 3) { printf("usage: smk_ota_test SMK.fwsc FM1.fwsc\n"); return 2; }
    smk = load_logical(argv[1], &slen);
    fm1 = load_logical(argv[2], &flen);
    logical = smk; logical_len = slen;
    if (ota_ufw(hdr, &fo, &fl, &oo, &ol, &fc)) { printf("not a package: %s\n", argv[1]); return 2; }
    memset(nor, 0xFF, sizeof nor);
    memcpy(nor, smk + fo, fl);                  /* the device runs this package */
    bad += check("our SMK package: flash.bin 0x9D000 (the SMK app area)", fl == 0x9D000u);

    logical = fm1; logical_len = flen;
    rc = ota_session();
    bad += check("an FM-1 package on the SMK build: refused (-15) before anything is erased",
                 rc == -15 && erases == 0 && progs == 0 && commits == 0);

    logical = smk; logical_len = slen;
    rc = ota_session();
    bad += check("our SMK package: staged at 0xBB000, committed", rc == 0 && commits == 1 && erases == 5 && bad_range == 0 &&
                 !memcmp(nor + 0xBB000u + 16u, "LOADER.BIN", 10));
    bad += check("the record: ota-" SMK_MODEL "_016, the staging at 0xBB000",
                 !memcmp(committed + 8, "ota-" SMK_MODEL "_016", sizeof("ota-" SMK_MODEL "_016") - 1u) &&
                 committed[8 + sizeof("ota-" SMK_MODEL "_016") - 1u] == 0 && ota_rd32(committed + 72) == 0xBB000u);
    bad += check("the record's flash copy at 0xBFF00", !memcmp(nor + 0xBFF00u, committed, sizeof committed));

    rx_full = 0;
    ota_ufw(hdr, &fo, &fl, &oo, &ol, &fc);
    bad += check("board check, an official loader's package for this keyboard: passes", ota_board_check(hdr, fo, fl, 1) == 0);
    nor[OTA_RT_OFF + 7] ^= 0x40u;
    bad += check(".. another region table (another board's layout): -16", ota_board_check(hdr, fo, fl, 1) == -16);
    nor[OTA_RT_OFF + 7] ^= 0x40u;
    {
        uint8_t *e = hdr + 0x40 + 1u * 0x50u;   /* ota.bin's entry posing as a USR image (type 50) */
        e[0] = 50; e[1] = 0;
        ota_wr32(e + 0x1C, 0xEA000u); ota_wr32(e + 0x20, 0x12000u);   /* the FM-1's USR place */
        bad += check(".. a USR image for the FM-1's place: -17", ota_board_check(hdr, fo, fl, 1) == -17);
        ota_wr32(e + 0x1C, 0xF4000u); ota_wr32(e + 0x20, 0xA000u);
        bad += check(".. a USR image for 0xF4000 / 0xA000: passes", ota_board_check(hdr, fo, fl, 1) == 0);
    }
    bad += check(".. our own loader: the size alone", ota_board_check(hdr, fo, 0x93000u, 0) == -15 &&
                 ota_board_check(hdr, fo, fl, 0) == 0);
    printf("%s\n", bad ? "SMK OTA TEST FAILED" : "smk ota test passed");
    return bad != 0;
}
