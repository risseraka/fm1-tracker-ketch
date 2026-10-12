/* SPDX-License-Identifier: GPL-3.0-only */
/* The FX commands (docs/ux/commands.md; the prototype's model.js FX / ENGINE_CMD / SOUND_CMD / cmdTarget).
 *
 * A phrase row's FX cell is a command byte and a value byte:
 *   0          none
 *   1 .. 12    player commands: ARP CHA DEL HOP KIL OFF PIT RET SWG TPO VIB TV1 (the same on every instrument), and 17
 *   13 .. 16   ramp commands: REP RTO RTI RCU (written under the command they ramp)
 *   17         the player command SCN (the scene crossfader; after the ramps, so older songs keep their codes)
 *   18, 19     the player commands MUT, SOL (the track's mute / solo while playing; after SCN, for the same reason)
 *   20 .. 22   TV2 TV3 TV4: with TV1 (12, TVL's code until 2026-10-12), track 1-4's mix level from any track
 *   0x80 | id  a sound command: Felucca parameter id (P_LEVEL = VOL, P_PAN, P_ATK.., P_E0..P_E7) of the row's instrument,
 *              shown with the engine's own 3-letter name for E0..E7 (ANALOG E4 = CUT, DRUM E1 = TUN)
 * A sound command's value: a number spans its whole range over 00..FF (00 = min, 80 ~ middle, FF = max);
 * a list is its item number (00 = first item). Needs sound.c (Felucca's parameters, the instruments). */

enum { CMD_NONE, CMD_ARP, CMD_CHA, CMD_DEL, CMD_HOP, CMD_KIL, CMD_OFF, CMD_PIT, CMD_RET, CMD_SWG, CMD_TPO, CMD_VIB,
       CMD_TV1, CMD_REP, CMD_RTO, CMD_RTI, CMD_RCU, CMD_SCN, CMD_MUT, CMD_SOL, CMD_TV2, CMD_TV3, CMD_TV4, CMD_NPLAIN };
#define CMD_SOUND 0x80u
static const char CMD_NAME[CMD_NPLAIN][4] = {"---", "ARP", "CHA", "DEL", "HOP", "KIL", "OFF", "PIT", "RET", "SWG",
                                              "TPO", "VIB", "TV1", "REP", "RTO", "RTI", "RCU", "SCN", "MUT", "SOL",
                                              "TV2", "TV3", "TV4"};
/* the player commands in the grid's order (model.js FX), by the screen whose values they move (decisions.md "The FX
 * grid by screen"): GLO (G), MXR (X), PHR (the phrase); PLAY_GLO / PLAY_MXR / PLAY_PHR: where each group starts */
static const uint8_t PLAY_CMDS[18] = {CMD_TPO, CMD_SWG, CMD_SCN, CMD_TV1, CMD_TV2, CMD_TV3, CMD_TV4, CMD_MUT, CMD_SOL,
                                      CMD_ARP, CMD_DEL, CMD_KIL, CMD_OFF, CMD_PIT, CMD_RET, CMD_VIB, CMD_CHA, CMD_HOP};
#define PLAY_MXR 3u
#define PLAY_PHR 9u
/* TV1-TV4 (decisions.md "TV1-TV4"): the track a level command sets, else -1 (model.js tvTrack) */
static int tv_track(uint32_t c) { return c == CMD_TV1 ? 0 : c >= CMD_TV2 && c <= CMD_TV4 ? (int)(c - CMD_TV2) + 1 : -1; }
static int cmd_is_ramp(uint32_t c) { return c >= CMD_REP && c <= CMD_RCU; }
static const char *const RCU_NAMES[3] = {"LIN", "EXP", "LOG"};

/* E0..E7 per engine (commands.md section 5). A gap: the slot has no command and no place on the instrument page,
 * either unused by Felucca or removed for coherence (section 3, 4: LOFI SWP VIB ARP -> PIT VIB ARP; SAMPLE TUNE,
 * GRAIN PTCH -> the note and the transpose); firmware keeps the removed ones at 0 */
