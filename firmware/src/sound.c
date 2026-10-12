/* SPDX-License-Identifier: GPL-3.0-only */
/* Felucca's sound layer (fw/firmware/sound/, copied from hugelton/Felucca, unchanged but
 * for two overflow fixes in drum_voice.c: fw/UPSTREAM.md) inside
 * the tracker's one compilation unit, in Felucca's own order (felucca.c). This file supplies what the rest of
 * Felucca would: the few names its sound code takes from the sequencer, the UI and the platform. */

/* Felucca's names that Dinghy's platform.c also defines (the same meaning, a different block size) */
#undef HALF_FRAMES
#undef RING_PUBLISH
#undef FS
#ifdef DINGHY_HOST                              /* no flash window on a computer: the user sample slots read erased */
static const uint8_t no_sample_slot[1024] = {[0 ... 1023] = 0xFF};
#define SMP_USER_XIP(k) ((void)(k), no_sample_slot)
#endif
#include "felucca_tables.h"
#undef BOOTGUARD_MAGIC                         /* core.h ends with Felucca main.c's boot guard: unused here, */
#define bootguard felucca_bootguard             /* and Dinghy has its own (bootguard.h): renamed, not edited */
#include "core.h"
#undef bootguard
#include "engines.c"
#include "params.c"
#include "mod.c"
#include "voice.c"
/* Felucca's live layers are off (decisions.md 2026-10-12, fw/OPTIMIZE.md M1 + M2): the SLICER insert (slicer.c, 32 KB of
 * pool) and the FX hold layer (perform.c; fx.c's include of it dropped). fx.c's calls land here and fold away */
static void slicer_track(const track_t *t, int32_t *b, uint32_t n) { (void)t; (void)b; (void)n; }
static int slicer_busy(const track_t *t) { (void)t; return 0; }
static struct { uint8_t mute; } pf;
static int perf_begin(uint32_t n) { (void)n; return 0; }
static void perf_mute(uint32_t k, int32_t *b, uint32_t n) { (void)k; (void)b; (void)n; }
static void perf_pre(const int32_t *ml, const int32_t *mr, int32_t *sd, int32_t *sr, uint32_t n) { (void)ml; (void)mr; (void)sd; (void)sr; (void)n; }
static void perf_block(int32_t *bl, int32_t *br, uint32_t n) { (void)bl; (void)br; (void)n; }
#include "fx.c"

/* ---- a sound into a part: Felucca's ui.c set_engine_of / apply_preset_to, without the UI's undo, panic
 * request and screen sync. Called from the audio ISR (events_block) or before the audio starts: no IRQ
 * switching here. Felucca's engine switch fades the old engine out first (voice.c engine_block). */
static uint32_t preset_orig(const engine_t *e, uint32_t k)
{
    return e->presets == SMP_PRESET_TABLE && k < SMP_NSETS ? SMP_SET_ORIG[k] : k;
}
static int param_kept(uint32_t i)               /* the track's settings a sound load leaves alone (ui.c) */
{
    return i == P_LEVEL || i == P_PAN || i == P_MUTE || (i >= P_AMODE && i <= P_SGATE) ||
           (i >= P_SLCR && i <= P_SLDEPTH) || i == P_CHRD || i == P_VOIC;
}
static void sound_preset(track_t *t, uint32_t pi)
{
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    const preset_t *pr;
    uint32_t i;
    static const uint8_t FX_DEF[4] = {0, 24, 28, 36};
    trk_all_off(t);                             /* MONO / POLY may change: release what sounds */
    t->user = 0;
    if (!e->npresets) return;
    pi = preset_orig(e, pi % e->npresets);
    pr = &e->presets[pi];
    t->preset = (uint8_t)pi;
    for (i = 0; i < P_E0; i++)
        if (!param_kept(i)) t->p[i] = TP[i].def;
    for (i = 0; i < 8u; i++) t->p[P_E0 + i] = (int16_t)pr->e[i];
    t->p[P_ATK] = pr->env[0];
    t->p[P_DEC] = pr->env[1];
    t->p[P_SUS] = pr->env[2];
    t->p[P_REL] = pr->env[3];
    t->p[P_ED_FLT] = pr->fenv;
    t->p[P_VOICE] = pr->mono ? V_LEGATO : V_POLY;
    for (i = 0; i < 4u; i++) t->p[P_DIST + i] = (int16_t)(pr->fx[i] ? pr->fx[i] - 1 : FX_DEF[i]);
    fm6_track_loaded(t);
}
static void sound_load(track_t *t, uint32_t ei, uint32_t pi)
{
    const engine_t *e = ENGINES[ei % NENGINES];
    uint32_t i;
    t->eng_req = (uint8_t)(ei % NENGINES);
    for (i = 0; i < 8u; i++) t->p[P_E0 + i] = e->edit[i].def;
    sound_preset(t, pi);
}
static uint32_t engine_by_name(const char *n)
{
    uint32_t i;
    for (i = 0; i < NENGINES; i++) if (str_eq(ENGINES[i]->name, n)) return i;
    return 0;
}
static uint32_t preset_by_name(uint32_t ei, const char *n)
{
    const engine_t *e = ENGINES[ei];
    uint32_t i;
    for (i = 0; i < e->npresets; i++) if (str_eq(e->presets[i].name, n)) return i;
    return 0;
}

