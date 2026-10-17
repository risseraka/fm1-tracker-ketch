/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 port: Copyright (C) 2026 M. Geisler (genkigenki/makkuro) */
/* The SMK-37 flash map on the host (hal/fm1_flash_map.h, src/smk_store.h): every 4 KiB sector and every page of the
 * 1 MiB part against the app's and the update loader's allow-lists and the RAM driver's guard.
 *   cc -DBOARD_SMK37=1 -Ifirmware/hal -Ifirmware/src -o smk_flash_test tests/smk_flash_test.c && ./smk_flash_test
 * Never allowed: the head 0x0-0x3FFF, BTIF / USRTRIM 0xC0000-0xC1FFF, USR 0xF4000-0xFDFFF, key_mac 0xFF000-. The app
 * may write exactly: the store's 56 sectors, the owner tag, the settings, the update staging, the stock marker's
 * sector (whole, as one erase); the loader the app area and the VM below BTIF. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "smk_store.h"

static int bad;
static void check(const char *what, int ok) { printf("%-72s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }
static int protected_sector(uint32_t s) { return s < 0x4000u || (s >= 0xC0000u && s < 0xC2000u) ||
                                                 (s >= 0xF4000u && s < 0xFE000u) || s >= 0xFF000u; }

int main(void)
{
    uint32_t s, i, ok = 1, app = 0, prot = 0, ldr = 0, distinct = 1, inside = 1, wrap = 1;
    for (s = 0; s < 0x100000u; s += 0x1000u) {
        int want = 0, a = FL_GUARD(s, 0x1000u), l = FL_LDR_RANGE_OK(s, 0x1000u) && !FL_PROTECTED(s, 0x1000u);
        for (i = 0; i < SMK_STORE_SECTORS; i++) want |= smk_store_flash(i << 12) == s;
        want |= s == OWNER_AT || s == SETTINGS_AT || s == FL_STOCKTAG_LO || (s >= FL_OTA_LO && s < FL_OTA_HI);
        ok &= a == want;
        app += a;
        ldr += l;
        if (protected_sector(s)) {
            prot += a || l || FL_GUARD(s, 256u) || FL_GUARD(s + 0xF00u, 256u);
            prot += !FL_PROTECTED(s, 1u);
        }
        if (s == FL_STOCKTAG_LO)
            check("the stock marker's sector: only as one whole-sector erase",
                  a && !FL_GUARD(s, 256u) && !FL_GUARD(s + 0xF00u, 256u) && !FL_GUARD(s, 0x2000u));
    }
    check("the app's allow-list: exactly the store, owner tag, settings, staging, the stock marker", ok && app == 56u + 2u + 5u + 1u);
    check("no protected sector is ever allowed (app, loader, any page)", prot == 0);
    check("the loader: the app area and the VM below BTIF only", ldr == (0xC0000u - 0x4000u) / 0x1000u);
    for (i = 0; i < SMK_STORE_SECTORS; i++) {
        uint32_t f = smk_store_flash(i << 12), j;
        inside &= (f >= FL_DATA_LO && f < FL_DATA_HI) || (f >= FL_SMP_LO && f < FL_SMP_HI);
        for (j = 0; j < i; j++) distinct &= smk_store_flash(j << 12) != f;
        distinct &= f != OWNER_AT && f != SETTINGS_AT;
        inside &= smk_store_flash((i << 12) | 0xABCu) == f + 0xABCu;
    }
    check("the store: 56 distinct sectors in the VM and USRFLASH, beside the owner tag and the settings", inside && distinct);
    check("projects 1 and 2 (store sectors 0..27) in the VM, 3 and 4 in USRFLASH",
          smk_store_flash(27u << 12) < FL_DATA_HI && smk_store_flash(28u << 12) == FL_SMP_LO);
    wrap &= !FL_GUARD(0xFFFFF000u, 0x2000u) && !FL_GUARD(FL_OTA_LO, 0xFFFFFFFFu) && !FL_GUARD(0xBF000u, 0x2000u);
    wrap &= !FL_GUARD(SMK_RUN3_AT - 0x100u, 0x200u) && !FL_GUARD(0xBFF00u, 0x200u);
    check("ranges that wrap or run over a window's end into protected ranges: refused", wrap);
    {
        uint8_t t[16];
        smk_owner_tag(t);
        check("owner tag: SMK37DATA + KETCH, zero padded", !memcmp(t, "SMK37DATAKETCH\0\0", 16));
    }
    printf("%s\n", bad ? "SMK FLASH TEST FAILED" : "smk flash test passed");
    return bad != 0;
}