static const struct { const char *engine; const char *e[8]; } ENGINE_CMD[] = {
    {"ANALOG", {"WAV", "DTN", "MIX", "NOI", "CUT", "RES", "DRV", "KTR"}},
    {"FM6", {"ALG", "FB", "MLV", "MRT", "MEG", "VMD", "DTN", "SLT"}},
    {"PHASE", {"WAV", "WV2", "DCW", "ENV", "DTN", "LIN", "SUB", 0}},
    {"LOFI", {"CHP", "WAV", "DUT", "CRS", 0, 0, 0, "TON"}},
    {"SAMPLE", {"SET", 0, "BIT", "LOP", "CUT", 0, "DRV", 0}},
    {"VOICE", {"VOW", "VW2", "TLK", "SHF", "BUZ", "BRT", "Q", "RND"}},
    {"TRIO", {"WAV", "IN2", "IN3", "DTN", "MOD", "CUT", "RES", "PW"}},
    {"WHEEL", {"REG", "SUB", "BDY", "TOP", "PRC", "CLK", "DRV", "ROT"}},
    {"GRAIN", {"SRC", "POS", "SIZ", "DNS", 0, "SPR", "RND", "TON"}},
    {"PHYS", {"MDL", "STR", "BRI", "DMP", "POS", "ACC", "BOW", "EXC"}},
    {"NOISE", {"MOD", "COL", "FRQ", "RES", "TRK", "DNS", "DRF", "CRS"}},
    {"SLICE", {"SRC", "DIV", "STA", "PCH", "MOD", "RVS", "DCY", "TON"}},
    {"DRUM", {"KIT", "TUN", "TON", "DCY", "SNP", "ACC", "KCK", "DRV"}},
};
static const char *const *eng_row(uint32_t eng)   /* the engine's E0..E7 names, 0 = not in the table */
{
    uint32_t i;
    for (i = 0; i < NELEM(ENGINE_CMD); i++)
        if (str_eq(ENGINE_CMD[i].engine, ENGINES[eng]->name)) return ENGINE_CMD[i].e;
    return 0;
}
static const char *eng_slot_name(uint32_t eng, uint32_t k)
{
    const char *const *r = eng_row(eng);
    return r ? r[k & 7u] : 0;
}
#ifdef WASM_EXPORT
/* the emulator's check that every engine has a row (tracker_emu.mjs): the engines without one, a bit each */
WASM_EXPORT("emu_eng_rowless") uint32_t emu_eng_rowless(void)
{
    uint32_t e, m = 0;
    for (e = 0; e < NENGINES; e++) if (!eng_row(e) && !str_eq(ENGINES[e]->name, "-")) m |= 1u << e;
    return m;
}
#endif
static int eng_slot_used(uint32_t eng, uint32_t k)   /* Felucca has a parameter there (not "-") */
{
    const param_desc_t *d = &ENGINES[eng]->edit[k & 7u];
    return d->max != d->min;
}
/* the slots taken off the engines (section 3, 4): Felucca uses them, the tracker does not. An engine missing from
 * the table (a rename in a Felucca sync) keeps its values: zeroing them would wipe every saved instrument on load */
static int eng_slot_removed(uint32_t eng, uint32_t k) { return eng_slot_used(eng, k) && !eng_slot_name(eng, k); }
static void ins_force_removed(uint32_t i)
{
    uint32_t k;
    if (!eng_row(ins[i].eng)) return;
    for (k = 0; k < 8u; k++) if (eng_slot_removed(ins[i].eng, k)) ins[i].p[P_E0 + k] = 0;
}

/* the shared sound commands: every instrument-page parameter but ENGINE PRESET NAME, and VOL (LEVEL: cmd_list puts it
 * first on the ENG line, decisions.md "Levels") */
