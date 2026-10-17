/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 port: Copyright (C) 2026 M. Geisler (genkigenki/makkuro) */
/* Ketch on the SMK-37 (BOARD_SMK37; tracker.c includes this before app_main, SMK37.md has the table).
 *
 *   pads       the gamepad, as the M8 lays it out: the arrows left (UP over LEFT DOWN RIGHT), OPTION EDIT over
 *              SHIFT PLAY right; OFF in the bottom row writes a note off in the notes modes (the FM-1's F#3).
 *              The FM-1's gamepad keys (F3 G3 G#3 A3, D5 E5 F5 G5) are notes here like every other key
 *   keybed     all 37 keys, C3 .. C6 (OCT- / OCT+ shift it), with velocity: in the notes modes (REC) they play
 *              the cursor's channel and write the note with its velocity (step entry, live recording); with SEL
 *              (SCALE) held, B3 .. B4 pick the scenes as on the FM-1
 *   faders     the mixer's levels of channels 1-4 (X LEVEL); a fader takes over once it moves
 *   wheels     pitch bend (+-2 semitones) and the mod wheel (the instrument's MODW source) on the cursor's channel
 *   K1-K4      KNOB 1-4; K5 MASTER; K6 SELECT (the crossfader with SEL held); K7, K8 and KNOB BANK, FADER BANK,
 *              < > do nothing (as ALGORITHM and PRESETS on the FM-1)
 *   LEDs       the pads in three colours: off dark orange, on light blue, active bright (held; PLAY while playing); the
 *              buttons lit for a state (GLO on G, HOME following, REC notes / armed / recording, PLAY playing, SAVE
 *              unsaved, OCT- / OCT+ off octave 4, SEL held) */

enum { SP_UP = 1, SP_OPTION = 6, SP_EDIT = 7, SP_LEFT = 8, SP_DOWN = 9, SP_RIGHT = 10, SP_OFF = 12, SP_SHIFT = 14,
       SP_PLAY = 15 };                         /* hardware pads: 0..7 the top row, 8..15 the bottom row */
#define SP_NONE 0xFFu
static const uint8_t SMK_PAD_KEY[16] = {        /* hardware pad -> the gamepad key (PAD_*) it is */
    SP_NONE, PAD_UP, SP_NONE, SP_NONE, SP_NONE, SP_NONE, PAD_OPTION, PAD_EDIT,
    PAD_LEFT, PAD_DOWN, PAD_RIGHT, SP_NONE, SP_NONE, SP_NONE, PAD_SHIFT, PAD_PLAY};
static const int8_t SMK_PAD_OWN[16] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};   /* every pad
                                                 * is the app's (hal/smk_input.h smk_pad_map: hits through smk_pad_take) */
static uint16_t smk_pads, smk_pads_up;          /* the pads held; releases kept for the next pass (a tap within a pass) */
static uint8_t smk_off_hit;                     /* OFF hit (written in smk_notes) */
static uint32_t smk_note_prev;                  /* the middle keys held, as fm1_in.notes (F3 .. G5) */
static uint8_t smk_slot_key[NKEYS];             /* the keybed key (+1) each key_want slot sounds */
static uint32_t smk_rr;                         /* the next slot to try */
static uint32_t smk_pad_of(uint32_t k)          /* the pad of gamepad key k (PAD_*), 16 if none */
{
    uint32_t p;
    for (p = 0; p < 16u && SMK_PAD_KEY[p] != k; p++) {}
    return p;
}

/* ---- the faders: the mixer's levels. A fader takes over once it moves 2 steps (so a resting fader's jitter never
 * moves a level), then follows while it moves (a pause of 0.5 s: 2 steps again) */