/* ---- the tracker's 16 instruments are Felucca sounds: an engine, its preset, a name and the part's parameters
 * (P_LEVEL..P_E7). A part loads an instrument when a row plays it (part_load). Sound loads use Felucca's own rules
 * (sound_load / sound_preset above) on a scratch track, so an instrument holds exactly what a Felucca track would.
 * A new song: the prototype's (model.js newInstrument): 0..3 as Felucca at power-on (ANALOG ACID, FM6 PAD,
 * LOFI PULSE LD, DRUM KIT), 4..F ANALOG SAW LEAD. */
#define NINS 16u
#define NAME_LEN 12u
typedef struct {
    uint8_t eng, preset, renamed;
    char name[NAME_LEN + 1u];
    int16_t p[P_COUNT];
} inst_t;
static inst_t ins[NINS];
static volatile uint16_t ins_gen[NINS];         /* bumped by the main loop at every change (the ISR reloads) */
static track_t scratch;                         /* sound loads and labels for an instrument not in a part */
static volatile uint8_t in_audio;               /* tracker.c app_audio: the ISR's own calls (nothing preempts them) */
static void ins_force_removed(uint32_t i);      /* commands.c: the slots the tracker took off the engines */

static void scratch_from(const inst_t *I)
{
    scratch.eng_req = scratch.engine = I->eng;
    scratch.preset = I->preset;
    memcpy(scratch.p, I->p, sizeof scratch.p);
}
static void ins_name_follow(inst_t *I)          /* the name follows the preset, unless renamed */
{
    if (!I->renamed) str_cpy(I->name, ENGINES[I->eng]->presets[I->preset].name, sizeof I->name);
}
static void scratch_to(inst_t *I)
{
    I->eng = scratch.eng_req;
    I->preset = scratch.preset;
    memcpy(I->p, scratch.p, sizeof I->p);
    ins_name_follow(I);
}
#ifdef DINGHY_HOST
/* a computer has no interrupts: the audio runs between the main loop's polls. The emulator's S1 checks
 * (tracker_emu.mjs) fire one where irq_point() stands (in ins_engine's and ins_desc's windows), or at irq_restore
 * when masked: n audio blocks (emu_irq_due), or for the next w windows just the ISR's lookup of instrument i's
 * E slot k (emu_irq_look: what cmd_target does at each tick of a ramp on it) */
static const param_desc_t *ins_desc(uint32_t i, uint32_t id);
static uint32_t irq_off, irq_due, irq_look, irq_li, irq_lk;
static void irq_fire(void)
{
    static int32_t b[2u * 64u];
    uint32_t n = irq_due;
    irq_due = 0;
    while (n--) app_audio(b, 64u);
    if (irq_look) { irq_look--; in_audio = 1; (void)ins_desc(irq_li, P_E0 + irq_lk); in_audio = 0; }
}
static uint32_t irq_save(void) { irq_off = 1; return 0; }
static void irq_restore(uint32_t f) { (void)f; irq_off = 0; irq_fire(); }
static void irq_point(void) { if (!irq_off && !in_audio) irq_fire(); }
#ifdef WASM_EXPORT
WASM_EXPORT("emu_irq_due") void emu_irq_due(uint32_t n) { irq_due = n; }
WASM_EXPORT("emu_irq_look") void emu_irq_look(uint32_t i, uint32_t k, uint32_t w) { irq_li = i % NINS; irq_lk = k & 7u; irq_look = w; }
#endif
#ifdef WASM_EXPORT                              /* debug export for tracker_emu.mjs: instrument i's saved E0..E7 */
WASM_EXPORT("emu_ins_e") int32_t emu_ins_e(uint32_t i, uint32_t k) { return ins[i % NINS].p[P_E0 + (k & 7u)]; }
#endif
#else
#define irq_point()
#endif
/* a new engine reloads the whole sound from its first preset, LEVEL and PAN kept (Felucca's param_kept).
 * The audio ISR is masked: it reads E0..E7 labels through scratch (ins_desc) and would swap its sound in here */
