/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 port: Copyright (C) 2026 M. Geisler (genkigenki/makkuro) */
/* Ketch's flash data on the SMK-37 (BOARD_SMK37; platform.c includes this instead of its FM-1 lines).
 * On the FM-1 the store is 0x97000-0xDFFFF and the settings sector 0xFC000: on the SMK that is the app area, the
 * calibration and the protected USR area, so the data moves into the stock VM (FL_DATA) and USRFLASH (FL_SMP):
 *   0x9D000-0xAEFFF  store sectors 0..17      (project slot 1, slot 2's first copy begins)
 *   0xAF000          owner tag "SMK37DATA" + KETCH_DATA_FAMILY: another firmware's data here is cleared, never parsed
 *   0xB0000          settings (the FM-1's 0xFC000 sector, the same format)
 *   0xB1000-0xBAFFF  store sectors 18..27     (project slot 2)
 *   0xC2000-0xDDFFF  store sectors 28..55     (project slots 3 and 4, USRFLASH: the stock firmware's mapping until
 *                    a project is first saved there; its end marker is released first, platform.c)
 * The store keeps its FM-1 layout (4 slots, 2 copies of 7 sectors each, docs/API.md "Store"): 56 sectors, its
 * offsets mapped sector by sector (smk_store_flash). Calibration (0xC1000), BT pairing, USR and key_mac are never in
 * reach: FL_RANGE_OK below is the app's allow-list, and the RAM flash driver also refuses every protected range on
 * its own (FL_GUARD, fm1_flash_map.h). Macros, compile-time checks and pure functions, so host tests include it. */
#pragma once
#include <stdint.h>

#define SMK_STORE_SECTORS 56u                   /* 4 slots x 2 copies x 7 sectors (tracker.c PJ_SLOTS, PJ_SECT) */
#define SMK_RUN1_AT 0x0009D000u                 /* store sectors 0..17 */
#define SMK_RUN1_N 18u
#define OWNER_AT 0x000AF000u                    /* the store's owner tag (the kit's place for it) */
#define SETTINGS_AT 0x000B0000u                 /* one 4 KiB sector */
#define SMK_RUN2_AT 0x000B1000u                 /* store sectors 18..27 */
#define SMK_RUN2_N 10u
#define SMK_RUN3_AT 0x000C2000u                 /* store sectors 28..55, in USRFLASH */
#define SMK_RUN3_N 28u
#ifndef KETCH_DATA_FAMILY
#define KETCH_DATA_FAMILY "KETCH"               /* (tools/build.py sets it: SMK_DATA_FAMILY) */
#endif
#define SMK_IN_RUN(off, n, at, cnt) FL_IN(off, n, at, (at) + (cnt) * 0x1000u)
#define FL_RANGE_OK(off, n) (FL_IN(off, n, FL_OTA_LO, FL_OTA_HI) || FL_IN(off, n, SETTINGS_AT, SETTINGS_AT + 0x1000u) || \
                             FL_IN(off, n, OWNER_AT, OWNER_AT + 0x1000u) || SMK_IN_RUN(off, n, SMK_RUN1_AT, SMK_RUN1_N) || \
                             SMK_IN_RUN(off, n, SMK_RUN2_AT, SMK_RUN2_N) || SMK_IN_RUN(off, n, SMK_RUN3_AT, SMK_RUN3_N) || \
                             FL_STOCKTAG_OK(off, n))
#include "fm1_flash_map.h"

#if !BOARD_SMK37
#error "smk_store.h is the SMK-37 layout (BOARD_SMK37)"
#endif
_Static_assert(SMK_RUN1_N + SMK_RUN2_N + SMK_RUN3_N == SMK_STORE_SECTORS, "SMK store: 56 sectors");
_Static_assert(SMK_RUN1_AT == FL_DATA_LO && SMK_RUN1_AT + SMK_RUN1_N * 0x1000u == OWNER_AT &&
               OWNER_AT + 0x1000u == SETTINGS_AT && SETTINGS_AT + 0x1000u == SMK_RUN2_AT &&
               SMK_RUN2_AT + SMK_RUN2_N * 0x1000u <= FL_DATA_HI, "SMK store: the FL_DATA part");
_Static_assert(SMK_RUN3_AT == FL_SMP_LO && SMK_RUN3_AT + SMK_RUN3_N * 0x1000u <= FL_SMP_HI, "SMK store: the USRFLASH part");
_Static_assert(FL_WIN_SAFE(SMK_RUN1_AT, SMK_RUN2_AT + SMK_RUN2_N * 0x1000u), "SMK store: overlaps a protected range");
_Static_assert(FL_WIN_SAFE(SMK_RUN3_AT, SMK_RUN3_AT + SMK_RUN3_N * 0x1000u), "SMK store: overlaps a protected range");
_Static_assert(sizeof KETCH_DATA_FAMILY - 1u >= 1u && sizeof KETCH_DATA_FAMILY - 1u <= 7u, "SMK store: family 1..7 characters");

/* the flash offset of store offset off (< SMK_STORE_SECTORS * 4 KiB): sector by sector over the three runs */
static uint32_t smk_store_flash(uint32_t off)
{
    uint32_t s = off >> 12, in = off & 0xFFFu;
    if (s < SMK_RUN1_N)
        return SMK_RUN1_AT + (s << 12) + in;
    s -= SMK_RUN1_N;
    if (s < SMK_RUN2_N)
        return SMK_RUN2_AT + (s << 12) + in;
    return SMK_RUN3_AT + ((s - SMK_RUN2_N) << 12) + in;
}

/* the 16-byte owner tag: "SMK37DATA" + the family, zero padded (a 7-letter family fills it, no NUL) */
static void smk_owner_tag(uint8_t tag[16])
{
    static const char FAM[] = KETCH_DATA_FAMILY;
    uint32_t i;
    for (i = 0; i < 16u; i++)
        tag[i] = i < 9u ? (uint8_t)"SMK37DATA"[i] : i - 9u < sizeof FAM - 1u ? (uint8_t)FAM[i - 9u] : 0u;
}