static uint32_t smk_faders(void)
{
    static uint8_t last[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    static uint32_t moved[4];
    uint32_t i, d = 0;
    for (i = 0; i < 4u && i < NCH; i++) {
        uint32_t v = fm1_in.fader[i];
        int32_t step = (int32_t)v - (int32_t)last[i];
        if (last[i] == 0xFFu || fm1_in.frames < 200u) {   /* (the first readings settle) */
            last[i] = (uint8_t)v;
            continue;
        }
        if (!step || ((step < 2 && step > -2) && (!moved[i] || fm1_ms - moved[i] >= 500u)))
            continue;
        last[i] = (uint8_t)v;
        moved[i] = fm1_ms;
        if (kb != KB_REC) undo_mark(1u << 31 | V_MIX << 24 | 0x81u << 8 | i, 0);   /* (one move: one step) */
        mix_set(i, 0, (int32_t)v);
        d = DIRTY_ALL;
    }
    return d;
}

/* ---- the wheels (mux C address 4 = pitch, 5 = mod), self-calibrating: the pitch wheel's rest is its centre (taken at
 * power-on, followed slowly while it rests), each wheel's range grows to what it has reached (the travel is short).
 * Pitch: +-2 semitones with a dead band around the centre; mod: 0..127, up (lower raw) = more. Both on the cursor's
 * channel (the keys play there); a channel left gets its bend and mod wheel back to 0 */
static void smk_wheels(void)
{
    enum { DEAD = 12, SPAN0 = 80, MOD_LO = 485, MOD_HI = 615 };
    static int32_t pc, plo, phi, mlo, mhi, rest;
    static uint32_t ch_was;
    int32_t p = fm1_in.wheel[0], m = fm1_in.wheel[1], d, b, mw;
    if (fm1_in.frames < 300u)                   /* (the readings settle) */
        return;
    if (!pc) {
        pc = p; plo = p - SPAN0; phi = p + SPAN0;
        mlo = m < MOD_LO ? m : MOD_LO;
        mhi = m > MOD_HI ? m : MOD_HI;
    }
    if (p < plo) plo = p;
    if (p > phi) phi = p;
    if (m < mlo) mlo = m;
    if (m > mhi) mhi = m;
    d = p - pc;
    if (d > -DEAD && d < DEAD) {
        b = 0;
        if (++rest > 200) {                     /* resting: the centre follows a slow drift */
            pc += d > 0 ? 1 : d < 0 ? -1 : 0;
            rest = 0;
        }
    } else {
        rest = 0;
        b = d > 0 ? (d - DEAD) * 512 / (phi - pc - DEAD > 1 ? phi - pc - DEAD : 1)
                  : (d + DEAD) * 512 / (pc - plo - DEAD > 1 ? pc - plo - DEAD : 1);
        b = clamp_i(b, -512, 512);              /* Q8 semitones: +-2 */
    }
    mw = mhi - m <= DEAD ? 0 : (mhi - m - DEAD) * 127 / (mhi - mlo - DEAD > 1 ? mhi - mlo - DEAD : 1);
    if (ui.ch != ch_was) {
        midi_bend_target[ch_was] = 0;
        trk[ch_was].mw = 0;
        ch_was = ui.ch;
    }
    midi_bend_target[ui.ch] = b;
    trk[ui.ch].mw = (uint8_t)clamp_i(mw, 0, 127);
}

/* ---- the pads and the rest, once a pass (app_main, instead of fm1_in.notes): the gamepad bits of the pads held, the
 * keybed's FM-1 keys at the other bits (scenes, edges), the faders, the wheels */
static uint32_t smk_poll(uint32_t *dirty)
{
    uint32_t k, g = 0, hit = 0;
    int32_t e;
    smk_pad_map = SMK_PAD_OWN;
    smk_pads &= (uint16_t)~smk_pads_up;
    smk_pads_up = 0;
    while ((e = smk_pad_take()) >= 0) {
        uint32_t p = ((uint32_t)e >> 8) & 15u;
        if (e & 0xFF) {
            smk_pads |= (uint16_t)(1u << p);
            hit |= 1u << p;
            if (p == SP_OFF) smk_off_hit = 1;
        } else if (hit >> p & 1u)
            smk_pads_up |= (uint16_t)(1u << p);   /* (hit and let go within this pass: held for it) */
        else
            smk_pads &= (uint16_t)~(1u << p);
    }
    for (k = 0; k < 16u; k++)
        if (smk_pads >> k & 1u && SMK_PAD_KEY[k] != SP_NONE)
            g |= 1u << SMK_PAD_KEY[k];
    *dirty |= smk_faders();
    smk_wheels();
    return (fm1_in.notes & ~PAD_KEYS) | g;
}

/* ---- the keys as notes (tracker.c's notes mode, app.js keyDown): written as on the FM-1, with the key's velocity */
static void smk_note_write(uint32_t note, uint32_t vel, uint32_t *dirty)
{
    smk_vel = (uint8_t)vel;
    if (kb == KB_REC) rec_write(note);
    else if (kb == KB_NOTES && !playing && ui.view == V_PHRASE && !ui.on_row && !ui.on_head && ensure_phrase() != EMPTY) {
        row_t *w = &cur_phrase_rows()[ui.row];
        if (note != NOTE_OFF) w->vol = (uint8_t)vel;
        w->n = (uint8_t)note;
        if (w->i == EMPTY && note != NOTE_OFF) w->i = last.inst;
        if (note != NOTE_OFF) { last.note = (uint8_t)note; last.inst = w->i; last.ni = NONE; }
        ui.row = (ui.row + S.pj.step) % 16u;    /* STEP (G page): 0 stays */
        *dirty |= DIRTY_ALL;
    }
}
static void smk_key_on(uint32_t key, uint32_t vel, uint32_t *dirty)   /* keybed key 0 (C3) .. 36 (C6) */
{
    uint32_t note = 48u + key + (uint32_t)(12 * oct), s = 0, i;
    if (note < NOTE_MIN || note > NOTE_MAX) return;
    smk_note_write(note, vel, dirty);
    for (i = 0; i < NKEYS; i++) {               /* a free slot, the next after the last taken (the ISR never sees one
                                                 * reused within a block) */
        s = (smk_rr + i) % NKEYS;
        if (!(key_want >> s & 1u)) break;
    }
    if (i == NKEYS) return;
    smk_rr = s + 1u;
    key_note[s] = (uint8_t)note;
    key_ins[s] = (uint8_t)(IS_PAGE(ui.view) ? ui.inst : last.inst);
    key_ch[s] = (uint8_t)ui.ch;
    key_vel[s] = (uint8_t)vel;
    smk_slot_key[s] = (uint8_t)(key + 1u);
    key_want |= 1u << s;
}
static void smk_key_off(uint32_t key)
{
    uint32_t s;
    for (s = 0; s < NKEYS; s++)
        if (smk_slot_key[s] == key + 1u) { smk_slot_key[s] = 0; key_want &= ~(1u << s); }
}
static void smk_notes(uint32_t *dirty)          /* every pass; plays in the notes modes only (kb) */
{
    uint32_t now = fm1_in.notes, dn = now & ~smk_note_prev, up = smk_note_prev & ~now, k;
    int32_t e;
    smk_note_prev = now;
    for (k = 0; k < NKEYS; k++)
        if (!(key_want >> k & 1u)) smk_slot_key[k] = 0;   /* (key_want cleared by REC) */
    if (sel_held)
        for (k = 0; k < NSCENE; k++) dn &= ~(1u << SCENE_KEYS[k]);   /* (scene picks, not notes) */
    for (k = 0; k < 27u; k++) {
        if (up >> k & 1u) smk_key_off(k + SMK_KEY_F3);
        if (kb && dn >> k & 1u) smk_key_on(k + SMK_KEY_F3, fm1_in.key_vel[k + SMK_KEY_F3], dirty);
    }
    while ((e = smk_extkey_take()) >= 0) {      /* the outer keys C3 .. E3, G#5 .. C6 */
        uint32_t key = (uint32_t)e >> 8, v = (uint32_t)e & 0xFFu;
        if (!v) smk_key_off(key);
        else if (kb) smk_key_on(key, v, dirty);
    }
    if (smk_off_hit && kb) smk_note_write(NOTE_OFF, 0, dirty);
    smk_off_hit = 0;
}

/* ---- the LEDs (tracker.c leds_update on the FM-1): written when they change */
/* three states told apart by colour, not only by level: off dark orange, on light blue, active bright white-blue */
#define SMK_OFF 0x6E2600u
#define SMK_ON 0x2878D2u
#define SMK_ACT 0xBEE6FFu
static void smk_padc(uint32_t pad, uint32_t c)  /* c = 0xRRGGBB */
{
    padled_set(pad, (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c);
}
static void leds_update(void)
{
    static uint8_t was[SMK_NLED];
    uint8_t L[SMK_NLED];
    uint32_t k, blink = fm1_ms / 250u % 2u == 0u;
    memset(L, 0, sizeof L);
    L[BTN_GLO] = ui.view == V_PROJ;
    L[BTN_HOME] = ui.follow != 0;
    L[BTN_REC] = kb == KB_PAD ? 0 : kb == KB_NOTES || blink;
    L[BTN_PLAY] = playing != 0;
    L[BTN_SAVE] = proj_dirty != 0;
    L[BTN_OCT_DOWN] = oct < 0;
    L[BTN_OCT_UP] = oct > 0;
    L[BTN_SEL] = sel_held;
    for (k = 0; k < SMK_NLED; k++)
        if (L[k] != was[k]) { was[k] = L[k]; fm1_led_key(k, L[k]); }
    for (k = 0; k < 16u; k++) {
        uint32_t held = smk_pads >> k & 1u;
        if (k == SP_OFF)                        /* (OFF writes only in the notes modes) */
            smk_padc(k, !kb ? SMK_OFF : held ? SMK_ACT : SMK_ON);
        else if (k == SP_PLAY)                  /* stopped: off; playing: active */
            smk_padc(k, held || playing ? SMK_ACT : SMK_OFF);
        else if (SMK_PAD_KEY[k] != SP_NONE)     /* the gamepad */
            smk_padc(k, held ? SMK_ACT : SMK_ON);
        else
            padled_set(k, 0, 0, 0);             /* (pads that do nothing) */
    }
    padled_flush();                             /* (between LCD transfers) */
}

#ifdef DINGHY_HOST
static void smk_host_dump(void)                 /* tests/host.c "dump": what tests/smk_ui_test.py checks */
{
    const row_t *w = cur_phrase_rows();
    uint32_t k, r = ui.row ? ui.row - 1u : 15u;
    printf("%6u ms  LEDS", fm1_ms);
    for (k = 0; k < SMK_NLED; k++) printf(" %u", fm1_led[k]);
    printf("\n%6u ms  PADS", fm1_ms);
    for (k = 0; k < SMK_NPAD; k++) printf(" %06X", padled_rgb[k]);
    printf("\n%6u ms  STATE view=%u ch=%u row=%u kb=%u oct=%d playing=%u lvl=%u,%u,%u,%u bend=%d mw=%u want=%X "
           "prev_n=%u prev_vol=%u msg=%s\n", fm1_ms, ui.view, ui.ch, ui.row, kb, oct, playing, S.lvl[0], S.lvl[1],
           S.lvl[2], S.lvl[3], midi_bend_target[ui.ch], trk[ui.ch].mw, key_want, w ? w[r].n : 255u, w ? w[r].vol : 255u,
           msg[0] ? msg : "-");
}
#endif