static const struct { uint8_t id; char name[4]; } SOUND_CMD[] = {
    {P_PAN, "PAN"},
    {P_ATK, "ATK"}, {P_DEC, "DEC"}, {P_SUS, "SUS"}, {P_REL, "REL"}, {P_ED_FLT, "EFL"}, {P_ED_PIT, "EPI"}, {P_ED_SHP, "ESH"},
    {P_LRATE, "LRT"}, {P_LWAVE, "LWV"}, {P_LPHASE, "LPH"}, {P_LFADE, "LFD"},
    {P_LD_PIT, "LPI"}, {P_LD_FLT, "LFL"}, {P_LD_SHP, "LSH"}, {P_LD_AMP, "LVO"},
    {P_VOICE, "VCE"}, {P_GLIDE, "GLD"}, {P_DETUNE, "VDT"}, {P_DIST, "SDS"}, {P_CHOR, "SCH"}, {P_DLY, "SDL"}, {P_REV, "SRV"},
};
#define SOUND_ENV 1u                             /* the groups of the list above: PAN | ENV | LFO | VOX */
#define SOUND_LFO 8u
#define SOUND_VOX 16u

/* the 3-letter name of a command for instrument i (EMPTY: no instrument: E0..E7 shown as "E0 ") */
static void cmd_name(uint32_t c, uint32_t i, char *s)
{
    uint32_t k;
    if (c < CMD_NPLAIN) { str_cpy(s, CMD_NAME[c], 4); return; }
    c &= 0x7Fu;
    if (c >= P_E0 && c <= P_E7) {
        const char *n = i < NINS ? eng_slot_name(ins[i].eng, c - P_E0) : 0;
        if (n) str_cpy(s, n, 4);
        else { s[0] = 'E'; s[1] = (char)('0' + c - P_E0); s[2] = ' '; s[3] = 0; }
        return;
    }
    for (k = 0; k < NELEM(SOUND_CMD); k++) if (SOUND_CMD[k].id == c) { str_cpy(s, SOUND_CMD[k].name, 4); return; }
    str_cpy(s, c == P_LEVEL ? "VOL" : "???", 4);  /* (VOL: the instrument's level, decisions.md "Levels") */
}

/* every command for a row playing instrument i, in the FX grid's order (model.js cmdGroups):
 * GLO, MXR, PHR, INS (VOL, the engine's named slots, then PAN), ENV, LFO, VOX, RAMP (they ramp a command of any line).
 * Returns the count; group starts in g[] */
#define CMD_MAX 54u
typedef char cmd_max_fits[CMD_NPLAIN - 1u + 1u + 8u + NELEM(SOUND_CMD) <= CMD_MAX ? 1 : -1];   /* cmd_list's most */
#define GRID_LINES 16u                           /* grid_lines' most: a group of n fills n / GRID_PER + 1 lines */
enum { G_GLO, G_MXR, G_PHR, G_INS, G_ENV, G_LFO, G_VOX, G_RAMP, G_N };   /* (G_INS .. G_VOX: the sound commands) */
static const char *const GROUP_TITLE[G_N] = {"GLO", "MXR", "PHR", "INS", "ENV", "LFO", "VOX", "RAMP"};
static uint32_t cmd_list(uint32_t i, uint8_t *out, uint8_t *g)
{
    uint32_t n = 0, k;
    if (g) { g[G_GLO] = 0; g[G_MXR] = PLAY_MXR; g[G_PHR] = PLAY_PHR; }
    for (k = 0; k < NELEM(PLAY_CMDS); k++) out[n++] = PLAY_CMDS[k];
    if (g) g[G_INS] = (uint8_t)n;
    out[n++] = (uint8_t)(CMD_SOUND | P_LEVEL);
    if (i < NINS)
        for (k = 0; k < 8u; k++)
            if (eng_slot_used(ins[i].eng, k) && eng_slot_name(ins[i].eng, k)) out[n++] = (uint8_t)(CMD_SOUND | (P_E0 + k));
    for (k = 0; k < NELEM(SOUND_CMD); k++) {
        if (g && k == SOUND_ENV) g[G_ENV] = (uint8_t)n;
        if (g && k == SOUND_LFO) g[G_LFO] = (uint8_t)n;
        if (g && k == SOUND_VOX) g[G_VOX] = (uint8_t)n;
        out[n++] = (uint8_t)(CMD_SOUND | SOUND_CMD[k].id);
    }
    if (g) g[G_RAMP] = (uint8_t)n;
    for (k = CMD_REP; k <= CMD_RCU; k++) out[n++] = (uint8_t)k;
    return n;
}
static int32_t cmd_index(uint32_t c, uint32_t i)    /* c's place in instrument i's list, -1 = not there */
{
    uint8_t l[CMD_MAX];
    uint32_t n = cmd_list(i, l, 0), k;
    for (k = 0; k < n; k++) if (l[k] == c) return (int32_t)k;
    return -1;
}
static uint32_t cmd_by_name(const char *name, uint32_t i)   /* instrument i's command of that name, 0 = none */
{
    uint8_t l[CMD_MAX];
    uint32_t n = cmd_list(i, l, 0), k;
    char s[4];
    for (k = 0; k < n; k++) { cmd_name(l[k], i, s); if (str_eq(s, name)) return l[k]; }
    return 0;
}