static void ins_engine(uint32_t i, uint32_t ei)
{
    uint32_t f = irq_save();
    scratch_from(&ins[i]);
    sound_load(&scratch, ei, 0);
    irq_point();
    ins[i].renamed = 0;                         /* a new sound: its preset's name (app.js reload) */
    scratch_to(&ins[i]);
    irq_restore(f);
    ins_force_removed(i);
    ins_gen[i]++;
}
/* a new preset on the same engine only changes the timbre (E0..E7): everything else stays (HANDOFF section 5) */
static void ins_preset(uint32_t i, uint32_t pi)
{
    const engine_t *e = ENGINES[ins[i].eng];
    uint32_t k;
    if (!e->npresets) return;
    pi = preset_orig(e, pi % e->npresets);
    ins[i].preset = (uint8_t)pi;
    for (k = 0; k < 8u; k++) ins[i].p[P_E0 + k] = (int16_t)e->presets[pi].e[k];
    ins_name_follow(&ins[i]);
    ins_force_removed(i);
    ins_gen[i]++;
}
/* the descriptor of parameter id of instrument i (the engine's mode-dependent labels included). Also the audio
 * ISR's (cmd_target): only E0..E7 need the scratch track (an engine's desc hook). The main loop masks the ISR
 * around it, or a sound command could swap in another engine's range for the value it is about to save */
static const param_desc_t *ins_desc(uint32_t i, uint32_t id)
{
    const param_desc_t *d;
    uint32_t f;
    if (id < P_E0) return &TP[id];
    f = in_audio ? 0u : irq_save();
    scratch_from(&ins[i]);
    irq_point();
    d = track_desc(&scratch, id);
    if (!in_audio) irq_restore(f);
    return d;
}
static void instruments_new(void)
{
    static const char *const START[4][2] = {{"ANALOG", "ACID"}, {"FM6", "PAD"}, {"LOFI", "PULSE LD"}, {"DRUM", "DRUM KIT"}};
    uint32_t i, k;
    for (i = 0; i < NINS; i++) {
        const char *en = i < 4u ? START[i][0] : "ANALOG", *pn = i < 4u ? START[i][1] : "SAW LEAD";
        uint32_t ei = engine_by_name(en);
        for (k = 0; k < P_E0; k++) scratch.p[k] = TP[k].def;
        sound_load(&scratch, ei, preset_by_name(ei, pn));
        ins[i].renamed = 0;
        scratch_to(&ins[i]);
        ins_force_removed(i);
    }
}

/* instrument i into part t: its engine (Felucca fades the old one out: engine_block) and its parameters.
 * full = 0: the same instrument again, changed meanwhile: the parameters only, the notes ring on */
static void part_load(track_t *t, uint32_t i, int full)
{
    const inst_t *I = &ins[i];
    int e7 = t->p[P_E7] != I->p[P_E7];
    if (full || t->eng_req != I->eng) {
        trk_all_off(t);
        t->eng_req = I->eng;
        full = 1;
    }
    t->preset = I->preset;
    t->user = 0;
    memcpy(t->p, I->p, sizeof t->p);
    if (full || e7) fm6_track_loaded(t);        /* FM6: the SLOT's patch */
}

/* power-on: Felucca's globals and every part's defaults (main.c felucca_init, without the steps) */
static void sound_init(void)
{
    uint32_t i;
    for (i = 0; i < G_COUNT; i++) song.g[i] = GP[i].def;
    fm6_init();
    for (i = 0; i < NTRK; i++) {
        part_load(&trk[i], 0, 1);
        trk[i].engine = trk[i].eng_req;
    }
    song.sel = 0;
    song.master_q12 = MASTER_FULL;              /* the MASTER knob is the platform's (platform.c) */
}