/* a sound command's parameter for instrument i: its descriptor, and how many items if it is a list */
static uint32_t desc_items(const param_desc_t *d) { return d->fmt == F_ENUM || d->fmt == F_ONOFF ? (uint32_t)(d->max - d->min + 1) : 0u; }
static const param_desc_t *cmd_desc(uint32_t c, uint32_t i, uint32_t *items)
{
    const param_desc_t *d;
    if (c < CMD_SOUND || i >= NINS) return 0;
    d = ins_desc(i, c & 0x7Fu);
    *items = desc_items(d);
    return d;
}
/* the parameter value a command value sets. The audio ISR calls it (a strike, a ramp's tick, the preview), so it
 * reads the engine's own descriptor, never ins_desc's shared scratch track: an ISR writing that track between the
 * main loop's scratch_from and its use put another instrument's engine into the one being edited (code-review.md
 * S1, S1b). The engines' mode-dependent descriptors (desc hooks) change only the names: the same range, no list */
static int32_t cmd_target(uint32_t c, uint32_t i, uint32_t v)
{
    uint32_t items;
    const param_desc_t *d;
    if (c < CMD_SOUND || i >= NINS) return 0;
    d = param_desc_of(eng_idx(ins[i].eng), c & 0x7Fu);
    items = desc_items(d);
    if (items) return d->min + (int32_t)(v < items - 1u ? v : items - 1u);
    return d->min + ((int32_t)v * (d->max - d->min) + 127) / 255;
}
/* the lowest v of 00..FF with cmd_target's min + (v * range + 127) / 255 >= min + up (FF if none): the inverse */
static uint32_t cmd_encode_num(int32_t up, int32_t range)
{
    int32_t v;
    if (up <= 0) return 0;
    if (range <= 0) return 255u;
    v = (255 * up - 127 + range - 1) / range;
    return v > 255 ? 255u : (uint32_t)v;
}
static uint32_t cmd_encode_raw(uint32_t c, uint32_t i, int32_t raw)   /* the lowest value that sets raw (model.js cmdEncode) */
{
    uint32_t items;
    const param_desc_t *d = cmd_desc(c, i, &items);
    if (!d) return 0;
    if (items) return (uint32_t)(raw - d->min);
    return cmd_encode_num(raw - d->min, d->max - d->min);
}
static uint32_t cmd_encode(uint32_t c, uint32_t i) { return cmd_encode_raw(c, i, ins[i].p[c & 0x7Fu]); }   /* instrument i's setting */
#ifdef WASM_EXPORT
/* the emulator's check of cmd_encode_num (tracker_emu.mjs): against the search it replaced (v from 0 up while
 * cmd_target < the setting; v only grows with the setting, so it carries on), for every range up to n and every
 * setting in it and one past each end. Returns the mismatches */
WASM_EXPORT("emu_encode_check") uint32_t emu_encode_check(int32_t n)
{
    int32_t r, up, v, bad = 0;
    for (r = 0; r <= n; r++)
        for (up = -1, v = 0; up <= r + 1; up++) {
            while (v < 255 && (v * r + 127) / 255 < up) v++;
            bad += cmd_encode_num(up, r) != (uint32_t)v;
        }
    return (uint32_t)bad;
}
#endif
static uint32_t cmd_value_max(uint32_t c, uint32_t i)   /* a list: its last item number; else FF */
{
    uint32_t items;
    if (c == CMD_RCU) return NELEM(RCU_NAMES) - 1u;
    return cmd_desc(c, i, &items) && items ? items - 1u : 255u;
}
