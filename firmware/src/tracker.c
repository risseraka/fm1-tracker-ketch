/* SPDX-License-Identifier: GPL-3.0-only */
/* The tracker, on Dinghy's platform (platform.c), as agreed in the prototype (ux-proto/, HANDOFF section 0).
 * The sound is Felucca's (sound.c): instruments are Felucca sounds, channel k plays on Felucca's part k.
 *
 *   screens    G project, S song, C chain, P phrase, I E L V instrument pages, on the 2D map (HANDOFF section 5):
 *              SHIFT + left / right along S C P I (going right into a chain or a phrase creates it if the cell is
 *              empty); from S / C / P: up = G, which hangs off that screen (down / sideways: back to it / on from it);
 *              from I: down = E L V, up = Q (left = P, right = I); from S / C / P: down = F, then X (as G);
 *              GLO: G, again: back; panel FX on V: F; SEQ: Q
 *   Q          the event view (docs/ux/event-view.md, steps Q-1 to Q-3): the timeline (SONG, the BARS page, NOTE,
 *              the focus curve, REST) over every phrase row the instrument plays, once, its plays counted (x12) and
 *              its channels as dots; up / down a line, left / right its cells (the note, the commands); OPTION +
 *              left / right: the previous / next instrument the song plays, up / down: the previous / next phrase;
 *              EDIT + arrows edit the row (every play), OPTION + EDIT: the default, an EDIT tap goes there in the
 *              phrase; the bottom line says where it plays first; SEQ in Q = only the lines with the cursor cell's
 *              command (a note cell: with a note; followed by name to another instrument), SEQ again: every line;
 *              EDIT held names SEQ first; SHIFT + OPTION selects lines: EDIT + arrows / OPTION + EDIT change the cursor cell's
 *              command (or the note) on each, a plain OPTION or SEQ drops them; PLAY = the song from the song row
 *              where the line plays first, the playhead drawn in the timeline (a red tick on SONG, a line through
 *              the page), the page following it; the bottom line and the EDIT tap then take the next play; EDIT + PLAY previews the line; SHIFT + right
 *              on a sound command: its parameter on the instrument's page
 *   EDIT + PLAY  preview while PLAY is held (app.js auditionStart / previewQ; not on the phrase, picks review #4: its
 *              preview is EDIT's audition): on I E L V the instrument (the
 *              last note), on Q the line (its note and sound commands, the instrument shown, on the channel of its
 *              first play); playing, it takes the channel's voice
 *   HOME       follow (app.js followState): while playing, S / C / P put the cursor on the row the cursor's channel
 *              plays (song row, chain row, phrase row); up / down stop it, left / right keep it; HOME lit while on
 *   F          the effects (render.js viewFx): Felucca's delay, reverb and chorus in framed groups, edited as the
 *              instrument pages' fields (EDIT + d-pad, OPTION + EDIT, a list's popup while EDIT is held)
 *   X          the mixer (render.js viewMix): a level bar, LEVEL, MUTE, SOLO per channel; left / right = the
 *              channel (the screens come back on it), up / down = the field; EDIT toggles MUTE / SOLO, EDIT + d-pad
 *              changes, OPTION + EDIT = the default. The level adds to the instruments' (0.5 dB steps, 104 = -4 dB)
 *   keybed     F3 left, G3 down, G#3 up, A3 right; D5 SHIFT, E5 PLAY, F5 OPTION, G5 EDIT (control-map.js PAD)
 *   EDIT       on an empty cell: the last value of that column (song / chain: else the next free chain /
 *              phrase); on a filled one: copies it as the last (a note with its instrument cell, an empty one too;
 *              a sound command's value 00 counts as empty). EDIT EDIT (song, chain): a new empty chain / phrase
 *   defaults   a new note: the last instrument, on an empty row velocity 64 (then the last edited); a new command:
 *              the last one used = the last value, a sound command = the instrument's setting, a player one = 00
 *              an OFF row takes no velocity or instrument (a paste: only with a copied note); keys and live
 *              record overwrite it with a note, which gets the last velocity
 *   EDIT + d-pad   left/right -1 / +1, down/up -big / +big (note: an octave, transpose: 12, hex: 16)
 *   OPTION + EDIT  back to the default: empty (an empty note: a note off; transpose: 0)
 *   OPTION + d-pad chain / phrase: left/right = channel, up/down = song row / chain row (never onto an empty one);
 *              instrument page: instrument -1 / +1, +4 / -4
 *   REC        notes mode (notes-mode.md option B): A#3..C#5 play notes on the cursor's channel (OCT- / OCT+: the
 *              octave), F#3 a note off; stopped, on the phrase, a key writes its note at the cursor, which moves
 *              down; the gamepad keys keep working; NOTE in the top bar. REC again: ARM, then PLAY records
 *              (live-record.md): a bar of count-in with the metronome, the notes written where the cursor's channel
 *              plays (phrase: a loop overdub; chain: through it, a new phrase on an empty row), late hits kept as
 *              DEL ticks; REC stops writing (playback goes on); one take = one undo step
 *   PLAY       what the screen shows: the song from the cursor row (all channels), this chain from the cursor
 *              row (looped), this phrase (looped, following the screen); SHIFT + PLAY: the song from the song row
 *   instrument pages  I: ENGINE PRESET / LEVEL PAN NAME / E0..E7 / KNOBS; E: the ADSR drawn, its amounts; L: the LFO
 *              drawn (moving), its amounts; V: voice and sends; panel ENV / LFO / FX jump to E / L / V; EDIT +
 *              d-pad, KNOB 1-4 = the cursor's row (a bar in knob n's colour under each field it turns); NAME: left /
 *              right walk its 12 letters, EDIT = the last letter (on a filled one: copies it), EDIT held = the
 *              character grid, letting EDIT go after a new letter moves on, OPTION + EDIT empties the letter
 *   KNOBS      the I page's last row: the 4 knob slots (what KNOB 1-4 turn away from the instrument pages), drawn
 *              as rings in knob n's colour (grey unless the cursor is on them), the names over the values, two cursor rows: on the names EDIT + arrows /
 *              EDIT held (the FX grid: the sound commands, VOL first) pick slot n's parameter, OPTION + EDIT = the
 *              engine's HOME knob; on the values EDIT + arrows / OPTION + EDIT edit the parameter as its own field;
 *              on either KNOB n turns slot n's value (never the choice); a new engine resets them.
 *              Elsewhere KNOB 1-4 turn the focus instrument's slots (phrase: the cursor row's instrument or the
 *              nearest above; else the last used), the knob strip shows them for 1.5 s away from the cursor
 *   knobs, take 3 (docs/decisions.md) the row of four: F's KNOB n = field n of the cursor's frame row (none past the
 *              last field; in REC not recorded), X's KNOB n = track n's TVn from any row (in REC recorded as TVn on
 *              that track), bars in the knob colours, no strip. The keybed EDIT held + KNOB n on I E L V puts the
 *              cursor's parameter in slot n (the KNOBS rows: that slot's), the hold's first detent only, EDIT let go
 *              then copies nothing; elsewhere a message. Acceleration x1 / x2 / x4 (50 / 25 ms between detents),
 *              not on lists, ranges of 16 steps or fewer, the crossfader, MASTER
 *
 *   popups     while EDIT is held: the note's 2-octave keyboard, the instrument list (phrase); ENGINE, PRESET and the
 *              named lists (instrument page); up / down one item, left / right four
 *   scenes     (docs/ux/scenes.md) 8 a song; SEL + SELECT = the crossfader A -> B (a detent 1/32, SHIFT + a turn snaps
 *              to that end; without SEL: `hold SEL to crossfade`), SCN xx sets it from a phrase; SEL held: the middle white keys B3..B4 pick scenes (tap: the
 *              far side, SHIFT: the near side), edits (I E L V, the knob strip, X's levels, F) go into the near scene,
 *              OPTION + EDIT takes the value out; without SEL a value the near scene holds is edited there. The morph
 *              plays every block (scene_apply, ch_level; F in the main loop). A turn or a pick shows the scene strip
 *              for 1.5 s (A SC1 ---o--- SC2 B, A's and B's values per parameter); up / down give it the focus (a line
 *              cursor, the screen's cursor dimmed), EDIT = that value's page, OPTION = close. The fader mark on the top
 *              bar's line when it is not at A. SEL held, the pages (I E L V, KNOBS, X, F) show the near scene's values,
 *              the others dim; otherwise a value the morph moves shows what plays, a diamond after it. SEL + OPTION +
 *              a scene key empties it (EDIT confirms)
 *   knob recording (docs/ux/knob-record.md) in REC a knob turn (the knob strip's or an instrument page's) is written as
 *              its command on the recorded channel's row nearest it, rows playing that instrument only, in the slot
 *              holding it, else the cursor's FX column or right of it (elsewhere: the first free), not above a ramp
 *              command; the crossfader writes SCN xx; touch: while the hand moves, the recorded values of that
 *              parameter do not play and the rows passed lose them; the take's cells red, the strip's REC title red
 *   FX         3 columns of command + value (commands.c, docs/ux/commands.md): player commands ARP CHA DEL HOP KIL
 *              OFF PIT RET SCN SWG TPO VIB TV1-TV4 (a track's level while playing, from any track; TVL before
 *              2026-10-12) MUT SOL, ramps REP RTO RTI RCU under a
 *              command, sound commands = the row instrument's parameters by their engine's names (VOL = its level),
 *              held per instrument until changed (decisions.md "Levels: VOL and TVL")
 *
 *              EDIT held on a command: the FX grid; on any G / S / C / P cell, row number, column label or
 *              selection: what it is in the bottom line (gen/help.h, from app.js)
 *
 *   shown while playing (commands.md 11, 14-17): the timelines under the notes (DEL KIL OFF, RET's ticks lit),
 *              ramp lines and ^^, CHA's lost side and the rows a HOP skips dimmed, the swing (SWxx, the playhead
 *              shifted), held / ramping values on the instrument page (channel colour, triangle, `CUT set .., now ..`)
 *
 *   SHIFT + right on a sound command: its parameter on the instrument page; EDIT tapped on an instrument-page
 *              parameter: copied as the last command with its value (`COPIED PAN 80`)
 *
 *   OPTION then SHIFT  mute / unmute the cursor's channel; OPTION + PLAY solo; OPTION + SHIFT + PLAY clear them
 *              (on a track one turns the other off; solo wins; the song shows M / S, a silent channel is grey on S C P)
 *   rows       the row number is a cell (left of the first column): EDIT EDIT inserts a row, EDIT + up/down
 *              moves it, OPTION + EDIT deletes it; the phrase's column labels are cells (up from row 0)
 *   selection  SHIFT + OPTION, the d-pad extends it (whole rows / columns from a row number / a label; again: cancels);
 *              while selecting OPTION + EDIT deletes it, EDIT + d-pad changes it all (whole rows: up/down move
 *              them); OPTION copies (the block stays marked, drawn only; a plain OPTION drops it), SHIFT + EDIT pastes
 *   SHIFT + EDIT on a selection  inside one value column: interpolates first -> last row (stays selected); else clones
 *              its chains / phrases (next free numbers); again right away: FX values become a ramp (command,
 *              RTO, RTI; 3+ rows), in the song a deep clone (phrases too)
 *   panel EDIT + OCT- / OCT+  undo / redo (4 steps): the cursor stays, the changed cells flash white, the bottom
 *              line says what changed (`UNDO P00 4 C-3>---`)
 *
 *   SAVE       the panel button saves the project into its slot (STOP TO SAVE while playing), lit while there are
 *              unsaved changes; the slot opened last is opened at power-on (docs/flash-map.md)
 *   G          4 project slots: EDIT load, EDIT EDIT on an empty one new, SHIFT + OPTION then OPTION copy, SHIFT + EDIT
 *              paste, OPTION + EDIT delete; what loses work asks (EDIT confirms, other keys cancel); while playing
 *              each asks, then acts at the cursor's channel's phrase end (PLAY: now). NAME, TEMPO, STEP (step entry),
 *              SWING, TUNE (cents), METRO OFF / REC / ON, COUNT (count-in bars), QNT ROW / FEEL (live record) */
#define APP_NAME "KETCH"
#include "platform.c"
#include "font_6x12.h"
#include "sound.c"
#include "commands.c"
#include "help.h"

static void mem_move(void *d, const void *s, uint32_t n)   /* (overlapping; Dinghy's libc has no memmove) */
{
    uint8_t *p = d;
    const uint8_t *q = s;
    if (p < q) while (n--) *p++ = *q++;
    else { p += n; q += n; while (n--) *--p = *--q; }
}

/* ---------------------------------------------------------------- the song (docs/ux/data-model.md) --- */
#define EMPTY 0xFFu
#define NOTE_OFF 0xFEu
#define NOTE_MIN 24u                            /* C-1 */
#define NOTE_MAX 107u                           /* B-7 */
#define VOL_MAX 0x7Fu
#if BOARD_SMK37
#define KNOB_NAME "K"                           /* the SMK-37's KNOB 1-4 are K1-K4 (K5-K8: MASTER, SELECT, ...) */
#define KNOB_NO(k) ((int32_t)(k) + 1)
#else
#define KNOB_NAME "KNOB "
#define KNOB_NO(k) ((int32_t)(k) + 1)
#endif
#define NSONG 64u
#define NCH 4u
#define NCHN 64u
#define NPHR 128u
#define NFX 3u

typedef struct { uint8_t c, v; } fx_t;          /* command 0 = none */
typedef struct { uint8_t n, vol, i; fx_t fx[NFX]; } row_t;   /* 9 bytes */
typedef struct { uint8_t ph; int8_t tr; } crow_t;
/* scenes (docs/ux/scenes.md): 8 a song, each up to 8 parameter values (so A + B fit the scene strip's page). A value:
 * t = the instrument (p = its parameter), SC_MIX (p = the channel's mixer level) or SC_FX (p = F's field); EMPTY = none,
 * the used ones first, in the order they were added. Every value Felucca's pages and F have fits a byte (-64 .. 127) */
#define NSCENE 8u
#define SCENE_MAX 8u
#define XF_MAX 127u
enum { SC_MIX = 0xF0, SC_FX };
typedef struct { uint8_t t, p; int8_t v; } sc_t;

static struct {
    uint8_t tempo;
    uint8_t song[NSONG][NCH];
    crow_t chains[NCHN][16];
    row_t phrases[NPHR][16];
    uint8_t mute[NCH], solo[NCH];               /* the mixer (data-model.md); the levels: S.lvl, last */
    struct {                                    /* the G page (render.js viewProj): last, so a version 2 project */
        char name[NAME_LEN + 1];                /* loads with these appended (proj_load) */
        uint8_t step, swing, metro, count, qnt; /* METRO OFF / REC / ON, COUNT OFF / 1BAR / 2BAR, QNT ROW / FEEL */
        int8_t tune;                            /* cents, -50 .. 50 */
    } pj;
    uint8_t knob[NINS][4];                      /* knob slots (knob-slots.md): what KNOB n turns away from the
                                                 * instrument pages, a parameter key; EMPTY = the engine's HOME knob.
                                                 * Last, so a version 2 / 3 project loads with them appended */
    uint8_t lvl[NCH];                           /* the mixer's levels (X), as an instrument's LEVEL: 104 = -4 dB;
                                                 * last, so a version 2..4 project loads with them appended */
    uint8_t fx[9];                              /* the effects (F): Felucca's globals FXF[k].g; last, as S.lvl */
    struct {                                    /* the scenes (model.js scenes, sceneA / sceneB, xfade); last, as S.lvl */
        sc_t k[NSCENE][SCENE_MAX];
        uint8_t a, b, xf;                       /* the scenes on side A and B; the crossfader 0 = A .. 127 = B */
    } scn;
    uint8_t mute_mode;                          /* G's MUTE: 0 NOTE (no new notes), 1 AUDIO (plays on at level 0); last,
                                                 * as S.scn (decisions.md "... audio mute ...", 2026-10-11) */
} S;
/* F's fields (render.js FX_FIELDS, FX_PAGE): x, row (the DELAY / REVERB / CHORUS frames), Felucca's global, label */
static const struct { uint8_t x, row, g; const char *lbl; } FXF[9] = {
    {1, 0, G_DTIME, "TIME"}, {9, 0, G_DFDBK, "FDBK"}, {17, 0, G_DCOLOR, "COLOUR"}, {25, 0, G_DMIX, "MIX"},
    {1, 1, G_RTYPE, "TYPE"}, {9, 1, G_RSIZE, "SIZE"}, {17, 1, G_RDAMP, "DAMP"},
    {1, 2, G_CRATE, "RATE"}, {9, 2, G_CDEPTH, "DEPTH"},
};
static const int16_t FX_Y[3] = {48, 110, 172};  /* each frame's label line (render.js FX_PAGE rowY) */
static void fx_defaults(void) { uint32_t k; for (k = 0; k < 9u; k++) S.fx[k] = (uint8_t)GP[FXF[k].g].def; }
static void scn_defaults(void)                  /* model.js newScenes: all empty, scene 1 on A, 2 on B, the fader at A */
{
    memset(&S.scn, 0, sizeof S.scn);
    memset(S.scn.k, EMPTY, sizeof S.scn.k);     /* (t EMPTY; p, v unused) */
    S.scn.b = 1;
}
static void fx_change(uint32_t f, int32_t steps);   /* (F, below: through the scenes) */
static void pj_defaults(void)                   /* model.js newSong */
{
    memset(S.pj.name, 0, sizeof S.pj.name);
    S.pj.step = 1; S.pj.swing = 0; S.pj.tune = 0; S.pj.metro = 1; S.pj.count = 1; S.pj.qnt = 1; S.mute_mode = 0;
}
/* MUT / SOL (decisions.md "MUT / SOL"): the track's mute / solo while playing, over the mixer's (EMPTY = the mixer's);
 * the ISR's alone (a hand press asks it through mix_req), never in S (undo, saving), dropped at start / stop */
static volatile uint8_t live_mute[NCH] = {EMPTY, EMPTY, EMPTY, EMPTY}, live_solo[NCH] = {EMPTY, EMPTY, EMPTY, EMPTY};
static volatile uint8_t mix_req;                /* main -> ISR: drop the live mute (bit c) / solo (bit 4 + c), the hand wins */
static int heard_mute(uint32_t c) { return live_mute[c] != EMPTY ? live_mute[c] : S.mute[c]; }
static int heard_solo(uint32_t c) { return live_solo[c] != EMPTY ? live_solo[c] : S.solo[c]; }
/* solo wins over mute: with any channel soloed only the soloed ones are heard (model.js audible) */
static int audible(uint32_t c)
{
    uint32_t k;
    for (k = 0; k < NCH; k++) if (heard_solo(k)) return heard_solo(c);
    return !heard_mute(c);
}

static void song_new(void)                      /* an empty song (model.js newSong) */
{
    uint32_t i, r, k;
    S.tempo = 120;
    memset(S.song, EMPTY, sizeof S.song);
    for (i = 0; i < NCHN; i++)
        for (r = 0; r < 16u; r++) { S.chains[i][r].ph = EMPTY; S.chains[i][r].tr = 0; }
    for (i = 0; i < NPHR; i++)
        for (r = 0; r < 16u; r++) {
            row_t *w = &S.phrases[i][r];
            w->n = w->vol = w->i = EMPTY;
            for (k = 0; k < NFX; k++) { w->fx[k].c = 0; w->fx[k].v = 0; }
        }
    memset(S.mute, 0, NCH); memset(S.solo, 0, NCH); memset(S.lvl, 104, NCH);
    fx_defaults();
    scn_defaults();
    memset(S.knob, EMPTY, sizeof S.knob);
    pj_defaults();
}
static int phrase_empty(uint32_t p)
{
    uint32_t r, k;
    for (r = 0; r < 16u; r++) {
        const row_t *w = &S.phrases[p][r];
        if (w->n != EMPTY || w->vol != EMPTY || w->i != EMPTY) return 0;
        for (k = 0; k < NFX; k++) if (w->fx[k].c) return 0;
    }
    return 1;
}
static int phrase_used(uint32_t p)
{
    uint32_t c, r;
    for (c = 0; c < NCHN; c++) for (r = 0; r < 16u; r++) if (S.chains[c][r].ph == p) return 1;
    return 0;
}
static int chain_empty(uint32_t c) { uint32_t r; for (r = 0; r < 16u; r++) if (S.chains[c][r].ph != EMPTY) return 0; return 1; }
static int chain_used(uint32_t c) { uint32_t r, k; for (r = 0; r < NSONG; r++) for (k = 0; k < NCH; k++) if (S.song[r][k] == c) return 1; return 0; }
/* the next unused, empty chain / phrase after `from` (EMPTY: from the start), wrapping; EMPTY if none (model.js nextFree) */
static uint32_t next_free_chain(uint32_t from)
{
    uint32_t k, i;
    for (k = 1; k <= NCHN; k++) {
        i = ((from == EMPTY ? NCHN - 1u : from) + k) % NCHN;
        if (!chain_used(i) && chain_empty(i)) return i;
    }
    return EMPTY;
}
static volatile uint8_t rec_spare = EMPTY;      /* main -> ISR: a free phrase for the next empty chain row */
static uint32_t next_free_phrase(uint32_t from)   /* (not rec_spare: the ISR may take it any moment, RS1) */
{
    uint32_t k, i;
    for (k = 1; k <= NPHR; k++) {
        i = ((from == EMPTY ? NPHR - 1u : from) + k) % NPHR;
        if (i != rec_spare && !phrase_used(i) && phrase_empty(i)) return i;
    }
    return EMPTY;
}

/* ---------------------------------------------------------------- the player --- */
/* Felucca's mix_block() renders 32-sample control blocks (CTL) and calls events_block() at the start of each:
 * its sequencer's hook, so the tracker's player runs there, in the audio ISR. A row is 6 ticks (data-model.md);
 * the tick length in samples is a whole number with the remainder carried, so nothing drifts; a row starts at
 * the start of the control block its tick falls in (<= 0.73 ms late, as Felucca's own steps).
 * Each channel walks song row -> chain row -> phrase row on its own (audio.js resolve / advance): an empty chain
 * row ends the chain (next song row), an empty song cell ends that channel's song (back to row 00). Channel k
 * plays on Felucca's part k; the row's instrument is loaded into it when it changes.
 * The FX commands (docs/ux/commands.md; audio.js tick): player commands act on the channel, sound commands set a
 * parameter of the row's instrument and hold it until changed (per instrument, dropped at start and stop), and
 * ramp commands under a command move its value every tick. */
#define TICKS 6u
#define TICK_X_TEMPO (FS * 60u / 24u)            /* a tick's samples x the tempo (24 ticks a beat): 110250 */
enum { M_SONG, M_CHAIN, M_LOOP, M_SEL };
static volatile uint8_t playing, play_req;      /* play_req: 1 start (play_* below), 2 stop (main loop -> ISR) */
static volatile uint8_t play_mode, play_ch, play_chain, play_phrase, play_from_song, play_from_chain;
static volatile uint8_t loop_next = EMPTY;      /* M_LOOP: the phrase on screen now, taken at a phrase end */
/* M_SEL, PLAY on a selection (decisions.md "PLAY on a selection"): its rows looped. sel_lv: what was selected (0 phrase
 * rows r0..r1 at song row s0 / chain row c0; 1 chain rows c0..c1 at s0; 2 song rows s0..s1); sel_on: the tracks that
 * play, sel_fill: those that fill a gap with their nearest phrase above (SHIFT + PLAY). Set by play() before the start */
static uint8_t sel_lv, sel_s0, sel_s1, sel_c0, sel_c1, sel_r0, sel_r1, sel_on, sel_fill;
static uint16_t sel_src[NCH];                   /* ISR: where the place channel c plays sits (song row << 4 | chain row) */
static char sel_tag[12];                        /* the top bar's LOOP 4-7 */
static volatile uint32_t ch_pos[NCH];           /* ISR -> main loop: bit 31 active, song row << 16, chain row << 12,
                                                 * phrase << 4, row */
#define POS_AT(sr, cr, ph, r) (1u << 31 | (sr) << 16 | (cr) << 12 | (ph) << 4 | (r))
#define POS_ON(p) ((p) >> 31)
#define POS_SROW(p) ((p) >> 16 & 63u)
#define POS_CROW(p) ((p) >> 12 & 15u)
#define POS_PH(p) ((p) >> 4 & 127u)
#define POS_ROW(p) ((p) & 15u)
static uint32_t tick, tick_rem, gtick, loop_ch, loop_chain, loop_phrase;
static volatile uint32_t mode;                  /* ISR -> main loop too (player_start) */
static volatile uint8_t tempo_ch = EMPTY;      /* the channel whose TPO set tempo (EMPTY: the song's; param-values.md 4) */
static volatile uint8_t tempo = 120;            /* playback's tempo: S.tempo at PLAY, then TPO and its ramp; the song's
                                                 * own (S.tempo) changes only on G, so a save keeps it (U7, my pick,
                                                 * to review) */
static int32_t tick_left;
typedef struct {                                /* a ramp (commands.md 10): the command it moves, its stack's values */
    uint8_t on, code, ins, curve;
    int16_t from, step, to, rows;               /* step 0 = none (by time), to -1 = none, rows 0 = none */
    uint32_t ticks;
    int32_t now;
} ramp_t;
static struct {
    const row_t *rows;                          /* the phrase it plays, and the row being started */
    row_t pend;
    uint32_t srow, crow, row, ins, note, base, active, pending, delay, entering;
    int32_t tr, hop, kill_at, kill_cut, swing;
    int32_t arp_on, arp_x, arp_y, bend_q8, bend_rate, vib_speed, vib_depth, vib_ph;
    int32_t ret_left, ret_gap, ret_next, ret_x, ret_vel;
    ramp_t ramp[NFX];
} chs[NCH];
/* what the screen shows of the playback (commands.md 11, 14-16), written here, read by the main loop */
static volatile uint8_t ui_tick;                /* ticks played of the current row (1..6) */
static volatile uint8_t ch_swing[NCH];          /* each channel's swing now, % */
static volatile uint8_t ramp_ph[NCH][NFX];      /* the phrase a running ramp started in, EMPTY = none */
static uint16_t skip_l[NCH][NPHR], skip_r[NCH][NPHR];   /* CHA: the side that lost its roll, per phrase row */
static uint16_t unheard[NCH][NPHR];             /* a note without an instrument that found nothing sounding */
static volatile int16_t cin_left[NCH];          /* at a phrase's entry: ticks to a running KIL / OFF (0 none), */
static volatile uint8_t cin_how[NCH];           /* its kind (1 cut, 2 release) | 4 = a note sounding (U37) */
static uint8_t entry_row[NCH][NSONG][16];       /* HOP: the row each song row / chain row's phrase was entered at,
                                                 * EMPTY = not played since the song or a chain's phrases changed */
static uint32_t part_ins[NCH] = {0, 0, 0, 0};   /* the instrument loaded in each part */
static uint16_t part_gen[NCH];                  /* ins_gen[part_ins] when it was loaded: a change reloads it */
static volatile uint8_t aud_req, aud_note, aud_ins, aud_ch, aud_hold;   /* an audition: main loop -> ISR; aud_hold:
                                                 * EDIT held, the audition lasts until it is let go (picks review #27) */
static volatile uint8_t aud_ph = 0xFF, aud_r;   /* with the sound commands of this phrase row (EMPTY: none; #26) */
static volatile int16_t ch_tvl[NCH] = {-1, -1, -1, -1};   /* ISR: the track level a TV1-TV4 set while playing (0..127),
                                                 * -1 = the mix */
static volatile uint8_t tvl_by[NCH];           /* ISR: the track whose row set it (X draws it in that track's colour) */
static uint32_t tvl_raw(uint32_t v) { return (v * 127u + 127u) / 255u; }   /* TVn 00..FF -> a mix level (model.js tvlRaw) */
static int16_t prv_lvl = -1;                    /* ISR: the Q line's VOL on prv_ch while it previews, -1 = none */
static volatile uint8_t prv_req, prv_ch, prv_note, prv_ins, prv_ph, prv_r;   /* EDIT + PLAY: 1 start, 2 stop (main ->
                                                 * ISR); prv_ph EMPTY: no row (an instrument page) */
static volatile uint32_t key_want;              /* notes mode: the note keys held (bit = key), main loop -> ISR, */
#define NKEYS 27u                               /* the keybed: F3 .. G5 */
#define KEY_BASE 53u                            /* key 0's note, F3 (app.js KEY_BASE) */
static volatile uint8_t key_note[NKEYS], key_ins[NKEYS], key_ch[NKEYS];   /* each one's note, instrument, channel */
#if BOARD_SMK37
static volatile uint8_t key_vel[NKEYS];         /* SMK-37: each one's velocity (the keys' slots: smk_keys.c) */
static uint8_t smk_vel = 100;                   /* SMK-37: the velocity rec_write writes (smk_keys.c) */
#define KEY_VEL(k) key_vel[k]
#else
#define KEY_VEL(k) VOL_MAX
#endif
static uint32_t key_had;                        /* ISR: the keys it has started */
static volatile uint8_t hit_seq[NCH], hit_note[NCH];   /* ISR -> main: a note struck (after DEL, every RET hit), */
static volatile uint16_t hit_ms[NCH];           /* for the key flash: how long it flashes */
/* live recording (docs/ux/live-record.md), main loop <-> ISR */
static volatile uint8_t rec_on;                 /* recording: the metronome clicks, a chain grows */
static volatile uint8_t play_count;             /* PLAY: beats of count-in before the first row */
static volatile uint32_t count_left;            /* ISR: count-in ticks left */
static volatile int32_t row_q8;                 /* ISR -> main: ticks played of the current row, x 256 */
static uint32_t click_left, click_ph, click_inc;   /* the metronome's click: samples left, its sine */
static uint32_t beat_tick;                      /* ticks since PLAY (count-in included): a beat = 24, a bar = 96 */
static volatile uint16_t rec_skip;              /* QNT ROW: 0x8000 | phrase << 4 | row, a row written ahead: not struck
                                                 * on this pass (it was heard live); main sets it, the ISR clears it */
static volatile uint8_t end_ch;                 /* main -> ISR: channel + 1 whose phrase end stops playback (G page) */
static uint8_t end_now;                         /* ISR: that phrase ended, stop at the next block */
static int32_t aud_left;                        /* samples until it is released (with aud_wait: > 0 = sounding) */
static uint8_t aud_wait;                        /* ISR: this audition ends when aud_hold does, not after 180 ms */
static uint8_t aud_on;                          /* ISR: the channel it sounds on (aud_ch may already name the next) */
static uint8_t aud_fxd;                         /* ISR: it set a row's sound commands on the part (undone at its end) */
/* knob recording, touch (docs/ux/knob-record.md 7, HANDOFF "touch" rule): while the hand moves a control in REC, the
 * parameter's recorded commands and ramps do not play (main loop -> ISR) */
#define NTOUCH 5u                               /* 4 knob gestures, then the crossfader (SCN) */
#define TOUCH_SCN 0xFF00u
#define TOUCH_TVL 0xFE00u                       /* | n: X's KNOB n, TVn on track n (decisions.md "Knobs, take 3" (1)) */
static volatile uint16_t tch[NTOUCH] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};   /* instrument << 8 | parameter,
                                                 * TOUCH_SCN; 0xFFFF = none */
static volatile uint8_t kh_req, kh_ins, kh_id, kh_ch;   /* main -> ISR: a recorded value, heard at once (held) */
static volatile int16_t kh_raw;
static int touched(uint32_t i, uint32_t id)
{
    uint32_t k;
    for (k = 0; k < NTOUCH - 1u; k++) if (tch[k] == (i << 8 | id)) return 1;
    return 0;
}
#define REC_BITS (NPHR * 16u * NFX)
static uint8_t tw[REC_BITS / 8u] __attribute__((section(".pool")));   /* cells a running gesture wrote: kept */
static uint8_t rc[REC_BITS / 8u] __attribute__((section(".pool")));   /* cells this take wrote: red */

/* ---- held sound values: per instrument, the value a sound command (or its ramp) set, until changed */
/* only LEVEL, E0..E7 and the shared sound commands can be held: a slot each (my pick, to review: SOUND_CMD
 * searched, not a 99-byte id -> slot map, so the list stays the one source) */
#define NHELD (9u + NELEM(SOUND_CMD))
static uint32_t held_slot(uint32_t id)          /* NHELD = never held */
{
    uint32_t k;
    if (id == P_LEVEL) return 0;
    if (id >= P_E0 && id <= P_E7) return 1u + id - P_E0;
    for (k = 0; k < NELEM(SOUND_CMD); k++) if (SOUND_CMD[k].id == id) return 9u + k;
    return NHELD;
}
static uint32_t held_id(uint32_t s) { return s == 0 ? P_LEVEL : s < 9u ? P_E0 + s - 1u : SOUND_CMD[s - 9u].id; }
static uint8_t held_ch[NINS][NHELD];            /* the channel that set it, EMPTY = not held */
static int16_t held_val[NINS][NHELD];
static volatile uint32_t held_gen;              /* bumped at every change (the instrument pages redraw) */
static uint8_t held_eng[NINS];                  /* the engine instrument i's held E0..E7 were set on */
static void held_check(uint32_t i)              /* another engine now (an edit, undo, a load): its held E0..E7 and
                                                 * their ramps go, they meant the old engine's slots (H1) */
{
    uint32_t c, k, s;
    if (held_eng[i] == ins[i].eng) return;
    held_eng[i] = ins[i].eng;
    for (s = 1; s < 9u; s++) held_ch[i][s] = EMPTY;
    for (c = 0; c < NCH; c++)
        for (k = 0; k < NFX; k++) {
            ramp_t *rp = &chs[c].ramp[k];
            if (rp->on && rp->ins == i && rp->code >= (CMD_SOUND | P_E0) && rp->code <= (CMD_SOUND | P_E7)) { rp->on = 0; ramp_ph[c][k] = EMPTY; }
        }
    held_gen++;
}
static void held_apply(uint32_t c)              /* part c: its instrument's held values over its own */
{
    uint32_t s, i = part_ins[c];
    held_check(i);
    for (s = 0; s < NHELD; s++)
        if (held_ch[i][s] != EMPTY) {
            uint32_t id = held_id(s);
            int e7 = id == P_E7 && trk[c].p[id] != held_val[i][s];
            trk[c].p[id] = held_val[i][s];
            if (e7) fm6_track_loaded(&trk[c]);
        }
}
static void hold(uint32_t i, uint32_t id, int32_t raw, uint32_t ch)
{
    uint32_t c, s = held_slot(id);
    held_check(i);
    if (s == NHELD) return;
    held_ch[i][s] = (uint8_t)ch;
    held_val[i][s] = (int16_t)raw;
    held_gen++;
    for (c = 0; c < NCH; c++) if (part_ins[c] == i) held_apply(c);
}
static void held_clear(void)                    /* start / stop: the instruments' own values again */
{
    uint32_t c;
    memset(held_ch, EMPTY, sizeof held_ch);
    held_gen++;
    for (c = 0; c < NCH; c++) part_load(&trk[c], part_ins[c], 0);
}

/* ---- scenes (docs/ux/scenes.md; model.js morph): the crossfader blends every value the scenes on A and B hold. Absolute,
 * as the Octatrack: a side that does not hold a value counts as what the song plays now (the instrument's value or the
 * held one); a number blends, a list switches at the middle. Applied every control block (scene_apply, ch_level) and
 * to F's effects in the main loop. Cost: per part at most 16 values a block, each a look-up in 8 (device item) */
static int32_t rdiv(int32_t a, int32_t b);
static int is_list(const param_desc_t *d) { return d->fmt == F_ENUM || d->fmt == F_ONOFF; }
static int32_t sc_find(uint32_t s, uint32_t t, uint32_t p)   /* scene s's value of (t, p): its place, -1 = none */
{
    uint32_t k;
    for (k = 0; k < SCENE_MAX && S.scn.k[s][k].t != EMPTY; k++) if (S.scn.k[s][k].t == t && S.scn.k[s][k].p == p) return (int32_t)k;
    return -1;
}
static int sc_holds(uint32_t t, uint32_t p) { return sc_find(S.scn.a, t, p) >= 0 || sc_find(S.scn.b, t, p) >= 0; }   /* (model.js
                                                 * sceneHolds: the scene on A or B holds it, the morph moves it) */
static int32_t morph(uint32_t t, uint32_t p, int32_t base, int list)
{
    int32_t a = sc_find(S.scn.a, t, p), b = sc_find(S.scn.b, t, p), av, bv;
    if (a < 0 && b < 0) return base;
    if (t < NINS) base = ins[t].p[p];           /* a scene holds it: the other end is the song's own setting, never a
                                                 * held or commanded value (decisions.md "Scenes over commands") */
    else if (t == SC_MIX) base = S.lvl[p];
    av = a < 0 ? base : S.scn.k[S.scn.a][a].v; bv = b < 0 ? base : S.scn.k[S.scn.b][b].v;
    return list ? (S.scn.xf * 2u < XF_MAX ? av : bv) : av + rdiv((bv - av) * (int32_t)S.scn.xf, (int32_t)XF_MAX);
}
static void scn_changed(void)                   /* the scenes hold other values: every part reloads, then morphs (main loop) */
{
    uint32_t i;
    for (i = 0; i < NINS; i++) ins_gen[i]++;
}
static int32_t ins_value(uint32_t i, uint32_t p)   /* what the song plays: the held value, else the instrument's */
{
    uint32_t h = held_slot(p);
    return h < NHELD && held_ch[i][h] != EMPTY ? held_val[i][h] : ins[i].p[p];
}
static void scene_apply(uint32_t c)             /* part c: the morph over its instrument's values (LEVEL: ch_level) */
{
    uint32_t i = part_ins[c], s, k;
    for (s = 0; s < 2u; s++) {
        uint32_t n = s ? S.scn.b : S.scn.a;
        for (k = 0; k < SCENE_MAX && S.scn.k[n][k].t != EMPTY; k++) {
            uint32_t p = S.scn.k[n][k].p;
            const param_desc_t *d;
            int32_t v;
            if (S.scn.k[n][k].t != i || p >= P_COUNT || p == P_LEVEL || (s && sc_find(S.scn.a, i, p) >= 0)) continue;   /* (B: what A lacks) */
            d = param_desc_of(eng_idx(ins[i].eng), p);   /* (the ISR: never ins_desc's scratch track, code-review S1) */
            v = param_fit(d, morph(i, p, ins_value(i, p), is_list(d)));   /* (a value set under another engine) */
            if (trk[c].p[p] == v) continue;
            trk[c].p[p] = (int16_t)v;
            if (p == P_E7) fm6_track_loaded(&trk[c]);
        }
    }
}

/* part c's level: its instrument's (or the held one) with the channel's mixer level added, both in 0.5 dB steps,
 * 112 = 0 dB, 0 = off (app.js syncMix: the gains multiply); both through the scenes */
static int16_t ch_level(uint32_t c)
{
    uint32_t i = part_ins[c];
    int32_t l = morph(i, P_LEVEL, prv_lvl >= 0 && c == prv_ch ? prv_lvl : ins_value(i, P_LEVEL), 0);
    int32_t m = morph(SC_MIX, c, ch_tvl[c] >= 0 ? ch_tvl[c] : S.lvl[c], 0), v = l + m - 112;   /* (TVL: over the mix; a
                                                 * scene's morph over both, decisions.md "Scenes over commands") */
    if (playing && S.mute_mode && !audible(c)) return 0;   /* MUTE AUDIO: a muted track plays on, silent (its sends too:
                                                 * fx.c mix_part takes them after the level) */
    return (int16_t)(!l || !m || v < 1 ? 0 : v > 127 ? 127 : v);
}

static void part_take(uint32_t c, uint32_t i)    /* part c plays instrument i from now on */
{
    if (i == part_ins[c]) return;
    part_load(&trk[c], i, 1);                   /* another sound: Felucca fades the engine over (engine_block) */
    part_ins[c] = i;
    part_gen[c] = ins_gen[i];
    held_apply(c);
}
static void part_play(uint32_t c, uint32_t note, uint32_t i, uint32_t vel)
{
    track_t *t = &trk[c];
    part_take(c, i);
    if (chs[c].note) trk_note_off(t, chs[c].note);
    trk_note_on(t, note, vel);
    chs[c].note = note;
}
/* a note without an instrument (HANDOFF section 0 "Playback", user): the sounding note moves to it, its envelope
 * goes on (no new attack; the instrument's GLIDE applies). Felucca's own legato is its MONO mode only, so the voices
 * and the mono stack are moved here (audio.js: the voice's frequency) */
static void part_legato(uint32_t c, uint32_t note)
{
    track_t *t = &trk[c];
    uint32_t i, old = chs[c].note;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && t->v[i].note == old) {
            t->v[i].note = (uint8_t)note;
            t->v[i].pitch16 = (int32_t)note * 16;
            glide_set(t, &t->v[i], t->p[P_GLIDE] != 0);
        }
    for (i = 0; i < t->nmono; i++) if (t->mono_stack[i] == old) t->mono_stack[i] = (uint8_t)note;
    if (t->mono_note == old) t->mono_note = (uint8_t)note;
    chs[c].note = note;
}
static void part_stop(uint32_t c)               /* the note's release */
{
    if (chs[c].note) trk_note_off(&trk[c], chs[c].note);
    chs[c].note = 0;
}
static void part_cut(uint32_t c)                /* KIL: a cut, faded over a block (~0.7 ms) so it does not click */
{
    uint32_t v;
    for (v = 0; v < NVOICE; v++) if (trk[c].v[v].active) voice_kill(&trk[c].v[v]);
    chs[c].note = 0;
}

/* the phrase (and transpose) channel c plays now; 0 = it is idle (audio.js resolve) */
static const crow_t *sel_at(uint32_t s, uint32_t c, uint32_t cr)   /* track c's phrase at song row s, chain row cr */
{
    uint32_t cn = S.song[s][c];
    return cn == EMPTY || S.chains[cn][cr].ph == EMPTY ? 0 : &S.chains[cn][cr];
}
static const crow_t *sel_place(uint32_t c, uint32_t s, uint32_t cr)   /* the place, or with sel_fill the nearest phrase
                                                 * above it (back up the chain, then up the song); sel_src: where */
{
    int32_t ss = (int32_t)s, cc = (int32_t)cr;
    for (; ss >= 0; ss--, cc = 15)              /* (at most 64 x 16 looks, at a phrase start) */
        for (; cc >= 0; cc--) {
            const crow_t *e = sel_at((uint32_t)ss, c, (uint32_t)cc);
            if (e) { sel_src[c] = (uint16_t)((uint32_t)ss << 4 | (uint32_t)cc); return e; }
            if (!(sel_fill >> c & 1u)) return 0;
        }
    return 0;
}
static row_t *sel_walk(uint32_t c, uint32_t *srow, uint32_t *crow, int32_t *tr)   /* M_SEL: the next place from here */
{
    uint32_t guard;
    const crow_t *e = 0;
    for (guard = 0; guard < NSONG * 16u + 2u && !e; guard++) {
        if (sel_lv == 0u) { e = sel_place(c, sel_s0, sel_c0); break; }
        if (sel_lv == 1u) {                     /* chain rows c0..c1 */
            if (*crow < sel_c0 || *crow > sel_c1) *crow = sel_c0;
            if (!(e = sel_place(c, sel_s0, *crow))) *crow = *crow + 1u;
            continue;
        }
        if (*srow < sel_s0 || *srow > sel_s1) { *srow = sel_s0; *crow = 0; }   /* song rows s0..s1 */
        if (S.song[*srow][c] == EMPTY) {        /* an empty song cell: a gap, its one phrase from above (SHIFT) */
            if (!*crow && (e = sel_place(c, *srow, 15))) break;
        } else if (*crow < 16u && (e = sel_at(*srow, c, *crow))) { sel_src[c] = (uint16_t)(*srow << 4 | *crow); break; }
        *crow = 0; *srow = *srow + 1u;
    }
    if (!e) return 0;
    *tr = e->tr;
    return S.phrases[e->ph];
}
static row_t *walk(uint32_t c, uint32_t *srow, uint32_t *crow, int32_t *tr)   /* from song row / chain row on */
{
    uint32_t guard;
    *tr = 0;
    if (mode == M_LOOP) return loop_phrase == EMPTY ? 0 : S.phrases[loop_phrase];
    if (mode == M_SEL) return sel_walk(c, srow, crow, tr);
    for (guard = 0; guard < NSONG + 2u; guard++) {   /* (every chain empty: idle, no endless loop) */
        uint32_t cn = mode == M_CHAIN ? loop_chain : S.song[*srow][c];
        const crow_t *cr;
        if (cn == EMPTY) {                      /* the end of this channel's song: back to the top */
            if (*srow) { *srow = 0; *crow = 0; continue; }
            return 0;
        }
        cr = &S.chains[cn][*crow];
        if (cr->ph == EMPTY) {                  /* the end of the chain: the next song row (a chain loops); an empty
                                                 * row 0 too, as q_walk (my pick, to review) */
            if (mode == M_SONG) { *crow = 0; *srow = (*srow + 1u) % NSONG; continue; }
            if (*crow) { *crow = 0; continue; }
            return 0;
        }
        *tr = cr->tr;
        return S.phrases[cr->ph];
    }
    return 0;
}
static row_t *resolve(uint32_t c, int32_t *tr) { return walk(c, &chs[c].srow, &chs[c].crow, tr); }
static uint32_t phrase_num(const row_t *rows) { return (uint32_t)((rows - &S.phrases[0][0]) / 16); }

/* ---- ramps (commands.md 10, model.js rampSpec / rampValue) */
/* curves over t = 0..16/16 (Q8): EXP (16^t - 1) / 15, LOG ln(1 + 15t) / ln 16; computed once, written here */
static const uint16_t CURVE_EXP[17] = {0, 3, 7, 12, 17, 24, 31, 40, 51, 64, 79, 98, 119, 145, 176, 213, 256};
static const uint16_t CURVE_LOG[17] = {0, 61, 98, 124, 144, 161, 175, 187, 198, 207, 216, 224, 231, 238, 245, 250, 256};
static int32_t rdiv(int32_t a, int32_t b) { return a >= 0 ? (a + b / 2) / b : -((-a + b / 2) / b); }
static int32_t clamp_i(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static int32_t s8(uint32_t v) { return v > 127u ? (int32_t)v - 256 : (int32_t)v; }   /* a byte as -128 .. 127 */
static int32_t curve_q12(uint32_t curve, int32_t t)   /* t, result: 0..4096 */
{
    const uint16_t *tb = curve == 1u ? CURVE_EXP : CURVE_LOG;
    int32_t i = t >> 8, f = t & 255;
    if (curve == 0u || t >= 4096) return t > 4096 ? 4096 : t;
    return (tb[i] * (256 - f) + tb[i + 1] * f) >> 4;
}
static int ramp_spec(const row_t *rows, uint32_t r, uint32_t k, ramp_t *sp)   /* 0 = no ramp under row r, column k */
{
    const fx_t *f = &rows[r].fx[k];
    uint32_t j, end = r;
    int has_step = 0, has_to = 0;
    if (!f->c || cmd_is_ramp(f->c)) return 0;
    if (f->c == CMD_KIL || f->c == CMD_OFF || f->c == CMD_DEL || f->c == CMD_HOP || f->c == CMD_CHA
        || f->c == CMD_MUT || f->c == CMD_SOL) return 0;           /* never moved by the player: no ramp (U44, my pick, to review) */
    sp->code = f->c; sp->from = f->v; sp->step = 0; sp->to = -1; sp->rows = 0; sp->curve = 0;
    for (j = r + 1u; j < 16u; j++) {
        const fx_t *g = &rows[j].fx[k];
        if (!cmd_is_ramp(g->c)) break;
        end = j;
        if (g->c == CMD_REP) { sp->step = (int16_t)s8(g->v); has_step = 1; }
        else if (g->c == CMD_RTO) { sp->to = g->v; has_to = 1; }
        else if (g->c == CMD_RTI) sp->rows = g->v;
        else sp->curve = (uint8_t)(g->v < 2 ? g->v : 2);
    }
    if (end == r || (!has_step && !has_to) || (has_step && !sp->step)) return 0;
    sp->ticks = 0;
    sp->on = 1;
    return 1;
}
static int32_t ramp_value(ramp_t *sp, int *done)   /* after sp->ticks ticks */
{
    int32_t v;
    *done = 0;
    if (sp->code == CMD_ARP || sp->code == CMD_VIB) {   /* each digit on its own, 0-F, moving once a row; a REP adds its
                                                 * step to both (decisions.md "Rampable ARP and VIB"; model.js digitSpec) */
        ramp_t d = *sp;
        int32_t dv[2];
        int dd[2];
        uint32_t i;
        d.code = 0;
        d.ticks = sp->ticks - sp->ticks % TICKS;
        for (i = 0; i < 2u; i++) {
            d.from = (int16_t)(sp->from >> (4u - 4u * i) & 15);
            d.to = (int16_t)(sp->to < 0 ? -1 : sp->to >> (4u - 4u * i) & 15);
            dv[i] = ramp_value(&d, &dd[i]);
            dd[i] = dd[i] || dv[i] <= 0 || dv[i] >= 15;
            dv[i] = clamp_i(dv[i], 0, 15);
        }
        *done = dd[0] && dd[1];
        return dv[0] << 4 | dv[1];
    }
    if (sp->rows || !sp->step) {               /* by time: RTI rows (RTO alone: 1 row) to RTO, or where REP gets */
        int32_t n = sp->rows ? sp->rows : 1, to = sp->to >= 0 ? sp->to : sp->from + sp->step * n, t;
        to = clamp_i(to, 0, 255);
        t = (int32_t)(sp->ticks * 4096u / (uint32_t)(n * (int32_t)TICKS));
        if (t >= 4096) { t = 4096; *done = 1; }
        return sp->from + rdiv((to - sp->from) * curve_q12(sp->curve, t), 4096);
    }
    v = sp->from + rdiv(sp->step * (int32_t)sp->ticks, TICKS);   /* by step: REP a row, until RTO or the range end */
    if (sp->to >= 0 && (sp->step > 0 ? v >= sp->to : v <= sp->to)) { v = sp->to; *done = 1; }
    if (v < 0 || v > 255) { v = v < 0 ? 0 : 255; *done = 1; }
    return v;
}

/* ---- a row: started at its tick 0, struck at its DEL tick */
static uint32_t rng_state_cha = 0x2545F491u;
static uint32_t cha_rnd15(void)                 /* 0..14, uniform enough */
{
    rng_state_cha ^= rng_state_cha << 13;
    rng_state_cha ^= rng_state_cha >> 17;
    rng_state_cha ^= rng_state_cha << 5;
    return (rng_state_cha >> 8) % 15u;
}
static int cha_skips(uint32_t n) { return n < 15u && (n == 0u || cha_rnd15() >= n); }   /* model.js chanceRow roll */
static void cha_apply(row_t *w, uint32_t k, int l, int r)   /* the row as it plays, CHA in column k: a lost side gone
                                                 * (model.js chanceApply) */
{
    uint32_t j;
    for (j = 0; j < NFX; j++) if ((j < k && l) || (j > k && r)) { w->fx[j].c = 0; w->fx[j].v = 0; }
    if (l) { w->n = EMPTY; w->vol = EMPTY; w->i = EMPTY; }
}

static void channel_row(uint32_t c)             /* channel c at its row's tick 0 */
{
    int32_t tr, sw;
    const row_t *rows = resolve(c, &tr);
    row_t *w = &chs[c].pend;
    uint32_t k;
    if (!rows) { chs[c].active = 0; ch_pos[c] = 0; part_stop(c); return; }
    chs[c].active = 1;
    chs[c].rows = rows;
    *w = rows[chs[c].row];
    if (chs[c].entering && mode != M_LOOP) {    /* HOP: the row this phrase was entered at, where it sits */
        entry_row[c][chs[c].srow][chs[c].crow] = (uint8_t)chs[c].row;
    }
    if (chs[c].entering) {                      /* what sounds into this phrase, for its timeline (U37) */
        int32_t left = chs[c].kill_at >= 0 ? chs[c].kill_at - (int32_t)gtick : 0;
        cin_left[c] = (int16_t)(left > 0 ? left : 0);
        cin_how[c] = (uint8_t)((chs[c].note != 0) << 2 | (chs[c].kill_at >= 0 ? (chs[c].kill_cut ? 1 : 2) : 0));
    }
    chs[c].entering = 0;
    if (rec_skip == (0x8000u | phrase_num(rows) << 4 | chs[c].row)) { rec_skip = 0; w->n = EMPTY; }   /* app.js recSkip */
    skip_l[c][phrase_num(rows)] &= (uint16_t)~(1u << chs[c].row);
    skip_r[c][phrase_num(rows)] &= (uint16_t)~(1u << chs[c].row);
    ch_pos[c] = mode == M_SEL ? POS_AT((uint32_t)sel_src[c] >> 4, sel_src[c] & 15u, phrase_num(rows), chs[c].row)
                             : POS_AT(chs[c].srow, chs[c].crow, phrase_num(rows), chs[c].row);
    for (k = 0; k < NFX; k++)                   /* CHA xy: x = chance of what is left of it, y = of the right */
        if (w->fx[k].c == CMD_CHA) {
            int l = cha_skips(w->fx[k].v >> 4), r = cha_skips(w->fx[k].v & 15u);
            if (l) skip_l[c][phrase_num(rows)] |= (uint16_t)(1u << chs[c].row);   /* dimmed until it plays again */
            if (r) skip_r[c][phrase_num(rows)] |= (uint16_t)(1u << chs[c].row);
            cha_apply(w, k, l, r);
            break;
        }
    chs[c].delay = 0;
    for (k = 0; k < NFX; k++) {
        if (w->fx[k].c == CMD_DEL) chs[c].delay = w->fx[k].v < 5u ? w->fx[k].v : 5u;
        if (w->fx[k].c == CMD_SWG) { chs[c].swing = w->fx[k].v * 100 / 255; ch_swing[c] = (uint8_t)chs[c].swing; }   /* holds */
    }
    sw = chs[c].swing;                          /* SWING: odd rows up to half a row (3 ticks) late */
    if (chs[c].row % 2u == 1u && sw) {
        uint32_t d = (uint32_t)rdiv(sw * 3, 100);
        if (d > chs[c].delay) chs[c].delay = d < 5u ? d : 5u;
    }
    chs[c].pending = 1;
    chs[c].ret_left = 0;
    chs[c].tr = tr;
}

/* ARP xy (+x, +y semitones), VIB xy (speed x, depth y; its wobble goes on from where it is), PIT xx (signed speed): set,
 * or with 00 stopped; held until changed (decisions.md "Rampable ARP and VIB"); pfx_held keeps them for the ghost */
static volatile uint8_t pfx_held[NCH][3];       /* ISR -> main: ARP, VIB, PIT held per channel (0 = none) */
static void pitch_fx(uint32_t c, uint32_t code, uint32_t v)
{
    if (code == CMD_ARP) { chs[c].arp_on = v != 0u; chs[c].arp_x = (int32_t)(v >> 4); chs[c].arp_y = (int32_t)(v & 15u); }
    else if (code == CMD_VIB) { chs[c].vib_speed = (int32_t)(v >> 4); chs[c].vib_depth = (int32_t)(v & 15u); }
    else chs[c].bend_rate = s8(v) * 16;         /* 1/16 st a tick */
    pfx_held[c][code == CMD_ARP ? 0 : code == CMD_VIB ? 1 : 2] = (uint8_t)v;
}
static void channel_strike(uint32_t c)          /* the row's DEL tick: its commands, then its note */
{
    const row_t *w = &chs[c].pend;
    uint32_t k, ri = w->i != EMPTY ? w->i : chs[c].ins;
    int32_t tr = chs[c].tr, ret = 0, kil = 0;
    chs[c].pending = 0;
    chs[c].ins = ri;                            /* a row's instrument, note or not: its commands' and the rows' below
                                                 * (as row_inst / q_walk; my pick, to review) */
    for (k = 0; k < NFX; k++) {
        uint32_t cc = w->fx[k].c, v = w->fx[k].v;
        if (tv_track(cc) >= 0) {                /* any track's (touch: the hand wins on that track) */
            uint32_t tt = (uint32_t)tv_track(cc);
            if (!touched(TOUCH_TVL >> 8, tt)) { ch_tvl[tt] = (int16_t)tvl_raw(v); tvl_by[tt] = (uint8_t)c; }
        }
        else if ((cc == CMD_MUT || cc == CMD_SOL) && v >> 4 <= NCH) {   /* xy: track x (0 = this one, 1-4; 5-F: nothing),
                                                 * y off / on; before the row's note (MUT 01 + a note is silent) */
            uint32_t t = v >> 4 ? (v >> 4) - 1u : c, on = (v & 15u) != 0u;
            if (cc == CMD_MUT) { live_mute[t] = (uint8_t)on; if (on) live_solo[t] = 0; }   /* one on turns the other off */
            else { live_solo[t] = (uint8_t)on; if (on) live_mute[t] = 0; }                 /* (X's rule, unit feedback #17) */
        }
        /* TPO below 40: 40, as the ramp and the text; two HOPs: the first, as first_hop (my picks, to review) */
        else if (cc == CMD_TPO) { tempo = (uint8_t)(v < 40u ? 40u : v); tempo_ch = (uint8_t)c; }
        else if (cc == CMD_SCN && tch[NTOUCH - 1u] == 0xFFFFu) S.scn.xf = (uint8_t)((v * XF_MAX + 127u) / 255u);   /* global: the last
                                                 * channel wins; not while the hand is on the fader (touch) */
        else if (cc == CMD_HOP && chs[c].hop < 0) chs[c].hop = (int32_t)(v & 15u);
        else if (cc >= CMD_SOUND && !touched(ri, cc & 0x7Fu)) hold(ri, cc & 0x7Fu, cmd_target(cc, ri, v), c);   /* held until
                                                 * changed; not while the hand moves it (touch) */
    }
    for (k = 0; k < NFX; k++) {                 /* ramps start; another command in the column, or REP 00, stops one */
        const fx_t *f = &w->fx[k];
        if (f->c == CMD_REP && !f->v) { chs[c].ramp[k].on = 0; ramp_ph[c][k] = EMPTY; }
        else if (f->c && !cmd_is_ramp(f->c)) {
            if (!ramp_spec(chs[c].rows, chs[c].row, k, &chs[c].ramp[k])) { chs[c].ramp[k].on = 0; ramp_ph[c][k] = EMPTY; }
            else { chs[c].ramp[k].ins = (uint8_t)ri; chs[c].ramp[k].now = f->v; ramp_ph[c][k] = (uint8_t)phrase_num(chs[c].rows); }
        }
    }
    if (w->n == NOTE_OFF)
        part_stop(c);
    else if (w->n != EMPTY && w->i == EMPTY) {  /* no instrument: legato, or nothing (shown dimmed on this pass) */
        int32_t n = (int32_t)w->n + tr;
        uint16_t bit = (uint16_t)(1u << chs[c].row);
        if (chs[c].note && audible(c)) {
            chs[c].base = (uint32_t)clamp_i(n, 0, 127);
            chs[c].bend_q8 = 0;
            part_legato(c, chs[c].base);
            unheard[c][phrase_num(chs[c].rows)] &= (uint16_t)~bit;
        } else unheard[c][phrase_num(chs[c].rows)] |= bit;
    } else if (w->n != EMPTY && (audible(c) || S.mute_mode)) {   /* transposed by the chain row; the volume column = the velocity */
        int32_t n = (int32_t)w->n + tr, vel = w->vol == EMPTY ? (int32_t)VOL_MAX : (int32_t)w->vol;
        chs[c].base = (uint32_t)clamp_i(n, 0, 127);
        chs[c].bend_q8 = 0;
        chs[c].kill_at = -1;                    /* a new note: an earlier KIL / OFF no longer applies */
        chs[c].ret_vel = vel < 1 ? 1 : vel;
        part_play(c, chs[c].base, ri, (uint32_t)chs[c].ret_vel);
    }
    for (k = 0; k < NFX; k++) {                 /* RET: from its cell, or its ramp's value */
        if (w->fx[k].c == CMD_RET) ret = w->fx[k].v;
        else if (!w->fx[k].c && chs[c].ramp[k].on && chs[c].ramp[k].code == CMD_RET) ret = chs[c].ramp[k].now;
    }
    if (w->n < NOTE_OFF) {                      /* the key flash (app.js leds: lastHit), when it is really struck */
        int32_t n = (int32_t)w->n + tr;
        hit_note[c] = (uint8_t)clamp_i(n, 0, 127);
        hit_ms[c] = 100;
        hit_seq[c]++;
    }
    if ((ret & 15) > 1 && chs[c].note) {        /* y hits spread evenly over the rest of the row, x = volume per hit */
        int32_t dur = (int32_t)(TICKS - chs[c].delay) * (int32_t)(TICK_X_TEMPO / tempo);
        chs[c].ret_left = (ret & 15) - 1;
        chs[c].ret_gap = dur / (ret & 15);
        chs[c].ret_next = chs[c].ret_gap;
        chs[c].ret_x = ret >> 4;
        hit_ms[c] = (uint16_t)(chs[c].ret_gap * 500 / FS);   /* RET: a flash of half the gap between hits */
    }
    for (k = 0; k < NFX; k++) {                 /* two KIL / OFF on a row: the first (U48, my pick, to review) */
        uint32_t cc = w->fx[k].c, v = w->fx[k].v;
        if (cc == CMD_ARP || cc == CMD_VIB || cc == CMD_PIT) pitch_fx(c, cc, v);
        else if ((cc == CMD_KIL || cc == CMD_OFF) && !kil) { chs[c].kill_at = (int32_t)(gtick + v); chs[c].kill_cut = cc == CMD_KIL; kil = 1; }   /* across rows */
    }
}

static void channel_tick(uint32_t c)            /* every tick, after the row's start / strike */
{
    uint32_t k;
    int32_t vib = 0, arp = 0;
    for (k = 0; k < NFX; k++) {                 /* the ramps move */
        ramp_t *rp = &chs[c].ramp[k];
        int done;
        int32_t v;
        if (!rp->on) continue;
        if (rp->code >= CMD_SOUND) { held_check(rp->ins); if (!rp->on || rp->ins != part_ins[c]) continue; }   /* (a ramp
                                                 * belongs to its instrument: paused while the channel plays another,
                                                 * picks review #28) */
        rp->ticks++;
        v = ramp_value(rp, &done);
        if (tv_track(rp->code) >= 0) {
            uint32_t tt = (uint32_t)tv_track(rp->code);
            if (!touched(TOUCH_TVL >> 8, tt)) { ch_tvl[tt] = (int16_t)tvl_raw((uint32_t)v); tvl_by[tt] = (uint8_t)c; }
        }
        else if (rp->code == CMD_TPO) { tempo = (uint8_t)(v < 40 ? 40 : v); tempo_ch = (uint8_t)c; }
        else if (rp->code == CMD_SCN) { if (tch[NTOUCH - 1u] == 0xFFFFu) S.scn.xf = (uint8_t)(((uint32_t)v * XF_MAX + 127u) / 255u); }
        else if (rp->code == CMD_PIT || rp->code == CMD_ARP || rp->code == CMD_VIB) pitch_fx(c, rp->code, (uint32_t)v);
        else if (rp->code == CMD_SWG) chs[c].swing = v * 100 / 255;
        else if (rp->code >= CMD_SOUND && !touched(rp->ins, rp->code & 0x7Fu)) hold(rp->ins, rp->code & 0x7Fu, cmd_target(rp->code, rp->ins, (uint32_t)v), c);
        rp->now = v;
        if (rp->code == CMD_SWG) ch_swing[c] = (uint8_t)chs[c].swing;
        if (done) { rp->on = 0; ramp_ph[c][k] = EMPTY; }
    }
    if (!chs[c].note) return;
    chs[c].bend_q8 += chs[c].bend_rate;         /* PIT: the slide adds up while the note lasts */
    if (chs[c].vib_depth) {                     /* VIB: +-depth/8 semitones, a cycle every 64/speed ticks */
        chs[c].vib_ph += chs[c].vib_speed ? chs[c].vib_speed : 1;
        vib = SINE[(uint32_t)(chs[c].vib_ph * 16) & 1023u] * chs[c].vib_depth / 1024;   /* /32 /8 x 256 */
    }
    if (chs[c].arp_on) arp = (tick % 3u == 1u ? chs[c].arp_x : tick % 3u == 2u ? chs[c].arp_y : 0) * 256;
    midi_bend_target[c] = arp + chs[c].bend_q8 + vib;
    if (chs[c].kill_at == (int32_t)gtick) {     /* KIL: cut; OFF: the release */
        if (chs[c].kill_cut) part_cut(c); else part_stop(c);
        chs[c].kill_at = -1;
    }
}

static void channel_advance(uint32_t c)         /* audio.js advance; HOP y: the next phrase from its row y */
{
    if (mode == M_SEL) {                        /* the loop wins: a HOP is ignored; past its last row, the next place */
        chs[c].hop = -1;
        if (++chs[c].row <= (sel_lv ? 15u : sel_r1)) return;
        chs[c].row = sel_lv ? 0u : sel_r0;
        chs[c].entering = 1;
        chs[c].crow++;                          /* (sel_walk wraps it, and the song row) */
        if (sel_lv == 2u && (chs[c].crow >= 16u || S.song[chs[c].srow][c] == EMPTY)) { chs[c].crow = 0; chs[c].srow++; }
        return;
    }
    chs[c].row++;
    if (chs[c].hop >= 0) { chs[c].row = 16u + (uint32_t)chs[c].hop; chs[c].hop = -1; }
    if (chs[c].row < 16u) return;
    if (end_ch == c + 1u) end_now = 1;
    chs[c].row &= 15u;
    chs[c].entering = 1;
    if (mode == M_LOOP) {
        if (loop_next != EMPTY) loop_phrase = loop_next;   /* the phrase loop follows the screen */
    } else if (mode == M_CHAIN) {
        if (++chs[c].crow < 16u && S.chains[loop_chain][chs[c].crow].ph == EMPTY && rec_on && rec_spare != EMPTY) {
            S.chains[loop_chain][chs[c].crow].ph = rec_spare;   /* recording through a chain: an empty row gets */
            rec_spare = EMPTY;                  /* a new phrase instead of wrapping (audio.js onEmpty) */
        }
        if (chs[c].crow >= 16u || S.chains[loop_chain][chs[c].crow].ph == EMPTY) chs[c].crow = 0;
    } else {
        uint32_t cn = S.song[chs[c].srow][c];
        if (++chs[c].crow >= 16u || cn == EMPTY || S.chains[cn][chs[c].crow].ph == EMPTY) {
            chs[c].crow = 0;
            chs[c].srow = (chs[c].srow + 1u) % NSONG;
        }
    }
}

static int channel_plays(uint32_t c) { return mode == M_SONG || (mode == M_SEL ? sel_on >> c & 1u : c == loop_ch); }

static void player_tick(void)
{
    uint32_t c;
    if ((count_left || S.pj.metro == 2u || (S.pj.metro == 1u && rec_on)) && beat_tick % 24u == 0u) {
        /* the metronome (the count-in, METRO ON, METRO REC while recording; app.js playOpts): accented on the bar */
        click_left = FS * 40 / 1000;
        click_inc = beat_tick % 96u == 0u ? 155826479u : 97391549u;   /* 1600 / 1000 Hz x 2^32 / 44100 */
    }
    beat_tick++;
    if (count_left) count_left--;               /* the count-in: clicks, no rows */
    else {
    for (c = 0; c < NCH; c++) {
        if (!channel_plays(c)) continue;
        if (tick == 0) channel_row(c);
        if (chs[c].active && chs[c].pending && tick == chs[c].delay) channel_strike(c);
        if (chs[c].active) channel_tick(c);
    }
    gtick++;
    ui_tick = (uint8_t)(tick + 1u);
    if (++tick == TICKS) {
        tick = 0;
        for (c = 0; c < NCH; c++) if (channel_plays(c) && chs[c].active) channel_advance(c);
    }
    }
    tick_left += (int32_t)(TICK_X_TEMPO / tempo);   /* the remainder kept */
    tick_rem += TICK_X_TEMPO % tempo;
    if (tick_rem >= tempo) { tick_rem -= tempo; tick_left++; }
}

static void player_retrig(uint32_t c, uint32_t n)   /* RET's hits, at their samples (to the control block) */
{
    if (chs[c].ret_left <= 0 || !chs[c].note) return;
    chs[c].ret_next -= (int32_t)n;
    if (chs[c].ret_next > 0) return;
    if (chs[c].ret_x) {                         /* x: 1..7 softer, 8..F louder, per hit */
        int32_t x = chs[c].ret_x, v = chs[c].ret_vel;
        v = x < 8 ? v * (16 - x) / 16 : v * (16 + x - 7) / 16;
        chs[c].ret_vel = clamp_i(v, 1, 127);
    }
    trk_note_off(&trk[c], chs[c].note);
    trk_note_on(&trk[c], chs[c].note, (uint32_t)chs[c].ret_vel);
    hit_seq[c]++;                               /* (hit_note / hit_ms: the strike's) */
    chs[c].ret_left--;
    chs[c].ret_next += chs[c].ret_gap;
}

static void player_start(void)
{
    uint32_t c, k;
    for (c = 0; c < NCH; c++) part_stop(c);
    held_clear();
    mode = play_mode;
    tempo = S.tempo;
    tempo_ch = EMPTY;
    loop_ch = play_ch;
    loop_chain = play_chain;
    loop_phrase = play_phrase;
    for (c = 0; c < NCH; c++) {
        chs[c].srow = mode == M_SEL ? sel_s0 : play_from_song;
        chs[c].crow = mode == M_CHAIN ? play_from_chain : mode == M_SEL && sel_lv < 2u ? sel_c0 : 0;
        chs[c].row = mode == M_SEL && !sel_lv ? sel_r0 : 0;
        chs[c].ins = 0;
        chs[c].active = chs[c].pending = 0;
        chs[c].hop = -1;
        chs[c].entering = 1;
        chs[c].kill_at = -1;
        ch_tvl[c] = -1;
        live_mute[c] = live_solo[c] = EMPTY;
        chs[c].swing = S.pj.swing;              /* the song's SWING, until a SWG */
        chs[c].arp_on = chs[c].bend_q8 = chs[c].bend_rate = chs[c].vib_depth = chs[c].vib_ph = chs[c].ret_left = 0;
        memset((void *)pfx_held[c], 0, 3);
        for (k = 0; k < NFX; k++) { chs[c].ramp[k].on = 0; ramp_ph[c][k] = EMPTY; }
        ch_swing[c] = S.pj.swing;
        midi_bend_target[c] = 0;
        ch_pos[c] = 0;
    }
    tick = 0; gtick = 0; tick_left = 0; tick_rem = 0; aud_left = 0;
    beat_tick = 0; count_left = 24u * play_count; end_now = 0;
    memset(skip_l, 0, sizeof skip_l);
    memset(skip_r, 0, sizeof skip_r);
    memset(unheard, 0, sizeof unheard);
    memset(entry_row, EMPTY, sizeof entry_row);
    playing = 1;
}

static void player_stop(void)                   /* the ISR at a stop; play() when the ISR does not come (P1) */
{
    uint32_t c;
    playing = 0;
    end_now = 0;
    for (c = 0; c < NCH; c++) { part_stop(c); ch_tvl[c] = -1; live_mute[c] = live_solo[c] = EMPTY; ch_pos[c] = 0; midi_bend_target[c] = 0; memset((void *)ramp_ph[c], EMPTY, NFX); ch_swing[c] = 0; }
    held_clear();
    play_req = 0;
}
static void events_block(uint32_t n)            /* fx.c mix_block: once per control block, before the parts */
{
    uint32_t c;
    if (play_req == 1) { player_start(); play_req = 0; }
    else if (play_req == 2 || end_now) player_stop();
    if (kh_req) { hold(kh_ins, kh_id, kh_raw, kh_ch); kh_req = 0; }
    if (mix_req) {                              /* a hand on MUTE / SOLO: its live value goes (idempotent: a race only repeats it) */
        uint8_t r = mix_req;
        mix_req = 0;
        for (c = 0; c < NCH; c++) { if (r >> c & 1u) live_mute[c] = EMPTY; if (r >> (4u + c) & 1u) live_solo[c] = EMPTY; }
    }   /* a knob recorded: heard at once (app.js setHeld) */
    if (aud_req) {                              /* the prototype's audition: while EDIT is held, else 180 ms (a knob) */
        aud_req = 0;
        if (!playing) {                         /* (one on another channel meanwhile: that one ends first, A1) */
            if (aud_left > 0) part_stop(aud_on);
            if (aud_fxd) { aud_fxd = 0; part_load(&trk[aud_on], part_ins[aud_on], 0); held_apply(aud_on); }   /* (the last
                                                 * one's row commands go) */
            aud_on = aud_ch;
            aud_wait = aud_hold;
            part_stop(aud_on);
            if (aud_ph != EMPTY) {              /* an FX cell (unit-feedback #26): the row's sound commands on it */
                uint32_t k;
                part_take(aud_on, aud_ins);
                for (k = 0; k < NFX; k++) {     /* shortcut: a VOL command is not heard here (ch_level; Q's has prv_lvl) */
                    const fx_t *f = &S.phrases[aud_ph][aud_r].fx[k];
                    if (f->c >= CMD_SOUND) { trk[aud_on].p[f->c & 0x7Fu] = (int16_t)cmd_target(f->c, aud_ins, f->v); aud_fxd = 1; }
                }
            }
            part_play(aud_on, aud_note, aud_ins, VOL_MAX); aud_left = FS * 180 / 1000;
        }
    }
    if (aud_left > 0 && (aud_wait ? !aud_hold : (aud_left -= (int32_t)n) <= 0)) {   /* (the envelope's release) */
        aud_left = 0; part_stop(aud_on);
        if (aud_fxd) { aud_fxd = 0; part_load(&trk[aud_on], part_ins[aud_on], 0); held_apply(aud_on); }
    }
    if (prv_req) {                              /* EDIT + PLAY: the note while PLAY is held, its row's sound commands
                                                 * on it alone (audio.js audition: steal, fx) */
        uint32_t c = prv_ch, k;
        if (prv_req == 1u) {
            part_cut(c);                        /* (playing: the channel's sounding note goes) */
            aud_left = 0;                       /* (EDIT's own audition would release it) */
            part_take(c, prv_ins);
            if (prv_ph != EMPTY)
                for (k = 0; k < NFX; k++) {
                    const fx_t *f = &S.phrases[prv_ph][prv_r].fx[k];
                    if (f->c >= CMD_SOUND) trk[c].p[f->c & 0x7Fu] = (int16_t)cmd_target(f->c, prv_ins, f->v);
                    if (f->c == (CMD_SOUND | P_LEVEL)) prv_lvl = trk[c].p[P_LEVEL];   /* (ch_level sets it each block) */
                }
            trk_note_on(&trk[c], prv_note, VOL_MAX);
            chs[c].note = prv_note;
        } else {
            if (chs[c].note == prv_note) part_stop(c);
            prv_lvl = -1;
            part_load(&trk[c], part_ins[c], 0); /* the instrument's own values again */
            held_apply(c);
        }
        prv_req = 0;
    }
    if (key_want != key_had) {                  /* notes mode: a key sounds while held, on the cursor's channel; stopped,
                                                 * the part takes its instrument; playing, the channel's own sound */
        uint32_t k, want = key_want;
        for (k = 0; k < NKEYS; k++) {
            uint32_t b = 1u << k;
            if ((want ^ key_had) & b) {
                if (want & b) { if (!playing) part_take(key_ch[k], key_ins[k]); trk_note_on(&trk[key_ch[k]], key_note[k], KEY_VEL(k)); }
                else trk_note_off(&trk[key_ch[k]], key_note[k]);
            }
        }
        key_had = want;
    }
    if (playing) {
        while (tick_left <= 0) player_tick();   /* the ticks that fall in this block */
        tick_left -= (int32_t)n;
        row_q8 = (int32_t)((tick ? tick : TICKS) * 256u) - tick_left * 256 / (int32_t)(TICK_X_TEMPO / tempo);
        for (c = 0; c < NCH; c++) player_retrig(c, n);
    }
    for (c = 0; c < NCH; c++) {
        if (playing && chs[c].note && !audible(c) && !S.mute_mode) part_cut(c);   /* (MUTE AUDIO: it plays on at level 0) */   /* muted / soloed out: silent at once;
                                                 * shortcut: unmuted mid-note, heard from the next note (the
                                                 * prototype's gain fade brings the sounding one back) */
        if (ins_gen[part_ins[c]] != part_gen[c]) {   /* the instrument was edited: its new values, live */
            part_gen[c] = ins_gen[part_ins[c]];
            part_load(&trk[c], part_ins[c], 0);
            held_apply(c);
        }
        scene_apply(c);
        trk[c].p[P_LEVEL] = ch_level(c);
        engine_block(&trk[c]);                  /* engine switch: fade, then switch (as seq.c) */
    }
}

static volatile uint8_t sound_ready;            /* app_main -> ISR: sound_init has run */
/* overload (Felucca's audio.c): a half that took > 85 % of its time sheds one voice before the next one, over all parts:
 * the quietest releasing voice fades out over the next block (voice_kill), else the oldest held one goes into its
 * release (stopped by a later shed if still needed). The only held voice is never touched, so a dense chord on a heavy
 * engine thins out instead of a late half (a click) */
static volatile uint8_t shed_req;
static uint32_t shed_count;
static volatile uint16_t cpu_pm;                /* the last half's time, per mille of its budget (G's SYSTEM popup) */
static void shed_voice(void)
{
    uint32_t p, i, ngate = 0;
    voice_t *best = 0;
    for (p = 0; p < NCH; p++)
        for (i = 0; i < NVOICE; i++) {
            voice_t *v = &trk[p].v[i];
            if (v->active && !v->gate && v->stage != 4u && (!best || v->env < best->env)) best = v;
        }
    if (best) { voice_kill(best); shed_count++; return; }
    for (p = 0; p < NCH; p++)
        for (i = 0; i < NVOICE; i++) {
            voice_t *v = &trk[p].v[i];
            if (v->active && v->gate) { ngate++; if (!best || v->age < best->age) best = v; }
        }
    if (ngate > 1u) { best->gate = 0; best->stage = 3; shed_count++; }
}
#ifdef WASM_EXPORT
WASM_EXPORT("emu_overload") void emu_overload(void) { shed_req = 1; }   /* (the emulator renders in no time: a half over
                                                 * budget, for tracker_emu.mjs) */
WASM_EXPORT("emu_sheds") uint32_t emu_sheds(void) { return shed_count; }
WASM_EXPORT("emu_voices") uint32_t emu_voices(void)   /* the sounding voices, two decimal digits each: part, then its
                                                 * envelope stage (+ 5 while held) */
{
    uint32_t p, i, m = 0;
    for (p = 0; p < NCH; p++)
        for (i = 0; i < NVOICE; i++) if (trk[p].v[i].active) m = m * 100u + p * 10u + trk[p].v[i].stage + (trk[p].v[i].gate ? 5u : 0u);
    return m;
}
#endif
static void app_audio(int32_t *out, uint32_t n)
{
    uint32_t i, t0 = fm1_ticks(), us;
    if (!sound_ready) return;                   /* (the platform starts the audio before app_main: Felucca's slicer
                                                 * divides by the song's BPM, still 0 until sound_init) */
    in_audio = 1;
    if (shed_req) { shed_req = 0; shed_voice(); }
    for (i = 0; i + CTL <= n; i += CTL) mix_block(out + 2u * i, CTL);
    in_audio = 0;
    us = (fm1_ticks() - t0) / FM1_TICKS_PER_US;
    if (us * 100u > n * 1000000u / FS * 85u) shed_req = 1;   /* (n: a half, 256 frames = 5.8 ms) */
    cpu_pm = (uint16_t)(us * 1000u / (n * 1000000u / FS));
    for (i = 0; i < n && click_left; i++, click_left--) {   /* the click: a sine, its level falling over 40 ms */
        int32_t v = (osc_sine(click_ph += click_inc) >> 2) * (int32_t)click_left / (int32_t)(FS * 40 / 1000);
        out[2u * i] += v; out[2u * i + 1u] += v;
    }
}

static int any_block(void);
static void blip_row(uint32_t c, uint32_t note, uint32_t i, uint32_t ph, uint32_t r)   /* audition on channel c's part
                                                 * (not while playing, not in a selection: picks review #27), with
                                                 * phrase ph row r's sound commands (EMPTY: none) */
{
    if (playing || any_block()) return;
    aud_ch = (uint8_t)c;
    aud_note = (uint8_t)note;
    aud_ins = (uint8_t)i;
    aud_ph = (uint8_t)ph; aud_r = (uint8_t)r;
    aud_req = 1;
}
static void blip(uint32_t c, uint32_t note, uint32_t i) { blip_row(c, note, i, EMPTY, 0); }

/* ---------------------------------------------------------------- the screen (render.js) --- */
#define CW 6
#define LH 12
#define X0 4
#define TOP 20
#define ROWS_Y (TOP + LH)
#define STRIP_Y (ROWS_Y + 16 * LH)
#define MAP_X 215
#define PREV_END 34                             /* previews end at this column, just left of the map */
static const uint16_t
    P_BG = RGB(0, 0, 0), P_BAND = RGB(0x2a, 0x20, 0x10), P_LINE = RGB(0x33, 0x33, 0x33),
    P_DIM = RGB(0x55, 0x55, 0x55), P_LABEL = RGB(0x77, 0x77, 0x77), P_ROW4 = RGB(0xbb, 0xbb, 0xbb),
    P_TEXT = RGB(0xe8, 0xe8, 0xe8), P_ACCENT = RGB(0xff, 0x9a, 0x2e), P_PLAY = RGB(0xff, 0x4f, 0x6d),
    P_CTX = RGB(0xd9, 0x9b, 0xd9), P_HELD = RGB(0xff, 0xc2, 0x3d), P_SEL = RGB(0x16, 0x32, 0x4a),
    P_SURF = RGB(0x1a, 0x1f, 0x2a), P_SURF2 = RGB(0x28, 0x30, 0x41), P_WHITE = RGB(0xff, 0xff, 0xff),
    P_CUROFF = RGB(0x7a, 0x4a, 0x18);           /* the cursor while an overlay has the focus (colours.md cursorOff) */
static const uint16_t P_CH[NCH] = {RGB(0x4f, 0xa3, 0xff), RGB(0x5f, 0xd3, 0x6a), RGB(0xf2, 0xd1, 0x4b), RGB(0x47, 0xd7, 0xe0)};
static const uint16_t P_GHOST[NCH] = {RGB(0x27, 0x51, 0x7f), RGB(0x2f, 0x69, 0x35), RGB(0x79, 0x68, 0x25), RGB(0x23, 0x6b, 0x70)};   /* the
                                                 * channels' at half brightness (render.js ghostCol) */

static void cv_glyph(int32_t x, int32_t y, char ch, uint16_t col)
{
    const uint8_t *g = FONT6X12[(ch < 32 || ch > 126) ? '?' - 32 : ch - 32];
    int32_t r, i;
    for (r = 0; r < (int32_t)FONT6X12_H; r++)
        if (g[r])
            for (i = 0; i < (int32_t)FONT6X12_W; i++)
                if (g[r] >> (FONT6X12_W - 1u - (uint32_t)i) & 1u)
                    cv_pset(x + i, y + r, col);
}
static void cv_tri(int32_t x, int32_t y, int dir, uint16_t col)   /* render.js triangle: 5 x 3, a ramp's way */
{
    int32_t j;
    for (j = 0; j < 3; j++) cv_rect(x + j, dir > 0 ? y + 6 - j : y + 4 + j, 5 - 2 * j, 1, col);
}
static void cv_frame(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t col)   /* render.js frame: 1 px */
{
    cv_rect(x, y, w, 1, col); cv_rect(x, y + h - 1, w, 1, col);
    cv_rect(x, y, 1, h, col); cv_rect(x + w - 1, y, 1, h, col);
}
static void cv_str(int32_t x, int32_t y, const char *s, uint16_t col)   /*  / : a small up / down triangle */
{
    for (; *s; s++, x += CW)
        if (*s == 1 || *s == 2) cv_tri(x, y, *s == 1 ? 1 : -1, col);
        else cv_glyph(x, y, *s, col);
}
static int32_t gx(int32_t cx) { return X0 + cx * CW; }
/* a cell of text on the line canvas: on a background (the cursor), or plain */
static void cv_cell(int32_t cx, const char *s, uint16_t col, int bg, uint16_t bgcol)
{
    if (bg) cv_rect(gx(cx) - 1, 0, (int32_t)str_len(s) * CW + 2, LH, bgcol);
    cv_str(gx(cx), 0, s, col);
}

static int in_block(uint32_t r, uint32_t c);      /* (rows and selections, below) */
static int flashed(uint32_t r, uint32_t c);       /* (undo / redo, below) */
static int undo_close(void);
static const char HEX[] = "0123456789ABCDEF";
static void fmt_hex(char *s, uint32_t v, uint32_t w) { uint32_t i; for (i = 0; i < w; i++) s[i] = HEX[(v >> (4u * (w - 1u - i))) & 15u]; s[w] = 0; }
static void fmt_hex2e(char *s, uint32_t v) { if (v == EMPTY) str_cpy(s, "--", 3); else fmt_hex(s, v, 2); }
static void fmt_tr(char *s, int32_t t)          /* +07 / -12 */
{
    uint32_t a = (uint32_t)(t < 0 ? -t : t);
    s[0] = t < 0 ? '-' : '+';
    s[1] = (char)('0' + a / 10u % 10u);
    s[2] = (char)('0' + a % 10u);
    s[3] = 0;
}
static void fmt_note(char *s, uint32_t n)
{
    static const char *const NAME[12] = {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"};
    if (n == EMPTY) { str_cpy(s, "---", 4); return; }
    if (n == NOTE_OFF) { str_cpy(s, "OFF", 4); return; }
    s[0] = NAME[n % 12u][0];
    s[1] = NAME[n % 12u][1];
    s[2] = (char)('0' + n / 12u - 1u);
    s[3] = 0;
}
static void sa_n(char *s, const char *t, uint32_t n) { str_cpy(s + str_len(s), t, n - str_len(s)); }   /* append, s holds n */
static void sn_n(char *s, int32_t v, uint32_t n) { char b[12]; fmt_int(b, v); sa_n(s, b, n); }
static void sa(char *s, const char *t) { sa_n(s, t, 40u); }   /* (the bottom line's 40) */
static void sn(char *s, int32_t v) { sn_n(s, v, 40u); }

/* ---------------------------------------------------------------- where the cursor is --- */
enum { V_SONG, V_CHAIN, V_PHRASE, V_INST, V_ENV, V_LFO, V_VOX, V_PROJ, V_MIX, V_FX, V_Q };   /* I E L V: the instrument's field pages;
                                                 * G: the project page */
#define IS_PAGE(v) ((v) >= V_INST && (v) <= V_VOX)
static const uint8_t PH_X[9] = {2, 6, 9, 11, 15, 18, 22, 25, 29}, PH_W[9] = {3, 2, 1, 3, 2, 3, 2, 3, 2};
static struct { uint32_t view, ch, song_row, chain_row, ccol, row, col, inst, field, char_pos, on_row, on_head, pf[4], prow, back, mf, ff, follow, qr, qc, qw, qt; } ui = {V_SONG};   /* mf: X's field (level, mute, solo); ff:
                                                 * F's; qr qc: Q's line and cell, qw: the cell picked with left / right, qt: its first line shown */
#define Q_NOTE 0x100u
static uint32_t q_filt;                         /* Q's SEQ filter (app.js qFilter): 0 = every line, Q_NOTE = the lines
                                                 * with a note, else a command */
static char q_want[4];                          /* its command's name, followed to other instruments (app.js qWant) */
static struct { uint8_t on, rows, cols; uint32_t r, c; } sel;            /* a selection being made: its start */
static struct { uint8_t on, view, rows, at; uint32_t r0, r1, c0, c1; } mark;  /* the copied block, still marked (at: its
                                                 * phrase / chain, mark_place) */
static uint8_t opt_copy;                        /* the OPTION held now copied a selection: + EDIT deletes it */
static uint32_t shown_pos[NCH];                 /* ch_pos as drawn */
static int16_t shown_tvl[NCH];                  /* ch_tvl as drawn (X) */
static uint8_t shown_mix[NCH];
static uint32_t shown_tempo;                    /* G's TEMPO as drawn (tempo | its channel << 8 | ramp + 1 << 16 | live) */                  /* the heard mute | solo << 1 as drawn */
enum { KB_PAD, KB_NOTES, KB_ARMED, KB_REC };
static uint32_t kb;                             /* the keybed (REC): the gamepad, notes (practise / step entry), armed
                                                 * (PLAY records), recording (live-record.md; notes-mode.md option B) */
static uint32_t rec_count, rec_wait;            /* notes this take; waiting for the next phrase (started while playing) */
static int32_t oct;                             /* OCT- / OCT+: the note keys' octave, -3 .. 3 */
static char msg[40];
static uint8_t msg_dirty;
static uint8_t proj_dirty;                      /* unsaved changes: every step, undone or redone (saving) */
static uint8_t pj_slot;                         /* the open project's slot (G page; kept in Dinghy's settings) */
static uint8_t msg_held;                        /* the message is a question / a wait (G page): amber */
static void say(const char *s) { str_cpy(msg, s, sizeof msg); msg_dirty = 1; msg_held = 0; }
static void say_held(const char *s) { say(s); msg_held = 1; }

static uint32_t cur_chain(void) { return S.song[ui.song_row][ui.ch]; }
static uint32_t cur_phrase(void)
{
    uint32_t cn = cur_chain();
    return cn == EMPTY ? EMPTY : S.chains[cn][ui.chain_row].ph;
}
static row_t *cur_phrase_rows(void) { uint32_t p = cur_phrase(); return p == EMPTY ? 0 : S.phrases[p]; }
static uint32_t mark_place(void) { return ui.view == V_PHRASE ? cur_phrase() : ui.view == V_CHAIN ? cur_chain() : 0; }
static int mark_shown(void) { return mark.on && mark.view == ui.view && mark.at == mark_place(); }
static uint32_t ensure_chain(void)              /* app.js ensureChain: a chain in the song cell, a free one if empty */
{
    uint32_t c = cur_chain();
    if (c == EMPTY) {
        c = next_free_chain(EMPTY);
        if (c == EMPTY) { say("no free chain"); return EMPTY; }
        S.song[ui.song_row][ui.ch] = (uint8_t)c;
    }
    return c;
}
static uint32_t ensure_phrase(void)
{
    uint32_t c = ensure_chain(), p;
    if (c == EMPTY) return EMPTY;
    p = S.chains[c][ui.chain_row].ph;
    if (p == EMPTY) {
        p = next_free_phrase(EMPTY);
        if (p == EMPTY) { say("no free phrase"); return EMPTY; }
        S.chains[c][ui.chain_row].ph = (uint8_t)p;
    }
    return p;
}
static uint32_t song_top(void)                  /* render.js scrollTop: the cursor in the middle, clamped */
{
    int32_t t = (int32_t)ui.song_row - 8;
    return (uint32_t)clamp_i(t, 0, (int32_t)NSONG - 16);
}

/* the S / C / P screen the G (F X) column hangs off (render.js mapAnchor): the screen itself, the one G was
 * entered from, else P */
static uint32_t map_anchor(void)
{
    return ui.view <= V_PHRASE ? ui.view : (ui.view == V_PROJ || ui.view == V_FX || ui.view == V_MIX) && ui.back <= V_PHRASE ? ui.back : V_PHRASE;
}

/* the map, 2D (render.js navMap, HANDOFF section 5): S C P I a row, G above and F X below the anchor, Q above I
 * and E L V below, on a surf panel; the current screen in the accent colour (no outline: no room at 7 px a letter). Drawn into
 * whatever canvas covers screen rows y0 .. y0 + h */
#define MAP_X0 212
static uint8_t map_lit = EMPTY;                 /* the FX grid open: the screen its cursor's group moves (draw_cmd_grid), white */
static void cv_map(int32_t y0, int32_t h)
{
    static const char L[] = "SCPIGFXQELV";
    static const uint8_t V[11] = {V_SONG, V_CHAIN, V_PHRASE, V_INST, V_PROJ, V_FX, V_MIX, V_Q, V_ENV, V_LFO, V_VOX},
        COL[11] = {0, 1, 2, 3, 0xFF, 0xFF, 0xFF, 3, 3, 3, 3}, ROW[11] = {1, 1, 1, 1, 0, 2, 3, 0, 2, 3, 4};
    uint32_t i, a = map_anchor();
    /* its own panel (user, 2026-10-11, principles-review.md #2): without it the letters read as more preview columns on S / C */
    cv_rect(MAP_X0 - 3, ROWS_Y - 2 - y0, 240 - MAP_X0 + 3, 5 * LH + 3, P_SURF);
    for (i = 0; i < 11u; i++) {
        int32_t y = ROWS_Y + ROW[i] * LH - y0, x = MAP_X0 + 7 * (int32_t)(COL[i] == 0xFF ? a : COL[i]);
        char s[2];
        s[0] = L[i];
        s[1] = 0;
        if (y + LH <= 0 || y >= h) continue;
        cv_str(x, y, s, V[i] == ui.view ? P_ACCENT : V[i] == map_lit ? P_TEXT : P_LABEL);
    }
}

/* the breadcrumb (render.js seqBar): CH2>S02>C04:0>P09>I2 NAME; fixed columns, crumbs past the screen's depth
 * hidden, the current one in the accent colour, the channel in its own; the phrase screen shows the chain row's
 * transpose, dim, in the free slot after the phrase */
/* scenes: the crossfader's position as a 3 x 3 mark on the top bar's line, only when it is not at A (render.js faderMark) */
static int32_t fader_x(void) { return (int32_t)((S.scn.xf * 235u + 63u) / 127u) + 1; }
static int loop_tag(void) { return playing && mode == M_SEL && kb != KB_ARMED && kb != KB_REC; }   /* LOOP in the bar */
/* the battery (docs/ux/system-popup.md 6): ADC ch3 read every 100 ms, averaged over ~3 s in x32 fixed point (Felucca's
 * raw += (v - raw) / 32 stops up to 31 counts short, 0.2 V) */
static int32_t batt_acc, batt_raw;
/* shortcut: volts = raw x 6.6 / 1023 (3.3 V reference, the divider halving the cell; Felucca's bars at raw 531 / 561 /
 * 591 = 3.43 / 3.62 / 3.81 V) and 3.30 V = empty, 4.15 V = full in a line, until the unit is calibrated (system-popup.md) */
static uint32_t batt_mv(void) { return (uint32_t)batt_raw * 6600u / 1023u; }
static uint32_t batt_pm(void)                   /* 0..1000 */
{
    uint32_t v = batt_mv();
    return v <= 3300u ? 0u : v >= 4150u ? 1000u : (v - 3300u) * 20u / 17u;   /* (x 1000 / 850: written so, the compiler's
                                                 * -3300000 reads as a ROM address to build.py's check) */
}
/* what the mark shows (redrawn when it changes): steps 0..4, colour (0 grey, 1 orange < 20 %, 2 red < 5 %, 3 green: on
 * USB), the red blink's off half second. On USB (charging: there is no charge-status line; user 2026-10-12) the fill
 * climbs from the measured step to full, a step every 0.6 s (Felucca's pace), still once full; it never blinks */
static uint32_t batt_state(void)
{
    uint32_t pm = batt_pm(), n = (pm * 4u + 500u) / 1000u, low = pm < 50u;
    if (!batt_raw) return 0;
    if (usb_up()) return 1u | (n < 4u ? n + fm1_ms / 600u % (5u - n) : n) << 1 | 3u << 4;
    return 1u | n << 1 | (low ? 2u : pm < 200u) << 4 | (low && (fm1_ms / 500u) % 2u) << 7;
}
#define BATT_W 14                               /* the mark's room at the far right: the tag and tempo sit that much left */
static void batt_mark(void)                     /* render.js battMark: a 12 x 7 outline + nub, filled in 4 steps */
{
    static const uint8_t WID[5] = {0, 3, 5, 8, 10};
    uint32_t st = batt_state(), n = st >> 1 & 7u, c = st >> 4 & 3u;
    uint16_t col = c == 3u ? P_CH[1] : c == 2u ? P_PLAY : c == 1u ? P_ACCENT : P_LABEL;
    int32_t x = 240 - 4 - 13, y = 6;
    if (!st || st >> 7 & 1u) return;            /* (not read yet; the blink's off half) */
    cv_frame(x, y, 12, 7, col); cv_rect(x + 12, y + 2, 1, 3, col);
    cv_rect(x + 1, y + 1, WID[n], 5, c == 3u || c == 0u ? P_TEXT : col);
}
static void draw_bar(void)
{
    char s[8], w[24];
    uint32_t t, cn = cur_chain(), ph = cur_phrase(), depth = (IS_PAGE(ui.view) || ui.view == V_Q ? V_INST : ui.view) + 1u, k;   /* (E L V Q: I's; Q's since
                                                 * principles-review.md #11, user 2026-10-11) */
    static const uint8_t COL[5] = {0, 4, 8, 14, 18};
    /* the tag (render.js barTag): ARM / REC, then a selection's LOOP, then NOTE; right-aligned a space before the tempo's
     * 3-digit column (user, 2026-10-12), so its room is its letters and a space */
    const char *tg = kb == KB_ARMED ? "ARM" : kb == KB_REC ? "REC" : loop_tag() ? sel_tag : kb ? "NOTE" : 0;
    uint32_t tw = tg ? str_len(tg) + 1u : 0u;
    int32_t tx = 240 - 4 - BATT_W - (int32_t)(3u + tw) * CW;
    cv_begin(240, TOP, P_BG);
    if (ui.view == V_PROJ || ui.view == V_MIX || ui.view == V_FX) {   /* render.js topBar: PROJECT 00  NAME*, MIXER  NAME,
                                                 * EFFECTS  NAME (NAME* = unsaved changes, the editors' convention; user,
                                                 * 2026-10-11, principles-review.md #7); the name cut to the room the tag,
                                                 * the tempo and the battery mark leave */
        uint32_t tl = ui.view == V_PROJ ? 10u : ui.view == V_MIX ? 5u : 7u, room = 40u - 1u - (tl + 2u) - 3u - tw - 1u - 3u;
        if (ui.view == V_PROJ) {
            cv_str(gx(0), 4, "PROJECT", P_ACCENT);
            fmt_hex(s, pj_slot, 2); cv_str(gx(8), 4, s, P_ACCENT);
            str_cpy(w, S.pj.name[0] ? S.pj.name : "UNTITLED", sizeof w);
            if (proj_dirty) sa_n(w, "*", sizeof w);
        } else {
            cv_str(gx(0), 4, ui.view == V_MIX ? "MIXER" : "EFFECTS", P_ACCENT);
            str_cpy(w, S.pj.name, sizeof w);
        }
        if (room < sizeof w) w[room] = 0;
        cv_str(gx((int32_t)tl + 2), 4, w, P_LABEL);
    } else
    for (k = 0; k <= depth && k < 5u; k++) {
        uint16_t col = k == depth ? P_ACCENT : k == 0 ? P_CH[ui.ch] : P_LABEL;
        if (k) cv_str(gx(COL[k] - 1), 4, ">", P_DIM);
        if (k == 0) { s[0] = 'C'; s[1] = 'H'; s[2] = (char)('1' + ui.ch); s[3] = 0; }
        else if (k == 1) { s[0] = 'S'; fmt_hex(s + 1, ui.song_row, 2); }
        else if (k == 2) { s[0] = 'C'; fmt_hex2e(s + 1, cn); s[3] = ':'; s[4] = HEX[ui.chain_row & 15u]; s[5] = 0; }
        else if (k == 3) { s[0] = 'P'; fmt_hex2e(s + 1, ph); }
        else { s[0] = 'I'; s[1] = HEX[ui.inst & 15u]; s[2] = 0; }
        cv_str(gx(COL[k]), 4, s, col);
    }
    if (depth == 3 && cn != EMPTY && S.chains[cn][ui.chain_row].tr) {
        fmt_tr(s, S.chains[cn][ui.chain_row].tr);
        cv_str(gx(18), 4, s, P_DIM);
    }
    if (depth <= 3 && playing && ch_swing[ui.ch] != S.pj.swing && (!tg || gx(26) < tx)) {   /* the channel swings (SWG),
                                                 * unlike the song; only where it clears the tag (a LOOP 00-3F takes its room) */
        uint32_t sw = ch_swing[ui.ch] > 99u ? 99u : ch_swing[ui.ch];
        s[0] = 'S'; s[1] = 'W'; s[2] = (char)('0' + sw / 10u); s[3] = (char)('0' + sw % 10u); s[4] = 0;
        cv_str(gx(22), 4, s, P_DIM);
    }
    if (depth == 4 && tw <= 7u) {               /* the name: a space before the tag; under 4 letters none (a fragment says
                                                 * nothing; user, 2026-10-12) */
        char nm[14];
        str_cpy(nm, ins[ui.inst].name, sizeof nm);
        if (ui.view == V_Q && q_filt) {         /* the filter takes the name's place (principles-review.md #11) */
            if (q_filt == Q_NOTE) str_cpy(nm, "NOTE", 5); else cmd_name(q_filt, ui.inst, nm);
            sa_n(nm, " ONLY", sizeof nm);
        }
        nm[11u - tw] = 0;
        cv_str(gx(21), 4, nm, P_LABEL);
    }
    if (tg) cv_str(tx, 4, tg, kb == KB_NOTES && !loop_tag() ? P_LABEL : kb == KB_ARMED ? P_HELD : P_PLAY);
    t = playing ? tempo : S.tempo;              /* (playing: what TPO made of it) */
    s[0] = (char)('0' + t / 100u); s[1] = (char)('0' + t / 10u % 10u); s[2] = (char)('0' + t % 10u); s[3] = 0;
    cv_str(240 - 4 - BATT_W - (t >= 100u ? 3 : 2) * CW, 4, t >= 100u ? s : s + 1, P_TEXT);
    batt_mark();
    cv_rect(0, TOP - 1, 240, 1, P_LINE);
    if (S.scn.xf) cv_rect(fader_x(), TOP - 2, 3, 2, P_TEXT);   /* the fader mark's upper rows (its third: redraw) */
    cv_blit(0, 0);
}

/* the song map under the map panel (render.js songMap / songLit; user, 2026-10-11, principles-review.md #13): the 64 rows
 * at 2 px, a column per channel; the lit chains in their channel's colour wherever they play, the other chains grey,
 * empty rows dark, the playing rows red, the 16 rows on screen framed in the accent. Lit: a selection's or marked
 * block's chains; on the row number, the row's four; else the cursor's. Drawn into whatever canvas covers screen rows
 * y0 .. y0 + h (its last rows reach into the bottom line: draw_strip draws them too) */
#define SMAP_X 226
#define SMAP_Y 98
static void cv_songmap(int32_t y0, int32_t h)
{
    uint8_t lit[(NCHN + 7u) / 8u] = {0};
    uint32_t r, k, top = song_top();
    int32_t fy = SMAP_Y + (int32_t)top * 2 - 1 - y0;
    int blk = sel.on || mark_shown();
    for (r = 0; r < NSONG; r++)
        for (k = 0; k < NCH; k++) {
            uint32_t v = S.song[r][k];
            if (v != EMPTY && (blk ? in_block(r, k) : r == ui.song_row && (ui.on_row || k == ui.ch))) lit[v >> 3] |= (uint8_t)(1u << (v & 7u));
        }
    for (r = 0; r < NSONG; r++) {
        int32_t y = SMAP_Y + 2 * (int32_t)r - y0;
        if (y + 2 <= 0 || y >= h) continue;
        for (k = 0; k < NCH; k++) {
            uint32_t v = S.song[r][k], p = shown_pos[k];
            cv_rect(SMAP_X + 3 * (int32_t)k, y, 2, 2, POS_ON(p) && POS_SROW(p) == r ? P_PLAY : v == EMPTY ? P_SURF
                    : lit[v >> 3] >> (v & 7u) & 1u ? P_CH[k] : P_DIM);
        }
    }
    cv_frame(SMAP_X - 2, fy, 15, 34, P_ACCENT);
    cv_str(SMAP_X - 4 - 2 * CW, SMAP_Y - 2 - y0, "00", P_LABEL);
    cv_str(SMAP_X - 4 - 2 * CW, SMAP_Y + 2 * (int32_t)NSONG - LH - y0, "3F", P_LABEL);
}
static uint32_t smap_line(uint32_t r)          /* the S line whose redraw covers song row r on the map (the last: 15 + the strip) */
{
    uint32_t l = (SMAP_Y + 2u * r - ROWS_Y) / LH;
    return 1u << (l > 15u ? 15u : l);
}

/* ---- song screen: 64 rows x 4 channels, 16 on screen; the chain under the cursor previewed on the right */
static int song_reach(uint32_t k, uint32_t r)   /* a chain only plays from row 00 or the start row, down to an empty cell */
{
    uint32_t from = playing && mode == M_SONG ? play_from_song : ui.song_row, s0, i, j;
    for (j = 0; j < 2u; j++) {
        s0 = j ? from : 0u;
        if (r < s0) continue;
        for (i = s0; i <= r && S.song[i][k] != EMPTY; i++)
            ;
        if (i > r) return 1;
    }
    return 0;
}
static void line_song(int32_t i)
{
    char s[8];
    uint32_t k, cn = cur_chain();
    if (i < 0) {                                /* the track numbers */
        for (k = 0; k < NCH; k++) {
            s[0] = (char)('1' + k); s[1] = 0;
            cv_str(gx(3 + 3 * (int32_t)k), 0, s, P_CH[k]);
            if (heard_solo(k)) cv_str(gx(4 + 3 * (int32_t)k), 0, "S", P_TEXT);   /* solo wins over mute; S white = on (as the
                                                 * mixer's SOLO), M grey = not heard (as its cells): amber and red are taken
                                                 * by "waits for you" and the playhead (user, 2026-10-11) */
            else if (heard_mute(k)) cv_str(gx(4 + 3 * (int32_t)k), 0, "M", P_LABEL);
        }
        cv_str(gx(PREV_END - 8), 0, "CHAIN", P_CTX);
        fmt_hex2e(s, cn);
        cv_str(gx(PREV_END - 2), 0, s, P_CTX);
    } else {
        uint32_t r = song_top() + (uint32_t)i;
        if (r == ui.song_row && !sel.on) cv_rect(gx(0), 0, 15 * CW, LH, P_BAND);
        fmt_hex(s, r, 2);
        if (ui.on_row && r == ui.song_row) cv_cell(0, s, P_BG, 1, P_ACCENT);   /* the row number as a cell */
        else cv_str(gx(0), 0, s, r % 4u == 0u ? P_ROW4 : P_LABEL);
        for (k = 0; k < NCH; k++) {
            uint32_t v = S.song[r][k], p = shown_pos[k];
            int32_t x = 3 + 3 * (int32_t)k;
            fmt_hex2e(s, v);
            if (flashed(r, k)) cv_cell(x, s, P_BG, 1, P_TEXT);   /* undo / redo: white, unlike anything else */
            else if (r == ui.song_row && k == ui.ch && !ui.on_row) cv_cell(x, s, P_BG, 1, P_ACCENT);
            else if (in_block(r, k)) cv_cell(x, s, v == EMPTY ? P_DIM : P_TEXT, 1, P_SEL);
            else if (v != EMPTY && !audible(k)) cv_cell(x, s, P_LABEL, 0, 0);   /* muted / soloed out: grey */
            else cv_cell(x, s, v == EMPTY || !song_reach(k, r) ? P_DIM : P_TEXT, 0, 0);
            if (POS_ON(p) && POS_SROW(p) == r) {   /* the playhead: a small triangle before the cell */
                int32_t j, x0 = gx(x - 1) + 1, h = LH - 4;
                for (j = 0; j < h; j++) {
                    int32_t d = (h - 1) / 2, w = (CW - 2) * (d - (j > d ? j - d : d - j)) / d;
                    cv_rect(x0, 2 + j, w < 1 ? 1 : w, 1, P_PLAY);
                }
            }
        }
        if (cn != EMPTY) {                      /* the chain preview */
            const crow_t *cr = &S.chains[cn][i];
            s[0] = HEX[i]; s[1] = 0;
            cv_str(gx(PREV_END - 8), 0, s, P_LABEL);
            fmt_hex2e(s, cr->ph);
            cv_str(gx(PREV_END - 6), 0, s, cr->ph == EMPTY ? P_DIM : P_TEXT);
            if (cr->ph != EMPTY && cr->tr) { fmt_tr(s, cr->tr); cv_str(gx(PREV_END - 3), 0, s, P_LABEL); }
        }
    }
    cv_rect(gx(PREV_END - 10) + CW / 2, 0, 1, LH, P_LINE);
}

/* ---- chain screen: 16 rows of phrase + transpose; the phrase under the cursor previewed on the right */
static void line_chain(int32_t i)
{
    char s[8];
    uint32_t cn = cur_chain(), ph = cur_phrase();
    if (i < 0) {
        cv_str(gx(2), 0, "PH", P_LABEL);
        cv_str(gx(5), 0, "TRN", P_LABEL);
        cv_str(gx(PREV_END - 8), 0, "PHRASE", P_CTX);   /* right of the separator, past PREV_END on this line */
        fmt_hex2e(s, ph);
        cv_str(gx(PREV_END - 1), 0, s, P_CTX);
    } else {
        uint32_t r = (uint32_t)i, p = shown_pos[ui.ch];
        crow_t cr = {EMPTY, 0};
        int playing_here = POS_ON(p) && POS_SROW(p) == ui.song_row && POS_CROW(p) == r && cur_chain() != EMPTY;
        if (cn != EMPTY) cr = S.chains[cn][r];
        if (r == ui.chain_row && !sel.on) cv_rect(gx(0), 0, 9 * CW, LH, P_BAND);
        s[0] = HEX[r]; s[1] = 0;
        if (playing_here) cv_cell(0, s, P_BG, 1, P_PLAY);
        else if (ui.on_row && r == ui.chain_row) cv_cell(0, s, P_BG, 1, P_ACCENT);
        else cv_cell(0, s, r % 4u == 0u ? P_ROW4 : P_LABEL, 0, 0);
        fmt_hex2e(s, cr.ph);
        if (flashed(r, 0)) cv_cell(2, s, P_BG, 1, P_TEXT);
        else if (r == ui.chain_row && ui.ccol == 0 && !ui.on_row) cv_cell(2, s, P_BG, 1, P_ACCENT);
        else if (in_block(r, 0)) cv_cell(2, s, cr.ph == EMPTY ? P_DIM : P_TEXT, 1, P_SEL);
        else cv_cell(2, s, cr.ph == EMPTY ? P_DIM : audible(ui.ch) ? P_TEXT : P_LABEL, 0, 0);   /* silent: grey */
        if (cr.ph == EMPTY && !cr.tr) str_cpy(s, "---", 4); else fmt_tr(s, cr.tr);
        if (flashed(r, 1)) cv_cell(5, s, P_BG, 1, P_TEXT);
        else if (r == ui.chain_row && ui.ccol == 1 && !ui.on_row) cv_cell(5, s, P_BG, 1, P_ACCENT);
        else if (in_block(r, 1)) cv_cell(5, s, cr.tr ? P_TEXT : P_DIM, 1, P_SEL);
        else cv_cell(5, s, !cr.tr ? P_DIM : audible(ui.ch) ? P_TEXT : P_LABEL, 0, 0);
        if (ph != EMPTY) {                      /* the phrase preview: row, note, instrument */
            const row_t *w = &S.phrases[ph][r];
            s[0] = HEX[r]; s[1] = 0;
            cv_str(gx(PREV_END - 7), 0, s, P_LABEL);
            fmt_note(s, w->n);
            cv_str(gx(PREV_END - 5), 0, s, w->n == EMPTY ? P_DIM : P_TEXT);
            s[0] = w->i == EMPTY ? '-' : HEX[w->i & 15u]; s[1] = 0;
            cv_str(gx(PREV_END - 1), 0, s, w->i == EMPTY ? P_DIM : P_TEXT);
        }
    }
    cv_rect(gx(PREV_END - 9) + CW / 2, 0, 1, LH, P_LINE);
}

/* ---- phrase screen */
static uint32_t row_inst(uint32_t r);
static void cell_text(const row_t *w, uint32_t k, uint32_t inst, char *s, int *empty)
{
    const fx_t *f;
    if (k == 0) { fmt_note(s, w->n); *empty = w->n == EMPTY; return; }
    if (k == 1) { fmt_hex2e(s, w->vol); *empty = w->vol == EMPTY; return; }
    if (k == 2) {                               /* a note without an instrument: a dim tie (render.js phraseCellText) */
        s[0] = w->i != EMPTY ? HEX[w->i & 15u] : w->n != EMPTY && w->n != NOTE_OFF ? '~' : '-'; s[1] = 0;
        *empty = w->i == EMPTY;
        return;
    }
    f = &w->fx[(k - 3u) / 2u];
    *empty = !f->c;
    if ((k - 3u) % 2u == 0u) cmd_name(f->c, inst, s);   /* named by the row's instrument (commands.md 1) */
    else if (f->c) fmt_hex(s, f->v, 2);
    else str_cpy(s, "--", 3);
}
/* ---- what the phrase screen shows of the playback (commands.md 10, 11, 14-17; render.js viewPhrase) */
/* HOP Y = row Y of the next phrase (M8): the rows a phrase never plays where it sits are dimmed, above the row it is
 * entered at (the previous phrase's HOP, or where the last pass really came in) and below its own first HOP from
 * there (model.js playedRows / prevPhrase / firstHop) */
static int first_hop(const row_t *rows, uint32_t from, uint32_t *r, uint32_t *to)
{
    uint32_t j, k;
    for (j = from; j < 16u; j++)
        for (k = 0; k < NFX; k++)
            if (rows[j].fx[k].c == CMD_HOP) { *r = j; *to = rows[j].fx[k].v & 15u; return 1; }
    return 0;
}
static uint32_t prev_phrase(uint32_t ch, uint32_t sr, uint32_t cr)
{
    uint32_t cn, r, ph = EMPTY;
    if (cr > 0u) return S.chains[S.song[sr][ch]][cr - 1u].ph;
    if (sr == 0u || (cn = S.song[sr - 1u][ch]) == EMPTY) return EMPTY;   /* song row 00, or after a gap */
    for (r = 0; r < 16u && S.chains[cn][r].ph != EMPTY; r++) ph = S.chains[cn][r].ph;
    return ph;
}
/* the song's cells and the chains' phrases, hashed: the main loop forgets every entry row when it changes (any edit,
 * undo, a load; my pick, to review: all of them, not just the changed cell's, as one place catches every edit) */
static uint32_t song_shape(void)
{
    uint32_t h = 0, k, r;
    for (k = 0; k < NSONG; k++) for (r = 0; r < NCH; r++) h = h * 33u + S.song[k][r];
    for (k = 0; k < NCHN; k++) for (r = 0; r < 16u; r++) h = h * 33u + S.chains[k][r].ph;
    return h;
}
static uint32_t walk_entry(uint32_t ch, uint32_t sr0, uint32_t cr0)   /* the row q_walk enters song row sr0 / chain row
                                                 * cr0 at: each phrase where the one before really left (U49); EMPTY = not reached */
{
    uint32_t sr, cr, from = 0, hr, hto;
    for (sr = 0; sr <= sr0 && S.song[sr][ch] != EMPTY; sr++) {
        uint32_t cn = S.song[sr][ch], ph;
        for (cr = 0; cr < 16u && (ph = S.chains[cn][cr].ph) != EMPTY; cr++) {
            if (sr == sr0 && cr == cr0) return from;
            from = first_hop(S.phrases[ph], from, &hr, &hto) ? hto : 0u;
        }
    }
    return EMPTY;
}
static void played_rows(uint32_t *from, uint32_t *to)
{
    uint32_t cn = cur_chain(), ph, prev, hr, hto, e;
    *from = 0; *to = 15;
    if (cn == EMPTY || (ph = S.chains[cn][ui.chain_row].ph) == EMPTY) return;
    if (entry_row[ui.ch][ui.song_row][ui.chain_row] != EMPTY) *from = entry_row[ui.ch][ui.song_row][ui.chain_row];
    else if ((e = walk_entry(ui.ch, ui.song_row, ui.chain_row)) != EMPTY) *from = e;
    else if ((prev = prev_phrase(ui.ch, ui.song_row, ui.chain_row)) != EMPTY && first_hop(S.phrases[prev], 0, &hr, &hto)) *from = hto;
    if (first_hop(S.phrases[ph], *from, &hr, &hto)) *to = hr;
}

/* under the note, the row's own timeline (render.js noteTimeline / timeLine): a line from where the note starts
 * (DEL) to where it is cut (KIL: a small upright) or released (OFF: a short dotted tail); RET's hits as small
 * ticks on it, lit in the channel colour as they play. A KIL / OFF longer than its row carries on under the next
 * rows. Fractions of the row in 1/6 (ticks) */
#define TL_X0 (gx(PH_X[0]))
#define TL_W (3 * CW - 1)
static void time_line(int32_t a6, int32_t b6, int how)   /* a stretch, ticks a6..b6 of 6; how 1 cut, 2 release */
{
    int32_t xa = TL_X0 + rdiv(a6 * TL_W, 6), xb = TL_X0 + rdiv(b6 * TL_W, 6), y = LH - 1, i;
    cv_rect(xa, y, xb - xa < 1 ? 1 : xb - xa, 1, P_LABEL);
    if (how == 1) cv_rect(xb, y - 2, 1, 3, P_LABEL);
    if (how == 2) for (i = 1; i <= 5; i += 2) cv_pset(xb + i, y, P_LABEL);
}
/* a phrase's note timeline as the player plays it (render.js timings; the player's rules, the screen follows, U37-U42,
 * U46, U48: my picks, to review), rows from..to: a note is struck at DEL (at most 5), on an odd row at least the
 * swing's late ticks; the first KIL / OFF on a row ends what sounds DEL + xx ticks in, counted across rows, also on a
 * row without a note; a struck note or OFF ends a running one, a note without an instrument does not; CHA: the row
 * as it played (sk: the lost sides' bits); RET's hits from the strike to the row end, none from the cut on. cy: what
 * comes in (and goes out); row `want`: its line a..b ticks, its end mark, its hits (n from d); n < 0: nothing */
typedef struct { int32_t left; uint32_t how, on; } tl_cy_t;   /* ticks to a KIL / OFF (0 none), 1 cut 2 release, a note */
typedef struct { int32_t a, b, how, d, n, end; } tl_row_t;
#define TL_NEVER 0x7FFF
static void tl_walk(const row_t *rows, uint32_t from, uint32_t to, tl_cy_t *cy, int32_t swing, const uint16_t *sk,
                    uint32_t want, int32_t ret_now, tl_row_t *o)
{
    uint32_t r, k;
    o->n = -1;
    for (r = from; r <= to; r++) {
        row_t w = rows[r];
        int32_t d = 0, kil = -1, n = ret_now, end, a = 0;
        int del = 0, struck;
        uint32_t kh = 0;
        for (k = 0; sk && k < NFX; k++) if (w.fx[k].c == CMD_CHA) { cha_apply(&w, k, sk[0] >> r & 1u, sk[1] >> r & 1u); break; }
        for (k = 0; k < NFX; k++) {
            uint32_t c = w.fx[k].c, v = w.fx[k].v;
            if (c == CMD_DEL) { d = v < 5u ? (int32_t)v : 5; del = 1; }
            else if ((c == CMD_KIL || c == CMD_OFF) && kil < 0) { kil = (int32_t)v; kh = c == CMD_KIL ? 1u : 2u; }
            else if (c == CMD_RET) n = (int32_t)(v & 15u);
        }
        if (r % 2u == 1u && swing) { int32_t sd = rdiv(swing * 3, 100); sd = sd < 5 ? sd : 5; if (sd > d) d = sd; }
        struck = w.n < NOTE_OFF && w.i != EMPTY;
        if (w.n == NOTE_OFF) { cy->on = 0; cy->left = 0; if (r == want) return; continue; }
        end = cy->left > 0 ? cy->left : TL_NEVER;
        if (struck) { cy->on = 1; a = d; end = TL_NEVER; }
        if (!cy->on) { cy->left = 0; if (r == want) return; continue; }
        if (kil >= 0 && end > d) { end = d + kil; cy->how = kh; }
        if (r == want) {
            o->a = a; o->d = d; o->end = end; o->n = n > 1 ? n : 0;
            o->b = end == TL_NEVER ? (struck && del ? 6 : a) : end < 6 ? end : 6;
            o->how = end <= 6 ? (int32_t)cy->how : 0;
            return;
        }
        cy->left = end == TL_NEVER ? 0 : end - 6;
        if (end <= 6) cy->on = 0;
    }
}
static void draw_timeline(const row_t *rows, uint32_t r, uint32_t from, uint32_t to, uint32_t ph, int here, uint16_t lit_col, int32_t lit6)
{
    uint32_t prev = prev_phrase(ui.ch, ui.song_row, ui.chain_row), k;
    int32_t swing = playing ? ch_swing[ui.ch] : S.pj.swing, ret_now = -1, h;
    uint16_t sk[2];
    tl_cy_t cy = {0, 0, 0};
    tl_row_t o;
    if (here) {                                 /* playing in this phrase: what came in, and RET as the player has it */
        cy.left = cin_left[ui.ch]; cy.how = cin_how[ui.ch] & 3u; cy.on = cin_how[ui.ch] >> 2 & 1u;
        for (k = 0; lit6 >= 0 && k < NFX; k++)  /* (U41: a RET ramp's value on a row without a RET cell) */
            if (!rows[r].fx[k].c && chs[ui.ch].ramp[k].on && chs[ui.ch].ramp[k].code == CMD_RET) ret_now = chs[ui.ch].ramp[k].now & 15;
    } else if (prev != EMPTY) {                 /* stopped: the phrase before, its own timeline */
        tl_walk(S.phrases[prev], 0, 15, &cy, swing, 0, 16, -1, &o);
        if (cy.left < 0) cy.left = 0;
    }
    sk[0] = playing && ph != EMPTY ? skip_l[ui.ch][ph] : 0; sk[1] = playing && ph != EMPTY ? skip_r[ui.ch][ph] : 0;
    tl_walk(rows, from, to, &cy, swing, sk, r, ret_now, &o);
    if (o.n < 0) return;
    if (o.b > o.a || o.how) time_line(o.a, o.b, (int)o.how);
    for (h = 0; h < o.n; h++) {                 /* RET: the hits, spread over the rest of the row */
        int32_t f = o.d * 64 + h * (6 - o.d) * 64 / o.n;   /* 1/64 ticks */
        if (f < o.end * 64) cv_rect(TL_X0 + rdiv(f * TL_W, 6 * 64), LH - 1, 2, 1, lit6 * 64 >= f && lit6 >= 0 ? lit_col : P_TEXT);
    }
}

/* ramps (render.js rampLines): a thin line down the left edge of the FX column over the rows a ramp covers, to the
 * row where it arrives (an end tick); the part already played in the channel colour; an endless REP dotted over
 * the phrase's last rows */
static uint32_t ramp_stop(const row_t *rows, uint32_t r0, uint32_t k)   /* where the player stops a ramp set on row
                                                 * r0: the next command in column k after its stack, or REP 00; 16 = none (U43) */
{
    uint32_t j = r0 + 1u;
    while (j < 16u && cmd_is_ramp(rows[j].fx[k].c)) j++;
    for (; j < 16u; j++) if (rows[j].fx[k].c && (!cmd_is_ramp(rows[j].fx[k].c) || (rows[j].fx[k].c == CMD_REP && !rows[j].fx[k].v))) return j;
    return 16u;
}
static void span_r1(const ramp_t *sp, uint32_t r0, uint32_t *r1, int *endless)   /* the row it arrives on */
{
    *endless = 0;
    if (sp->rows) *r1 = r0 + (uint32_t)sp->rows;
    else if (!sp->step) *r1 = r0 + 1u;
    else if (sp->to >= 0) {
        int32_t d = sp->to - sp->from, st = sp->step < 0 ? -sp->step : sp->step;
        /* REP heading away from RTO: ramp_value snaps to RTO on the first tick (my pick, to review) */
        *r1 = r0 + (d * sp->step < 0 ? 1u : (uint32_t)(((d < 0 ? -d : d) + st - 1) / st));
    } else { *r1 = 99; *endless = 1; }
}
static int ramp_span(const row_t *rows, uint32_t r0, uint32_t k, uint32_t *r1, int *endless)
{
    ramp_t sp;
    uint32_t st;
    if (!ramp_spec(rows, r0, k, &sp)) return 0;
    if (sp.code == CMD_ARP || sp.code == CMD_VIB) {   /* a digit each: the later one decides (model.js rampReach) */
        ramp_t a = sp, b = sp;
        uint32_t ra, rb;
        int ea, eb;
        a.from = (int16_t)(sp.from >> 4); a.to = (int16_t)(sp.to < 0 ? -1 : sp.to >> 4);
        b.from = (int16_t)(sp.from & 15); b.to = (int16_t)(sp.to < 0 ? -1 : sp.to & 15);
        span_r1(&a, r0, &ra, &ea); span_r1(&b, r0, &rb, &eb);
        *r1 = ra > rb ? ra : rb; *endless = ea || eb;
    } else span_r1(&sp, r0, r1, endless);
    if ((st = ramp_stop(rows, r0, k)) < *r1) { *r1 = st; *endless = 0; }   /* (a command below stops it, U43) */
    return 1;
}
static void draw_ramps(const row_t *rows, uint32_t r, int32_t now, uint16_t col)
{
    uint32_t k, r0;
    for (k = 0; k < NFX; k++) {
        int32_t x = gx(PH_X[3 + 2 * k]) - 3;
        for (r0 = 0; r0 <= r; r0++) {
            uint32_t r1, last;
            int endless, y;
            if (!ramp_span(rows, r0, k, &r1, &endless)) continue;
            last = r1 < 15u ? r1 : 15u;
            if (r > last) continue;
            for (y = r == r0 ? 1 : 0; y < (r == last ? LH - 1 : LH); y++) {
                int fade = endless && r >= 14u && y % 2;
                if (!fade) cv_pset(x, y, now >= 0 && (int32_t)r <= now ? col : P_LINE);
            }
            if (!endless && r1 <= 15u && r == last) cv_rect(x, LH - 2, 3, 1, now >= (int32_t)last ? col : P_LINE);
        }
    }
}

static int ramp_dir(uint32_t c, uint32_t code);
static void line_phrase(int32_t i)
{
    static const row_t blank = {EMPTY, EMPTY, EMPTY, {{0, 0}, {0, 0}, {0, 0}}};
    const row_t *rows = cur_phrase_rows(), *w;
    char s[8];
    uint32_t k, r = (uint32_t)i, p = shown_pos[ui.ch], from, to, ph = cur_phrase(), kc = 99;
    int here = playing && POS_ON(p) && rows && POS_PH(p) == ph, sl = 0, sr = 0;
    uint16_t chc = P_CH[ui.ch];
    if (i < 0) {
        cv_str(gx(PH_X[0]), 0, "N", P_LABEL);
        cv_str(gx(PH_X[1]), 0, "V", P_LABEL);
        cv_str(gx(PH_X[2]), 0, "I", P_LABEL);
        cv_str(gx(PH_X[3]), 0, "FX1", P_LABEL);
        cv_str(gx(PH_X[5]), 0, "FX2", P_LABEL);
        cv_str(gx(PH_X[7]), 0, "FX3", P_LABEL);
        if (ui.on_head) {                       /* the column labels are cells: the cursor's column */
            static const char *const HEAD[9] = {"N", "V", "I", "FX1", "", "FX2", "", "FX3", ""};
            char h[4] = "   ";
            uint32_t j;
            for (j = 0; HEAD[ui.col][j]; j++) h[j] = HEAD[ui.col][j];
            h[PH_W[ui.col]] = 0;
            cv_cell(PH_X[ui.col], h, P_BG, 1, P_ACCENT);
        }
        for (k = 0; playing && ph != EMPTY && k < NFX; k++)   /* ^^ = a ramp carried over from an earlier phrase */
            if (ramp_ph[ui.ch][k] != EMPTY && ramp_ph[ui.ch][k] != ph) cv_str(gx(PH_X[3 + 2 * k] + 3), 0, "^^", chc);
        return;
    }
    played_rows(&from, &to);
    w = rows ? &rows[r] : &blank;
    if (playing && ph != EMPTY) {               /* CHA: the side that lost its roll, until the row plays again */
        sl = skip_l[ui.ch][ph] >> r & 1u;
        sr = skip_r[ui.ch][ph] >> r & 1u;
        for (k = 0; k < NFX; k++) if (w->fx[k].c == CMD_CHA) kc = 3u + 2u * k;
    }
    if (r == ui.row && !sel.on && !ui.on_head) cv_rect(gx(0), 0, 31 * CW, LH, P_BAND);
    if (ui.on_head && !sel.on) cv_rect(gx(PH_X[ui.col]) - 1, 0, PH_W[ui.col] * CW + 2, LH, P_BAND);   /* the column */
    s[0] = HEX[r]; s[1] = 0;
    if (here && POS_ROW(p) == r) {               /* the playhead; a swung odd row plays late: the mark shifts right */
        int32_t dx = r % 2u ? rdiv(ch_swing[ui.ch] * 3, 100) : 0;
        cv_rect(gx(0) - 1 + dx, 0, CW + 2, LH, P_PLAY);
        cv_str(gx(0) + dx, 0, s, P_BG);
    } else
        cv_cell(0, s, ui.on_row && r == ui.row ? P_BG : r < from || r > to ? P_DIM : r % 4u == 0u ? P_ROW4 : P_LABEL,
                ui.on_row && r == ui.row, P_ACCENT);
    for (k = 0; k < 9u; k++) {
        int empty, dim = r < from || r > to || (sl && k < kc) || (sr && kc != 99u && k > kc + 1u)
                         || (playing && ph != EMPTY && unheard[ui.ch][ph] >> r & 1u && (k == 0 || k == 2));   /* found nothing */
        cell_text(w, k, row_inst(r), s, &empty);
        if (flashed(r, k)) cv_cell(PH_X[k], s, P_BG, 1, P_TEXT);
        else if (r == ui.row && k == ui.col && !ui.on_row && !ui.on_head) cv_cell(PH_X[k], s, P_BG, 1, P_ACCENT);
        else if (in_block(r, k)) cv_cell(PH_X[k], s, empty || dim ? P_DIM : P_TEXT, 1, P_SEL);
        else cv_cell(PH_X[k], s, empty || dim ? P_DIM : audible(ui.ch) ? P_TEXT : P_LABEL, 0, 0);   /* silent: grey */
        if (k >= 3u && rows && !(r == ui.row && k == ui.col && !ui.on_row && !ui.on_head)) {   /* written by this take
                                                 * (knob recording): REC red until it ends */
            uint32_t b = (ph * 16u + r) * NFX + (k - 3u) / 2u;
            if (rc[b >> 3] >> (b & 7u) & 1u) cv_str(gx(PH_X[k]), 0, s, P_PLAY);
        }
    }
    if (here && POS_ROW(p) == r) {              /* the ghost: ARP / VIB / PIT held from an earlier row, at half the channel's
                                                 * brightness in the row's first empty FX cells (a triangle while it ramps),
                                                 * when the row does not write it (decisions.md "Rampable ARP and VIB") */
        static const uint8_t GC[3] = {CMD_ARP, CMD_VIB, CMD_PIT};
        uint32_t g, j, f = 0;
        for (g = 0; g < 3u; g++) {
            uint32_t v = pfx_held[ui.ch][g], x, vx;
            int wr = 0, d;
            char hx[3];
            for (j = 0; j < NFX; j++) if (w->fx[j].c == GC[g]) wr = 1;
            if (!v || wr) continue;
            while (f < NFX && w->fx[f].c) f++;
            if (f >= NFX) break;
            x = PH_X[3 + 2 * f]; vx = PH_X[4 + 2 * f];
            if (!(r == ui.row && (ui.col == 3u + 2u * f || ui.col == 4u + 2u * f) && !ui.on_row && !ui.on_head)) {
                cv_rect(gx((int32_t)x), 0, (int32_t)(vx + 2u - x) * CW, LH, r == ui.row && !sel.on && !ui.on_head ? P_BAND : P_BG);
                cv_str(gx((int32_t)x), 0, CMD_NAME[GC[g]], P_GHOST[ui.ch]);
                fmt_hex(hx, v, 2); cv_str(gx((int32_t)vx), 0, hx, P_GHOST[ui.ch]);
                if ((d = ramp_dir(ui.ch, GC[g])) != 0) cv_tri(gx((int32_t)vx + 2), 0, d, P_GHOST[ui.ch]);
            }
            f++;
        }
    }
    if (rows) {
        int32_t lit = here && POS_ROW(p) == r ? (int32_t)ui_tick : -1;
        if (!(r < from || r > to)) draw_timeline(rows, r, from, to, ph, here, chc, lit);
        draw_ramps(rows, r, here ? (int32_t)POS_ROW(p) : -1, chc);
    }
}

/* one text line of the S / C / P screens (-1 = the column labels), with its slice of the map */
/* the scene strip (below): shown, stuck (it has the focus: the screen's cursor dims), its line cursor */
static struct { uint8_t on, stuck, line, drawn; uint32_t until; } ss;
static struct { int32_t x0, y0, x1, y1; } ov_clip; /* the overlay over the rows (a popup, the knob or scene strip), set as
                                                 * it is drawn: rows are not blitted under it, so it does not flicker
                                                 * when a row under it changes (unit-feedback.md #2) */
static void blit_rows(uint32_t y)               /* a full-width band of the screen; the strip stuck: the cursor's accent
                                                 * in the list area dimmed where it is (render.js dimCursor, cursorOff) */
{
    uint32_t r, x, a, b;
    if (ss.on && ss.stuck)
        for (r = 0; r < cv_h; r++)
            if (y + r >= ROWS_Y - LH && y + r < STRIP_Y)
                for (x = 0; x < MAP_X - 6u; x++) if (cv_px[r * cv_w + x] == swap16(P_ACCENT)) cv_px[r * cv_w + x] = swap16(P_CUROFF);
    if (ov_clip.y1 <= (int32_t)y || ov_clip.y0 >= (int32_t)(y + cv_h)) { cv_blit(0, y); return; }
    a = ov_clip.y0 > (int32_t)y ? (uint32_t)ov_clip.y0 - y : 0u;   /* canvas rows a .. b - 1 hold the overlay */
    b = ov_clip.y1 < (int32_t)(y + cv_h) ? (uint32_t)ov_clip.y1 - y : cv_h;
    lcd_blit(0, y, 240, a, cv_px);
    for (r = a; r < b; r++) {
        lcd_blit(0, y + r, (uint32_t)ov_clip.x0, 1, cv_px + r * 240u);
        lcd_blit((uint32_t)ov_clip.x1, y + r, 240u - (uint32_t)ov_clip.x1, 1, cv_px + r * 240u + (uint32_t)ov_clip.x1);
    }
    lcd_blit(0, y + b, 240, cv_h - b, cv_px + b * 240u);
}
static void clip_to(int32_t x, int32_t y, int32_t w, int32_t h) { ov_clip.x0 = x < 0 ? 0 : x; ov_clip.y0 = y; ov_clip.x1 = x + w > 240 ? 240 : x + w; ov_clip.y1 = y + h; }
static void draw_line(int32_t i)
{
    cv_begin(240, LH, P_BG);
    if (ui.view == V_SONG) line_song(i);
    else if (ui.view == V_CHAIN) line_chain(i);
    else line_phrase(i);
    cv_map(ROWS_Y + i * LH, LH);
    if (ui.view == V_SONG) cv_songmap(ROWS_Y + i * LH, LH);
    blit_rows((uint32_t)(ROWS_Y + i * LH));
}

static uint8_t strip_map;                       /* the bottom line holds the song map's end */
static char help[40];                           /* while EDIT is held: what the cell is (over the message) */
static void draw_strip(void)
{
    cv_begin(240, 240 - STRIP_Y, P_BG);
    const char *t = help[0] ? help : msg;
    uint32_t i;
    if (ui.view == V_SONG) cv_songmap(STRIP_Y, 240 - STRIP_Y);   /* (the song map's last row and frame) */
    strip_map = ui.view == V_SONG;
    cv_str(gx(0), 2, t, help[0] || !msg_held ? P_TEXT : P_HELD);
    for (i = 0; t[i] && t[i + 1] && t[i + 2]; i++)  /* every CHn in that channel's colour */
        if (t[i] == 'C' && t[i + 1] == 'H' && t[i + 2] >= '1' && t[i + 2] <= '4') {
            char ch[4] = {'C', 'H', t[i + 2], 0};
            cv_rect(gx((int32_t)i), 2, 3 * CW, LH, P_BG);
            cv_str(gx((int32_t)i), 2, ch, P_CH[t[i + 2] - '1']);
        }
    cv_blit(0, STRIP_Y);
}

/* ---------------------------------------------------------------- the instrument pages (render.js) --- */
/* I E L V: rows of four fields (one row = KNOB 1..4). A field: x in characters, its row, what it is, its width, and
 * a longer label where the page has room (render.js LONG). Each row: the label line, the value under it.
 * I (INST_FIELDS): ENGINE PRESET / LEVEL PAN NAME / E0..E3 / E4..E7 / the KNOBS rows, a row every 38 px (the KNOBS:
 * row 4 = the slots' names, row 5 = their values, drawn as one row: the names over the values). E (ENV_FIELDS), L
 * (LFO_FIELDS), V (VOX_FIELDS): Felucca's envelope, LFO, voice and sends, under a drawing (E: the ADSR, L: the
 * moving waveform), some rows in a titled frame (render.js PAGES), then the KNOBS rows as on I (rows 2 and 3, y 190) */
enum { FK_ENGINE = 0x100, FK_PRESET, FK_NAME, FK_KNOB, FK_KVAL = FK_KNOB + 4 };   /* FK_KNOB + n: what knob slot n
                                                 * turns; FK_KVAL + n: its value (the slotted parameter) */
typedef struct { uint8_t x, row; uint16_t key; uint8_t w; } field_t;
static const field_t IF_I[21] = {
    {0, 0, FK_ENGINE, 16}, {16, 0, FK_PRESET, 16}, {0, 1, P_LEVEL, 8}, {8, 1, P_PAN, 8},
    {0, 2, P_E0, 8}, {8, 2, P_E0 + 1, 8}, {16, 2, P_E0 + 2, 8}, {24, 2, P_E0 + 3, 8},
    {0, 3, P_E0 + 4, 8}, {8, 3, P_E0 + 5, 8}, {16, 3, P_E0 + 6, 8}, {24, 3, P_E0 + 7, 8},
    {0, 4, FK_KNOB, 8}, {8, 4, FK_KNOB + 1, 8}, {16, 4, FK_KNOB + 2, 8}, {24, 4, FK_KNOB + 3, 8},
    {16, 1, FK_NAME, 16},
    {0, 5, FK_KVAL, 8}, {8, 5, FK_KVAL + 1, 8}, {16, 5, FK_KVAL + 2, 8}, {24, 5, FK_KVAL + 3, 8},
};
/* the KNOBS rows on E L V too, in I's place (user, 2026-10-10: a slot can be any page's parameter) */
#define KNOB_ROWS \
    {0, 2, FK_KNOB, 8}, {8, 2, FK_KNOB + 1, 8}, {16, 2, FK_KNOB + 2, 8}, {24, 2, FK_KNOB + 3, 8}, \
    {0, 3, FK_KVAL, 8}, {8, 3, FK_KVAL + 1, 8}, {16, 3, FK_KVAL + 2, 8}, {24, 3, FK_KVAL + 3, 8}
static const field_t IF_E[15] = {
    {0, 0, P_ATK, 8}, {8, 0, P_DEC, 8}, {16, 0, P_SUS, 8}, {24, 0, P_REL, 8},
    {1, 1, P_ED_FLT, 8}, {9, 1, P_ED_PIT, 8}, {17, 1, P_ED_SHP, 8}, KNOB_ROWS,
};
static const field_t IF_L[16] = {
    {0, 0, P_LRATE, 8}, {8, 0, P_LWAVE, 8}, {16, 0, P_LPHASE, 8}, {24, 0, P_LFADE, 8},
    {1, 1, P_LD_PIT, 8}, {9, 1, P_LD_FLT, 8}, {17, 1, P_LD_SHP, 8}, {25, 1, P_LD_AMP, 8}, KNOB_ROWS,
};
static const field_t IF_V[15] = {
    {1, 0, P_VOICE, 8}, {9, 0, P_GLIDE, 8}, {17, 0, P_DETUNE, 8},
    {1, 1, P_DIST, 8}, {9, 1, P_CHOR, 8}, {17, 1, P_DLY, 8}, {25, 1, P_REV, 8}, KNOB_ROWS,
};
enum { DRAW_NONE, DRAW_ADSR, DRAW_LFO };
#define KNOBS_Y 190                             /* the KNOBS names line, in its titled frame (render.js KNOBS_Y, review #14) */
static const struct {
    const field_t *f;
    uint8_t n, graph;
    int16_t y[6];                               /* each row's label line, screen y (the KNOBS values: drawn with the names) */
    const char *frame[2];                       /* a titled frame around row 0 / 1 (0 = none) */
} IPAGES[4] = {
    {IF_I, 21, DRAW_NONE, {34, 72, 110, 148, KNOBS_Y}, {0, 0}},
    {IF_E, 15, DRAW_ADSR, {106, 146, KNOBS_Y}, {0, "AMOUNT TO"}},
    {IF_L, 16, DRAW_LFO, {106, 146, KNOBS_Y}, {0, "AMOUNT TO"}},
    {IF_V, 15, DRAW_NONE, {48, 110, KNOBS_Y}, {"VOICE", "SENDS"}},
};
/* the page on screen: PG, IF and NIF read ui.view, so every helper using them works on the page shown (code that walks
 * another page reads IPAGES itself, as param_jump and scene_jump do) */
#define PG (IPAGES[IS_PAGE(ui.view) ? ui.view - V_INST : 0u])
#define IF (PG.f)                               /* the fields of the page on screen */
#define NIF ((uint32_t)PG.n)
#define IROW_H 38
#define IROW_Y(r) (ROWS_Y + (int32_t)(r) * IROW_H)

#define IS_KNOB(key) ((key) >= FK_KNOB && (key) < FK_KNOB + 4u)
#define IS_KVAL(key) ((key) >= FK_KVAL && (key) < FK_KVAL + 4u)
#define FROW(f) (IF[f].row - IS_KVAL(IF[f].key))   /* the screen row: the KNOBS values share the names' */
/* knob slot k of instrument i: its parameter key; v = the stored byte (EMPTY = the engine's HOME knob) */
static uint32_t slot_of(uint32_t i, uint32_t k, uint32_t v) { return v == EMPTY ? ENGINES[ins[i].eng]->knob[k] : v; }
static uint32_t slot_key(uint32_t i, uint32_t k) { return slot_of(i, k, S.knob[i][k]); }
/* what field f edits (model.js slotField): a KNOBS value = the slotted parameter itself */
static uint32_t fkey(uint32_t f) { uint32_t key = IF[f].key; return IS_KVAL(key) ? slot_key(ui.inst, key - FK_KVAL) : key; }

static const param_desc_t *field_desc(uint32_t f)
{
    uint32_t key = fkey(f);
    return key < P_COUNT ? ins_desc(ui.inst, key) : 0;
}
static int field_usable(uint32_t f)             /* an engine's unused or removed E slot is skipped */
{
    const param_desc_t *d = field_desc(f);
    uint32_t key = fkey(f);
    if (key >= P_E0 && key <= P_E7 && !eng_slot_name(ins[ui.inst].eng, key - P_E0)) return 0;
    return !d || d->max != d->min;
}
static void field_text_v(uint32_t f, char *label, char *val, int32_t raw)
{
    const inst_t *I = &ins[ui.inst];
    uint32_t key = fkey(f);
    if (key == FK_ENGINE) { str_cpy(label, "ENGINE", 8); str_cpy(val, ENGINES[I->eng]->name, 16); return; }
    if (key == FK_PRESET) { str_cpy(label, "PRESET", 8); str_cpy(val, ENGINES[I->eng]->presets[I->preset].name, 16); return; }
    {
        const param_desc_t *d = field_desc(f);
        const char *unit;
        char v[12];
        cmd_name(CMD_SOUND | key, ui.inst, label);   /* one name per parameter: the command's, as the phrases,
                                                 * the FX grid and the knob slots (user, 2026-10-11, principles-review.md #3) */
        param_format(d, raw, v, &unit);
        str_cpy(val, v, 12);
        sa_n(val, unit, 12);
    }
}
static void field_text(uint32_t f, char *label, char *val) { field_text_v(f, label, val, fkey(f) < P_COUNT ? ins[ui.inst].p[fkey(f)] : 0); }

/* editing into a scene (docs/ux/scenes.md 4, 6; app.js editScene / nearHolds / sceneSet): SEL held, an edit goes into the
 * scene on the side the fader is nearer, not into the song; without SEL, a value that scene holds is edited there (else
 * a turn on a value both scenes hold would change nothing heard or shown) */
static uint8_t sel_held;                        /* the panel SEL, this loop */
static const uint8_t SCENE_KEYS[NSCENE] = {6, 7, 9, 11, 12, 14, 16, 18};   /* B3 C4 D4 E4 F4 G4 A4 B4 = scenes 1-8 (app.js) */
static uint32_t near_scene(void) { return S.scn.xf * 2u > XF_MAX ? S.scn.b : S.scn.a; }   /* (app.js nearB: 64 on = B) */
static int sc_editing(uint32_t t, uint32_t p) { return sel_held || sc_find(near_scene(), t, p) >= 0; }
static int32_t sc_value(uint32_t t, uint32_t p, int32_t base) { int32_t k = sc_find(near_scene(), t, p); return k < 0 ? base : S.scn.k[near_scene()][k].v; }
static void sc_set(uint32_t t, uint32_t p, int32_t v)
{
    sc_t *e = S.scn.k[near_scene()];
    int32_t k = sc_find(near_scene(), t, p);
    if (k < 0) {
        for (k = 0; k < (int32_t)SCENE_MAX && e[k].t != EMPTY; k++) {}
        if (k == (int32_t)SCENE_MAX) { say("scene full: 8 parameters max"); return; }
        e[k].t = (uint8_t)t; e[k].p = (uint8_t)p;
    }
    e[k].v = (int8_t)v;
    scn_changed();
}
static void sc_drop(uint32_t t, uint32_t p)     /* SEL + OPTION + EDIT: out of the scene */
{
    sc_t *e = S.scn.k[near_scene()];
    int32_t k = sc_find(near_scene(), t, p);
    if (k < 0) return;
    mem_move(&e[k], &e[k + 1], (SCENE_MAX - 1u - (uint32_t)k) * sizeof *e);
    memset(&e[SCENE_MAX - 1u], EMPTY, sizeof *e);
    scn_changed();
}

/* what a value shows (render.js sceneVal): SEL held, the near scene's value (SM_DIM: it holds none, the song's, dim);
 * else the value the morph plays, SM_MORPH when it differs from the song's (a diamond after it, as a ramp's triangle) */
enum { SM_NONE, SM_DIM, SM_MORPH };
static int32_t sc_shown(uint32_t t, uint32_t p, int32_t base, int list, int *mark)
{
    int32_t k = sc_find(near_scene(), t, p), v;
    *mark = SM_NONE;
    if (sel_held) { if (k < 0) *mark = SM_DIM; return k < 0 ? base : S.scn.k[near_scene()][k].v; }
    v = morph(t, p, base, list);
    if (v != base) *mark = SM_MORPH;
    return v;
}
/* the size rule (render.js fitValue, param-values.md 1): a mark after the value (a ramp's triangle, the morph's diamond)
 * takes one character of the room; a value that no longer fits drops its unit first, then is cut */
static void fit_value(char *v, int mark, uint32_t room)
{
    static const char *const UNITS[] = {"dB", "Hz", "ms", "s", "%", "ct"};
    uint32_t n = room - (mark ? 1u : 0u), k, l = str_len(v);
    if (l > n)
        for (k = 0; k < NELEM(UNITS); k++) {
            uint32_t u = str_len(UNITS[k]);
            if (l > u && str_eq(v + l - u, UNITS[k])) { v[l - u] = 0; break; }
        }
    if (str_len(v) > n) v[n] = 0;
}
static void cv_diamond(int32_t x, int32_t y, uint16_t col)   /* render.js diamond: 5 x 5 after a value */
{
    static const uint8_t D[5][2] = {{2, 1}, {1, 3}, {0, 5}, {1, 3}, {2, 1}};
    uint32_t j;
    for (j = 0; j < 5u; j++) cv_rect(x + D[j][0], y + 4 + (int32_t)j, D[j][1], 1, col);
}

/* while playing, a value a sound command holds (or a ramp moves) on the instrument shown: render.js liveIns /
 * valueCell. EDIT held shows the configured values: what you edit */
static uint8_t edit_down;
static uint8_t live_ch[NHELD];                  /* the held values as drawn: one snapshot per redraw, so a */
static int16_t live_val[NHELD];                 /* cell and the bottom line show the same moment */
static uint32_t live_i;                         /* whose: the page's instrument, or the knob strip's */
static void live_snapshot(uint32_t i)
{
    live_i = i;
    memcpy(live_ch, held_ch[i], sizeof live_ch);
    memcpy(live_val, held_val[i], sizeof live_val);
}
static int ramp_dir(uint32_t c, uint32_t code)  /* channel c's ramp of a command: +1 / -1 while it moves, else 0 */
{
    uint32_t k;
    for (k = 0; k < NFX; k++) {
        const ramp_t *rp = &chs[c].ramp[k];
        if (rp->on && rp->code == code) { int32_t d = rp->to >= 0 ? rp->to - rp->from : rp->step; return d > 0 ? 1 : d < 0 ? -1 : 0; }
    }
    return 0;
}
static int live_value(uint32_t key, uint32_t *ch, int *dir)   /* 1: live_val[held_slot(key)] is drawn */
{
    uint32_t c, k;
    uint32_t s = held_slot(key);
    if (!playing || edit_down || sel_held || s == NHELD || live_ch[s] == EMPTY || sc_holds(live_i, key)) return 0;   /* (the
                                                 * morph wins over commands; SEL held: the edit view; param-values.md 1, 5) */
    *ch = live_ch[s] & 3u;
    *dir = 0;
    for (c = 0; c < NCH; c++)
        for (k = 0; k < NFX; k++) {
            const ramp_t *rp = &chs[c].ramp[k];
            if (rp->on && rp->ins == live_i && rp->code >= CMD_SOUND && (rp->code & 0x7Fu) == key) {
                int32_t d = rp->to >= 0 ? rp->to - rp->from : rp->step;
                *dir = d > 0 ? 1 : d < 0 ? -1 : 0;
            }
        }
    return 1;
}
static void live_line(char *s)                  /* `CUT set 355, now 4.2k` + the ramp's triangle */
{
    uint32_t key = fkey(ui.field), ch;
    int dir;
    const char *unit;
    char v[12];
    s[0] = 0;
    if (!live_value(key, &ch, &dir)) return;
    cmd_name(CMD_SOUND | key, ui.inst, s);
    sa(s, " set ");
    param_format(field_desc(ui.field), ins[ui.inst].p[key], v, &unit);
    sa(s, v); sa(s, unit);
    sa(s, ", now ");
    param_format(field_desc(ui.field), live_val[held_slot(key)], v, &unit);
    sa(s, v); sa(s, unit);
    if (dir) { char t[2] = {(char)(dir > 0 ? 1 : 2), 0}; sa(s, t); }
}
/* the cursor's value the scenes hold, in the bottom line (render.js sceneLine, param-values.md 2): scene A's, heard now,
 * scene B's, `CUT A 81Hz now 478Hz B 6.9kHz` (a side without it: the song's own); 0 = in neither scene, s untouched */
static int scene_line(char *s)
{
    uint32_t t, p, k;
    int32_t base, n[3];
    const param_desc_t *d;
    char v[12];
    const char *unit;
    if (IS_PAGE(ui.view)) {
        p = fkey(ui.field);
        if (p >= P_COUNT || !field_usable(ui.field)) return 0;
        t = ui.inst; d = field_desc(ui.field); base = ins[t].p[p];
        cmd_name(CMD_SOUND | p, t, s);
    } else if (ui.view == V_MIX && ui.mf == 0u) { t = SC_MIX; p = ui.ch; d = &TP[P_LEVEL]; base = S.lvl[p]; str_cpy(s, "TV1", 40); s[2] = (char)('1' + p); }
    else if (ui.view == V_FX) { t = SC_FX; p = ui.ff; d = &GP[FXF[p].g]; base = S.fx[p]; str_cpy(s, d->label, 40); }
    else return 0;
    if (!sc_holds(t, p)) return 0;
    k = (uint32_t)sc_find(S.scn.a, t, p); n[0] = (int32_t)k < 0 ? base : S.scn.k[S.scn.a][k].v;
    n[1] = morph(t, p, base, is_list(d));
    k = (uint32_t)sc_find(S.scn.b, t, p); n[2] = (int32_t)k < 0 ? base : S.scn.k[S.scn.b][k].v;
    for (k = 0; k < 3u; k++) {
        sa(s, k == 0u ? " A " : k == 1u ? " now " : " B ");
        param_format(d, n[k], v, &unit);
        sa(s, v); sa(s, unit);
    }
    return 1;
}
/* the drawings (render.js adsrGraph / lfoGraph), 200 x 62 under the breadcrumb, from what the part plays: the
 * held value while playing, as the fields show it */
#define GX X0
#define GY (ROWS_Y + 4)
#define GW 200
#define GH 62
static int32_t shown_p(uint32_t key) { uint32_t ch; int dir; return live_value(key, &ch, &dir) ? live_val[held_slot(key)] : ins[ui.inst].p[key]; }
/* the ADSR to time (user, 2026-10-11, principles-review.md #5 option B; render.js adsrGraph): A D R in proportion to
 * their lengths (Felucca's TIME_MS_X10) over GW - 40 px, the sustain a fixed 40 px dotted stretch, the peak a line
 * down, A + D + R as TIME top right. In the text colour: the accent means "where you are" */
static void draw_adsr(void)
{
    uint32_t ta = TIME_MS_X10[shown_p(P_ATK) & 127], td = TIME_MS_X10[shown_p(P_DEC) & 127], tr = TIME_MS_X10[shown_p(P_REL) & 127];
    uint32_t t = ta + td + tr;                  /* ms x 10, at most 300000: x 160 fits 32 bits */
    int32_t top = GY + LH, bot = GY + GH, sy = bot - ((bot - top) * shown_p(P_SUS) * 2 + 126) / 254, q;   /* rounded, as render.js */
    int32_t xa = GX + (int32_t)((ta * (GW - 40u) * 2u + t) / (2u * t)), xd = GX + (int32_t)(((ta + td) * (GW - 40u) * 2u + t) / (2u * t));
    char span[16], v[8];
    str_cpy(span, "TIME ", sizeof span);
    if (t < 10000u) { fmt_int(v, (int32_t)((t + 5u) / 10u)); sa_n(span, v, sizeof span); sa_n(span, "ms", sizeof span); }
    else { fmt_fix(v, (int32_t)((t + 500u) / 1000u), 1); sa_n(span, v, sizeof span); sa_n(span, "s", sizeof span); }
    cv_str(GX + GW - (int32_t)str_len(span) * CW, GY - 2, span, P_LABEL);
    cv_rect(GX, GY + GH, GW, 1, P_LINE);
    cv_line(GX, bot, xa, top, P_TEXT);
    cv_line(xa, top, xd, sy, P_TEXT);
    for (q = 0; q < 40; q += 3) cv_rect(xd + q, sy, 1, 1, P_TEXT);
    cv_line(xd + 40, sy, GX + GW, bot, P_TEXT);
}
/* 2 s of the waveform at its rate, moving, starting at its phase and fading in over FADE: Felucca's own LFO
 * (voice.c track_lfo_tick: LFO_INC a control block, the fade's step, lfo_wave) */
static void draw_lfo(void)
{
    uint32_t inc = LFO_INC[shown_p(P_LRATE) & 127], ph0 = (uint32_t)shown_p(P_LPHASE) << 25, wave = (uint32_t)shown_p(P_LWAVE);
    uint32_t now = fm1_ms / (CTL * 10u) * (FS / 100u) + fm1_ms % (CTL * 10u) * (FS / 100u) / (CTL * 10u), x;   /* control
                                                 * blocks: FS / CTL a second = 441 every 320 ms */
    int32_t fade = shown_p(P_LFADE), step = (int32_t)(ENV_LIN[fade & 127] >> 9), mid = GY + GH / 2, amp = GH / 2 - 2, py = 0;
    cv_rect(GX, mid, GW, 1, P_LINE);
    for (x = 0; x < GW; x++) {
        uint32_t tb = x * 2756u / GW, b = now + tb;
        uint64_t ph64 = (uint64_t)b * inc + ph0;
        uint32_t ph = (uint32_t)ph64, cyc = (uint32_t)(ph64 >> 32);
        int32_t v, g = !fade ? 32767 : (int32_t)tb * (step ? step : 1) > 32767 ? 32767 : (int32_t)tb * (step ? step : 1), y;
        if (wave == 1u) v = osc_tri(ph);
        else if (wave == 2u) v = (int32_t)(ph >> 16) - 32768;
        else if (wave == 3u) v = ph < 0x80000000u ? 32767 : -32767;
        else if (wave == 4u) v = (int32_t)((cyc * 2654435761u) >> 16) - 32768;   /* S&H: a value a cycle */
        else v = osc_sine(ph);
        y = mid - (((v * g) >> 15) * amp >> 15);
        if (x) cv_line(GX + (int32_t)x - 1, py, GX + (int32_t)x, y, P_TEXT);   /* not the accent (principles-review #5) */
        py = y;
    }
}
static void cv_name(int32_t px, int32_t y, const char *nm, int32_t cur)   /* 12 letters, empty ones as dots; cur: the
                                                                            * cursor's letter, -1 = none */
{
    uint32_t k;
    int end = 0;
    for (k = 0; k < NAME_LEN; k++) {
        char ch;
        if (!nm[k]) end = 1;
        ch = end || nm[k] == ' ' ? '.' : nm[k];
        if ((int32_t)k == cur) {
            cv_rect(px + (int32_t)k * CW - 1, y, CW + 2, LH, P_ACCENT);
            cv_glyph(px + (int32_t)k * CW, y, ch == '.' ? ' ' : ch, P_BG);
        } else
            cv_glyph(px + (int32_t)k * CW, y, ch, ch == '.' ? P_DIM : P_TEXT);
    }
}
/* a knob (render.js knobIcon): a 15 px ring in the colour given (knob n's, or grey), a white pointer at the value, 7 o'clock = lowest,
 * 5 o'clock = highest; then the parameter's name and value beside it (render.js fitValue: past 5 characters the
 * value drops its unit). Shows what plays: a held value in the channel's colour, as the fields (live_snapshot(i)
 * first). KN_CUR / KN_NAME: the page's cursor on its value / its name; KN_HOT: the knob strip's turned one (name in
 * the accent, value white). Through the scenes (sc_shown): SEL held, the near scene's value (dim if it holds none:
 * not on the knob strip, render.js knobStrip), else the morph's (a diamond after it); a mark leaves 4 characters */
#define KNOB_R 7
enum { KN_PLAIN, KN_CUR, KN_HOT, KN_NAME };
static uint8_t kn_strip;                        /* drawing the knob strip */
static void draw_knob(int32_t px, int32_t y, uint32_t i, uint32_t key, int mode, uint16_t ring)
{
    const param_desc_t *d = ins_desc(i, key);
    int cur = mode == KN_CUR;
    int32_t cx = px + KNOB_R, cy = y + LH - 1, tx = px + 2 * KNOB_R + 2, x, yy, v, q, lv;
    uint32_t ph, ch;
    int dir, mk;
    const char *unit;
    char nm[4], val[16];
    for (yy = -KNOB_R; yy <= KNOB_R; yy++)      /* the ring: 5.9 <= distance <= 7.3 */
        for (x = -KNOB_R; x <= KNOB_R; x++)
            if (x * x + yy * yy >= 35 && x * x + yy * yy <= 53) cv_rect(cx + x, cy + yy, 1, 1, ring);
    lv = live_value(key, &ch, &dir);
    v = sc_shown(i, key, lv ? live_val[held_slot(key)] : ins[i].p[key], is_list(d), &mk);
    q = d->max > d->min ? (int32_t)((uint32_t)(v - d->min) * 65536u / (uint32_t)(d->max - d->min)) : 0;   /* 0..65536 of 270 degrees
                                                                            * from -135: fine enough to round as render.js */
    ph = 0xA0000000u + (uint32_t)q * 0xC000u;
    for (x = 0; x <= 4 * (KNOB_R - 1); x++)     /* the pointer in quarter-pixel steps, each rounded (render.js knobIcon) */
        cv_rect(cx + ((osc_sine(ph) * x + 65536) >> 17), cy + ((65536 - osc_sine(ph + 0x40000000u) * x) >> 17), 1, 1, P_WHITE);
    cmd_name(CMD_SOUND | key, i, nm);
    if (mode == KN_NAME) { cv_rect(tx - 1, y, (int32_t)str_len(nm) * CW + 2, LH, P_ACCENT); cv_str(tx, y, nm, P_BG); }
    else cv_str(tx, y, nm, mode == KN_HOT ? P_ACCENT : P_LABEL);
    param_format(d, v, val, &unit);
    sa_n(val, unit, sizeof val);
    fit_value(val, (lv && dir) || mk == SM_MORPH, 5u);   /* (a SEL-dimmed value has no mark) */
    if (cur) { cv_rect(tx - 1, y + LH, (int32_t)str_len(val) * CW + 2, LH, P_ACCENT); cv_str(tx, y + LH, val, P_BG); }
    else cv_str(tx, y + LH, val, mode == KN_HOT ? P_WHITE : mk == SM_DIM && !kn_strip ? P_DIM : lv ? P_CH[ch] : P_TEXT);
    if (lv && dir) cv_tri(tx + (int32_t)str_len(val) * CW + 1, y + LH, dir, cur ? P_ACCENT : P_CH[ch]);
    if (mk == SM_MORPH) cv_diamond(tx + (int32_t)str_len(val) * CW + 1 + (lv && dir ? 6 : 0), y + LH,
                                   mode == KN_HOT ? P_WHITE : lv ? P_CH[ch] : P_TEXT);
}
/* a page between screen rows y0 and y0 + h, in bands the canvas holds (platform.c CV_MAX: 240 x 64): body draws each
 * (cv_oy set), then its slice of the map, blitted as the rows (blit_rows) */
#define CV_ROWS ((int32_t)(CV_MAX / 240u))
static void draw_bands(int32_t y0, int32_t h, void (*body)(int32_t y0, int32_t h))
{
    for (; h > 0; y0 += CV_ROWS, h -= CV_ROWS) {
        int32_t bh = h < CV_ROWS ? h : CV_ROWS;
        cv_begin(240, (uint32_t)bh, P_BG);
        cv_oy = -y0;
        body(y0, bh);
        cv_oy = 0;
        cv_map(y0, bh);
        blit_rows((uint32_t)y0);
    }
}
/* render.js groupFrame: a frame around the row whose label line is at y, its title on the top line */
static void group_frame(int32_t y, const char *title)
{
    int32_t w = 33 * CW, fh = 2 * LH + 14;
    y -= 8;
    cv_frame(X0 - 2, y, w, fh, P_LINE);
    cv_rect(X0 + 4, y - 5, (int32_t)(str_len(title) + 2u) * CW, LH, P_BG);
    cv_str(X0 + 4 + CW, y - 5, title, P_CTX);
}
static void draw_band(int32_t y0, int32_t h)   /* the page (drawing, frames, fields, the knob bars under the cursor's row) */
{
    uint32_t f, r, n = 0, cur_row = FROW(ui.field);
    int on_slots = IS_KNOB(IF[ui.field].key) || IS_KVAL(IF[ui.field].key);
    if (PG.graph == DRAW_ADSR) draw_adsr();
    if (PG.graph == DRAW_LFO) draw_lfo();
    for (r = 0; r < 2u; r++) if (PG.frame[r]) group_frame(PG.y[r], PG.frame[r]);
    group_frame(KNOBS_Y, "KNOBS");
    for (f = 0; f < NIF; f++) {
        char label[8], val[24];
        int32_t px = gx(IF[f].x), y = PG.y[IF[f].row];
        if (IF[f].row == IF[ui.field].row && IF[f].key != FK_NAME && field_usable(f) && n < 4u)   /* render.js knobBars:
                                                 * a bar in knob n's colour under the field KNOB n turns (the loop below) */
            cv_rect(px, PG.y[cur_row] + 2 * LH + 1, ((int32_t)IF[f].w - 1) * CW, 2, P_CH[n++]);
        if (IF[f].key == FK_NAME) {             /* NAME, then its 12 letters */
            cv_str(px, y, "NAME", P_LABEL);
            cv_name(px, y + LH, ins[ui.inst].name, f == ui.field ? (int32_t)ui.char_pos : -1);
            continue;
        }
        if (IS_KVAL(IF[f].key)) continue;       /* (drawn with its knob) */
        if (IS_KNOB(IF[f].key)) {
            uint32_t k = IF[f].key - FK_KNOB;   /* the rings grey unless the cursor is on these rows: only then do
                                                 * the knobs turn the slots here (render.js slotCell onSlots) */
            draw_knob(px, y, ui.inst, slot_key(ui.inst, k), f == ui.field ? KN_NAME : IF[ui.field].key == FK_KVAL + k ? KN_CUR : KN_PLAIN, on_slots ? P_CH[k] : P_DIM);
            continue;
        }
        if (!field_usable(f)) continue;
        {
            uint32_t key = fkey(f), ch;
            int dir, mk = SM_NONE;
            int lv = live_value(key, &ch, &dir);  /* held by a command: the channel's colour; ramping: a triangle */
            uint16_t col;
            if (key < P_COUNT)                  /* through the scenes (render.js sceneField; not ENGINE PRESET) */
                field_text_v(f, label, val, sc_shown(ui.inst, key, lv ? live_val[held_slot(key)] : ins[ui.inst].p[key], is_list(field_desc(f)), &mk));
            else field_text(f, label, val);
            fit_value(val, (lv && dir) || mk == SM_MORPH, IF[f].w - 1u);
            col = mk == SM_DIM ? P_DIM : lv ? P_CH[ch] : P_TEXT;
            cv_str(px, y, label, P_LABEL);
            if (f == ui.field) {
                cv_rect(px - 1, y + LH, (int32_t)str_len(val) * CW + 2, LH, P_ACCENT);
                cv_str(px, y + LH, val, P_BG);
            } else
                cv_str(px, y + LH, val, col);
            if (lv && dir) cv_tri(px + (int32_t)str_len(val) * CW + 1, y + LH, dir, f == ui.field ? P_ACCENT : P_CH[ch]);
            if (mk == SM_MORPH) cv_diamond(px + (int32_t)str_len(val) * CW + 1 + (lv && dir ? 6 : 0), y + LH, col);
        }
    }
}

/* d-pad on a page of fields (app.js formMove), over a table whose entries start with x, line, stride bytes apart: up /
 * down = the nearest line that way, the field under the cursor's column (wo: the width's place in an entry, 0 = none:
 * the nearest x); left / right = the nearest on the same line; usable 0 = every field. -> the field, n = none */
static uint32_t form_move(const uint8_t *t, uint32_t stride, uint32_t wo, uint32_t n, uint32_t cur, int32_t dx, int32_t dy,
                          int (*usable)(uint32_t))
{
    uint32_t f, best = n, bl = 99, bd = 99, x = t[cur * stride], line = t[cur * stride + 1u];
    for (f = 0; f < n; f++) {
        const uint8_t *e = t + f * stride;
        uint32_t d = e[0] > x ? e[0] - x : x - e[0], dl = e[1] > line ? e[1] - line : line - e[1];
        if (dy ? (dy > 0 ? e[1] <= line : e[1] >= line) : e[1] != line || !(dx > 0 ? e[0] > x : e[0] < x)) continue;
        if (usable && !usable(f)) continue;
        if (dy && wo) d = x >= e[0] && x < (uint32_t)e[0] + e[wo] ? 0 : d + 1u;
        if (dl < bl || (dl == bl && d < bd)) { bl = dl; bd = d; best = f; }
    }
    return best;
}
/* the instrument pages: inside NAME left / right walk its letters first */
static void inst_move(int32_t dx, int32_t dy)
{
    uint32_t best;
    if (IF[ui.field].key == FK_NAME && dx) {
        int32_t p = (int32_t)ui.char_pos + dx;
        if (p >= 0 && p < (int32_t)NAME_LEN) { ui.char_pos = (uint32_t)p; return; }
    }
    best = form_move(&IF[0].x, sizeof(field_t), __builtin_offsetof(field_t, w), NIF, ui.field, dx, dy, field_usable);
    if (best < NIF) {
        ui.field = best;
        if (IF[best].key == FK_NAME) ui.char_pos = dx < 0 ? NAME_LEN - 1u : 0;
    }
}

/* a field's value changes by steps (EDIT + d-pad, a knob): engines in Felucca's shown order, presets past the
 * retired aliases, parameters through Felucca's param_turn; lists stop at their ends. Heard at once (the ISR
 * reloads the part, ins_gen), auditioned with C-4 as in the prototype when nothing plays */
static void name_move(int32_t dx, int32_t dy);   /* (names, below) */
static uint32_t grid_lines(uint32_t i, uint8_t *l, uint8_t (*ln)[3], int slots);   /* (the FX grid, below) */
static uint32_t grid_cur(int slots, const uint8_t *l, uint8_t (*ln)[3], uint32_t nl);
static void name_clear(void);
#define NONE 0x100u                            /* get / set: an empty cell (EMPTY, 0xFF, is a real FX value) */
/* one last value per column type, FX1-3 sharing one (docs/ux/edit-defaults.md): a new note gets velocity 64, a value
 * 00; ni = the copied note's instrument cell (EMPTY too), pasted with it; NONE: the last note was not a copy */
static struct { uint8_t song, ph, note, vol, inst, cmd, val, chr; char cmd_name[4]; uint16_t ni; } last = {EMPTY, EMPTY, 48, 0x64, 0, 0, 0, 0, "", NONE};   /* app.js last; cmd 0: none yet
                                                 * (picks review #13) */
static void inst_change(uint32_t f, int32_t steps)
{
    inst_t *I = &ins[ui.inst];
    uint32_t key = fkey(f);
    if (key == FK_NAME) return;                 /* (letters: name_move) */
    if (IS_KNOB(key)) {                         /* a knob slot: the next parameter in the grid's order, no sound */
        uint8_t l[CMD_MAX], ln[GRID_LINES][3];
        uint32_t nl = grid_lines(ui.inst, l, ln, 1), a = ln[0][0], e = (uint32_t)ln[nl - 1u][0] + ln[nl - 1u][1];
        int32_t c = (int32_t)grid_cur(1, l, ln, nl) + steps;
        if (sel_held) return;                   /* (a choice, never a scene value: app.js) */
        c = clamp_i(c, (int32_t)a, (int32_t)e - 1);
        S.knob[ui.inst][key - FK_KNOB] = l[c] & 0x7Fu;
        return;
    }
    if (key == FK_ENGINE) {
        int32_t r = (int32_t)eng_rank(I->eng) + steps;
        r = clamp_i(r, 0, (int32_t)NENG_SHOWN - 1);
        if (eng_vis((uint32_t)r) == I->eng) return;
        ins_engine(ui.inst, eng_vis((uint32_t)r));
        memset(S.knob[ui.inst], EMPTY, 4);      /* a new sound: its engine's knobs (app.js reload) */
    } else if (key == FK_PRESET) {
        const engine_t *e = ENGINES[I->eng];
        int32_t k = I->preset, dir = steps > 0 ? 1 : -1, n = steps > 0 ? steps : -steps;
        while (n--) {
            int32_t j = k + dir;
            while (j >= 0 && j < (int32_t)e->npresets && preset_orig(e, (uint32_t)j) != (uint32_t)j) j += dir;
            if (j < 0 || j >= (int32_t)e->npresets) break;
            k = j;
        }
        if (k == I->preset) return;
        ins_preset(ui.inst, (uint32_t)k);
    } else {
        const param_desc_t *d = field_desc(f);
        int32_t v = param_turn(d, sc_value(ui.inst, key, I->p[key]), steps);
        if (sc_editing(ui.inst, key)) sc_set(ui.inst, key, v);   /* (into the near scene) */
        else if (v == I->p[key]) return;
        else { I->p[key] = (int16_t)v; ins_gen[ui.inst]++; }
    }
    blip(ui.ch, last.note, ui.inst);           /* the last note used, as EDIT + PLAY (unit-feedback #30) */
}
static int32_t inst_big(uint32_t f)             /* EDIT + up/down: lists 1, values an eighth of their range */
{
    const param_desc_t *d = field_desc(f);
    int32_t r;
    if (!d || d->fmt == F_ENUM || d->fmt == F_ONOFF) return 1;
    r = (d->max - d->min + 4) / 8;
    return r < 1 ? 1 : r;
}
static void inst_reset(uint32_t f)              /* OPTION + EDIT: the default (ENGINE: ANALOG, PRESET: the first) */
{
    uint32_t key = fkey(f);
    if (key == FK_NAME) { name_clear(); return; }
    if (IS_KNOB(key)) { if (!sel_held) S.knob[ui.inst][key - FK_KNOB] = EMPTY; return; }   /* the engine's HOME knob
                                                 * (a choice, never a scene value: not with SEL, app.js) */
    if (key == FK_ENGINE) { ins_engine(ui.inst, engine_by_name("ANALOG")); memset(S.knob[ui.inst], EMPTY, 4); }
    else if (key == FK_PRESET) ins_preset(ui.inst, 0);
    else if (sel_held) sc_drop(ui.inst, key);   /* SEL: out of the scene; the near scene holding it: its default there */
    else if (sc_editing(ui.inst, key)) sc_set(ui.inst, key, field_desc(f)->def);
    else { ins[ui.inst].p[key] = field_desc(f)->def; ins_gen[ui.inst]++; }
    blip(ui.ch, last.note, ui.inst);           /* the last note used, as EDIT + PLAY (unit-feedback #30) */
}

/* ---------------------------------------------------------------- the grids (app.js GRID) --- */
/* song, chain and phrase cells through one interface: get / set / range / the value EDIT puts in an empty cell */
static uint32_t row_inst(uint32_t r)            /* row r's instrument, else the nearest above, else the last (app.js rowIns) */
{
    const row_t *rows = cur_phrase_rows();
    int32_t k;
    for (k = (int32_t)r; rows && k >= 0; k--) if (rows[k].i != EMPTY) return rows[k].i;
    return last.inst;
}
static uint32_t focus_inst(void) { return ui.view == V_PHRASE ? row_inst(ui.row) : ui.view == V_Q ? ui.inst : last.inst; }   /* app.js focusInst */
/* EDIT on an FX cell (pressed, + arrows, moving in the FX grid): the row's note, else the nearest above, with this row's
 * sound commands; player commands are not heard (unit-feedback #26; app.js fxAudition) */
static void fx_audition(void)
{
    const row_t *rows = cur_phrase_rows();
    int32_t r = (int32_t)ui.row;
    if (ui.view != V_PHRASE || ui.col < 3u || !rows) return;
    while (r >= 0 && rows[r].n == EMPTY) r--;
    if (r < 0 || rows[r].n == NOTE_OFF) return;
    blip_row(ui.ch, rows[r].n, row_inst((uint32_t)r), cur_phrase(), ui.row);
}
static uint32_t cur_col(void) { return ui.view == V_SONG ? ui.ch : ui.view == V_CHAIN ? ui.ccol : ui.col; }
static int emptyable(void) { return !(ui.view == V_CHAIN && ui.ccol == 1); }   /* a transpose is a number, 0 by default */
static int on_cmd(void) { return ui.view == V_PHRASE && ui.col >= 3u && (ui.col - 3u) % 2u == 0u; }
static int on_val(void) { return ui.view == V_PHRASE && ui.col >= 4u && (ui.col - 3u) % 2u == 1u; }
static fx_t *cur_fx(void) { row_t *rows = cur_phrase_rows(); return rows ? &rows[ui.row].fx[(ui.col - 3u) / 2u] : 0; }
/* the last command used, by name, as the row instrument's command (ANALOG CUT -> TRIO CUT); 0 = it has none */
static uint32_t last_cmd_at(uint32_t r)
{
    if (last.cmd < CMD_SOUND) return last.cmd;
    return cmd_by_name(last.cmd_name, row_inst(r));
}

static uint32_t get(void)                       /* NONE for an empty cell; a transpose as 0x80 + value; a command as
                                                 * its place in the row instrument's list */
{
    uint32_t cn = cur_chain();
    if (ui.view == V_SONG) return S.song[ui.song_row][ui.ch] == EMPTY ? NONE : S.song[ui.song_row][ui.ch];
    if (ui.view == V_CHAIN) {
        if (cn == EMPTY) return ui.ccol ? 0x80u : NONE;
        if (ui.ccol) return (uint32_t)(0x80 + S.chains[cn][ui.chain_row].tr);
        return S.chains[cn][ui.chain_row].ph == EMPTY ? NONE : S.chains[cn][ui.chain_row].ph;
    }
    {
        row_t *rows = cur_phrase_rows();
        fx_t *f;
        int32_t k;
        if (!rows) return NONE;
        if (ui.col <= 2u) {
            uint32_t v = ui.col == 0 ? rows[ui.row].n : ui.col == 1 ? rows[ui.row].vol : rows[ui.row].i;
            return v == EMPTY ? NONE : v;
        }
        f = cur_fx();
        if (!f->c) return NONE;
        if (on_val()) return f->v;
        k = cmd_index(f->c, row_inst(ui.row));
        return k < 0 ? NONE : (uint32_t)k;
    }
}

/* EDIT held on a command cell opens the FX grid: on its command, or on VOL for an empty one when no command was used
 * yet (an arrow writes; app.js kbBase, picks review #13) */
static int cmd_pick(void) { return on_cmd() && cur_phrase() != EMPTY && (get() != NONE || !last.cmd); }

/* a command cell set to the k-th command of the row's list (app.js GRID.phrase set): a new command brings the
 * last value if it is the last command used (a paste), else a sound command starts at the row instrument's
 * setting (it changes nothing until edited) and a player command at 00 (user, 2026-10-10) */
static void set_cmd(fx_t *f, uint32_t k)
{
    uint8_t l[CMD_MAX];
    uint32_t i = row_inst(ui.row), n = cmd_list(i, l, 0), c;
    if (k == NONE) { f->c = 0; f->v = 0; return; }
    c = l[k < n ? k : n - 1u];
    if (c != f->c) {
        char nm[4];
        cmd_name(c, i, nm);
        if (str_eq(nm, last.cmd_name)) f->v = last.val;
        else f->v = c >= CMD_SOUND ? (uint8_t)cmd_encode(c, i) : 0;
    }
    f->c = (uint8_t)c;
    last.cmd = (uint8_t)c;
    cmd_name(c, i, last.cmd_name);
}

static void set(uint32_t v)
{
    if (ui.view == V_SONG) {
        S.song[ui.song_row][ui.ch] = (uint8_t)(v == NONE ? EMPTY : v);
        if (v != NONE) last.song = (uint8_t)v;
    } else if (ui.view == V_CHAIN) {
        uint32_t cn = ensure_chain();
        if (cn == EMPTY) return;
        if (ui.ccol == 0) { S.chains[cn][ui.chain_row].ph = (uint8_t)(v == NONE ? EMPTY : v); if (v != NONE) last.ph = (uint8_t)v; }
        else S.chains[cn][ui.chain_row].tr = (int8_t)((int32_t)v - 0x80);
    } else {
        row_t *w;
        if (ensure_phrase() == EMPTY) return;
        w = &cur_phrase_rows()[ui.row];
        if (ui.col == 0) {                      /* a new note gets the last instrument (on an empty row: the velocity
                                                 * too); an edited one keeps its cells, a ~ note stays one (user) */
            uint32_t was = w->n;
            w->n = (uint8_t)(v == NONE ? EMPTY : v);
            if (v != NONE && v != NOTE_OFF) {
                last.note = (uint8_t)v; last.ni = NONE;
                if ((was == EMPTY || was == NOTE_OFF) && w->i == EMPTY) w->i = last.inst;
                if (was == EMPTY && w->vol == EMPTY) w->vol = last.vol;
                blip(ui.ch, v, row_inst(ui.row));
            }
        } else if (ui.col <= 2u && v != NONE && w->n == NOTE_OFF) return;   /* an OFF row takes no velocity or
                                                 * instrument (user) */
        else if (ui.col == 1) { w->vol = (uint8_t)(v == NONE ? EMPTY : v); if (v != NONE) last.vol = (uint8_t)v; }
        else if (ui.col == 2) { w->i = (uint8_t)(v == NONE ? EMPTY : v); if (v != NONE) last.inst = (uint8_t)v; }
        else if (on_cmd()) set_cmd(cur_fx(), v);
        else {                                  /* a value: on a cell with no command, the last command comes too */
            fx_t *f = cur_fx();
            if (v == NONE) { f->v = 0; return; }
            if (!f->c) {
                uint32_t c = last_cmd_at(ui.row);
                if (!c) return;
                f->c = (uint8_t)c;
            }
            f->v = (uint8_t)v;
            last.val = (uint8_t)v;
        }
    }
}

/* ---- names (app.js nameGet / nameSet / nameRelease, render.js NAME_CHARS / charGrid): the instrument's 12 letters,
 * ' ' = an empty one, the cursor's is ui.char_pos. EDIT on an empty letter writes the last one, on a filled one copies
 * it; EDIT held = the 10 x 4 character grid, the arrows move on it as they look (left / right stop at its row ends);
 * letting EDIT go after the letter was written or changed moves to the next letter; OPTION + EDIT empties it. The
 * name follows the preset until it differs from the preset's (app.js reload). */
static const char NAME_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._#";
#define NCHARS 40u
static char *name_buf(void) { return ui.view == V_PROJ ? S.pj.name : ins[ui.inst].name; }   /* (G: the project's) */
static uint32_t name_get(void)                  /* the letter's place in NAME_CHARS; NONE: empty (or none of them) */
{
    const char *nm = name_buf();
    uint32_t k;
    if (ui.char_pos >= str_len(nm)) return NONE;
    for (k = 0; k < NCHARS; k++) if (NAME_CHARS[k] == nm[ui.char_pos]) return k;
    return NONE;
}
static void name_set(uint32_t v)
{
    inst_t *I = &ins[ui.inst];
    char *nm = name_buf();
    uint32_t n = str_len(nm);
    for (; n <= ui.char_pos; n++) { nm[n] = ' '; nm[n + 1u] = 0; }
    nm[ui.char_pos] = v == NONE ? ' ' : NAME_CHARS[v];
    while (n && nm[n - 1u] == ' ') nm[--n] = 0;   /* (trailing empty letters are no letters) */
    if (v != NONE) last.chr = (uint8_t)v;
    if (ui.view != V_PROJ) I->renamed = (uint8_t)!str_eq(I->name, ENGINES[I->eng]->presets[I->preset].name);
}
static void name_press(void) { uint32_t v = name_get(); if (v == NONE) name_set(last.chr); else last.chr = (uint8_t)v; }
static void name_clear(void) { name_set(NONE); }
static void name_move(int32_t dx, int32_t dy)  /* EDIT + d-pad: on the grid */
{
    uint32_t v = name_get();
    int32_t t;
    if (v == NONE) { name_press(); return; }
    if ((dx < 0 && v % 10u == 0u) || (dx > 0 && v % 10u == 9u)) return;
    t = (int32_t)v + dx + 10 * dy;
    name_set((uint32_t)clamp_i(t, 0, (int32_t)NCHARS - 1));
}
/* EDIT on a letter (app.js nameRelease): letting EDIT go after the letter was written or changed moves to the next one */
typedef struct { uint32_t on, pos, was; } nhold_t;
static void name_down(nhold_t *h) { h->on = 1; h->pos = ui.char_pos; h->was = name_get(); name_press(); }
static int name_up(nhold_t *h, int on_name)     /* EDIT let go: 1 = moved on */
{
    uint32_t v;
    if (!h->on) return 0;
    v = name_get();
    h->on = 0;
    if (!on_name || ui.char_pos != h->pos || v == NONE || v == h->was || ui.char_pos >= NAME_LEN - 1u) return 0;
    ui.char_pos++;
    return 1;
}
static void draw_char_grid(void)                /* over the E rows (the name is above them), on its own canvas */
{
    int32_t x0 = gx(4) - 4, y0 = ROWS_Y + 10 * LH - 3, w = 20 * CW + 8, h = 4 * LH + 6, k;
    uint32_t cur = name_get();
    cv_begin((uint32_t)w, (uint32_t)h, P_SURF);
    cv_frame(0, 0, w, h, P_ACCENT);
    for (k = 0; k < (int32_t)NCHARS; k++) {
        int32_t px = gx(4 + (k % 10) * 2) - x0, py = 3 + (k / 10) * LH;
        char c[2];
        c[0] = NAME_CHARS[k]; c[1] = 0;
        if ((uint32_t)k == cur) { cv_rect(px - 1, py, CW + 2, LH, P_ACCENT); cv_str(px, py, c, P_BG); }
        else cv_str(px, py, c, P_TEXT);
    }
    cv_blit((uint32_t)x0, (uint32_t)y0);
}

static void say_not_param(void)                 /* "CUT: not a DRUM parameter" */
{
    char s[40];
    if (!last.cmd) { say("hold EDIT: pick a command"); return; }   /* (no command used yet) */
    str_cpy(s, last.cmd_name, sizeof s);
    sa(s, ": not a ");
    sa(s, ENGINES[ins[row_inst(ui.row)].eng]->name);
    sa(s, " parameter");
    say(s);
}

static void edit_press(void)                    /* EDIT: insert the last value, or copy the cell's (app.js insertLast) */
{
    uint32_t v = get(), fresh, ni = last.ni;
    if (on_val() && v == 0u && cur_fx()->c >= CMD_SOUND) { set(last.val); return; }   /* a sound command's 00 counts as
                                                 * empty; a player command's (HOP 00 = row 0) is a value (user) */
    if (v != NONE || !emptyable()) {
        if (emptyable()) {
            if (ui.view == V_SONG) last.song = (uint8_t)v;
            else if (ui.view == V_CHAIN) last.ph = (uint8_t)v;
            else if (ui.col == 0 && v != NOTE_OFF) {   /* with its instrument cell as it is, an empty one too (user) */
                last.note = (uint8_t)v; last.ni = cur_phrase_rows()[ui.row].i;
                blip(ui.ch, v, focus_inst());
            }
            else if (ui.col == 1) last.vol = (uint8_t)v;
            else if (ui.col == 2) last.inst = (uint8_t)v;
            else set(v);                        /* FX: the command / value becomes the last one */
        }
        return;
    }
    if (ui.view == V_SONG) fresh = last.song != EMPTY ? last.song : next_free_chain(EMPTY);
    else if (ui.view == V_CHAIN) fresh = last.ph != EMPTY ? last.ph : next_free_phrase(EMPTY);
    else if (ui.col <= 2u) fresh = ui.col == 0 ? last.note : ui.col == 1 ? last.vol : last.inst;
    else {                                      /* FX: the last command (by name), or the last value */
        uint32_t c = last_cmd_at(ui.row);
        if (!c) { say_not_param(); return; }
        if (on_val()) fresh = last.val;
        else fresh = (uint32_t)cmd_index(c, row_inst(ui.row));
    }
    if (fresh == EMPTY) { say(ui.view == V_SONG ? "no free chain" : "no free phrase"); return; }
    set(fresh);
    if (ui.view == V_PHRASE && ui.col == 0 && ni != NONE && cur_phrase_rows()) { cur_phrase_rows()[ui.row].i = (uint8_t)ni; last.ni = ni; }
}
static void edit_new(void)                      /* EDIT EDIT on a song / chain cell: a new empty chain / phrase */
{
    uint32_t n;
    char s[16];
    uint32_t g = get() == NONE ? EMPTY : get();
    if (ui.view == V_SONG) n = next_free_chain(g);
    else if (ui.view == V_CHAIN && ui.ccol == 0) n = next_free_phrase(g);
    else return;
    if (n == EMPTY) { say(ui.view == V_SONG ? "no free chain" : "no free phrase"); return; }
    set(n);
    str_cpy(s, ui.view == V_SONG ? "new chain " : "new phrase ", sizeof s);
    fmt_hex(s + str_len(s), n, 2);
    say(s);
}

static void edit_change(int32_t d, int big)     /* EDIT + d-pad (app.js change) */
{
    int32_t v = (int32_t)get(), lo, hi, step;
    if (v == (int32_t)NONE && emptyable()) { edit_press(); return; }
    if (ui.view == V_SONG) { lo = 0; hi = NCHN - 1; step = big ? 16 : 1; }
    else if (ui.view == V_CHAIN) {
        if (ui.ccol == 0) { lo = 0; hi = NPHR - 1; step = big ? 16 : 1; }
        else { lo = 0x80 - 48; hi = 0x80 + 48; step = big ? 12 : 1; }
    } else if (ui.col == 0) {
        if (v == (int32_t)NOTE_OFF) return;
        lo = NOTE_MIN; hi = NOTE_MAX; step = big ? 12 : 1;
        if (big && (v + d * step < lo || v + d * step > hi)) return;   /* an octave keeps the note name */
    } else if (ui.col == 1) { lo = 0; hi = VOL_MAX; step = big ? 16 : 1; }
    else if (ui.col == 2) { lo = 0; hi = 15; step = big ? 16 : 1; }
    else if (on_cmd()) {                        /* the command: one place in the list, either way */
        uint8_t l[CMD_MAX];
        lo = 0; hi = (int32_t)cmd_list(row_inst(ui.row), l, 0) - 1; step = 1;
    } else {                                    /* the value: 00..FF, or a list's items */
        uint32_t mx = cmd_value_max(cur_fx()->c, row_inst(ui.row));
        lo = 0; hi = (int32_t)mx; step = big && mx == 255u ? 16 : 1;
    }
    v += d * step;
    set((uint32_t)clamp_i(v, lo, hi));
}

static void edit_clear(void)                    /* OPTION + EDIT (app.js deleteCell) */
{
    if (!emptyable()) set(0x80);                /* transpose: 0 */
    else if (ui.view == V_PHRASE && ui.col == 0 && get() == NONE) set(NOTE_OFF);
    else if (on_val() && get() != NONE) set(0);        /* an FX value: 00 (stored as the last), the command stays */
    else set(NONE);
}

/* SHIFT + right on a sound command (its command or value cell): that parameter on the row's instrument's page,
 * the cursor on it (commands.md 9, app.js paramJump); in Q the cursor cell's, on the instrument shown; 0 = not a
 * sound command, or no page shows it */
static uint32_t q_cell_cmd(void);
static int param_jump(void)
{
    uint32_t k, pg, c, i = ui.inst;
    if (ui.view == V_Q) c = q_cell_cmd();
    else if (ui.view != V_PHRASE || ui.col < 3u || !cur_phrase_rows()) return 0;
    else { c = cur_fx()->c; i = row_inst(ui.row); }
    if (c < CMD_SOUND) return 0;
    for (pg = 0; pg < 4u; pg++)
        for (k = 0; k < IPAGES[pg].n; k++)
            if (IPAGES[pg].f[k].key == (c & 0x7Fu)) {
                ui.inst = i;
                ui.view = V_INST + pg;
                ui.field = k;
                ui.char_pos = 0;
                sel.on = mark.on = 0;
                return 1;
            }
    return 0;
}
/* EDIT tapped (let go without an arrow) on an instrument-page parameter: it becomes the last command, with its
 * current value, for EDIT on an empty FX command cell (commands.md 9, app.js copyParam); not ENGINE PRESET NAME
 * LEVEL (no command) */
static void param_copy(void)
{
    uint32_t key = IF[ui.field].key, c = CMD_SOUND | key;
    char s[24], hx[3];
    if (key >= P_COUNT || cmd_index(c, ui.inst) < 0) return;
    last.cmd = (uint8_t)c;
    last.val = (uint8_t)cmd_encode(c, ui.inst);
    cmd_name(c, ui.inst, last.cmd_name);
    str_cpy(s, "copied ", sizeof s);
    sa_n(s, last.cmd_name, sizeof s);
    sa_n(s, " ", sizeof s);
    fmt_hex(hx, last.val, 2);
    sa_n(s, hx, sizeof s);
    say(s);
}

/* ---------------------------------------------------------------- the FX grid and the explanations --- */
/* While EDIT is held on an FX cell, the bottom line explains it (commands.md 6, 7): the command (gen/help.h, the
 * prototype's texts), or what the value sets (`CUT 80 = 710Hz`, `TPO 78 = 120 BPM`). On a command, a grid of every
 * command opens under the column labels: one group a line (GLO MXR PHR INS ENV LFO VOX RAMP), 7 commands a line, a
 * long group wraps; left / right along the line (stopping at its ends), up / down to the line above / below
 * (the same place, or its last item); the highlighted command is the cell's (render.js cmdGrid, app.js cmdGridMove) */
static void cmd_help(uint32_t c, uint32_t i, char *s)
{
    char nm[4];
    const char *h = 0;
    uint32_t k, id = c & 0x7Fu;
    if (c < CMD_NPLAIN) { str_cpy(s, FX_HELP[c], 40); return; }
    cmd_name(c, i, nm);
    if (id >= P_E0 && id <= P_E7) {
        const param_desc_t *d = ins_desc(i, id);
        if (d != &ENGINES[ins[i].eng]->edit[id - P_E0])   /* a mode-dependent label (Felucca's desc hook) */
            for (k = 0; k < NELEM(RELABEL_HELP); k++) if (str_eq(RELABEL_HELP[k].label, d->label)) h = RELABEL_HELP[k].help;
        for (k = 0; !h && k < NELEM(PARAM_HELP); k++)
            if (str_eq(PARAM_HELP[k].engine, ENGINES[ins[i].eng]->name)) h = PARAM_HELP[k].help[id - P_E0];
    } else if (id == P_LEVEL) h = LVL_HELP;
    else
        for (k = 0; k < NELEM(SOUND_CMD); k++) if (SOUND_CMD[k].id == id) h = COMMON_HELP[k];
    str_cpy(s, nm, 40);
    sa(s, ": ");
    sa(s, h && *h ? h : "set the parameter");
}

static void cmd_value_text(uint32_t c, uint32_t i, uint32_t v, char *s)   /* model.js cmdValueText */
{
    char hx[4];
    int32_t sg = s8(v), x = (int32_t)(v >> 4), y = (int32_t)(v & 15u);
    cmd_name(c, i, s);
    fmt_hex(hx, v, 2);
    sa(s, " ");
    sa(s, hx);
    sa(s, " = ");
    switch (c) {
    case CMD_ARP: sa(s, "+"); sn(s, x); sa(s, " and +"); sn(s, y); sa(s, " semitones"); break;
    case CMD_DEL: sa(s, "start "); sn(s, (int32_t)v); sa(s, " ticks late"); break;
    case CMD_HOP: sa(s, "next phrase from row "); hx[0] = HEX[y]; hx[1] = 0; sa(s, hx); break;
    case CMD_KIL: sa(s, "cut after "); sn(s, (int32_t)v); sa(s, " ticks"); break;
    case CMD_OFF: sa(s, "release after "); sn(s, (int32_t)v); sa(s, " ticks"); break;
    case CMD_PIT: sa(s, sg > 0 ? "slide up " : sg < 0 ? "slide down " : "slide "); sn(s, sg < 0 ? -sg : sg); break;
    case CMD_TPO: sn(s, v >= 40u ? (int32_t)v : 40); sa(s, " BPM"); break;
    case CMD_MUT: case CMD_SOL:                 /* `MUT 31 = mute track 3` */
        if (x > 4) { sa(s, "nothing (tracks 1-4)"); break; }
        sa(s, y ? "" : "un"); sa(s, c == CMD_MUT ? "mute " : "solo ");
        if (x) { sa(s, "track "); sn(s, x); } else sa(s, "this track");
        break;
    case CMD_SCN: if (!v || v == 255u) sa(s, v ? "scene B" : "scene A"); else { sn(s, rdiv((int32_t)v * 100, 255)); sa(s, "% toward B"); } break;
    case CMD_VIB: sa(s, "speed "); sn(s, x); sa(s, ", depth "); sn(s, y); break;
    case CMD_TV1: case CMD_TV2: case CMD_TV3: case CMD_TV4: { char lv[10]; const char *u; param_format(&TP[P_LEVEL], tvl_raw(v), lv, &u); sa(s, lv); sa(s, u); } break;
    case CMD_SWG: sn(s, rdiv((int32_t)v * 100, 255)); sa(s, "% swing (odd rows late)"); break;
    case CMD_CHA: sa(s, "left "); sn(s, rdiv(x * 100, 15)); sa(s, "%, right "); sn(s, rdiv(y * 100, 15)); sa(s, "%"); break;
    case CMD_RET:
        if (y < 2) { sa(s, "no retrigger"); break; }
        sn(s, y); sa(s, " hits");
        if (x) { sa(s, x < 8 ? ", softer by " : ", louder by "); sn(s, x < 8 ? x : x - 7); }
        break;
    case CMD_REP: sa(s, sg >= 0 ? "+" : ""); sn(s, sg); sa(s, " per row"); if (!v) sa(s, " (stops)"); break;
    case CMD_RTO: sa(s, "stop at "); sa(s, hx); break;
    case CMD_RTI: sa(s, "reach it in "); sn(s, (int32_t)v); sa(s, v == 1u ? " row" : " rows"); break;
    case CMD_RCU: sa(s, RCU_NAMES[v < 2u ? v : 2u]); break;
    default: {                                  /* a sound command: what its parameter becomes */
        uint32_t items;
        const param_desc_t *d = cmd_desc(c, i, &items);
        const char *unit;
        char val[12];
        if (!d) break;
        param_format(d, cmd_target(c, i, v), val, &unit);
        sa(s, val);
        sa(s, unit);
    }
    }
}

/* the grid's lines: [start, count, group] in the row instrument's list (model.js cmdLines). For the knob slots
 * (model.js slotGroups): the sound commands, from ENG on (VOL first) */
#define GRID_PER 7u
typedef char grid_lines_fit[CMD_MAX / GRID_PER + G_N <= GRID_LINES ? 1 : -1];
static uint32_t grid_lines(uint32_t i, uint8_t *l, uint8_t (*ln)[3], int slots)
{
    uint8_t g[G_N];
    uint32_t n = cmd_list(i, l, g), k = 0, nl = 0;
    if (slots) k = G_INS;                       /* the knob slots: the sound commands' groups (model.js slotGroups) */
    for (; k < (slots ? G_RAMP : G_N); k++) {
        uint32_t a = g[k], b = k + 1u < G_N ? g[k + 1] : n, j;
        for (j = a; j < b; j += GRID_PER) {
            ln[nl][0] = (uint8_t)j;
            ln[nl][1] = (uint8_t)(b - j < GRID_PER ? b - j : GRID_PER);
            ln[nl][2] = (uint8_t)k;
            nl++;
        }
    }
    return nl;
}
/* the grid's highlighted place: the phrase cell's command (its place in the list, get()), or knob slot n's
 * parameter */
static uint32_t grid_cur(int slots, const uint8_t *l, uint8_t (*ln)[3], uint32_t nl)
{
    uint32_t k, c;
    if (!slots) return get() != NONE ? get() : (uint32_t)cmd_index(CMD_SOUND | P_LEVEL, row_inst(ui.row));
    c = CMD_SOUND | slot_key(ui.inst, IF[ui.field].key - FK_KNOB);
    for (k = ln[0][0]; k < (uint32_t)ln[nl - 1u][0] + ln[nl - 1u][1]; k++) if (l[k] == c) return k;
    return ln[0][0];
}
/* app.js cmdGridMove: arrows move as they look; up / down keep the place left / right last chose, so it comes back
 * after a short line (reset at each EDIT press) */
static int32_t grid_col = -1;
static void cmd_grid_move(int slots, int32_t dx, int32_t dy)
{
    uint8_t l[CMD_MAX], ln[GRID_LINES][3];
    uint32_t i = slots ? ui.inst : row_inst(ui.row), nl = grid_lines(i, l, ln, slots), cur = grid_cur(slots, l, ln, nl), y, x;
    int32_t nx, ny;
    if (cur == NONE) return;
    for (y = 0; y < nl && !(cur >= ln[y][0] && cur < (uint32_t)ln[y][0] + ln[y][1]); y++)
        ;
    if (y >= nl) return;
    x = cur - ln[y][0];
    nx = (int32_t)x + dx;
    ny = (int32_t)y + dy;
    if (ny < 0 || ny >= (int32_t)nl || nx < 0) return;
    if (dx) { if (nx >= (int32_t)ln[ny][1]) return; grid_col = nx; }
    else {
        if (grid_col < 0) grid_col = (int32_t)x;
        nx = grid_col < (int32_t)ln[ny][1] - 1 ? grid_col : (int32_t)ln[ny][1] - 1;
    }
    if (slots) { if (!sel_held) S.knob[ui.inst][IF[ui.field].key - FK_KNOB] = l[ln[ny][0] + (uint32_t)nx] & 0x7Fu; }
    else set(ln[ny][0] + (uint32_t)nx);
}
static void draw_cmd_grid(int slots)            /* drawn over the rows, in bands the canvas holds */
{
    uint8_t l[CMD_MAX], ln[GRID_LINES][3];
    uint32_t i = slots ? ui.inst : row_inst(ui.row), nl = grid_lines(i, l, ln, slots), cur = grid_cur(slots, l, ln, nl), k, ngroups = 1;
    int32_t top = ROWS_Y + 1, h, w = (6 - 1 + (int32_t)GRID_PER * 4) * CW, x0 = gx(1) - 4, band;
    static const uint8_t GROUP_VIEW[G_N] = {V_PROJ, V_MIX, V_PHRASE, V_INST, V_ENV, V_LFO, V_VOX, EMPTY};   /* (render.js
                                                 * GROUP_VIEW: the screen whose values a group moves; RAMP none) */
    for (k = 1; k < nl; k++) if (ln[k][2] != ln[k - 1][2]) ngroups++;
    for (k = 0; k < nl; k++) if (cur >= ln[k][0] && cur < (uint32_t)ln[k][0] + ln[k][1]) map_lit = GROUP_VIEW[ln[k][2]];
    h = (int32_t)nl * LH + ((int32_t)ngroups - 1) * 3 + 6;
    for (band = ROWS_Y; band < top + h; band += CV_ROWS) {   /* (from the rows' top: row 0's first line too) */
        int32_t bh = top + h - band < CV_ROWS ? top + h - band : CV_ROWS, y = top + 3;
        char nm[4];
        cv_begin(240, (uint32_t)bh, P_BG);
        cv_oy = -band;
        cv_rect(x0, top, w + 4, h, P_SURF);
        cv_frame(x0, top, w + 4, h, P_ACCENT);
        for (k = 0; k < nl; k++) {
            uint32_t j;
            if (k && ln[k][2] != ln[k - 1][2]) { cv_rect(gx(1) - 2, y, w, 1, P_LINE); y += 3; }
            if (!k || ln[k][2] != ln[k - 1][2]) cv_str(gx(1), y, GROUP_TITLE[ln[k][2]], P_CTX);
            for (j = 0; j < ln[k][1]; j++) {
                int32_t px = gx(6 + 4 * (int32_t)j);
                cmd_name(l[ln[k][0] + j], i, nm);
                if (ln[k][0] + j == cur) { cv_rect(px - 1, y, 3 * CW + 2, LH, P_ACCENT); cv_str(px, y, nm, P_BG); }
                else cv_str(px, y, nm, P_TEXT);
            }
            y += LH;
        }
        cv_oy = 0;
        cv_map(band, bh);
        cv_blit(0, (uint32_t)band);
    }
}


/* ---------------------------------------------------------------- rows and selections (docs/ux/row-ops.md) --- */
/* The row number is a cell (left of the first column): EDIT EDIT inserts an empty row, EDIT held + up/down moves
 * the row, OPTION + EDIT deletes it; a list keeps its length (the last row drops out / an empty one comes in).
 * The phrase's column labels are cells too (up from row 0). SHIFT + OPTION starts a selection (on a row number:
 * whole rows; on a column label: whole columns), the d-pad extends it, OPTION + EDIT deletes it, EDIT + d-pad
 * changes every value in it (empty stays empty; whole rows: up/down move them). OPTION copies it: it stays marked,
 * drawn only, every key working as without it (unit-feedback #28; a plain OPTION drops the mark). SHIFT + EDIT pastes
 * at the cursor. (app.js rowOp / startSel / copySel / paste /
 * deleteSel / changeBlock / moveRows) */
static struct { uint8_t view, nr, nc; uint16_t v[NSONG * NCH]; } clip;   /* (the song's 64 x 4 is the largest block) */
typedef struct { uint32_t r0, r1, c0, c1, rows; } bounds_t;

static uint32_t grid_rows(void) { return ui.view == V_SONG ? NSONG : 16u; }
static uint32_t grid_cols(void) { return ui.view == V_SONG ? NCH : ui.view == V_CHAIN ? 2u : 9u; }
static uint32_t *cur_r(void) { return ui.view == V_SONG ? &ui.song_row : ui.view == V_CHAIN ? &ui.chain_row : &ui.row; }
static uint32_t *cur_c(void) { return ui.view == V_SONG ? &ui.ch : ui.view == V_CHAIN ? &ui.ccol : &ui.col; }
static uint32_t get_at(uint32_t r, uint32_t c)  /* the grid cell (r, c), through get() */
{
    uint32_t sr = *cur_r(), sc = *cur_c(), v;
    *cur_r() = r; *cur_c() = c;
    v = get();
    *cur_r() = sr; *cur_c() = sc;
    return v;
}
static void at_cell(uint32_t r, uint32_t c, void (*fn)(uint32_t, int32_t, int32_t), uint32_t v, int32_t d, int32_t big)
{
    uint32_t sr = *cur_r(), sc = *cur_c();
    *cur_r() = r; *cur_c() = c;
    fn(v, d, big);
    *cur_r() = sr; *cur_c() = sc;
}
static void do_set(uint32_t v, int32_t d, int32_t big) { (void)d; (void)big; set(v); }
static void do_change(uint32_t v, int32_t d, int32_t big) { (void)v; edit_change(d, big); }

static int block(bounds_t *b)                   /* the selection: what EDIT + d-pad, OPTION + EDIT, PLAY work on */
{
    uint32_t r = *cur_r(), c = *cur_c();
    if (!sel.on) return 0;
    b->r0 = sel.r < r ? sel.r : r; b->r1 = sel.r < r ? r : sel.r;
    b->c0 = sel.c < c ? sel.c : c; b->c1 = sel.c < c ? c : sel.c;
    if (sel.rows) { b->c0 = 0; b->c1 = grid_cols() - 1u; }
    if (sel.cols) { b->r0 = 0; b->r1 = grid_rows() - 1u; }
    b->rows = sel.rows;
    return 1;
}
static int any_block(void) { bounds_t b; return block(&b); }
/* drawn as selected: the selection, else the marked block on this screen (drawn only: unit-feedback #28) */
static int in_block(uint32_t r, uint32_t c)
{
    bounds_t b;
    if (!block(&b)) {
        if (!mark_shown()) return 0;
        b.r0 = mark.r0; b.r1 = mark.r1; b.c0 = mark.c0; b.c1 = mark.c1;
    }
    return r >= b.r0 && r <= b.r1 && c >= b.c0 && c <= b.c1;
}

/* the rows of the screen's list as bytes: the song's 64 x 4, the chain's 16, the phrase's 16 (it is created if
 * the cell above is empty, as an edit would) */
static uint8_t *row_list(uint32_t *size, uint32_t *n)
{
    uint32_t x;
    if (ui.view == V_SONG) { *size = NCH; *n = NSONG; return &S.song[0][0]; }
    if (ui.view == V_CHAIN) { if ((x = ensure_chain()) == EMPTY) return 0; *size = sizeof(crow_t); *n = 16; return (uint8_t *)S.chains[x]; }
    if ((x = ensure_phrase()) == EMPTY) return 0;
    *size = sizeof(row_t); *n = 16;
    return (uint8_t *)S.phrases[x];
}
static void row_clear(uint8_t *row, uint32_t size)
{
    memset(row, ui.view == V_PHRASE ? 0 : EMPTY, size);
    if (ui.view == V_CHAIN) ((crow_t *)row)->tr = 0;
    if (ui.view == V_PHRASE) { row_t *w = (row_t *)row; w->n = w->vol = w->i = EMPTY; }
}
static void row_insert(void)
{
    uint32_t sz, n, r = *cur_r();
    uint8_t *b = row_list(&sz, &n);
    if (!b) return;
    mem_move(b + (r + 1u) * sz, b + r * sz, (n - r - 1u) * sz);
    row_clear(b + r * sz, sz);
    say("row inserted");
}
static void row_delete(void)
{
    uint32_t sz, n, r = *cur_r();
    uint8_t *b = row_list(&sz, &n);
    if (!b) return;
    mem_move(b + r * sz, b + (r + 1u) * sz, (n - r - 1u) * sz);
    row_clear(b + (n - 1u) * sz, sz);
    say("row deleted");
}
/* rows r0..r1 one row up (d -1) or down (+1), only their columns c0..c1 (whole rows: 0..~0; a grid column is its byte
 * in the row: song track, chain ph / tr, phrase n vol i and the FX pairs); the row landed on goes to the other end */
static void rows_move(uint32_t r0, uint32_t r1, int32_t d, uint32_t c0, uint32_t c1)
{
    uint32_t sz, n, r, w, from, to;
    uint8_t *b = row_list(&sz, &n), tmp[sizeof(row_t)];
    if (!b || (d < 0 && r0 == 0u) || (d > 0 && r1 + 1u >= n)) return;
    w = (c1 < sz ? c1 + 1u : sz) - c0;
    from = d < 0 ? r0 - 1u : r1 + 1u; to = d < 0 ? r1 : r0;
    memcpy(tmp, b + from * sz + c0, w);
    for (r = from; r != to; r = (uint32_t)((int32_t)r - d)) memcpy(b + r * sz + c0, b + (uint32_t)((int32_t)r - d) * sz + c0, w);
    memcpy(b + to * sz + c0, tmp, w);
    *cur_r() = (uint32_t)((int32_t)*cur_r() + d);
    if (sel.on) sel.r = (uint32_t)((int32_t)sel.r + d);
}

static void sel_start(void)                     /* SHIFT + OPTION */
{
    if (sel.on) { sel.on = 0; return; }         /* again: no selection, quietly (unit-feedback #27) */
    mark.on = 0;
    sel.on = 1;
    sel.r = *cur_r(); sel.c = *cur_c();
    sel.rows = (uint8_t)ui.on_row;
    sel.cols = (uint8_t)ui.on_head;
    if (sel.rows) sel.c = 0;
    if (sel.cols) sel.r = 0;
    say("select: D-PAD, OPTION copies");
}
static void sel_copy(void)                      /* OPTION: copied, and the block stays marked */
{
    bounds_t b;
    uint32_t r, c, k = 0;
    char s[20];
    block(&b);
    clip.view = (uint8_t)ui.view;
    clip.nr = (uint8_t)(b.r1 - b.r0 + 1u);
    clip.nc = (uint8_t)(b.c1 - b.c0 + 1u);
    for (r = b.r0; r <= b.r1; r++) for (c = b.c0; c <= b.c1; c++) clip.v[k++] = (uint16_t)get_at(r, c);
    mark.on = 1; mark.view = (uint8_t)ui.view; mark.rows = (uint8_t)b.rows; mark.at = (uint8_t)mark_place();
    mark.r0 = b.r0; mark.r1 = b.r1; mark.c0 = b.c0; mark.c1 = b.c1;
    sel.on = 0;
    str_cpy(s, "copied ", sizeof s);
    sn_n(s, (int32_t)clip.nr, sizeof s); sa_n(s, "x", sizeof s);
    sn_n(s, (int32_t)clip.nc, sizeof s);
    say(s);
}
static void paste(void)                         /* SHIFT + EDIT: the copied block, its corner at the cursor */
{
    static const char *const FROM[3] = {"copied from song", "copied from chain", "copied from phrase"};
    uint32_t i, j, r = *cur_r(), c = *cur_c();
    if (!clip.nr) { say("nothing copied"); return; }
    if (clip.view != ui.view) { say(FROM[clip.view]); return; }
    if (mark_shown() && r == mark.r0 && c == mark.c0) { say("already there"); return; }   /* onto itself: a no-op (#22) */
    for (i = 0; i < clip.nr; i++) {
        uint32_t n0 = clip.v[i * clip.nc];      /* over an OFF, N V I paste only with a copied note (user) */
        int off = ui.view == V_PHRASE && r + i < 16u && get_at(r + i, 0) == NOTE_OFF && !(c == 0 && n0 != NONE && n0 != NOTE_OFF);
        for (j = 0; j < clip.nc; j++)
            if (r + i < grid_rows() && c + j < grid_cols() && !(off && c + j < 3u)) at_cell(r + i, c + j, do_set, clip.v[i * clip.nc + j], 0, 0);
    }
    sel.on = 0;                                 /* the mark stays: another paste can follow (mark_check) */
    *cur_r() = r + clip.nr < grid_rows() ? r + clip.nr : grid_rows() - 1u;   /* the row after the block, or the last (#21) */
    say("pasted");
}
/* unit-feedback #22 (user, 2026-10-12): the mark goes as soon as a copied cell no longer holds what was copied (an
 * edit, an overlapping paste, recording, undo, a row moved); checked while its place is on screen, so a change made
 * elsewhere drops it on the way back. 1 = it went. */
static int mark_check(void)
{
    uint32_t r, c;
    if (!mark_shown() || clip.view != mark.view) return 0;
    for (r = mark.r0; r <= mark.r1; r++)
        for (c = mark.c0; c <= mark.c1; c++)
            if (get_at(r, c) != clip.v[(r - mark.r0) * clip.nc + c - mark.c0]) { mark.on = 0; return 1; }
    return 0;
}
static void block_delete(void)                  /* OPTION + EDIT on a block: back to empty (a transpose: 0) */
{
    bounds_t b;
    uint32_t r, c;
    if (!block(&b)) { b.r0 = mark.r0; b.r1 = mark.r1; b.c0 = mark.c0; b.c1 = mark.c1; }   /* (just copied: opt_copy) */
    for (r = b.r0; r <= b.r1; r++)
        for (c = b.c0; c <= b.c1; c++)
            at_cell(r, c, do_set, ui.view == V_CHAIN && c == 1u ? 0x80u : NONE, 0, 0);
    sel.on = mark.on = 0;
    say("deleted");
}
static void block_change(const bounds_t *b, int32_t d, int32_t big)   /* EDIT + d-pad on a block: empty stays empty */
{
    uint32_t r, c;
    for (r = b->r0; r <= b->r1; r++)
        for (c = b->c0; c <= b->c1; c++)
            if (get_at(r, c) != NONE || (ui.view == V_CHAIN && c == 1u)) at_cell(r, c, do_change, 0, d, big);
}
static void column_clear(void)                  /* OPTION + EDIT on a column label: every cell (a value: 00) */
{
    uint32_t r, c = ui.col, value = c >= 4u && (c - 3u) % 2u == 1u;
    for (r = 0; r < 16u; r++)
        if (!value || get_at(r, c) != NONE) at_cell(r, c, do_set, value ? 0u : NONE, 0, 0);
    say("column cleared");
}

/* SHIFT + EDIT on a selection (docs/ux/interpolate.md, option A; app.js cloneSel / interpolate / deepClone): inside
 * one value column, a straight line from the first row's value to the last row's (the ramp stays selected); else every
 * chain (song) / phrase (chain's first column) in it becomes a copy with the next free number. Again right away in
 * the song: deep clone, the cloned chains get their own copies of their phrases (one per phrase, shared inside). */
static uint8_t cloned[NCHN], n_cloned;          /* the chains the last song clone made */
static struct { uint8_t on, ph, k, r0, r1; } interp;   /* the last interpolated FX value stretch */
static int ref_col(uint32_t c) { return ui.view == V_SONG || (ui.view == V_CHAIN && c == 0u); }
static int interpolable(const bounds_t *b)
{
    return b->c0 == b->c1 && b->r0 != b->r1 && !ref_col(b->c0) && !(ui.view == V_PHRASE && b->c0 >= 3u && (b->c0 - 3u) % 2u == 0u);
}
static void interpolate(const bounds_t *b)
{
    uint32_t r, c = b->c0, v0 = get_at(b->r0, c), v1 = get_at(b->r1, c);
    int32_t d = (int32_t)v1 - (int32_t)v0, n = (int32_t)(b->r1 - b->r0);
    char s[24];
    if (v0 == NONE || v1 == NONE || (ui.view == V_PHRASE && c == 0u && (v0 == NOTE_OFF || v1 == NOTE_OFF))) {
        say("first and last row need a value"); return;
    }
    for (r = b->r0; r <= b->r1; r++) {
        int32_t num = 2 * d * (int32_t)(r - b->r0) + n, q = num / (2 * n);
        if (num % (2 * n) && num < 0) q--;      /* floor: rounds halves up, as Math.round */
        if (c >= 4u) {                          /* an FX value: the first row's command comes along */
            row_t *rows = cur_phrase_rows();
            fx_t *f = &rows[r].fx[(c - 3u) / 2u];
            if (!f->c) f->c = rows[b->r0].fx[(c - 3u) / 2u].c;
        }
        at_cell(r, c, do_set, (uint32_t)((int32_t)v0 + q), 0, 0);
    }
    /* (the ramp stays selected, unit-feedback #28: EDIT + d-pad can shift it) */
    if (c >= 4u) {                              /* SHIFT + EDIT again: a ramp (rampify) */
        interp.on = 1; interp.ph = (uint8_t)cur_phrase(); interp.k = (uint8_t)((c - 3u) / 2u);
        interp.r0 = (uint8_t)b->r0; interp.r1 = (uint8_t)b->r1;
    }
    str_cpy(s, "interpolated ", sizeof s);
    sn_n(s, n + 1, sizeof s); sa_n(s, " rows", sizeof s);
    say(s);
}
/* SHIFT + EDIT again right after interpolating FX values (app.js rampify, docs/ux/knob-record.md): the stretch
 * becomes a ramp, smooth every tick. The first row keeps its command and value, the next two get RTO <last value>
 * and RTI <rows>, the rest is emptied; it arrives at the last row. */
static void rampify(void)
{
    row_t *rows = S.phrases[interp.ph];
    uint32_t r, k = interp.k, n = (uint32_t)(interp.r1 - interp.r0), v1 = rows[interp.r1].fx[k].v;
    char s[32];
    if (n < 2u) { say("ramp needs 3 rows"); return; }
    for (r = interp.r0 + 1u; r <= interp.r1; r++) rows[r].fx[k].c = rows[r].fx[k].v = 0;
    rows[interp.r0 + 1u].fx[k].c = CMD_RTO; rows[interp.r0 + 1u].fx[k].v = (uint8_t)v1;
    rows[interp.r0 + 2u].fx[k].c = CMD_RTI; rows[interp.r0 + 2u].fx[k].v = (uint8_t)n;
    sel.on = 0;
    str_cpy(s, "ramp ", sizeof s);
    cmd_name(rows[interp.r0].fx[k].c, row_inst(interp.r0), s + 5);   /* (the phrase is still on screen) */
    sa_n(s, " ", sizeof s);
    fmt_hex(s + str_len(s), rows[interp.r0].fx[k].v, 2); sa_n(s, ">", sizeof s);
    fmt_hex(s + str_len(s), v1, 2); sa_n(s, " in ", sizeof s);
    sn_n(s, (int32_t)n, sizeof s); sa_n(s, " rows", sizeof s);
    say(s);
}
static void clone_sel(const bounds_t *b)
{
    uint32_t r, c, v, x = 0, done = 0;
    char s[24];
    sel.on = 0;
    n_cloned = 0;
    for (r = b->r0; r <= b->r1; r++)
        for (c = b->c0; c <= b->c1; c++) {
            if (!ref_col(c) || (v = get_at(r, c)) == NONE) continue;
            x = ui.view == V_SONG ? next_free_chain(v) : next_free_phrase(v);
            if (x == EMPTY) { say(ui.view == V_SONG ? "no free chain" : "no free phrase"); n_cloned = 0; return; }
            if (ui.view == V_SONG) { memcpy(S.chains[x], S.chains[v], sizeof S.chains[0]); cloned[n_cloned++] = (uint8_t)x; }
            else memcpy(S.phrases[x], S.phrases[v], sizeof S.phrases[0]);
            at_cell(r, c, do_set, x, 0, 0);     /* (marks x as used before the next next_free_*) */
            done++;
        }
    if (!done) { say("nothing to clone here"); return; }
    str_cpy(s, done == 1u ? "cloned as " : "cloned ", sizeof s);
    if (done == 1u) fmt_hex(s + str_len(s), x, 2); else sn_n(s, (int32_t)done, sizeof s);
    say(s);
}
static void deep_clone(uint32_t n)              /* n: the chains cloned[0..n-1] */
{
    uint8_t copy[NPHR];
    uint32_t i, r, made = 0;
    char s[40];
    memset(copy, EMPTY, sizeof copy);
    for (i = 0; i < n; i++)
        for (r = 0; r < 16u; r++) {
            crow_t *w = &S.chains[cloned[i]][r];
            if (w->ph == EMPTY) continue;
            if (copy[w->ph] == EMPTY) {
                uint32_t x = next_free_phrase(w->ph);
                if (x == EMPTY) { say("no free phrase"); return; }
                memcpy(S.phrases[x], S.phrases[w->ph], sizeof S.phrases[0]);
                copy[w->ph] = (uint8_t)x;
                made++;
            }
            w->ph = copy[w->ph];                /* (marks the copy as used before the next next_free_phrase) */
        }
    str_cpy(s, "deep cloned: ", sizeof s);
    sn_n(s, (int32_t)made, sizeof s); sa_n(s, " phrases", sizeof s);
    if (undo_close()) sa_n(s, " (no undo)", sizeof s);   /* (its own step, closed now to say so) */
    say(s);
}

/* the gamepad's keys (control-map.js PAD) */
enum { PAD_LEFT = 0, PAD_DOWN = 2, PAD_UP = 3, PAD_RIGHT = 4, PAD_SHIFT = 21, PAD_PLAY = 23, PAD_OPTION = 24, PAD_EDIT = 26 };
#define PAD_KEYS (1u << PAD_LEFT | 1u << PAD_DOWN | 1u << PAD_UP | 1u << PAD_RIGHT | 1u << PAD_SHIFT | 1u << PAD_PLAY \
                  | 1u << PAD_OPTION | 1u << PAD_EDIT)
#define REP_WAIT 350u                           /* a held arrow: the first repeat (ms), then one every REP_EVERY */
#define REP_EVERY 60u
static uint8_t key_rep;                         /* this pass's arrow is a held one's repeat: it stops before the row
                                                 * number and the column labels, a tap goes there (unit-feedback.md #50) */
/* ---- the popups while EDIT is held (app.js popupState, render.js popup / keyboard): a list of choices, the current
 * one highlighted, up / down one item, left / right four, stopping at the ends (the phrase's instrument; on the
 * instrument page ENGINE, PRESET and every named list; on / off fields have none), and the note's 2-octave keyboard
 * (it scrolls an octave when the note leaves it). Not with SHIFT or OPTION, a selection, a row number or a label. */
enum { POP_NONE, POP_LIST, POP_KEYS };
enum { OV_NONE, OV_GRID, OV_CHARS, OV_LIST, OV_KEYS, OV_KSTRIP, OV_SSTRIP = 8 };   /* what is drawn over the rows (OV_CHARS +
                                                 * POP_LIST / POP_KEYS = OV_LIST / OV_KEYS; OV_KSTRIP + 1: at the top) */
#define CHARS_UNDER (3u << 3)                   /* the I page rows the character grid covers: not redrawn under it */
#define POP_MAX 64u
static uint32_t kb_base = EMPTY;                /* the keyboard's lowest C while it is open */
static uint32_t popup_kind(uint32_t keys)
{
    bounds_t b;
    if (!(keys >> PAD_EDIT & 1u) || keys >> PAD_SHIFT & 1u || keys >> PAD_OPTION & 1u || ui.on_row || ui.on_head || block(&b)) return POP_NONE;
    if (ui.view == V_PHRASE) {
        uint32_t v = get();
        if (v == NONE) return POP_NONE;
        if (ui.col == 0u) return v == NOTE_OFF ? POP_NONE : POP_KEYS;
        return ui.col == 2u ? POP_LIST : POP_NONE;
    }
    if (ui.view == V_FX) return GP[FXF[ui.ff].g].fmt == F_ENUM ? POP_LIST : POP_NONE;
    if (!IS_PAGE(ui.view) || IF[ui.field].key == FK_NAME || IS_KNOB(IF[ui.field].key)) return POP_NONE;
    return fkey(ui.field) >= P_COUNT || field_desc(ui.field)->fmt == F_ENUM ? POP_LIST : POP_NONE;
}
static uint32_t param_walk(const param_desc_t *d, int32_t now, int16_t *vals)   /* every value, in knob order (Felucca's
                                                 * param_turn: from now all the way down, then up) */
{
    uint32_t n = 0;
    int32_t v = param_turn(d, now, -(d->max - d->min) - 1), w;
    for (;;) { vals[n++] = (int16_t)v; w = param_turn(d, v, 1); if (w == v || n == POP_MAX) break; v = w; }
    return n;
}
static void param_text(const param_desc_t *d, int32_t v, char *t) { const char *unit; param_format(d, v, t, &unit); sa(t, unit); }
/* the list's values in the order they are shown (as a knob steps them: past aliases), and the current one's place */
static uint32_t pop_values(int16_t *vals, uint32_t *cur)
{
    uint32_t n = 0, k, key = fkey(ui.field);
    const inst_t *I = &ins[ui.inst];
    int32_t now;
    if (ui.view == V_PHRASE) { for (k = 0; k < NINS; k++) vals[n++] = (int16_t)k; now = (int32_t)get(); }
    else if (ui.view == V_FX) {                 /* F: an effect's list (TIME, TYPE), in Felucca's knob order */
        n = param_walk(&GP[FXF[ui.ff].g], S.fx[ui.ff], vals);
        now = sc_value(SC_FX, ui.ff, S.fx[ui.ff]);   /* (the value an edit changes: the near scene's if it holds it) */
    } else if (key == FK_ENGINE) { for (k = 0; k < NENG_SHOWN; k++) vals[n++] = (int16_t)eng_vis(k); now = I->eng; }
    else if (key == FK_PRESET) {
        const engine_t *e = ENGINES[I->eng];
        for (k = 0; k < e->npresets && n < POP_MAX; k++) if (preset_orig(e, k) == k) vals[n++] = (int16_t)k;
        now = I->preset;
    } else {
        n = param_walk(field_desc(ui.field), I->p[key], vals);
        now = sc_value(ui.inst, key, I->p[key]);
    }
    for (*cur = 0; *cur < n && vals[*cur] != now; (*cur)++) {}
    return n;
}
static void pop_text(int16_t v, char *t)        /* one item, as the cell / field shows it */
{
    uint32_t key = fkey(ui.field);
    if (ui.view == V_PHRASE) { t[0] = HEX[v & 15]; t[1] = ' '; str_cpy(t + 2, ins[v].name, 14); }
    else if (ui.view == V_FX) param_text(&GP[FXF[ui.ff].g], v, t);
    else if (key == FK_ENGINE) str_cpy(t, ENGINES[v]->name, 16);
    else if (key == FK_PRESET) str_cpy(t, ENGINES[ins[ui.inst].eng]->presets[v].name, 16);
    else param_text(field_desc(ui.field), v, t);
}
static void pop_move(int32_t dx, int32_t dy)    /* EDIT + d-pad in a list: down / right = on */
{
    int32_t st = dx ? 4 * dx : dy;
    if (ui.view == V_PHRASE) {
        int32_t v = (int32_t)get() + st;
        set((uint32_t)clamp_i(v, 0, (int32_t)NINS - 1));
    } else if (ui.view == V_FX) fx_change(ui.ff, st);
    else inst_change(ui.field, st);
}
static uint32_t q_played;                       /* the instruments the song plays, a bit each (set by q_build, below) */
static void q_build(void);
static void draw_list(void)                     /* 14 characters wide: on F and the pages right under the value cell, else
                                                 * above its label (unit feedback #13); else on the side away from the cursor */
{
    int16_t vals[POP_MAX];
    uint32_t cur, n = pop_values(vals, &cur), first = cur < 16u || cur >= n ? 0u : cur - 15u, k;   /* (16 lines) */
    uint32_t cx = ui.view == V_PHRASE ? PH_X[2] : ui.view == V_FX ? FXF[ui.ff].x : IF[ui.field].x, lx = cx < 17u ? PREV_END - 14u : 0u, nl = n - first < 16u ? n - first : 16u;
    int32_t x0, y0 = ROWS_Y - 2, w = 14 * CW + 6, h = (int32_t)nl * LH + 4, b0, bh = (int32_t)(CV_MAX / (uint32_t)w);
    int32_t ly = ui.view == V_FX ? FX_Y[FXF[ui.ff].row] : IS_PAGE(ui.view) ? PG.y[IF[ui.field].row] : -1;   /* the label line */
    if (ly >= 0 && ly + 2 * LH + 1 + h <= STRIP_Y) { y0 = ly + 2 * LH + 1; lx = cx < PREV_END - 14u ? cx : PREV_END - 14u; }
    else if (ly >= 0 && ly - h - 1 >= ROWS_Y) { y0 = ly - h - 1; lx = cx < PREV_END - 14u ? cx : PREV_END - 14u; }
    x0 = gx((int32_t)lx) - 3;
    uint32_t mt = ui.view == V_FX ? SC_FX : IS_PAGE(ui.view) && fkey(ui.field) < P_COUNT ? ui.inst : EMPTY, mp = ui.view == V_FX ? ui.ff : fkey(ui.field);
    int32_t ma = mt == EMPTY ? -1 : sc_find(S.scn.a, mt, mp), mb = mt == EMPTY ? -1 : sc_find(S.scn.b, mt, mp);   /* the items
                                                 * the scenes on A / B hold: A / B / AB at the right (param-values.md 3) */
    if (ma >= 0) ma = S.scn.k[S.scn.a][ma].v; else ma = -1000;
    if (mb >= 0) mb = S.scn.k[S.scn.b][mb].v; else mb = -1000;
    clip_to(x0, y0, w, h);
    if (ui.view == V_PHRASE) q_build();         /* the instruments: dim = the song never plays it, the ones Q's
                                                 * OPTION + left / right skips (user, 2026-10-11, principles-review.md P1) */
    for (b0 = 0; b0 < h; b0 += bh) {            /* in bands the canvas holds */
        cv_begin((uint32_t)w, (uint32_t)(h - b0 < bh ? h - b0 : bh), P_SURF);
        cv_oy = -b0;
        cv_frame(0, 0, w, h, P_ACCENT);
        for (k = 0; k < nl; k++) {
            char t[40];
            pop_text(vals[first + k], t);
            t[14] = 0;
            if (first + k == cur) { cv_rect(2, 2 + (int32_t)k * LH, 14 * CW + 2, LH, P_ACCENT); cv_str(3, 2 + (int32_t)k * LH, t, P_BG); }
            else cv_str(3, 2 + (int32_t)k * LH, t, ui.view == V_PHRASE && !(q_played >> vals[first + k] & 1u) ? P_DIM : P_TEXT);
            if (vals[first + k] == ma || vals[first + k] == mb) {
                const char *m = vals[first + k] != mb ? "A" : vals[first + k] != ma ? "B" : "AB";
                cv_str(3 + (14 - (int32_t)str_len(m)) * CW, 2 + (int32_t)k * LH, m, first + k == cur ? P_BG : P_CTX);
            }
        }
        cv_oy = 0;
        cv_blit((uint32_t)x0, (uint32_t)(y0 + b0));
    }
}
static void draw_keys(void)                     /* two octaves from kb_base, C keys labelled, in the other half */
{
    static const uint8_t WHITE[7] = {0, 2, 4, 5, 7, 9, 11}, BLACK[5] = {1, 3, 6, 8, 10};
    uint32_t v = get(), o, i;
    int32_t kw = 12, kx = gx(2), top = ui.row < 8u ? ROWS_Y + 9 * LH : ROWS_Y, x0 = kx - 4, y0 = top - 2;
    int32_t w = 14 * kw + 8, h = 46 + LH + 6;
    clip_to(x0, y0, w, h);
    if (kb_base == EMPTY) kb_base = v - v % 12u;
    while (v < kb_base) kb_base -= 12u;
    while (v >= kb_base + 24u) kb_base += 12u;
    cv_begin((uint32_t)w, (uint32_t)h, P_SURF);
    cv_frame(0, 0, w, h, P_ACCENT);
    for (o = 0; o < 2u; o++)
        for (i = 0; i < 7u; i++) {
            uint32_t note = kb_base + o * 12u + WHITE[i];
            int32_t x = kx - x0 + (int32_t)(o * 7u + i) * kw;
            cv_rect(x, 2, kw - 1, 46, note == v ? P_ACCENT : P_TEXT);
            if (!WHITE[i]) {                    /* C-4, as the note cells (principles-review.md P2) */
                char t[4]; t[0] = 'C'; t[1] = '-'; t[2] = (char)('0' + note / 12u - 1u); t[3] = 0;
                cv_str(x, 2 + 46 + 2, t, P_LABEL);
            }
        }
    for (o = 0; o < 2u; o++)
        for (i = 0; i < 5u; i++) {
            uint32_t note = kb_base + o * 12u + BLACK[i], wi = BLACK[i] < 5u ? (BLACK[i] - 1u) / 2u : BLACK[i] / 2u;
            int32_t x = kx - x0 + (int32_t)(o * 7u + wi) * kw + kw - 4;
            cv_rect(x, 2, 8, 28, note == v ? P_ACCENT : P_BG);
        }
    cv_blit((uint32_t)x0, (uint32_t)y0);
}

/* ---------------------------------------------------------------- undo / redo (HANDOFF section 0, panel buttons) --- */
/* Panel EDIT + OCT- / OCT+. The prototype keeps 64 copies of the song; that would not fit here, so: one shadow copy
 * of what undo covers (S but its tempo, and the instruments) and a log of what each step changed, as runs
 * {offset (2 bytes), length, was ^ now per byte} (docs/ux/undo-log.md): undo and redo are the same XOR; a gap of up
 * to 3 unchanged bytes stays in its run as 00, cheaper than a new run's 3-byte head. A step bigger than the log is
 * kept and clears the history, and says so (user, 2026-10-11: no confirm, the only such step adds, deep clone).
 * A step closes at the next input; EDIT + d-pad or knob turns on one cell within 1.5 s stay one step (app.js
 * snapshot), and an edit that changes nothing is no step. Undo / redo leave the cursor where it is, flash the
 * changed cells on screen white for 1.2 s and say what changed (`UNDO P09 6 FX1 VOL 80>40`; app.js doUndo,
 * model.js songDiff / diffText: before > after). */
#define U_SIZE (sizeof S + sizeof ins)
#define U_LOG 4096u                             /* bytes; the oldest steps drop out (a 16-phrase deep clone is ~2.3 KB: 32 clear the history; user, 2026-10-12) */
#define U_STEPS 4u                              /* (user, 2026-10-10) */
#define U_FLASH 64u                             /* (more changed cells than this: the first ones flash) */
typedef char u_offsets_fit[U_SIZE < 65536u ? 1 : -1];
static uint8_t u_shadow[U_SIZE] __attribute__((section(".pool")));   /* (RAM is short: the pool) */
static uint8_t u_log[U_LOG] __attribute__((section(".pool")));
static uint16_t u_end[U_STEPS];                 /* where step k's runs end in u_log */
static uint32_t u_n, u_done, u_group, u_t;      /* steps stored, steps applied (the rest can be redone) */
static struct { uint8_t view, at, r, c; } u_fl[U_FLASH];
static uint32_t u_fl_n, u_fl_t;

static uint8_t *u_byte(uint32_t o) { return o < sizeof S ? (uint8_t *)&S + o : (uint8_t *)ins + (o - sizeof S); }
static void undo_reset(void) { memcpy(u_shadow, &S, sizeof S); memcpy(u_shadow + sizeof S, ins, sizeof ins); u_n = u_done = 0; }
#define U_XF ((uint32_t)((uint8_t *)&S.scn.xf - (uint8_t *)&S))   /* the crossfader: played (SCN), not undone, as the tempo */
/* the changes since the last step as runs; dst NULL: only their size. Writing moves the shadow on */
static uint32_t u_encode(uint8_t *dst)
{
    uint32_t o, n = 0, h = 0, at = 0, last = 0;  /* the open run: its head h, its first byte at, its last change */
    for (o = 1; o < U_SIZE; o++) {              /* (byte 0: the tempo, not undone) */
        uint8_t b = *u_byte(o);
        if (b == u_shadow[o] || o == U_XF) continue;
        if (!last || o - last > 4u || o - at >= 255u) {   /* a new run */
            if (dst && last) dst[h + 2u] = (uint8_t)(last - at + 1u);
            h = n; at = o; n += 3u;
            if (dst) { dst[h] = (uint8_t)o; dst[h + 1u] = (uint8_t)(o >> 8); }
        } else {                                /* the gap since the last change goes in as 00 */
            if (dst) memset(dst + n, 0, o - last - 1u);
            n += o - last - 1u;
        }
        if (dst) { dst[n] = b ^ u_shadow[o]; u_shadow[o] = b; }
        n++; last = o;
    }
    if (dst && last) dst[h + 2u] = (uint8_t)(last - at + 1u);
    return n;
}
static int undo_close(void)                     /* the changes since the last step become a step; 1: too big to undo */
{
    uint32_t n, at;
    u_shadow[U_XF] = S.scn.xf;                  /* (kept current for saving) */
    n = u_encode(0);
    if (!n) return 0;
    proj_dirty = 1;
    u_n = u_done;                               /* a new edit: nothing to redo any more */
    if (n > U_LOG) { undo_reset(); say("too big to undo: history cleared"); return 1; }
    while (u_n && (u_n == U_STEPS || U_LOG - u_end[u_n - 1u] < n)) {   /* the oldest step drops out */
        uint32_t k, d = u_end[0];
        mem_move(u_log, u_log + d, u_end[u_n - 1u] - d);
        for (k = 1; k < u_n; k++) u_end[k - 1u] = (uint16_t)(u_end[k] - d);
        u_n--;
    }
    at = u_n ? u_end[u_n - 1u] : 0u;
    u_end[u_n++] = (uint16_t)(at + u_encode(u_log + at));
    u_done = u_n;
    return 0;
}
#define U_TAKE 0x7FFFFFFFu                      /* a recording take: one step however long */
static void undo_mark(uint32_t group, int held)  /* an input: close the step, unless it goes on (same group, 1.5 s;
                                                 * held: a block change, as long as EDIT stays held, picks review #23) */
{
    if (group == U_TAKE && u_group == U_TAKE) return;
    if (group && group == u_group && (held || fm1_ms - u_t < 1500u)) { u_t = fm1_ms; return; }
    undo_close();
    u_group = group;
    u_t = fm1_ms;
}

/* a changed byte -> the change it belongs to: kind, the bytes of that value (base, size), the screen cell */
enum { UK_SONG, UK_CHAIN, UK_PHRASE, UK_MIX, UK_INS, UK_PROJ, UK_KNOB, UK_FX, UK_SCN };
typedef struct { uint32_t kind, at, r, c, base, size; } uchg_t;
static void u_what(uint32_t o, uchg_t *w)
{
    uint32_t song = (uint32_t)((uint8_t *)S.song - (uint8_t *)&S), chains = (uint32_t)((uint8_t *)S.chains - (uint8_t *)&S);
    uint32_t phrases = (uint32_t)((uint8_t *)S.phrases - (uint8_t *)&S), mix = (uint32_t)((uint8_t *)S.mute - (uint8_t *)&S);
    uint32_t pofs = (uint32_t)((uint8_t *)ins[0].p - (uint8_t *)&ins[0]), x;
    uint32_t pj = (uint32_t)((uint8_t *)&S.pj - (uint8_t *)&S), kn = (uint32_t)((uint8_t *)S.knob - (uint8_t *)&S);
    uint32_t lv = (uint32_t)((uint8_t *)S.lvl - (uint8_t *)&S), fxo = (uint32_t)((uint8_t *)S.fx - (uint8_t *)&S);
    uint32_t sco = (uint32_t)((uint8_t *)&S.scn - (uint8_t *)&S);
    w->at = w->r = w->c = 0; w->base = o; w->size = 1;
    if (o >= sizeof S) {                        /* an instrument: a parameter (2 bytes), or its engine / preset / name */
        x = o - sizeof S;
        w->kind = UK_INS; w->at = x / sizeof(inst_t); x %= sizeof(inst_t);
        w->r = x >= pofs ? (x - pofs) / 2u : P_COUNT;
        w->base = sizeof S + w->at * sizeof(inst_t) + (x >= pofs ? pofs + 2u * w->r : 0u);
        w->size = x >= pofs ? 2u : pofs;
    } else if (o >= (uint32_t)((uint8_t *)&S.mute_mode - (uint8_t *)&S)) w->kind = UK_PROJ;   /* (G's MUTE) */
    else if (o >= sco) { w->kind = UK_SCN; w->base = sco; w->size = sizeof S.scn; }
    else if (o >= fxo) { w->kind = UK_FX; w->base = fxo; w->size = sizeof S.fx; }
    else if (o >= lv) { w->kind = UK_MIX; w->base = lv; w->size = NCH; }
    else if (o >= kn) { w->kind = UK_KNOB; w->at = (o - kn) / 4u; w->r = (o - kn) % 4u; }
    else if (o >= pj) { w->kind = UK_PROJ; w->base = pj; w->size = sizeof S.pj; }
    else if (o >= mix) { w->kind = UK_MIX; w->base = mix; w->size = 2u * NCH; }
    else if (o >= phrases) {                    /* a row's bytes are its 9 columns; an FX is command + value */
        x = o - phrases;
        w->kind = UK_PHRASE; w->at = x / sizeof S.phrases[0]; w->r = x % sizeof S.phrases[0] / sizeof(row_t); w->c = x % sizeof(row_t);
        if (w->c >= 3u) { w->base = o - (w->c - 3u) % 2u; w->size = 2; }
    } else if (o >= chains) { x = o - chains; w->kind = UK_CHAIN; w->at = x / sizeof S.chains[0]; w->r = x % sizeof S.chains[0] / 2u; w->c = x % 2u; }
    else { x = o - song; w->kind = UK_SONG; w->r = x / NCH; w->c = x % NCH; }
}
/* the change's text from its bytes before and after (model.js songDiff) */
static void u_text(const uchg_t *w, const uint8_t *a, const uint8_t *b, char *s)
{
    char t[16];
    uint32_t j;
    s[0] = 0;
    if (w->kind == UK_MIX) { sa(s, "MIXER"); return; }
    if (w->kind == UK_PROJ) { sa(s, "PROJECT"); return; }
    if (w->kind == UK_FX) { sa(s, "EFFECTS"); return; }
    if (w->kind == UK_SCN) { sa(s, "SCENES"); return; }
    if (w->kind == UK_KNOB) {                   /* `I3 KNOB 2 CUT>RES` */
        s[0] = 'I'; s[1] = HEX[w->at & 15u]; s[2] = 0;
        sa(s, " " KNOB_NAME); sn(s, KNOB_NO(w->r)); sa(s, " ");
        cmd_name(CMD_SOUND | slot_of(w->at, w->r, a[0]), w->at, t); sa(s, t); sa(s, ">");
        cmd_name(CMD_SOUND | slot_of(w->at, w->r, b[0]), w->at, t); sa(s, t);
        return;
    }
    if (w->kind == UK_INS) {
        s[0] = 'I'; s[1] = HEX[w->at & 15u]; s[2] = ' '; s[3] = 0;
        if (w->r == P_COUNT) {                  /* inst_t: eng, preset, renamed, name */
            if (a[0] != b[0]) { sa(s, "ENGINE "); sa(s, ENGINES[a[0]]->name); sa(s, ">"); sa(s, ENGINES[b[0]]->name); }
            else sa(s, a[1] != b[1] ? "PRESET" : "NAME");
        } else {
            const param_desc_t *d = ins_desc(w->at, w->r);
            const char *unit;
            int16_t va, vb;
            memcpy(&va, a, 2); memcpy(&vb, b, 2);
            cmd_name(CMD_SOUND | w->r, w->at, t); sa(s, t); sa(s, " ");
            if (!d) { sn(s, va); sa(s, ">"); sn(s, vb); return; }
            param_format(d, va, t, &unit); sa(s, t); sa(s, unit); sa(s, ">");
            param_format(d, vb, t, &unit); sa(s, t); sa(s, unit);
        }
        return;
    }
    if (w->kind == UK_SONG) { s[0] = 'S'; fmt_hex(s + 1, w->r, 2); sa(s, " CH"); sn(s, (int32_t)w->c + 1); sa(s, " "); }
    else {
        s[0] = w->kind == UK_CHAIN ? 'C' : 'P'; fmt_hex(s + 1, w->at, 2);
        sa(s, w->kind == UK_CHAIN ? ":" : " ");
        t[0] = HEX[w->r & 15u]; t[1] = ' '; t[2] = 0; sa(s, t);
    }
    for (j = 0; j < 2u; j++) {                  /* before, then after */
        const uint8_t *v = j ? b : a;
        if (j) sa(s, ">");
        if (w->kind == UK_SONG || (w->kind == UK_CHAIN && w->c == 0u) || (w->kind == UK_PHRASE && w->c == 1u)) {
            if (!j && w->kind == UK_PHRASE) sa(s, "V ");
            fmt_hex2e(t, v[0]);
        } else if (w->kind == UK_CHAIN) { if (!j) sa(s, "TRN "); fmt_int(t, (int8_t)v[0]); }
        else if (w->c == 0u) fmt_note(t, v[0]);
        else if (w->c == 2u) { if (!j) sa(s, "I "); t[0] = v[0] == EMPTY ? '-' : HEX[v[0] & 15u]; t[1] = 0; }
        else {                                  /* an FX: named by the row's instrument (after) */
            int32_t r;
            uint32_t i = last.inst;
            for (r = (int32_t)w->r; r >= 0; r--) if (S.phrases[w->at][r].i != EMPTY) { i = S.phrases[w->at][r].i; break; }
            if (!j) { sa(s, "FX"); sn(s, (int32_t)(w->c - 3u) / 2 + 1); sa(s, " "); }
            if (!v[0]) str_cpy(t, "---", 4);
            else { cmd_name(v[0], i, t); sa(s, t); sa(s, " "); fmt_hex(t, v[1], 2); }
        }
        sa(s, t);
    }
}
static void undo_step(int32_t dir)              /* -1 undo, +1 redo */
{
    uint32_t k, j, i, len, a, e, nch = 0, last_base = 0xFFFFFFFFu;
    uint8_t ba[16], bb[16], xr[16] = {0};
    uchg_t w, first = {0, 0, 0, 0, 0, 0};
    char s[40], t[40];
    undo_close();
    u_group = 0;
    if (dir < 0 ? !u_done : u_done == u_n) { say(dir < 0 ? "nothing to undo" : "nothing to redo"); return; }
    k = dir < 0 ? u_done - 1u : u_done;
    a = k ? u_end[k - 1u] : 0u; e = u_end[k];
    u_fl_n = 0;
    for (j = a; j < e; j += 3u + len)
    for (len = u_log[j + 2u], i = 0; i < len; i++) {
        uint32_t o = (u_log[j] | (uint32_t)u_log[j + 1u] << 8) + i;
        uint8_t x = u_log[j + 3u + i];
        if (!x) continue;                       /* (a gap in the run) */
        *u_byte(o) ^= x; u_shadow[o] ^= x;
        if (o >= sizeof S) ins_gen[(o - sizeof S) / sizeof(inst_t)]++;   /* the parts reload it */
        u_what(o, &w);
        if (w.kind == UK_SCN) scn_changed();
        if (w.kind <= UK_PHRASE && u_fl_n < U_FLASH) {
            u_fl[u_fl_n].view = (uint8_t)w.kind; u_fl[u_fl_n].at = (uint8_t)w.at;
            u_fl[u_fl_n].r = (uint8_t)w.r; u_fl[u_fl_n].c = (uint8_t)w.c; u_fl_n++;
        }
        if (w.base != last_base) { if (!nch++) first = w; last_base = w.base; }
        if (nch == 1u && o - first.base < sizeof xr) xr[o - first.base] = x;
    }
    u_done = dir < 0 ? k : k + 1u;
    proj_dirty = 1;
    u_fl_t = fm1_ms;
    for (j = 0; j < first.size && j < sizeof bb; j++) {   /* the first change: after, before (scenes, project: more
                                                 * bytes than these, u_text reads none of them) */
        bb[j] = *u_byte(first.base + j);
        ba[j] = bb[j] ^ xr[j];
    }
    u_text(&first, ba, bb, t);
    str_cpy(s, dir < 0 ? "undo " : "redo ", sizeof s);
    if (nch == 1u) { t[33] = 0; sa(s, t); }
    else { for (j = 0; t[j] && t[j] != ' '; j++) {} t[j] = 0; sn(s, (int32_t)nch); sa(s, " changes "); sa(s, t); }
    say(s);
}
static int flashed(uint32_t r, uint32_t c)      /* a cell undo / redo just changed, on this screen */
{
    uint32_t k, at = ui.view == V_CHAIN ? cur_chain() : ui.view == V_PHRASE ? cur_phrase() : 0u;
    for (k = 0; k < u_fl_n; k++)
        if (u_fl[k].view == ui.view && u_fl[k].at == at && u_fl[k].r == r && u_fl[k].c == c) return 1;
    return 0;
}

/* ---------------------------------------------------------------- the gamepad --- */
#define DIRTY_ALL 0xFFFFFFFFu                   /* bits 0..15: lines / page rows; 16: bar; 17: column labels */
#define DIRTY_BAR (1u << 16)
#define DIRTY_LABELS (1u << 17)
#define DIRTY_GRAPH (1u << 18)                   /* L: the moving waveform; Q: the timeline (the playhead) */

static void draw_proj(void);                    /* (the G page, below) */
static void draw_mix(uint32_t dirty);           /* (X, below) */
static void draw_fx(void);                      /* (F, below) */
static void draw_q(int top);                    /* (Q, below) */
static void open_q(uint32_t i);
static void redraw_view(uint32_t dirty);
static uint32_t q_from(void);
static void redraw(uint32_t dirty)
{
    redraw_view(dirty);
    if (S.scn.xf) lcd_fill((uint32_t)fader_x(), TOP, 3, 1, P_TEXT);   /* (the fader mark's row under the bar's line) */
}
static void ov_moved(void (*draw)(void))        /* an overlay drawn (it sets the clip); moved or resized: the rows it
                                                 * left come back */
{
    int32_t x0 = ov_clip.x0, y0 = ov_clip.y0, x1 = ov_clip.x1, y1 = ov_clip.y1;
    draw();
    if (y1 > y0 && (x0 != ov_clip.x0 || y0 != ov_clip.y0 || x1 != ov_clip.x1 || y1 != ov_clip.y1)) redraw(0xFFFFu | DIRTY_LABELS);
}
static uint32_t rows_old;                       /* lines the playhead or the follow cursor just left: they go out first, so
                                                 * at a wrap (15 -> 0) the panel never shows both lit (unit feedback #45) */
static void redraw_view(uint32_t dirty)
{
    uint32_t r, first = rows_old;
    rows_old = 0;
    if (dirty & DIRTY_BAR) draw_bar();
    if (strip_map != (ui.view == V_SONG) || (ui.view == V_SONG && dirty >> 15 & 1u)) msg_dirty = 1;   /* the song map's end */
    if (ui.view == V_PROJ) { if (dirty & ~DIRTY_BAR) draw_proj(); return; }
    if (ui.view == V_MIX) { draw_mix(dirty); return; }
    if (ui.view == V_FX) { if (dirty & ~DIRTY_BAR) draw_fx(); return; }
    if (ui.view == V_Q) { if (dirty & ~(DIRTY_BAR | DIRTY_GRAPH)) draw_q(0); else if (dirty & DIRTY_GRAPH) draw_q(1); return; }
    if (IS_PAGE(ui.view)) {
        if (dirty & DIRTY_LABELS) { cv_begin(240, LH, P_BG); cv_map(TOP, LH); cv_blit(0, TOP); }   /* (the panel's top) */
        if (ui.view == V_INST) { for (r = 0; r < 5u; r++) if (dirty >> r & 1u) draw_bands(IROW_Y(r), IROW_H, draw_band); }
        else if (dirty & 0xFFFFu) draw_bands(ROWS_Y, STRIP_Y - ROWS_Y, draw_band);
        else if (dirty & DIRTY_GRAPH) draw_bands(ROWS_Y, GY + GH + 4 - ROWS_Y, draw_band);
        return;
    }
    if (dirty & DIRTY_LABELS) draw_line(-1);
    for (r = 0; r < 16u; r++) if ((dirty & first) >> r & 1u) draw_line((int32_t)r);
    for (r = 0; r < 16u; r++) if ((dirty & ~first) >> r & 1u) draw_line((int32_t)r);
}

/* ---------------------------------------------------------------- the knob strip (docs/ux/knob-slots.md) --- */
/* Away from the instrument pages KNOB 1-4 turn the focus instrument's knob slots (app.js turnKnobOn); the strip shows
 * for 1.5 s after the last turn (app.js showKnobs, render.js knobStrip): the instrument, then its 4 knobs as on the
 * KNOBS row, the turned one's name in the accent and its value white. It sits away from the cursor: at the top when
 * the cursor is in the lower half (render.js stripTop) */
static struct { uint8_t on, hot, inst, drawn, scn, rec; const char *why; uint32_t until, gen, t; } ks;   /* scn: the turn went into
                                                 * scene scn - 1; rec: recorded (why: why not) */
static uint32_t kstrip_top(void)                /* the knob or scene strip's first line: 0 or 12 (render.js stripTop) */
{
    uint32_t low = ui.view == V_PHRASE ? ui.row : ui.view == V_CHAIN ? ui.chain_row : ui.view == V_SONG ? ui.song_row % 16u
                 : ui.view == V_INST ? IF[ui.field].row * 3u : ui.view == V_VOX ? (IF[ui.field].row > 0u) * 12u : ui.view == V_PROJ ? 0u : 12u;
    return low >= 8u ? 0u : 12u;
}
static void draw_kstrip(void)
{
    int32_t top = (int32_t)kstrip_top(), y0 = ROWS_Y + top * LH - 2, w = MAP_X - 6, h = 3 * LH + 6;
    uint32_t k;
    char t[40];
    clip_to(0, y0, w, h);
    cv_begin((uint32_t)w, (uint32_t)h, P_SURF);   /* a line frame: it never takes the cursor (principles-review.md #15) */
    cv_frame(0, 0, w, h, P_LINE);
    t[0] = 0;
    if (ks.rec) sa(t, ks.why ? ks.why : "REC ");   /* knob recording: in the REC red (knob-record.md 6) */
    else if (ks.scn) { sa(t, "SCENE "); sn(t, ks.scn); sa(t, S.scn.xf * 2u > XF_MAX ? " (B) " : " (A) "); }   /* (app.js turnKnobOn) */
    if (!ks.rec || !ks.why) { sa(t, "I"); t[str_len(t) + 1] = 0; t[str_len(t)] = HEX[ks.inst & 15u]; sa(t, " "); sa(t, ins[ks.inst].name); }
    t[33] = 0;
    cv_str(gx(0), 2, t, ks.rec ? P_PLAY : P_CTX);
    live_snapshot(ks.inst);                     /* (what plays, as on the KNOBS row) */
    kn_strip = 1;
    for (k = 0; k < 4u; k++) draw_knob(gx((int32_t)k * 8), LH + 4, ks.inst, slot_key(ks.inst, k), k == ks.hot ? KN_HOT : KN_PLAIN, P_CH[k]);
    kn_strip = 0;
    cv_blit(0, (uint32_t)y0);
    ks.drawn = 1;
}

/* ---------------------------------------------------------------- the scene strip (docs/ux/scenes.md 6) --- */
/* Turning the crossfader or picking a scene shows it for 1.5 s (render.js sceneStrip): the scenes on A and B with the
 * fader between them (the near side's letter white, the far one grey: principles-review #16), then one line per value
 * either holds: A's under A, the name, B's under B (dim: that side does not hold it, the song's own counts). Up to 7
 * lines it takes the half away from the cursor, past that the whole list. Up / down give it the focus (app.js
 * sceneStripKey): it stays, a line cursor walks it, the screen's cursor dims (blit_rows); EDIT goes to that value's page,
 * OPTION closes it, other keys close it and act. A line frame while passing, the accent once it has the focus (#15) */
static uint32_t scene_keys(sc_t *out)           /* what the scenes on A and B hold, A's first (model.js sceneKeys) */
{
    uint32_t n = 0, s, k;
    for (s = 0; s < 2u; s++) {
        const sc_t *e = S.scn.k[s ? S.scn.b : S.scn.a];
        for (k = 0; k < SCENE_MAX && e[k].t != EMPTY; k++)
            if (!s || sc_find(S.scn.a, e[k].t, e[k].p) < 0) out[n++] = e[k];
    }
    return n;
}
static void sc_name(uint32_t t, uint32_t p, char *s)   /* I2 CUT, CH4, F MIX (render.js sceneKeyName) */
{
    static const char FXL[9][5] = {"TIME", "FDBK", "COLR", "MIX", "TYPE", "SIZE", "DAMP", "RATE", "DPTH"};   /* felucca.js GLOBAL */
    if (t == SC_MIX) { str_cpy(s, "CH", 40); sn(s, (int32_t)p + 1); }
    else if (t == SC_FX) { str_cpy(s, "F ", 40); sa(s, FXL[p]); }
    else { s[0] = 'I'; s[1] = HEX[t & 15u]; s[2] = ' '; cmd_name(CMD_SOUND | p, t, s + 3); }
}
static void sc_text(uint32_t sn, uint32_t t, uint32_t p, char *s)   /* scene sn's value, else the song's (render.js sceneText) */
{
    const param_desc_t *d = t == SC_MIX ? &TP[P_LEVEL] : t == SC_FX ? &GP[FXF[p].g] : ins_desc(t, p);
    int32_t k = sc_find(sn, t, p), v = k >= 0 ? S.scn.k[sn][k].v : t == SC_MIX ? S.lvl[p] : t == SC_FX ? S.fx[p] : ins[t].p[p];
    param_text(d, v, s);
    s[10] = 0;
}
static int32_t sstrip_top(uint32_t n) { return n > 7u ? -1 : kstrip_top() == 0u ? 0 : 14 - (int32_t)(n ? n : 1u); }
static void draw_sstrip(void)
{
    sc_t key[2u * SCENE_MAX];
    uint32_t n = scene_keys(key), nl = n ? n : 1u, nb = S.scn.xf * 2u > XF_MAX, i;
    int32_t top = sstrip_top(n), w = MAP_X - 6, y0 = ROWS_Y + top * LH - (nl > 7u ? 1 : 2), h = ((int32_t)nl + 1) * LH + (nl > 7u ? 2 : 4);
    int32_t yt = ROWS_Y + top * LH, band, bh = (int32_t)(CV_MAX / (uint32_t)w), pos = rdiv((int32_t)S.scn.xf * 20, (int32_t)XF_MAX);
    uint16_t fc = ss.stuck ? P_ACCENT : P_LINE;
    char s[40];
    clip_to(0, y0, w, h);
    for (band = y0; band < y0 + h; band += bh) {
        cv_begin((uint32_t)w, (uint32_t)(y0 + h - band < bh ? y0 + h - band : bh), P_SURF);
        cv_oy = -band;
        cv_frame(0, y0, w, h, fc);
        cv_str(gx(0), yt, "A", nb ? P_LABEL : P_WHITE);   /* the top line: A SC2 ------o------ SC6 B */
        s[0] = 'S'; s[1] = 'C'; s[2] = (char)('1' + S.scn.a); s[3] = 0;
        cv_str(gx(2), yt, s, P_CTX);
        s[2] = (char)('1' + S.scn.b);
        cv_str(gx(28), yt, s, P_CTX);
        cv_str(gx(32), yt, "B", nb ? P_WHITE : P_LABEL);
        for (i = 0; i < 21u; i++) cv_str(gx(6 + (int32_t)i), yt, (int32_t)i == pos ? "o" : "-", (int32_t)i == pos ? P_WHITE : P_DIM);
        if (!n) cv_str(gx(0), yt + LH, "NOTHING IN THESE SCENES", P_TEXT);
        for (i = 0; i < n; i++) {
            int32_t y = yt + (int32_t)(i + 1u) * LH, nx;
            int on = ss.stuck && i == (ss.line < n ? ss.line : n - 1u);
            if (on) cv_rect(0, y, w, LH, P_BAND);
            sc_text(S.scn.a, key[i].t, key[i].p, s);
            cv_str(gx(0), y, s, sc_find(S.scn.a, key[i].t, key[i].p) >= 0 ? P_TEXT : P_DIM);
            sc_text(S.scn.b, key[i].t, key[i].p, s);
            cv_str(gx(33 - (int32_t)str_len(s)), y, s, sc_find(S.scn.b, key[i].t, key[i].p) >= 0 ? P_TEXT : P_DIM);
            sc_name(key[i].t, key[i].p, s);
            nx = (33 - (int32_t)str_len(s)) / 2;
            if (on) { cv_rect(gx(nx) - 1, y, (int32_t)str_len(s) * CW + 2, LH, P_ACCENT); cv_str(gx(nx), y, s, P_BG); }
            else cv_str(gx(nx), y, s, P_LABEL);
        }
        cv_oy = 0;
        cv_blit(0, (uint32_t)band);
    }
    ss.drawn = 1;
}
static void set_view(uint32_t v);
static void rows_clear(void)                    /* a new screen: no clear first, every screen draws all of its rows over
                                                 * the last one (a full clear flickered on the device, unit-feedback.md
                                                 * #2); only I's five 38 px rows leave 2 px above the strip */
{
    lcd_fill(0, (uint32_t)IROW_Y(5), 240, (uint32_t)(STRIP_Y - IROW_Y(5)), P_BG);
}
static void screen_change(void)                 /* a new screen: off the row numbers and labels, no selection or mark */
{
    ui.on_row = ui.on_head = 0;
    sel.on = mark.on = 0;
    rows_clear();
}
static void scene_jump(uint32_t t, uint32_t p)  /* a scene value's own page, the cursor on it (app.js sceneJump) */
{
    uint32_t v, f;
    if (ui.view <= V_PHRASE) ui.back = ui.view;
    screen_change();
    if (t == SC_MIX) { set_view(V_MIX); ui.ch = p; ui.mf = 0; return; }
    if (t == SC_FX) { set_view(V_FX); ui.ff = p; return; }
    ui.inst = t;
    for (v = V_INST; v <= V_VOX; v++)
        for (f = 0; f < IPAGES[v - V_INST].n; f++)
            if (IPAGES[v - V_INST].f[f].key == p) { set_view(v); ui.field = f; return; }
}

/* a screen: each instrument page keeps its own cursor field (app.js instField / envField / lfoField / voxField) */
static void set_view(uint32_t v)
{
    if (IS_PAGE(ui.view)) ui.pf[ui.view - V_INST] = ui.field;
    ui.view = v;
    if (IS_PAGE(v)) { ui.field = ui.pf[v - V_INST]; ui.char_pos = 0; if (!field_usable(ui.field)) ui.field = 0; }
    if (v == V_PROJ) ui.char_pos = 0;
}
/* SHIFT + d-pad walks the map (app.js mapMove): left / right along S C P I; from S / C / P up = G, down = F, then
 * X; G F X: up / down along their column, G down and F up = back to the anchor, left / right = on along the main
 * line from it; I: up = Q, down = E L V; Q E L V: left = P, right = I, up / down along that column. Nothing at
 * the ends */
static void go_right(uint32_t v)                /* S -> C -> P -> I, creating the chain / phrase if the cell is empty */
{
    if (v == V_SONG) { if (ensure_chain() != EMPTY) ui.view = V_CHAIN; }
    else if (v == V_CHAIN) { if (ensure_phrase() != EMPTY) ui.view = V_PHRASE; }
    else { ui.inst = focus_inst(); set_view(V_INST); }
}
static uint32_t go_screen(int32_t dx, int32_t dy)
{
    uint32_t v = ui.view;
    if (v == V_Q) {                             /* Q: above I */
        if (dy < 0) return 0;
        set_view(dx < 0 ? V_PHRASE : V_INST);
    } else if (v == V_PROJ || v == V_FX || v == V_MIX) {
        uint32_t a = map_anchor();
        if ((v == V_PROJ && dy < 0) || (v == V_MIX && dy > 0) || (dx < 0 && a == V_SONG)) return 0;
        if (dy) set_view(v == V_MIX ? V_FX : v == V_FX && dy > 0 ? V_MIX : a);
        else { set_view(a); if (dx < 0) set_view(a - 1u); else go_right(a); }
    } else if (dx > 0) {
        if (v == V_INST) return 0;
        if (IS_PAGE(v)) set_view(V_INST); else go_right(v);
    } else if (dx < 0) {
        if (v == V_SONG) return 0;
        set_view(IS_PAGE(v) ? V_PHRASE : v - 1u);
    } else if (!IS_PAGE(v)) {
        ui.back = v;
        set_view(dy > 0 ? V_FX : V_PROJ);
    } else {
        if (dy < 0 && v == V_INST) open_q(ui.inst);
        else if (dy > 0 && v == V_VOX) return 0;
        else set_view(v + (uint32_t)dy);
    }
    if (ui.view == v) return 0;
    screen_change();
    return DIRTY_ALL;
}

/* OPTION + d-pad on the chain / phrase screens (app.js optionMove): never lands on an empty chain / phrase */
static void option_move(int32_t dx, int32_t dy)
{
    if (dx) {
        int32_t c = (int32_t)ui.ch + dx;
        ui.ch = (uint32_t)clamp_i(c, 0, (int32_t)NCH - 1);
    } else if (ui.view == V_CHAIN) {
        int32_t r = (int32_t)ui.song_row + dy;
        if (r >= 0 && r < (int32_t)NSONG && S.song[r][ui.ch] != EMPTY) ui.song_row = (uint32_t)r;
    } else {
        int32_t r = (int32_t)ui.chain_row + dy;
        uint32_t cn = cur_chain();
        if (cn != EMPTY && r >= 0 && r < 16 && S.chains[cn][r].ph != EMPTY) ui.chain_row = (uint32_t)r;
    }
}

/* PLAY: what the screen shows (app.js play); all = SHIFT + PLAY: the song from the song row */
static void rec_stop(void)                      /* the take ends: what it wrote (app.js recStopped, picks review #16) */
{
    char t[40];
    uint32_t k, b, fx = 0;
    for (k = 0; k < sizeof rc; k++) for (b = rc[k]; b; b &= b - 1u) fx++;   /* (its knob / SCN cells, still red) */
    kb = KB_PAD; rec_on = 0; rec_spare = EMPTY;
    str_cpy(t, "RECORDING STOPPED: ", sizeof t);
    if (!rec_count && !fx) sa(t, "NOTHING WRITTEN");
    else { sn(t, (int32_t)rec_count); sa(t, " NOTES "); sn(t, (int32_t)fx); sa(t, " FX"); }
    say(t);
}
static void rec_start(void);
static uint32_t count_in;                       /* rec_start -> play: beats of count-in */
/* PLAY on a selection (decisions.md "PLAY on a selection", app.js selLoop): its rows looped on the cursor's track (S:
 * the selected tracks); all (SHIFT): the same place on all four, a gap filled from the nearest phrase above. Sets sel_*;
 * 0 = no selection here, or nothing to play */
static int sel_setup(int all)
{
    bounds_t b;
    uint32_t c, sr, cr, any = 0, d;
    int32_t tr;
    if ((ui.view != V_SONG && ui.view != V_CHAIN && ui.view != V_PHRASE) || !block(&b)) return 0;
    sel_lv = (uint8_t)(ui.view == V_SONG ? 2u : ui.view == V_CHAIN ? 1u : 0u);
    sel_s0 = (uint8_t)(sel_lv == 2u ? b.r0 : ui.song_row); sel_s1 = (uint8_t)(sel_lv == 2u ? b.r1 : sel_s0);
    sel_c0 = (uint8_t)(sel_lv == 1u ? b.r0 : sel_lv ? 0u : ui.chain_row); sel_c1 = (uint8_t)(sel_lv == 1u ? b.r1 : sel_c0);
    sel_r0 = (uint8_t)(sel_lv ? 0u : b.r0); sel_r1 = (uint8_t)(sel_lv ? 15u : b.r1);
    if (sel_lv == 2u) { sel_on = (uint8_t)(all ? 15u : (2u << b.c1) - (1u << b.c0)); sel_fill = (uint8_t)(all ? 15u : 0u); }
    else { sel_on = (uint8_t)(all ? 15u : 1u << ui.ch); sel_fill = (uint8_t)(all ? 15u & ~(1u << ui.ch) : 0u); }
    for (c = 0; c < NCH; c++) { sr = sel_s0; cr = sel_c0; if (sel_on >> c & 1u && sel_walk(c, &sr, &cr, &tr)) any = 1; }
    d = sel_lv == 2u ? 2u : 1u;                 /* LOOP 4-7 / LOOP 03-05: the rows as the screen numbers them */
    str_cpy(sel_tag, "LOOP ", sizeof sel_tag);
    fmt_hex(sel_tag + 5, b.r0, d); sel_tag[5 + d] = '-'; fmt_hex(sel_tag + 6 + d, b.r1, d);
    return (int)any;
}
static void play(int all)
{
    int sel;
    if (kb == KB_ARMED && !all) { rec_start(); return; }
    play_count = (uint8_t)count_in;             /* (set before play_req: the ISR reads it at the start) */
    count_in = 0;
    sel = kb != KB_REC && !(playing && mode == M_SEL && !all) && sel_setup(all);   /* (PLAY on a loop stops it) */
    if (sel) { play_mode = M_SEL; play_req = 1; return; }   /* playing already: at once */
    if (playing) {
        if (kb == KB_REC) rec_stop();           /* stopping ends the take */
        play_req = 2;
        if (!all) return;
        {   /* the ISR takes the stop at its next block; a stalled audio (P1): stopped here after 50 ms */
            uint32_t t0 = fm1_ms;
            while (play_req && fm1_ms - t0 < 50u) platform_poll();
            if (play_req) player_stop();
        }
    }
    play_ch = (uint8_t)ui.ch;
    play_from_song = (uint8_t)ui.song_row;
    if (!all && ui.view == V_CHAIN) {
        if (cur_chain() == EMPTY) { say("empty chain"); return; }
        play_mode = M_CHAIN;
        play_chain = (uint8_t)cur_chain();
        play_from_chain = (uint8_t)ui.chain_row;
    } else if (!all && (ui.view == V_PHRASE || IS_PAGE(ui.view))) {
        if (cur_phrase() == EMPTY) { say("empty phrase"); return; }
        play_mode = M_LOOP;
        play_phrase = (uint8_t)cur_phrase();
    } else {
        play_mode = M_SONG;                     /* Q: the song from the song row where the cursor's line plays first */
        if (!all && ui.view == V_Q) play_from_song = (uint8_t)q_from();
    }
    play_req = 1;
}

/* PLAY while armed (app.js startRecording): from a chain or a phrase (or its instrument pages); playing already:
 * from the next phrase start, else PLAY with a bar of count-in. One take = one undo step (undo_mark U_TAKE) */
static void rec_start(void)
{
    if (ui.view != V_CHAIN && ui.view != V_PHRASE && !IS_PAGE(ui.view)) { say("record from a chain or a phrase"); return; }
    rec_count = 0;
    if (playing) { kb = KB_REC; rec_on = 1; rec_wait = 1; say("recording from the next phrase"); return; }
    kb = KB_PAD;
    count_in = 4u * S.pj.count;                 /* COUNT: bars of 4 beats (G page) */
    play(0);                                    /* what the screen shows, as PLAY */
    if (!play_req) return;
    kb = KB_REC; rec_on = 1; rec_wait = 0;
    say(S.pj.count ? "count-in, then recording" : "recording");
}
/* a note key while recording (app.js liveWrite): on the cursor's channel's row now; QNT FEEL: the row it falls in,
 * its timing kept as DEL = the ticks past its start (up to 5); QNT ROW: the nearest row (a row written ahead is not
 * struck on this pass: rec_skip). The row's instrument the last one used; heard at once (the key), played from the
 * next pass */
static uint32_t rec_next_phrase(uint32_t pos)    /* the phrase ui.ch plays after the one at pos (channel_advance) */
{
    uint32_t sr = POS_SROW(pos), cr = POS_CROW(pos) + 1u, cn = mode == M_CHAIN ? loop_chain : S.song[sr][ui.ch];
    int32_t tr;
    const row_t *rows;
    if (mode == M_LOOP) return loop_next != EMPTY ? loop_next : POS_PH(pos);
    if (mode == M_CHAIN && cr < 16u && S.chains[cn][cr].ph == EMPTY && rec_spare != EMPTY)
        S.chains[cn][cr].ph = rec_spare;        /* (the ISR would put it there: taken now, the main loop picks another) */
    if (cr >= 16u) { cr = 0; if (mode == M_SONG) sr = (sr + 1u) % NSONG; }
    rows = walk(ui.ch, &sr, &cr, &tr);
    return rows ? phrase_num(rows) : POS_PH(pos);
}
static void rec_write(uint32_t note)
{
    uint32_t pos, k, row, ph;
    int32_t q, del;
    row_t *w;
    do { pos = ch_pos[ui.ch]; q = row_q8; } while (pos != ch_pos[ui.ch]);   /* (one block's pair: R1) */
    row = POS_ROW(pos); ph = POS_PH(pos); del = (q + 128) >> 8;
    if (!playing || count_left || rec_wait || !POS_ON(pos)) return;
    if (!S.pj.qnt) {
        del = 0;
        if (q > (int32_t)(TICKS * 128u)) {     /* late on row 15: row 0 of the phrase played next (QR1, my pick, to
                                                 * review; shortcut: a HOP on row 15 is not followed) */
            if (++row == 16u) { row = 0; ph = rec_next_phrase(pos); }
            rec_skip = (uint16_t)(0x8000u | ph << 4 | row);
        }
    }
    w = &S.phrases[ph][row];
#if BOARD_SMK37
    if (note != NOTE_OFF) w->vol = smk_vel;     /* SMK-37: the key's velocity (smk_keys.c) */
#else
    if (note != NOTE_OFF && (w->n == EMPTY || w->n == NOTE_OFF) && w->vol == EMPTY) w->vol = last.vol;
#endif
    w->n = (uint8_t)note;
    if (note != NOTE_OFF) w->i = last.inst;
    del = del > 5 ? 5 : del;
    if (del > 0) {
        for (k = 0; k < NFX && w->fx[k].c != CMD_DEL; k++) {}
        if (k == NFX) for (k = 0; k < NFX && w->fx[k].c; k++) {}
        if (k < NFX) { w->fx[k].c = CMD_DEL; w->fx[k].v = (uint8_t)del; }
    }
    rec_count++;
}

/* KNOB 1-4 acceleration (decisions.md "Knobs, take 3" (3), app.js accel): by the time between detents, 50 ms or more = 1
 * step, 25-50 ms = 2, under 25 ms = 4; a direction change resets to 1. Lists and ranges of 16 steps or fewer stay 1
 * (fast); the crossfader and MASTER do not come here. shortcut: the thresholds are a pick, tune them on the unit; the
 * HAL counts detents, not their times, so a take's gap is shared out over its detents */
static int32_t kn_mul = 1;                      /* the factor of the last knob_take */
static int32_t knob_take(uint32_t k)            /* KNOB k + 1's detents since the last loop */
{
    static uint32_t at[4];
    static int8_t dir[4];
    int32_t t = fm1_enc_take(ENC_KNOB1 + k), s = t < 0 ? -1 : 1;
    uint32_t gap;
    if (!t) return 0;
    gap = (fm1_ms - at[k]) / (uint32_t)(t * s);
    kn_mul = dir[k] != s || gap >= 50u ? 1 : gap >= 25u ? 2 : 4;
    at[k] = fm1_ms; dir[k] = (int8_t)s;
    return t;
}
static int32_t fast(const param_desc_t *d, int32_t t)   /* a turn's steps on d (0: ENGINE, PRESET, a slot's choice) */
{
    return !d || is_list(d) || d->max - d->min <= 16 ? t : t * kn_mul;
}
/* ---------------------------------------------------------------- knob recording (docs/ux/knob-record.md) --- */
/* In REC a knob turn is written as the parameter's command on the recorded channel's row nearest the turn (the last
 * value in a row's window wins), only on rows playing the instrument, into the slot already holding that command, else
 * the cursor's FX column or the next free one right of it (cursor elsewhere: the first free), never a free cell right
 * above a ramp command; none free: nothing. The instrument is not changed; the value is heard at once (held). The
 * crossfader in REC writes SCN xx the same way, on any row. Touch (knob-record.md 7): while the hand moves it, the
 * parameter's recorded commands and ramps do not play (tch, the ISR) and the rows the playhead passes on the recorded
 * channel lose their older value for it; a gesture starts from what you hear, goes on from itself, ends 0.6 s after the
 * last detent. The take's cells are drawn red until it ends (app.js recordKnob / recordScn / touch / touchState) */
#define TOUCH_MS 600u
static struct { uint32_t until, at; int16_t raw; } tg[NTOUCH];   /* each gesture: its end, the phrase row it last passed,
                                                 * its value (it goes on from itself: app.js knobRec) */
static uint8_t tw_used;
static uint32_t cell_bit(uint32_t ph, uint32_t r, uint32_t k) { return (ph * 16u + r) * NFX + k; }
static int rec_at(uint32_t c, uint32_t *ph, uint32_t *row)   /* channel c's row nearest now (app.js recRow); 0: none yet */
{
    uint32_t pos = ch_pos[c];
    if (!playing || count_left || rec_wait || !POS_ON(pos)) return 0;
    *ph = POS_PH(pos);
    *row = (POS_ROW(pos) + (row_q8 > (int32_t)(TICKS * 128u))) & 15u;   /* (as QNT ROW: a command acts at its row's start) */
    return 1;
}
static uint32_t rec_ins(uint32_t ph, uint32_t row)   /* the instrument a row plays: its own or the nearest above, else
                                                 * the channel's (app.js recordKnob) */
{
    int32_t r;
    for (r = (int32_t)row; r >= 0; r--) if (S.phrases[ph][r].i != EMPTY) return S.phrases[ph][r].i;
    return chs[ui.ch].ins;
}
static int32_t rec_slot(uint32_t ph, uint32_t row, uint32_t code)   /* app.js recSlot; -1 = none */
{
    const fx_t *fx = S.phrases[ph][row].fx;
    uint32_t j = ui.view == V_PHRASE && ui.col >= 3u ? (ui.col - 3u) / 2u : 0u, k;
    for (k = 0; k < NFX; k++) if (fx[k].c == code) return (int32_t)k;
    for (; j < NFX; j++) if (!fx[j].c && !(row < 15u && cmd_is_ramp(S.phrases[ph][row + 1u].fx[j].c))) return (int32_t)j;
    return -1;
}
static void rec_cell(uint32_t ph, uint32_t row, uint32_t k, uint32_t code, uint32_t v)
{
    uint32_t b = cell_bit(ph, row, k);
    S.phrases[ph][row].fx[k].c = (uint8_t)code;
    S.phrases[ph][row].fx[k].v = (uint8_t)v;
    tw[b >> 3] |= (uint8_t)(1u << (b & 7u));
    rc[b >> 3] |= (uint8_t)(1u << (b & 7u));
    tw_used = 1;
}
static uint32_t touch(uint32_t key)             /* the hand on key: its gesture (+ 0x100: a new one) */
{
    uint32_t g = NTOUCH - 1u, k;
    if (key != TOUCH_SCN) {
        for (g = 0; g < NTOUCH - 1u && tch[g] != key; g++) {}
        for (k = 0; g == NTOUCH - 1u && k < NTOUCH - 1u; k++) if (tch[k] == 0xFFFFu) g = k;
        if (g == NTOUCH - 1u) g = 0;            /* (a fifth parameter at once: the first gesture gives way) */
    }
    tg[g].until = fm1_ms + TOUCH_MS;
    if (tch[g] == key) return g;
    tg[g].at = 0xFFFFFFFFu;
    tch[g] = (uint16_t)key;
    return g | 0x100u;
}
static const char *rec_knob(uint32_t i, uint32_t key, int32_t d)   /* app.js recordKnob: 0 = written (or not yet), else why not */
{
    static char why[40];
    uint32_t ph, row, c = CMD_SOUND | key, g;
    int32_t slot, raw;
    if (key >= P_COUNT || cmd_index(c, i) < 0) {
        str_cpy(why, "not recorded: ", sizeof why);
        sa(why, key == FK_ENGINE ? "ENGINE" : key == FK_PRESET ? "PRESET" : "it"); sa(why, " is not a command");
        return why;
    }
    if (!rec_at(ui.ch, &ph, &row)) return 0;    /* (count-in, waiting for the next phrase) */
    g = touch(i << 8 | key);
    raw = g >> 8 ? ins_value(i, key) : tg[g & 0xFFu].raw;   /* a new gesture starts from what you hear */
    raw = param_turn(ins_desc(i, key), raw, fast(ins_desc(i, key), d));
    if (rec_ins(ph, row) != i) { str_cpy(why, "not recorded: row plays I", sizeof why); why[str_len(why) + 1] = 0; why[str_len(why)] = HEX[rec_ins(ph, row) & 15u]; return why; }
    if ((slot = rec_slot(ph, row, c)) < 0) return "not recorded: no free FX";
    rec_cell(ph, row, (uint32_t)slot, c, cmd_encode_raw(c, i, raw));
    tg[g & 0xFFu].raw = (int16_t)raw;
    while (kh_req) platform_poll();             /* (the ISR takes one at a time) */
    kh_ins = (uint8_t)i; kh_id = (uint8_t)key; kh_raw = (int16_t)raw; kh_ch = (uint8_t)ui.ch; kh_req = 1;
    return 0;
}
static void rec_scn(void)                       /* the crossfader in REC: SCN xx (app.js recordScn) */
{
    uint32_t ph, row;
    int32_t slot;
    touch(TOUCH_SCN);
    if (!rec_at(ui.ch, &ph, &row)) return;
    if ((slot = rec_slot(ph, row, CMD_SCN)) < 0) { say("SCN not recorded: no free FX"); return; }
    rec_cell(ph, row, (uint32_t)slot, CMD_SCN, (S.scn.xf * 255u + 63u) / 127u);
}
static void rec_tvl(uint32_t n, int32_t d)      /* X in REC: KNOB n records TVn on track n, with touch; the mix level
                                                 * stays, the value is heard at once (app.js recordTvl) */
{
    uint32_t ph, row, g;
    int32_t slot, lvl;
    char s[40];
    str_cpy(s, "TV", sizeof s); sn(s, (int32_t)n + 1);
    if (!rec_at(n, &ph, &row)) { sa(s, " not recorded: track "); sn(s, (int32_t)n + 1); sa(s, " not playing"); say(s); return; }
    g = touch(TOUCH_TVL | n);
    lvl = g >> 8 ? (ch_tvl[n] >= 0 ? ch_tvl[n] : S.lvl[n]) : tg[g & 0xFFu].raw;   /* a new gesture: from what you hear */
    lvl = clamp_i(lvl + d, 0, 127);
    if ((slot = rec_slot(ph, row, PLAY_CMDS[PLAY_MXR + n])) < 0) { sa(s, " not recorded: no free FX"); say(s); return; }
    rec_cell(ph, row, (uint32_t)slot, PLAY_CMDS[PLAY_MXR + n], ((uint32_t)lvl * 255u + 63u) / 127u);
    tg[g & 0xFFu].raw = (int16_t)lvl;
    ch_tvl[n] = (int16_t)lvl; tvl_by[n] = (uint8_t)n;
    str_cpy(s, "REC TV", sizeof s); sn(s, (int32_t)n + 1); sa(s, " "); sn(s, lvl);
    say(s);
}
static uint32_t touch_state(void)               /* each loop (app.js touchState): gestures end; the rows the playhead
                                                 * passes lose their older value for a touched parameter. -> dirty */
{
    uint32_t g, j, pos, at, ph, row, dirty = 0, any = 0;
    for (g = 0; g < NTOUCH; g++) {
        uint32_t code, tv;
        if (tch[g] == 0xFFFFu) continue;
        if (fm1_ms - tg[g].until < 0x80000000u || kb != KB_REC || !playing) { tch[g] = 0xFFFF; continue; }
        any = 1;
        tv = (tch[g] & 0xFF00u) == TOUCH_TVL;   /* (X's TVn: on its own track) */
        pos = ch_pos[tv ? tch[g] & 0xFFu : ui.ch]; at = POS_PH(pos) << 4 | POS_ROW(pos); ph = POS_PH(pos); row = POS_ROW(pos);
        if (!POS_ON(pos) || at == tg[g].at) continue;
        tg[g].at = at;
        if (g < NTOUCH - 1u && !tv && rec_ins(ph, row) != (uint32_t)(tch[g] >> 8)) continue;   /* (a knob: rows playing its instrument) */
        code = tv ? PLAY_CMDS[PLAY_MXR + (tch[g] & 0xFFu)] : g < NTOUCH - 1u ? CMD_SOUND | (tch[g] & 0xFFu) : CMD_SCN;
        for (j = 0; j < NFX; j++) {
            uint32_t b = cell_bit(ph, row, j);
            if (S.phrases[ph][row].fx[j].c != code || tw[b >> 3] >> (b & 7u) & 1u) continue;
            S.phrases[ph][row].fx[j].c = S.phrases[ph][row].fx[j].v = 0;
            rc[b >> 3] &= (uint8_t)~(1u << (b & 7u));
            dirty = 0xFFFFu;
        }
    }
    if (!any && tw_used) { memset(tw, 0, sizeof tw); tw_used = 0; }   /* shortcut: gestures that overlap in time share
                                                 * tw (their cells are told apart by command); exact per gesture costs 768 B each */
    return dirty;
}

/* ---------------------------------------------------------------- the LEDs (app.js leds) --- */
#if !BOARD_SMK37                                /* (the SMK-37's: smk_keys.c) */
/* Buttons: dim when they do something here, lit for a state (REC: notes; blinking: armed, recording; PLAY while
 * playing, in its normal light; OCT- / OCT+ off octave 4). Keys: the gamepad keys dim; in notes mode the 16 white keys
 * are the phrase's steps (a note: lit, the cursor's row: dim, the playhead: the opposite); a note the cursor's channel
 * strikes flashes its key on C4..B4, whatever its octave (the opposite for 0.1 s; RET: half the gap between hits); a
 * note key held: lit. Off / dim / lit = fm1_led / fm1_led_dim (the glow). Written when they change. */
enum { L_OFF, L_DIM, L_ON };
#define NLED 41u                                /* ids: 14 buttons, then the 27 keys (platform.c) */
static const uint8_t WHITE_KEYS[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static uint8_t led_was[NLED];
static uint32_t flash_note, flash_t, flash_len, flash_seq;
static void led_set(uint32_t id, uint32_t st)
{
    uint32_t p, r;
    for (p = 0; p < FM1_NCOL; p++)
        for (r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][p] == (int8_t)id) {
                fm1_led[p] = (uint8_t)(st == L_ON ? fm1_led[p] | 1u << r : fm1_led[p] & ~(1u << r));
                fm1_led_dim[p] = (uint8_t)(st == L_DIM ? fm1_led_dim[p] | 1u << r : fm1_led_dim[p] & ~(1u << r));
            }
}
static void leds_update(void)
{
    static const uint8_t PADK[8] = {PAD_LEFT, PAD_DOWN, PAD_UP, PAD_RIGHT, PAD_SHIFT, PAD_PLAY, PAD_OPTION, PAD_EDIT}, BTNS[11] = {BTN_SEQ, BTN_FX, BTN_ENV, BTN_LFO, BTN_EDIT, BTN_PLAY, BTN_OCT_DOWN, BTN_OCT_UP, BTN_GLO, BTN_HOME, BTN_SEL};
    uint8_t L[NLED];
    uint32_t k, r;
    memset(L, L_OFF, sizeof L);
    for (k = 0; k < 11u; k++) L[BTNS[k]] = L_DIM;
    if (ui.view == V_PROJ) L[BTN_GLO] = L_ON;
    if (ui.follow) L[BTN_HOME] = L_ON;
    L[BTN_REC] = kb == KB_PAD ? L_DIM : kb == KB_NOTES || fm1_ms / 250u % 2u == 0u ? L_ON : L_OFF;
    if (playing) L[BTN_PLAY] = L_ON;
    L[BTN_SAVE] = proj_dirty ? L_ON : L_DIM;   /* lit: unsaved changes */
    if (oct < 0) L[BTN_OCT_DOWN] = L_ON;
    if (oct > 0) L[BTN_OCT_UP] = L_ON;
    if (sel_held) {                             /* the scenes: the near side's lit, the far side's blinking (the next), the
                                                 * others dim; the gamepad keys dim (app.js leds) */
        uint32_t nb = S.scn.xf * 2u > XF_MAX;
        L[BTN_SEL] = L_ON;
        for (k = 0; k < NSCENE; k++) L[14u + SCENE_KEYS[k]] = L_DIM;
        for (k = 0; k < 8u; k++) L[14u + PADK[k]] = L_DIM;
        L[14u + SCENE_KEYS[nb ? S.scn.a : S.scn.b]] = fm1_ms / 250u % 2u == 0u ? L_ON : L_OFF;
        L[14u + SCENE_KEYS[nb ? S.scn.b : S.scn.a]] = L_ON;
    } else if (kb == KB_PAD) for (k = 0; k < 8u; k++) L[14u + PADK[k]] = L_DIM;
    else {
        const row_t *rows = cur_phrase_rows();
        uint32_t pos = ch_pos[ui.ch];
        for (r = 0; r < 16u; r++) {
            uint32_t st = rows && rows[r].n != EMPTY ? L_ON : r == ui.row ? L_DIM : L_OFF;
            if (playing && POS_ON(pos) && POS_PH(pos) == cur_phrase() && POS_ROW(pos) == r) st = st == L_ON ? L_OFF : L_ON;
            L[14u + WHITE_KEYS[r]] = (uint8_t)st;
        }
    }
    if (hit_seq[ui.ch] != flash_seq) {          /* a new hit on the cursor's channel */
        flash_seq = hit_seq[ui.ch]; flash_note = hit_note[ui.ch]; flash_len = hit_ms[ui.ch]; flash_t = fm1_ms;
    }
    if (playing && fm1_ms - flash_t < flash_len) { k = 14u + 7u + flash_note % 12u; L[k] = L[k] == L_ON ? L_OFF : L_ON; }
    for (k = 0; k < NKEYS; k++) if (key_want >> k & 1u) L[14u + k] = L_ON;
    for (k = 0; k < NLED; k++) if (L[k] != led_was[k]) { led_was[k] = L[k]; led_set(k, L[k]); }
}
#endif

/* ---------------------------------------------------------------- saving (docs/flash-map.md) --- */
/* 4 project slots in the store, each kept twice (A / B, 7 sectors a copy): a save writes the copy that is not the
 * current one, the newer valid copy is the project (Felucca / Melodee's rules: the header written last, CRCs, read
 * back). A project is the song and the instruments as they lie in RAM (the undo shadow's bytes, U_SIZE), LZ4-packed:
 * empty cells repeat, so a typical project packs into one sector. RAM is short (96 KiB, the pool full of Felucca's
 * buffers), so nothing is staged: a save packs from the undo shadow (equal to the song once undo_close has run) and
 * writes the flash a page at a time; a load unpacks from the flash into the shadow, checks it, then copies it in.
 * The panel SAVE saves the open project (slot 0 until the G page picks slots), refused while playing (the flash write
 * stops the sound); it is lit while there are unsaved changes. At power-on slot 0 is opened. Everything read back is
 * checked before it reaches the song (a damaged or foreign record is never loaded). */
#define PJ_SLOTS 4u
#define PJ_SECT 7u                              /* sectors a copy */
#define PJ_HDR 256u                             /* the header's room in a copy's first sector */
#define PJ_SEC_DATA 3840u                       /* the payload a sector holds: its last 256 B stay erased, so no
                                                 * sector of a project ends like an update record (ldr_core.c) */
#define PJ_CAP (PJ_SECT * PJ_SEC_DATA - PJ_HDR)  /* 26,624 B: U_SIZE (24,233) packed, even at LZ4's worst */
#define PJ_MAGIC 0x4A50524Bu                    /* "KRPJ" */
#define PJ_VER 9u                               /* 9: TV1-TV4 (TVL's cells moved, proj_load); 8: G's MUTE (S.mute_mode); 7: the scenes (S.scn); 6: the effects (S.fx); 5: the mixer's levels (S.lvl); 4: the knob
                                                 * slots (S.knob); 3: the G page's settings (S.pj) and the name in the
                                                 * header; 2..6 still load (1: the first draft's sparse form, never on
                                                 * a device) */
typedef struct { uint32_t magic; uint8_t ver, slot, np, rsv; uint32_t seq, len, raw, crc, rsv2; char name[16]; uint32_t hcrc; } pj_hdr_t;
#define PJ_HDR2 28u                             /* version 2's header: up to rsv2, then its CRC (32 B) */

static uint32_t crc_upd(uint32_t c, const uint8_t *p, uint32_t n)   /* CRC-32 (zlib's), carried: start with 0 */
{
    uint32_t k;
    c = ~c;
    while (n--) { c ^= *p++; for (k = 0; k < 8u; k++) c = c & 1u ? (c >> 1) ^ 0xEDB88320u : c >> 1; }
    return ~c;
}
static uint32_t pj_base(uint32_t slot, uint32_t copy) { return (slot * 2u + copy) * PJ_SECT * 0x1000u; }
static uint32_t pj_at(uint32_t i)               /* payload byte i -> its offset in the copy */
{
    return i < PJ_SEC_DATA - PJ_HDR ? PJ_HDR + i
         : (1u + (i - (PJ_SEC_DATA - PJ_HDR)) / PJ_SEC_DATA) * 0x1000u + (i - (PJ_SEC_DATA - PJ_HDR)) % PJ_SEC_DATA;
}

/* the payload as a stream: written a page at a time (each sector erased when the stream reaches it), read back a
 * page at a time; the CRC kept along. err: a flash call failed, or the stream ran past PJ_CAP / its length */
static struct { uint32_t base, i, len, at, n, crc, sect; uint8_t err; uint8_t page[256]; } pjs;
static void pjs_open(uint32_t base, uint32_t len) { memset(&pjs, 0, sizeof pjs); pjs.base = base; pjs.len = len; pjs.sect = 0xFFFFFFFFu; }
static void pjs_flush(void)
{
    uint32_t s = pj_at(pjs.at) >> 12;
    if (!pjs.n || pjs.err) return;
    if (s != pjs.sect && (pjs.sect == 0xFFFFFFFFu || s > pjs.sect)) {   /* a sector the stream enters: erased first */
        if (store_erase(pjs.base + s * 0x1000u)) pjs.err = 1;
        pjs.sect = s;
    }
    if (!pjs.err && store_write(pjs.base + pj_at(pjs.at), pjs.page, pjs.n)) pjs.err = 1;
    pjs.at += pjs.n;
    pjs.n = 0;
}
static void pjs_put(uint8_t b)
{
    if (pjs.i >= PJ_CAP) { pjs.err = 1; return; }
    if (pjs.n && (pjs.n == sizeof pjs.page || (pj_at(pjs.i) & 0xFFFu) == 0u)) pjs_flush();   /* full, or a new sector */
    pjs.page[pjs.n++] = b;
    pjs.crc = crc_upd(pjs.crc, &b, 1);
    pjs.i++;
}
static int pjs_get(void)                        /* the next payload byte, -1 = the end (or a failed read) */
{
    if (pjs.i >= pjs.len || pjs.err) return -1;
    if (pjs.i >= pjs.at + pjs.n) {              /* refill: up to a page, inside one sector's payload */
        uint32_t a = pj_at(pjs.i), k = PJ_SEC_DATA - (a & 0xFFFu);
        k = k < sizeof pjs.page ? k : sizeof pjs.page;
        k = k < pjs.len - pjs.i ? k : pjs.len - pjs.i;
        if (store_read(pjs.base + a, pjs.page, k)) { pjs.err = 1; return -1; }
        pjs.crc = crc_upd(pjs.crc, pjs.page, k);
        pjs.at = pjs.i;
        pjs.n = k;
    }
    return pjs.page[pjs.i++ - pjs.at];
}

#define Q_MAX 255u                              /* shortcut: 255 lines per instrument (q_at holds a byte), more are
                                                 * left out; upgrade if a song reaches it */
typedef struct { uint16_t key, n, t, at; uint8_t chs, e; } qitem_t;   /* key ph << 4 | r; n plays; the first play: t
                                                 * rows from the song's start, at = ch << 10 | song row << 4 | chain row,
                                                 * e = the row its phrase was entered at */
/* shortcut: Q's view tables and the save's LZ4 hash share one pool block (-4 KB); a save clobbers Q's tables, so save_button
 * rebuilds them when Q is shown (a save from anywhere else leaves Q to q_build() on entry). */
static union { uint16_t ht[2048]; struct { qitem_t it[Q_MAX]; uint8_t at[NPHR * 16u]; } q; } qh __attribute__((section(".pool")));   /* q.at: a phrase row's line, EMPTY = none */

/* LZ4 block format (lz4.org; fw/tools/lz4blk.py is the reference): greedy, a 2048-entry hash of positions; the last 5
 * bytes are literals and no match starts in the last 12, as the format asks. Packs s into the stream (pjs_put) */
static void lz4_len(uint32_t n) { for (n -= 15u; n >= 255u; n -= 255u) pjs_put(255u); pjs_put((uint8_t)n); }
static void lz4_pack(const uint8_t *s, uint32_t n)
{
    uint32_t ip = 0, an = 0, k;
    memset(qh.ht, 0, sizeof qh.ht);
    while (n > 12u && ip < n - 12u) {
        uint32_t v = (uint32_t)s[ip] | s[ip + 1] << 8 | s[ip + 2] << 16 | (uint32_t)s[ip + 3] << 24, h = (v * 2654435761u) >> 21;
        uint32_t ref = qh.ht[h], len = 4, lit = ip - an;
        qh.ht[h] = (uint16_t)ip;
        if (ref >= ip || ip - ref > 0xFFFFu || s[ref] != s[ip] || s[ref + 1] != s[ip + 1] || s[ref + 2] != s[ip + 2] || s[ref + 3] != s[ip + 3]) { ip++; continue; }
        while (ip + len < n - 5u && s[ref + len] == s[ip + len]) len++;
        pjs_put((uint8_t)((lit < 15u ? lit : 15u) << 4 | (len - 4u < 15u ? len - 4u : 15u)));
        if (lit >= 15u) lz4_len(lit);
        for (k = 0; k < lit; k++) pjs_put(s[an + k]);
        pjs_put((uint8_t)(ip - ref)); pjs_put((uint8_t)((ip - ref) >> 8));
        if (len - 4u >= 15u) lz4_len(len - 4u);
        ip += len; an = ip;
    }
    pjs_put((uint8_t)((n - an < 15u ? n - an : 15u) << 4));   /* the last literals */
    if (n - an >= 15u) lz4_len(n - an);
    for (k = an; k < n; k++) pjs_put(s[k]);
}
static int32_t lz4_unpack(uint8_t *d, uint32_t cap)   /* from the stream (pjs_get) into d: the length, -1 = bad data */
{
    uint32_t o = 0;
    int32_t t, b;
    while ((t = pjs_get()) >= 0) {
        uint32_t lit = (uint32_t)t >> 4, len = (uint32_t)t & 15u, off, k;
        if (lit == 15u) do { if ((b = pjs_get()) < 0) return -1; lit += (uint32_t)b; } while (b == 255);
        if (lit > cap - o) return -1;
        for (k = 0; k < lit; k++) { if ((b = pjs_get()) < 0) return -1; d[o++] = (uint8_t)b; }
        if ((b = pjs_get()) < 0) break;         /* the last sequence: literals only */
        if ((t = pjs_get()) < 0) return -1;
        off = (uint32_t)b | (uint32_t)t << 8;
        if (!off || off > o) return -1;
        if (len == 15u) do { if ((b = pjs_get()) < 0) return -1; len += (uint32_t)b; } while (b == 255);
        len += 4u;
        if (len > cap - o) return -1;
        for (k = 0; k < len; k++, o++) d[o] = d[o - off];   /* (may overlap: byte by byte) */
    }
    return pjs.err ? -1 : (int32_t)o;
}

static uint8_t pj_val(uint8_t v, uint32_t max) { return v == EMPTY || v < max ? v : EMPTY; }
static void name_fix(char *nm)                  /* a name as read: ended, printable letters only */
{
    uint32_t k;
    nm[NAME_LEN] = 0;
    for (k = 0; k < NAME_LEN && nm[k]; k++) if (nm[k] < ' ' || nm[k] > '~') nm[k] = ' ';
}
static void pj_check(void)                      /* a loaded project, made safe: every value in range */
{
    uint32_t i, r, k;
    if (S.tempo < 40u) S.tempo = 120u;
    for (k = 0; k < NCH; k++) { S.mute[k] &= 1u; S.solo[k] &= 1u; if (S.lvl[k] > 127u) S.lvl[k] = 127; }
    for (k = 0; k < 9u; k++) S.fx[k] = (uint8_t)param_fit(&GP[FXF[k].g], S.fx[k]);
    name_fix(S.pj.name);
    if (S.pj.step > 8u) S.pj.step = 1;
    S.mute_mode &= 1u;
    if (S.pj.swing > 100u) S.pj.swing = 0;
    if (S.pj.tune < -50 || S.pj.tune > 50) S.pj.tune = 0;
    if (S.pj.metro > 2u) S.pj.metro = 1;
    if (S.pj.count > 2u) S.pj.count = 1;
    S.pj.qnt &= 1u;
    for (i = 0; i < sizeof S.knob; i++) (&S.knob[0][0])[i] = pj_val((&S.knob[0][0])[i], P_COUNT);
    for (i = 0; i < NSONG * NCH; i++) (&S.song[0][0])[i] = pj_val((&S.song[0][0])[i], NCHN);
    for (i = 0; i < NCHN; i++)
        for (r = 0; r < 16u; r++) {
            crow_t *w = &S.chains[i][r];
            w->ph = pj_val(w->ph, NPHR);
            w->tr = (int8_t)clamp_i(w->tr, -48, 48);
        }
    for (i = 0; i < NPHR; i++)
        for (r = 0; r < 16u; r++) {
            row_t *w = &S.phrases[i][r];
            if (w->n != EMPTY && w->n != NOTE_OFF && (w->n < NOTE_MIN || w->n > NOTE_MAX)) w->n = EMPTY;
            w->vol = pj_val(w->vol, VOL_MAX + 1u);
            w->i = pj_val(w->i, NINS);
            for (k = 0; k < NFX; k++)
                if (w->fx[k].c && !(w->fx[k].c < CMD_NPLAIN || (w->fx[k].c >= CMD_SOUND && (w->fx[k].c & 0x7Fu) < P_COUNT)))
                    w->fx[k].c = w->fx[k].v = 0;
        }
    for (i = 0; i < NINS; i++) {                /* the instruments: engine, preset, name, every parameter in range */
        inst_t *I = &ins[i];
        if (I->eng >= NENGINES) I->eng = 0;
        if (!ENGINES[I->eng]->npresets || I->preset >= ENGINES[I->eng]->npresets) I->preset = 0;
        I->renamed &= 1u;
        name_fix(I->name);
        for (k = 0; k < P_COUNT; k++) I->p[k] = (int16_t)param_fit(ins_desc(i, k), I->p[k]);
        ins_force_removed(i);
        ins_gen[i]++;                           /* the parts reload it */
    }
    for (i = 0; i < NSCENE; i++) {              /* the scenes: known parameters, in range, once each, the used first */
        sc_t *e = S.scn.k[i];
        uint32_t n = 0, j;
        for (k = 0; k < SCENE_MAX; k++) {
            uint32_t t = e[k].t, p = e[k].p;
            const param_desc_t *d = t < NINS && p < P_COUNT ? ins_desc(t, p) : t == SC_MIX && p < NCH ? &TP[P_LEVEL]
                                  : t == SC_FX && p < 9u ? &GP[FXF[p].g] : 0;
            for (j = 0; j < n && !(e[j].t == t && e[j].p == p); j++) {}
            if (!d || j < n) continue;          /* (unknown, or already in the scene above) */
            e[n].t = (uint8_t)t; e[n].p = (uint8_t)p; e[n].v = (int8_t)param_fit(d, e[k].v);
            n++;
        }
        for (; n < SCENE_MAX; n++) memset(&e[n], EMPTY, sizeof e[n]);
    }
    if (S.scn.a >= NSCENE) S.scn.a = 0;
    if (S.scn.b >= NSCENE) S.scn.b = 1;
    if (S.scn.xf > XF_MAX) S.scn.xf = 0;
}

static uint32_t pj_missing(uint32_t ver)        /* the bytes an older version lacks at the end of S */
{
    return ver >= 8u ? 0u : sizeof S.mute_mode + (ver <= 6u ? sizeof S.scn : 0u) + (ver <= 5u ? sizeof S.fx : 0u) + (ver <= 4u ? sizeof S.lvl : 0u) + (ver <= 3u ? sizeof S.knob : 0u) + (ver == 2u ? sizeof S.pj : 0u);
}
static int pj_head(uint32_t slot, uint32_t copy, pj_hdr_t *h)   /* 1: a valid header of this format (or 2, 3) */
{
    uint32_t v2c;
    if (store_read(pj_base(slot, copy), h, sizeof *h) || h->magic != PJ_MAGIC || h->slot != slot || h->np != P_COUNT
        || !h->len || h->len > PJ_CAP) return 0;
    memcpy(&v2c, h->name, 4);
    if (h->ver == 2u && h->raw == U_SIZE - pj_missing(2) && v2c == crc_upd(0, (const uint8_t *)h, PJ_HDR2)) { h->name[0] = 0; return 1; }
    if (h->ver < 3u || h->ver > PJ_VER || h->raw != U_SIZE - pj_missing(h->ver)
        || h->hcrc != crc_upd(0, (const uint8_t *)h, sizeof *h - 4u)) return 0;
    h->name[sizeof h->name - 1u] = 0;
    return 1;
}   /* shortcut: another Felucca parameter layout (np) is refused; convert it (core.h's np rule) when it changes */
/* the slot's copies: their headers, and the current one (the newer valid copy; a tie: A), -1 = none */
static int pj_cur(uint32_t slot, pj_hdr_t *h, int *ok)
{
    ok[0] = pj_head(slot, 0, &h[0]);
    ok[1] = pj_head(slot, 1, &h[1]);
    if (!ok[0] && !ok[1]) return -1;
    if (!ok[0]) return 1;
    if (!ok[1]) return 0;
    return h[1].seq != h[0].seq && h[1].seq - h[0].seq < 0x80000000u;   /* (sequence numbers wrap) */
}
/* TVL (code 12: this track's level, before 2026-10-12) -> TV of the first track whose chains use its phrase, TV1 if
 * none (model.js upgradeSong; my pick, to review) */
static void tvl_upgrade(void)
{
    uint8_t first[NPHR];
    uint32_t k, r, p, f;
    memset(first, 0, sizeof first);
    for (k = NCH; k-- > 0;)
        for (r = 0; r < NSONG; r++)
            if (S.song[r][k] != EMPTY)
                for (f = 0; f < 16u; f++) if (S.chains[S.song[r][k]][f].ph < NPHR) first[S.chains[S.song[r][k]][f].ph] = (uint8_t)k;
    for (p = 0; p < NPHR; p++)
        for (r = 0; r < 16u; r++)
            for (f = 0; f < NFX; f++)
                if (S.phrases[p][r].fx[f].c == CMD_TV1 && first[p]) S.phrases[p][r].fx[f].c = (uint8_t)(CMD_TV2 + first[p] - 1u);
}
static int proj_load(uint32_t slot)             /* 0: loaded (the current copy, else the other); else the song is as
                                                 * it was. Unpacks into the undo shadow: undo_reset afterwards */
{
    pj_hdr_t h[2];
    int ok[2], c = pj_cur(slot, h, ok), k;
    for (k = 0; c >= 0 && k < 2; k++, c ^= 1) {
        if (!ok[c]) continue;
        pjs_open(pj_base(slot, (uint32_t)c), h[c].len);
        if (lz4_unpack(u_shadow, U_SIZE) != (int32_t)h[c].raw || pjs.crc != h[c].crc) continue;
        if (pj_missing(h[c].ver)) mem_move(u_shadow + sizeof S, u_shadow + sizeof S - pj_missing(h[c].ver), sizeof ins);
        memcpy(&S, u_shadow, sizeof S);
        memcpy(ins, u_shadow + sizeof S, sizeof ins);
        if (h[c].ver == 2u) pj_defaults();
        if (h[c].ver < 4u) memset(S.knob, EMPTY, sizeof S.knob);
        if (h[c].ver < 5u) memset(S.lvl, 104, sizeof S.lvl);
        if (h[c].ver < 6u) fx_defaults();
        if (h[c].ver < 7u) scn_defaults();
        if (h[c].ver < 8u) S.mute_mode = 0;
        if (h[c].ver < 9u) tvl_upgrade();
        pj_check();
        return 0;
    }
    return -1;
}
static int proj_save(uint32_t slot)             /* 0: saved and read back; else the current copy is untouched */
{
    pj_hdr_t h[2], w;
    int ok[2], cur = pj_cur(slot, h, ok), c = cur < 0 ? 0 : !cur;   /* write the copy that is not the current one */
    uint32_t base = pj_base(slot, (uint32_t)c), crc, len;
    undo_close();                               /* the shadow = the song and the instruments now ... */
    u_shadow[0] = S.tempo;                      /* ... but the tempo, which undo leaves out */
    pjs_open(base, 0);
    lz4_pack(u_shadow, U_SIZE);
    pjs_flush();
    if (pjs.err) return -1;
    crc = pjs.crc; len = pjs.i;
    memset(&w, 0, sizeof w);
    w.magic = PJ_MAGIC; w.ver = PJ_VER; w.slot = (uint8_t)slot; w.np = P_COUNT;
    w.seq = cur < 0 ? 1u : h[cur].seq + 1u;
    w.len = len; w.raw = U_SIZE; w.crc = crc;
    str_cpy(w.name, S.pj.name, sizeof w.name);
    w.hcrc = crc_upd(0, (const uint8_t *)&w, sizeof w - 4u);
    if (store_write(base, &w, sizeof w)) return -1;   /* the header last: a cut save stays invalid */
    pjs_open(base, len);                        /* read back: the whole payload's CRC */
    while (pjs_get() >= 0) {}
    return pjs.err || pjs.crc != crc || !pj_head(slot, (uint32_t)c, &h[c]) ? -1 : 0;
}
/* ---------------------------------------------------------------- the G page (render.js viewProj; app.js slotEdit) --- */
/* Left: the 4 slots, * = open. EDIT = load, EDIT EDIT on an empty slot = new, SHIFT + OPTION then OPTION = copy,
 * SHIFT + EDIT = paste, OPTION + EDIT = delete. What loses unsaved work, deletes or replaces asks first (the next EDIT
 * confirms, any other key cancels); while playing every slot action asks, then waits for the end of the phrase the
 * cursor's channel plays, stops and acts (user, 2026-10-10: the flash writes stop the sound); PLAY: at once.
 * Copying the open project copies it as it is when pasted (unsaved changes too), another slot as saved: its packed
 * record sector by sector through a 256-B page (RAM is full). Right: NAME and the song's settings, edited as the
 * instrument page's fields (EDIT + d-pad, OPTION + EDIT = the default). */
enum { SA_LOAD = 1, SA_NEW, SA_PASTE, SA_DELETE, SA_SCENE };   /* (SA_SCENE: SEL + OPTION + a scene key, slot = the scene) */
static struct { uint8_t used; char name[NAME_LEN + 1]; uint32_t len; } slots[PJ_SLOTS];   /* as saved (the headers) */
static struct { uint8_t act, slot, wait; const char *verb; char t[40]; } pend;   /* asked, or waiting for the phrase end */
static uint8_t pj_sel = EMPTY, pj_clip = EMPTY; /* the slot marked (SHIFT + OPTION), the slot copied */
#define PR_N 14u                                /* the fields: slots 0..3, TEMPO, STEP, NAME, SWING, TUNE, METRO, COUNT, QNT, MUTE,
                                                 * the usage block */
#define PR_TEMPO 4u
#define PR_NAME 6u
#define PR_LISTS 9u                             /* METRO, COUNT, QNT, MUTE: words (proj_band's LIST) */
#define PR_SYS 13u                              /* the usage block: a button, EDIT opens SYSTEM (docs/ux/system-popup.md 3); no
                                                 * value (PR[].v NULL: never pr_get / pr_set it) */
static const struct { uint8_t x, line; int16_t lo, hi, big, def; const char *lbl; uint8_t *v; const char *unit; } PR[PR_N] = {
    {0, 0, 0, 0, 0, 0, 0}, {0, 1, 0, 0, 0, 0, 0}, {0, 2, 0, 0, 0, 0, 0}, {0, 3, 0, 0, 0, 0, 0},
    {21, 3, 40, 255, 10, 120, "TEMPO", &S.tempo, ""}, {21, 4, 0, 8, 4, 1, "STEP", &S.pj.step, ""}, {21, 1, 0, 0, 0, 0, "NAME"},
    {21, 5, 0, 100, 10, 0, "SWING", &S.pj.swing, "%"}, {21, 6, -50, 50, 10, 0, "TUNE", (uint8_t *)&S.pj.tune, "ct"},
    {21, 7, 0, 2, 1, 1, "METRO", &S.pj.metro}, {21, 8, 0, 2, 1, 1, "COUNT", &S.pj.count}, {21, 9, 0, 1, 1, 1, "QNT", &S.pj.qnt},
    {21, 10, 0, 1, 1, 0, "MUTE", &S.mute_mode}, {21, 11, 0, 0, 0, 0, 0},
};
/* G's SYSTEM popup (docs/ux/system-popup.md 4-5, render.js sysPopup): live things first, fixed info last; centred between
 * the top bar and the bottom line over the rows left of the map, framed in the accent (it has the focus: any key or
 * button closes it, app_main). Drawn at the end of proj_band, so a band of G and the popup over it go out together;
 * refreshed every 250 ms while open */
#define SYS_H (11 * LH + 4)
#define SYS_Y (TOP + (STRIP_Y - TOP - SYS_H) / 2)
static uint8_t sys_open;
static uint16_t sys_pk;                         /* the CPU peak since it opened */
static uint32_t sys_cut0, sys_shown, midi_in_ms;   /* shed_count when it opened; last drawn; the last MIDI in (0: none) */
static void fmt_k(char *d, uint32_t n)          /* 17.0K: KiB, one decimal */
{
    uint32_t t = (n * 10u + 512u) / 1024u;
    d[0] = 0; sn_n(d, (int32_t)(t / 10u), 12); sa_n(d, ".", 12); sn_n(d, (int32_t)(t % 10u), 12); sa_n(d, "K", 12);
}
static void sys_text(int32_t cx, uint32_t i, const char *t, uint16_t col) { cv_str(gx(cx), SYS_Y + 2 + (int32_t)i * LH, t, col); }
static void sys_bar(uint32_t i, uint32_t pm, uint16_t col)   /* 10 characters, pm 0..1000 */
{
    int32_t x = gx(6), y = SYS_Y + 5 + (int32_t)i * LH, w = 10 * CW - 2;
    cv_rect(x, y, w, LH - 6, P_LINE);
    cv_rect(x, y, (int32_t)(((pm > 1000u ? 1000u : pm) * (uint32_t)w + 500u) / 1000u), LH - 6, col);
}
static void sys_draw(void)
{
    static const char *const L[10] = {"CPU", "VOICE", "BATT", "USB", "RAM", "DATA", "SLOTS", "AUDIO", "UP", "FW"};
    char t[24], u[12];
    uint32_t i, p, nv = 0, cut = shed_count - sys_cut0, pm = batt_pm(), mv = batt_mv(), data = 0, up = fm1_ms / 1000u;
    uint16_t load = cpu_pm > 900u ? P_PLAY : cpu_pm > 700u ? P_HELD : P_TEXT, bc = pm < 50u ? P_PLAY : pm < 200u ? P_ACCENT : P_TEXT;
    for (p = 0; p < NCH; p++) for (i = 0; i < NVOICE; i++) nv += trk[p].v[i].active != 0;   /* (the ISR's: a glance) */
    if (nv > NVOICE) nv = NVOICE;               /* (the budget all parts share) */
    cv_rect(0, SYS_Y, MAP_X - 6, SYS_H, P_SURF);
    cv_frame(0, SYS_Y, MAP_X - 6, SYS_H, P_ACCENT);
    sys_text(0, 0, "SYSTEM", P_CTX);
    for (i = 0; i < 10u; i++) sys_text(0, i + 1u, L[i], P_LABEL);
    sys_bar(1, cpu_pm, load);
    t[0] = 0; sn_n(t, (cpu_pm + 5) / 10, sizeof t); sa_n(t, "% pk ", sizeof t); sn_n(t, (sys_pk + 5) / 10, sizeof t); sa_n(t, "%", sizeof t);
    sys_text(17, 1, t, load);
    sys_bar(2, nv * 1000u / NVOICE, P_TEXT);
    t[0] = 0; sn_n(t, (int32_t)nv, sizeof t); sa_n(t, "/8 cut ", sizeof t); sn_n(t, (int32_t)cut, sizeof t);
    sys_text(17, 2, t, cut ? P_HELD : P_TEXT);
    if (batt_raw) {
        sys_bar(3, pm, bc);
        t[0] = 0; sn_n(t, (int32_t)(mv / 1000u), sizeof t); sa_n(t, ".", sizeof t);
        sn_n(t, (int32_t)(mv / 100u % 10u), sizeof t); sn_n(t, (int32_t)(mv / 10u % 10u), sizeof t); sa_n(t, "V", sizeof t);
        if (usb_up()) sa_n(t, " CHG", sizeof t);
        sys_text(17, 3, t, bc);
    } else sys_text(6, 3, "--", P_DIM);
    sys_text(6, 4, usb_up() ? "on" : "off", usb_up() ? P_TEXT : P_DIM);
    sys_text(14, 4, "MIDI", P_LABEL);
    sys_text(19, 4, "IN", midi_in_ms && fm1_ms - midi_in_ms < 300u ? P_HELD : P_DIM);
    sys_text(22, 4, "OUT", P_DIM);              /* (the tracker sends no MIDI yet) */
    fmt_k(t, mem_free(0)); sa_n(t, " free", sizeof t); sys_text(6, 5, t, P_TEXT);
    sys_text(19, 5, "POOL", P_LABEL); fmt_k(t, mem_free(1)); sys_text(24, 5, t, P_TEXT);
    for (i = 0; i < PJ_SLOTS; i++) {
        if (slots[i].used) { data += slots[i].len; fmt_k(u, slots[i].len); } else str_cpy(u, "--", sizeof u);
        sys_text(6 + 6 * (int32_t)i, 7, u, slots[i].used ? P_TEXT : P_DIM);
    }
    fmt_k(t, data); sa_n(t, " of ", sizeof t); sn_n(t, (int32_t)(STORE_LEN / 1024u), sizeof t); sa_n(t, "K", sizeof t);
    sys_text(6, 6, t, P_TEXT);
    t[0] = 0; sn_n(t, (int32_t)(FS / 1000u), sizeof t); sa_n(t, ".", sizeof t); sn_n(t, (int32_t)(FS / 100u % 10u), sizeof t);
    sa_n(t, "kHz  256 frames", sizeof t);
    sys_text(6, 8, t, P_TEXT);
    t[0] = 0; sn_n(t, (int32_t)(up / 3600u), sizeof t); sa_n(t, ":", sizeof t);
    t[str_len(t) + 2] = 0; t[str_len(t) + 1] = (char)('0' + up / 60u % 60u % 10u); t[str_len(t)] = (char)('0' + up / 60u % 60u / 10u);
    sa_n(t, ":", sizeof t);
    t[str_len(t) + 2] = 0; t[str_len(t) + 1] = (char)('0' + up % 60u % 10u); t[str_len(t)] = (char)('0' + up % 60u / 10u);
    sys_text(6, 9, t, P_TEXT);
#ifdef FELUCCA_ID
    sys_text(6, 10, FELUCCA_ID, P_TEXT);
#else
    sys_text(6, 10, "EMU", P_TEXT);
#endif
}
static int32_t pr_get(uint32_t r) { return PR[r].lo < 0 ? (int8_t)*PR[r].v : *PR[r].v; }   /* (TUNE: signed) */
static void pr_set(uint32_t r, int32_t v)
{
    v = clamp_i(v, PR[r].lo, PR[r].hi);
    if (r == PR_TEMPO) { if (S.tempo != v) proj_dirty = 1; tempo = (uint8_t)v; tempo_ch = EMPTY; }   /* (no undo step; heard at once) */
    *PR[r].v = (uint8_t)v;
}

static void slots_read(void)
{
    pj_hdr_t h[2];
    int ok[2], c;
    uint32_t n;
    for (n = 0; n < PJ_SLOTS; n++) {
        c = pj_cur(n, h, ok);
        slots[n].used = c >= 0;
        slots[n].len = c >= 0 ? h[c].len : 0u;
        str_cpy(slots[n].name, c >= 0 ? h[c].name : "", sizeof slots[n].name);
    }
}
static int slot_copy(uint32_t from, uint32_t to)   /* from's current copy into to's other one, read back, header last */
{
    pj_hdr_t h[2], d[2], w;
    int ok[2], dk[2], c = pj_cur(from, h, ok), dc = pj_cur(to, d, dk), t = dc < 0 ? 0 : !dc;
    uint32_t src, dst, a, end, crc;
    if (c < 0) return -1;
    src = pj_base(from, (uint32_t)c); dst = pj_base(to, (uint32_t)t);
    end = pj_at(h[c].len - 1u) + 1u;
    for (a = 0; a < end; a += sizeof pjs.page) {   /* (page 0: the header's room, written last) */
        if (!(a & 0xFFFu) && store_erase(dst + a)) return -1;
        if (a && (store_read(src + a, pjs.page, sizeof pjs.page) || store_write(dst + a, pjs.page, sizeof pjs.page))) return -1;
    }
    pjs_open(dst, h[c].len);
    while (pjs_get() >= 0) {}
    if (pjs.err || pjs.crc != h[c].crc) return -1;
    w = h[c];
    w.slot = (uint8_t)to;
    w.seq = dc < 0 ? 1u : d[dc].seq + 1u;
    if (w.ver == 2u) { crc = crc_upd(0, (const uint8_t *)&w, PJ_HDR2); memcpy(w.name, &crc, 4); }   /* (its CRC there) */
    else w.hcrc = crc_upd(0, (const uint8_t *)&w, sizeof w - 4u);
    if (store_write(dst, &w, w.ver == 2u ? PJ_HDR2 + 4u : sizeof w)) return -1;
    return pj_head(to, (uint32_t)t, &d[t]) ? 0 : -1;
}
static void proj_opened(uint32_t n, const char *what)   /* app.js openProject: slot n's project is the song now */
{
    char t[40];
    undo_reset();
    proj_dirty = 0;
    pj_slot = (uint8_t)n;
    settings_save(&pj_slot, 1);                 /* power-on reopens it (if this write fails: slot 00, nothing lost) */
    ui.song_row = ui.chain_row = ui.row = ui.ch = ui.ccol = ui.col = 0;
    if (ui.view != V_PROJ) { set_view(V_SONG); rows_clear(); }   /* (C P I..: gone) */
    ui.back = V_SONG;
    slots_read();
    str_cpy(t, what, sizeof t); fmt_hex(t + str_len(t), n, 2);
    say(t);
}
static void slot_do(uint32_t act, uint32_t n)   /* stopped: the flash may be written */
{
    char t[40];
    int err;
    uint32_t i;
    if (act == SA_LOAD) {
        if (!proj_load(n)) { proj_opened(n, "loaded "); return; }
        str_cpy(t, "load failed ", sizeof t);
    } else if (act == SA_NEW) {
        song_new(); instruments_new();
        for (i = 0; i < NINS; i++) ins_gen[i]++;   /* the parts reload them */
        err = proj_save(n);
        proj_opened(n, "new project ");
        if (!err) return;
        proj_dirty = 1;
        str_cpy(t, "save failed ", sizeof t);
    } else if (act == SA_DELETE) {
        err = store_erase(pj_base(n, 0)) | store_erase(pj_base(n, 1));   /* (their headers: both copies invalid) */
        if (n == pj_slot) proj_dirty = 1;       /* (the song stays: SAVE writes it back) */
        str_cpy(t, err ? "delete failed " : "deleted ", sizeof t);
    } else {
        err = pj_clip == pj_slot ? proj_save(n) : slot_copy(pj_clip, n);
        if (!err && n == pj_slot && !proj_load(n)) { proj_opened(n, "pasted to "); return; }
        str_cpy(t, err ? "paste failed " : "pasted to ", sizeof t);
    }
    fmt_hex(t + str_len(t), n, 2);
    slots_read();
    say(t);
}
static void slot_act(uint32_t act, uint32_t n)  /* app.js slotEdit's guard / confirmThen */
{
    int run = playing || play_req, loses = proj_dirty && act != SA_DELETE && (act != SA_PASTE || n == pj_slot);
    char h[3];
    pend.verb = act == SA_LOAD ? "load " : act == SA_NEW ? "start " : act == SA_DELETE ? "delete " : slots[n].used ? "replace " : "paste ";
    if (!run && !loses && act != SA_DELETE && !(act == SA_PASTE && slots[n].used)) { slot_do(act, n); return; }
    fmt_hex(h, n, 2);
    pend.t[0] = 0;
    if (run) { if (loses) sa(pend.t, "unsaved! "); sa(pend.t, "EDIT: "); sa(pend.t, pend.verb); sa(pend.t, h); sa(pend.t, " at phrase end"); }
    else if (loses) { sa(pend.t, "unsaved changes: EDIT to "); sa(pend.t, pend.verb); sa(pend.t, h); }
    else { sa(pend.t, pend.verb); sa(pend.t, h); sa(pend.t, "? EDIT to confirm"); }
    pend.act = (uint8_t)act; pend.slot = (uint8_t)n; pend.wait = 0;
    say_held(pend.t);
}
static void slot_confirm(void)                  /* the EDIT after slot_act's question */
{
    uint32_t c;
    char h[3];
    if (pend.act == SA_SCENE) {                 /* empty a scene (app.js sceneKey): at once, playing or not */
        char t[24];
        pend.act = 0;
        undo_mark(0, 0);                        /* (its own undo step) */
        memset(S.scn.k[pend.slot], EMPTY, sizeof S.scn.k[0]);
        scn_changed();
        str_cpy(t, "scene ", sizeof t); t[6] = (char)('1' + pend.slot); t[7] = 0; sa(t, " emptied");
        say(t);
        return;
    }
    if (!playing && !play_req) { c = pend.act; pend.act = 0; slot_do(c, pend.slot); return; }
    for (c = 0; c < NCH && !POS_ON(ch_pos[c]); c++) {}   /* the cursor's channel, else the first playing one */
    end_ch = (uint8_t)((POS_ON(ch_pos[ui.ch]) || c == NCH ? ui.ch : c) + 1u);
    pend.wait = 1;
    fmt_hex(h, pend.slot, 2);
    str_cpy(pend.t, pend.verb, sizeof pend.t); sa(pend.t, h); sa(pend.t, " at phrase end (PLAY: now)");
    say_held(pend.t);
}

static void proj_move(int32_t dx, int32_t dy)   /* app.js formMove on the G page's fields */
{
    uint32_t best;
    if (ui.prow == PR_NAME && dx) {
        int32_t p = (int32_t)ui.char_pos + dx;
        if (p >= 0 && p < (int32_t)NAME_LEN) { ui.char_pos = (uint32_t)p; return; }
    }
    best = form_move(&PR[0].x, sizeof PR[0], 0, PR_N, ui.prow, dx, dy, 0);
    if (best < PR_N) { ui.prow = best; if (best == PR_NAME) ui.char_pos = dx < 0 ? NAME_LEN - 1u : 0; }
}
static uint32_t proj_input(uint32_t keys, uint32_t down, int32_t dx, int32_t dy, uint32_t edit_up)   /* -> dirty bits */
{
    static uint32_t edit_t, edit_r;
    static nhold_t nh;
    uint32_t r = ui.prow, shift = keys >> PAD_SHIFT & 1u, opt = keys >> PAD_OPTION & 1u, edit = keys >> PAD_EDIT & 1u;
    if (r < PJ_SLOTS && down >> PAD_OPTION & 1u) {   /* SHIFT + OPTION marks a slot, OPTION copies it (app.js slotOption) */
        if (shift) { if (pj_sel != EMPTY) pj_sel = EMPTY; else { pj_sel = (uint8_t)r; say("select: OPTION copies"); } }
        else if (pj_sel != EMPTY) {
            char t[24];
            if (pj_sel != pj_slot && !slots[pj_sel].used) say("empty slot");
            else { pj_clip = pj_sel; str_cpy(t, "copied project ", sizeof t); fmt_hex(t + 15, pj_clip, 2); say(t); }
            pj_sel = EMPTY;
        }
    }
    if (r < PJ_SLOTS && down >> PAD_EDIT & 1u) {
        int dbl = fm1_ms - edit_t < 300u && edit_r == r;
        edit_t = dbl ? 0 : fm1_ms; edit_r = r;
        if (shift) { if (pj_clip == EMPTY) say("nothing copied"); else slot_act(SA_PASTE, r); }
        else if (opt) { if (slots[r].used) slot_act(SA_DELETE, r); }
        else if (!slots[r].used) { if (dbl) slot_act(SA_NEW, r); else say("empty slot: EDIT EDIT = new"); }
        else if (r == pj_slot && !proj_dirty) say("already open");
        else slot_act(SA_LOAD, r);
    } else if (r == PR_SYS) {                   /* the usage block: EDIT alone opens SYSTEM, nothing else edits it */
        if (down >> PAD_EDIT & 1u && !shift && !opt) { sys_open = 1; sys_pk = 0; sys_cut0 = shed_count; sys_shown = fm1_ms; }
    } else if (down >> PAD_EDIT & 1u && opt) {  /* OPTION + EDIT: the default (a letter: empty) */
        if (r == PR_NAME) name_clear(); else pr_set(r, PR[r].def);
    } else if (down >> PAD_EDIT & 1u && r == PR_NAME && !shift) {   /* a letter, as on the instrument page */
        name_down(&nh);
    }
    if (edit_up) name_up(&nh, ui.prow == PR_NAME);   /* written or changed: the next letter */
    if (dx || dy) {
        if (edit && r == PR_NAME) name_move(dx, dy);
        else if (edit && r >= PJ_SLOTS && r != PR_SYS) pr_set(r, pr_get(r) + (dx ? dx : -dy * PR[r].big));
        else if (!edit && !opt) proj_move(dx, dy);
    }
    return down || edit_up ? DIRTY_ALL : 0u;
}

static int tempo_live(void) { return playing && tempo_ch != EMPTY && !edit_down; }   /* (EDIT held: the stored value) */
static void proj_band(int32_t y0, int32_t h)    /* render.js viewProj between screen rows y0 and y0 + h */
{
    static const char *const LIST[4][3] = {{"OFF", "REC", "ON"}, {"OFF", "1BAR", "2BAR"}, {"ROW", "FEEL", ""}, {"NOTE", "AUDIO", ""}};
    uint32_t r, k, used_p = 0, used_c = 0, on;
    char s[16];
    for (r = 0; r < PJ_SLOTS; r++) {
        int32_t y = ROWS_Y + (int32_t)r * LH;
        const char *nm = !slots[r].used ? "--" : slots[r].name[0] ? slots[r].name : "UNTITLED";
        fmt_hex(s, r, 2);
        cv_str(gx(0), y, s, r % 4u ? P_LABEL : P_ROW4);
        if (r == ui.prow) cv_rect(gx(3) - 1, y, 13 * CW + 2, LH, P_ACCENT);
        else if (r == pj_sel) cv_rect(gx(3) - 1, y, (int32_t)str_len(nm) * CW + 2, LH, P_SEL);
        cv_str(gx(3), y, nm, r == ui.prow ? P_BG : slots[r].used ? P_TEXT : P_DIM);
    }
    cv_rect(gx(19) + CW / 2, TOP, 1, STRIP_Y - TOP, P_LINE);
    cv_str(gx(21), ROWS_Y, "NAME", P_LABEL);
    cv_name(gx(21), ROWS_Y + LH, S.pj.name, ui.prow == PR_NAME ? (int32_t)ui.char_pos : -1);
    for (r = 4; r < PR_SYS; r++) {              /* the values end at column 32 */
        int tl = r == PR_TEMPO && tempo_live();  /* TEMPO: the tempo heard now while a TPO sets it, in its channel's
                                                 * colour, a triangle while it ramps (param-values.md 4) */
        int32_t v = tl ? tempo : r == PR_NAME ? 0 : pr_get(r), y = ROWS_Y + PR[r].line * LH, x;   /* (NAME has no value:
                                                 * pr_get would read address 0, a fault on the chip) */
        if (r == PR_NAME) continue;
        cv_str(gx(21), y, PR[r].lbl, P_LABEL);
        if (r >= PR_LISTS) str_cpy(s, LIST[r - PR_LISTS][v], sizeof s);
        else {
            s[0] = PR[r].lo < 0 && v > 0 ? '+' : 0; s[1] = 0;   /* (TUNE) */
            sn_n(s, v, sizeof s);
            sa_n(s, PR[r].unit, sizeof s);
        }
        x = gx(32 - (int32_t)str_len(s));
        if (r == ui.prow) { cv_rect(x - 1, y, (int32_t)str_len(s) * CW + 2, LH, P_ACCENT); cv_str(x, y, s, P_BG); }
        else cv_str(x, y, s, tl ? P_CH[tempo_ch & 3u] : P_TEXT);
        if (tl && ramp_dir(tempo_ch & 3u, CMD_TPO)) cv_tri(gx(32) + 1, y, ramp_dir(tempo_ch & 3u, CMD_TPO), P_CH[tempo_ch & 3u]);
    }
    for (k = 0; k < NPHR; k++) used_p += !phrase_empty(k);
    for (k = 0; k < NCHN; k++) used_c += !chain_empty(k);
    on = ui.prow == PR_SYS;                     /* the usage block, a button (render.js viewProj) */
    cv_frame(gx(21) - 3, ROWS_Y + 11 * LH - 1, 12 * CW + 6, 3 * LH + 2, on ? P_ACCENT : P_LINE);
    str_cpy(s, "PHR ", sizeof s); sn_n(s, (int32_t)used_p, sizeof s); sa_n(s, "/128", sizeof s);
    cv_str(gx(21), ROWS_Y + 11 * LH, s, on ? P_TEXT : P_LABEL);
    str_cpy(s, "CHN ", sizeof s); sn_n(s, (int32_t)used_c, sizeof s); sa_n(s, "/64", sizeof s);
    cv_str(gx(21), ROWS_Y + 12 * LH, s, on ? P_TEXT : P_LABEL);
    k = (slots[pj_slot].len * 10u + 512u) / 1024u;   /* the open project as saved, packed (a copy holds 26 K) */
    str_cpy(s, "SIZE ", sizeof s);
    if (slots[pj_slot].used) {
        sn_n(s, (int32_t)(k / 10u), sizeof s); sa_n(s, ".", sizeof s);
        sn_n(s, (int32_t)(k % 10u), sizeof s); sa_n(s, "K", sizeof s);
    } else str_cpy(s + 5, "--", 3);
    cv_str(gx(21), ROWS_Y + 13 * LH, s, on ? P_TEXT : P_LABEL);
    if (sys_open) sys_draw();
}
static void draw_proj(void) { draw_bands(TOP, STRIP_Y - TOP, proj_band); }

/* ---------------------------------------------------------------- X, the mixer (render.js viewMix, app.js grids.mix) --- */
static void mix_band(int32_t y0, int32_t h)     /* the page between screen rows y0 and y0 + h */
{
    uint32_t k, j;
    char s[12];
    const char *unit;
    for (k = 0; k < NCH; k++) {
        int mk;
        int32_t lvl = sc_shown(SC_MIX, k, ch_tvl[k] >= 0 ? ch_tvl[k] : S.lvl[k], 0, &mk);   /* the level as the morph plays
                                                 * it / SEL edits it; under it a TVL's (the mix again at stop) */
        int tv = playing && ch_tvl[k] >= 0 && !sc_holds(SC_MIX, k) && !sel_held && !edit_down;   /* (the morph wins) */
        int32_t x = 2 + 8 * (int32_t)k, bx = gx(x + 1) + 2, bh = 10 * LH - 2, fh = (bh * lvl + 63) / 127;
        s[0] = (char)('1' + k); s[1] = 0;
        cv_str(gx(x + 2), ROWS_Y - LH, s, P_CH[k]);
        { char tl[4] = "TV1"; tl[2] = (char)('1' + k); cv_str(gx(x), ROWS_Y + 10 * LH, tl, P_LABEL); }   /* the command that moves it */
        cv_rect(bx, ROWS_Y, 2 * CW - 4, bh, P_SURF2);   /* the bar: lines 0..9, filled to the level (127 = full) */
        cv_rect(bx, ROWS_Y + bh - fh, 2 * CW - 4, fh, audible(k) ? P_CH[k] : P_DIM);
        cv_rect(bx - 2, ROWS_Y + bh - (bh * 112 + 63) / 127, 2 * CW, 1, P_TEXT);   /* 0 dB (level 112): a full bar is
                                                 * +7.5 dB, so unity needs its mark (user, 2026-10-11, principles-review.md #9) */
        for (j = 0; j < 3u; j++) {
            int32_t y = ROWS_Y + (j ? 12 + (int32_t)j : 11) * LH, n;
            if (j) str_cpy(s, j == 1u ? "MUTE" : "SOLO", sizeof s);   /* on = white, off = dim: no meaning in the case
                                                 * (user, 2026-10-11, principles-review.md #10) */
            else {
                char v[10];
                param_format(&TP[P_LEVEL], lvl, v, &unit);
                sa_n(v, unit, sizeof v);
                for (n = 0; n + (int32_t)str_len(v) < 6; n++) s[n] = ' ';   /* (padStart(6)) */
                str_cpy(s + n, v, sizeof s - (uint32_t)n);
            }
            n = (int32_t)str_len(s);
            if (k == ui.ch && j == ui.mf) { cv_rect(gx(x) - 1, y, n * CW + 2, LH, P_ACCENT); cv_str(gx(x), y, s, P_BG); }
            else if (!j && tv) cv_str(gx(x), y, s, P_CH[tvl_by[k]]);   /* a TVn moving it: the colour of the track that wrote it */
            else cv_str(gx(x), y, s, (j == 0u || (j == 1u ? heard_mute(k) : heard_solo(k))) && !(sel_held && (j || mk == SM_DIM)) ? P_TEXT : P_DIM);
        }
        cv_rect(gx(x), ROWS_Y + 12 * LH + 1, 6 * CW, 2, P_CH[k]);   /* KNOB n = track n's TVn (decisions.md "Knobs, take 3") */
        if (mk == SM_MORPH) cv_diamond(gx(x + 7), ROWS_Y + 11 * LH, P_TEXT);
        else if (tv && ramp_dir(tvl_by[k], PLAY_CMDS[PLAY_MXR + k])) cv_tri(gx(x + 7), ROWS_Y + 11 * LH, ramp_dir(tvl_by[k], PLAY_CMDS[PLAY_MXR + k]), P_CH[tvl_by[k]]);
        if (playing && POS_ON(shown_pos[k]) && chs[k].note) {   /* the note it plays */
            fmt_note(s, chs[k].note);
            cv_str(gx(x + 1), ROWS_Y + 15 * LH, s, P_LABEL);
        }
    }
}
static void draw_mix(uint32_t dirty)            /* line 15 alone: the notes as they play */
{
    int32_t y0 = dirty & ~(DIRTY_BAR | 1u << 15) ? TOP : ROWS_Y + 15 * LH;
    if (dirty & ~DIRTY_BAR) draw_bands(y0, STRIP_Y - y0, mix_band);
}
static void mix_flag(uint32_t c, int solo, int on)   /* MUTE / SOLO exclude each other on a track: one on turns the
                                                 * other off (user, unit feedback #17); the hand wins over a MUT / SOL (both)
                                                 * until the next one plays */
{
    (solo ? S.solo : S.mute)[c] = (uint8_t)on;
    if (on) (solo ? S.mute : S.solo)[c] = 0;
    mix_req |= (uint8_t)(0x11u << c);
}
static void mix_set(uint32_t c, uint32_t f, int32_t v)   /* a field to v (app.js grids.mix set);
                                                 * a level into the near scene (SEL, or that scene holds it) */
{
    v = clamp_i(v, 0, 127);
    if (f == 0u && sc_editing(SC_MIX, c)) sc_set(SC_MIX, c, v);
    else if (sel_held) say(f == 1u ? "scenes hold levels, not MUTE" : "scenes hold levels, not SOLO");
    else if (f == 0u) S.lvl[c] = (uint8_t)v;
    else mix_flag(c, f == 2u, v > 0);
}
static uint32_t mix_input(uint32_t keys, uint32_t down, int32_t dx, int32_t dy)   /* -> dirty bits */
{
    uint32_t f = ui.mf, c = ui.ch, edit = keys >> PAD_EDIT & 1u, opt = keys >> PAD_OPTION & 1u;
    int32_t v = f == 0u ? sc_value(SC_MIX, c, S.lvl[c]) : f == 1u ? heard_mute(c) : heard_solo(c);   /* (as heard) */
    if (down >> PAD_EDIT & 1u && !(keys >> PAD_SHIFT & 1u)) {
        if (opt && sel_held && f == 0u) sc_drop(SC_MIX, c);   /* SEL: out of the scene */
        else if (opt) mix_set(c, f, f ? 0 : 104);   /* OPTION + EDIT: the default */
        else if (f) mix_set(c, f, !v);          /* MUTE / SOLO: EDIT toggles */
    }
    if (dx || dy) {
        if (edit) mix_set(c, f, v + (dx ? dx : -dy * (f ? 1 : 8)));   /* level: +-1, +-8 */
        else if (!opt) {
            int32_t ch = (int32_t)c + dx, mf = (int32_t)f + dy;
            ui.ch = (uint32_t)clamp_i(ch, 0, (int32_t)NCH - 1);
            ui.mf = (uint32_t)clamp_i(mf, 0, 2);
        }
    }
    return down ? DIRTY_ALL : 0u;
}

/* ---------------------------------------------------------------- F, the effects (render.js viewFx, framedRows) --- */
static void fx_band(int32_t y0, int32_t h)      /* the page between screen rows y0 and y0 + h */
{
    static const char *const TITLE[3] = {"DELAY", "REVERB", "CHORUS"};
    uint32_t f, r, n = 0;
    for (r = 0; r < 3u; r++) group_frame(FX_Y[r], TITLE[r]);
    for (f = 0; f < 9u; f++) {
        char val[16];
        int mk;
        int32_t px = gx(FXF[f].x), y = FX_Y[FXF[f].row];
        if (FXF[f].row == FXF[ui.ff].row)       /* the knob bars under the cursor's row (decisions.md "Knobs, take 3") */
            cv_rect(px, FX_Y[FXF[f].row] + 2 * LH + 1, 7 * CW, 2, P_CH[n++]);
        param_text(&GP[FXF[f].g], sc_shown(SC_FX, f, S.fx[f], is_list(&GP[FXF[f].g]), &mk), val);   /* (through the scenes) */
        val[7] = 0;
        cv_str(px, y, FXF[f].lbl, P_LABEL);
        if (f == ui.ff) { cv_rect(px - 1, y + LH, (int32_t)str_len(val) * CW + 2, LH, P_ACCENT); cv_str(px, y + LH, val, P_BG); }
        else cv_str(px, y + LH, val, mk == SM_DIM ? P_DIM : P_TEXT);
        if (mk == SM_MORPH) cv_diamond(px + (int32_t)str_len(val) * CW + 1, y + LH, P_TEXT);
    }
}
static void draw_fx(void) { draw_bands(TOP, STRIP_Y - TOP, fx_band); }
static void fx_change(uint32_t f, int32_t steps)   /* into the near scene (SEL, or that scene holds it), else the song */
{
    int32_t v = param_turn(&GP[FXF[f].g], sc_value(SC_FX, f, S.fx[f]), steps);
    if (sc_editing(SC_FX, f)) sc_set(SC_FX, f, v);
    else S.fx[f] = (uint8_t)v;
}
static uint32_t fx_input(uint32_t keys, uint32_t down, int32_t dx, int32_t dy)   /* -> dirty bits */
{
    const param_desc_t *d = &GP[FXF[ui.ff].g];
    uint32_t f;
    if (down >> PAD_EDIT & 1u && keys >> PAD_OPTION & 1u && !(keys >> PAD_SHIFT & 1u)) {   /* the default (SEL: out of the scene) */
        if (sel_held) sc_drop(SC_FX, ui.ff);
        else if (sc_editing(SC_FX, ui.ff)) sc_set(SC_FX, ui.ff, d->def);
        else S.fx[ui.ff] = (uint8_t)d->def;
    }
    if (!dx && !dy) return down ? DIRTY_ALL : 0u;
    if (keys >> PAD_EDIT & 1u) {                /* a list: as its popup (down / right = on); a value: +-1, +-an eighth */
        int32_t big = (d->max - d->min + 4) / 8;
        fx_change(ui.ff, d->fmt == F_ENUM ? (dx ? 4 * dx : dy) : dx ? dx : -dy * (big < 1 ? 1 : big));
    } else if (!(keys >> PAD_OPTION & 1u)) {    /* app.js formMove: left / right along the row, up / down the nearest */
        f = form_move(&FXF[0].x, sizeof FXF[0], 0, 9u, ui.ff, dx, dx ? 0 : dy, 0);
        if (f < 9u) ui.ff = f;
    }
    return DIRTY_ALL;
}

/* ---------------------------------------------------------------- Q, the event view (docs/ux/event-view.md) --- */
/* Every phrase row instrument ui.inst plays in the song (a note or a command), once, in the order they first play
 * (events.js qItems: list B), its plays counted, its channels as dots. EDIT + arrows edit the row (every play of
 * it), OPTION + EDIT empties a note / sets a value to 00, an EDIT tap goes to the row in the phrase view. The
 * timeline above the list: q_timeline */
#define Q_Y (TOP + 1 + 5 * LH + 4)              /* the list's first line (events.js draw: the song strip, the bars
                                                 * ruler and 3 lanes above) */
#define Q_LINES ((uint32_t)(STRIP_Y - Q_Y) / LH)   /* events.js qListLines: 11 */
static uint32_t q_n, q_pending;
static uint32_t q_cmds(const row_t *w, uint8_t *k)   /* the line's commands (not the ramps): their FX columns */
{
    uint32_t j, n = 0;
    for (j = 0; j < NFX; j++) if (w->fx[j].c && !cmd_is_ramp(w->fx[j].c)) k[n++] = (uint8_t)j;
    return n;
}
static uint32_t q_has(const row_t *w)           /* the row passes SEQ's filter (events.js qItems) */
{
    uint32_t j;
    if (q_filt == Q_NOTE) return w->n != EMPTY;
    for (j = 0; j < NFX; j++) if (w->fx[j].c == q_filt) return 1;
    return 0;
}
/* every row the song plays (model.js playedRows: HOPs followed), channel by channel, each with its time t (rows from
 * the song's start) and the instrument it plays (the row's own, else the last one the channel set; EMPTY = none yet);
 * returns the longest channel's length (events.js instEvents: len) */
typedef void (*q_fn_t)(uint32_t ch, uint32_t sr, uint32_t cr, uint32_t ph, uint32_t r, uint32_t t, uint32_t ins);
static int32_t q_tr;                            /* the transpose of the chain row q_walk is in (U45: the pitch played) */
static uint32_t q_e;                            /* the row q_walk entered this phrase at */
static uint32_t q_pitch(const row_t *w) { int32_t n = (int32_t)w->n + q_tr; return (uint32_t)(n < 0 ? 0 : n > 127 ? 127 : n); }
static uint32_t q_walk(q_fn_t fn)
{
    uint32_t ch, sr, cr, r, len = 1;
    for (ch = 0; ch < NCH; ch++) {
        uint32_t cur = EMPTY, t = 0, from = 0;  /* (from: where the phrase before really left, U49) */
        for (sr = 0; sr < NSONG && S.song[sr][ch] != EMPTY; sr++) {
            uint32_t cn = S.song[sr][ch], ph;
            for (cr = 0; cr < 16u && (ph = S.chains[cn][cr].ph) != EMPTY; cr++) {
                uint32_t to = 15, hr, hto, next = 0;
                if (first_hop(S.phrases[ph], from, &hr, &hto)) { to = hr; next = hto; }
                q_tr = S.chains[cn][cr].tr;
                q_e = from;
                for (r = from; r <= to; r++, t++) {
                    if (S.phrases[ph][r].i != EMPTY) cur = S.phrases[ph][r].i;
                    fn(ch, sr, cr, ph, r, t, cur);
                }
                from = next;
            }
        }
        if (t > len) len = t;
    }
    return len;
}
static int q_event(uint32_t ph, uint32_t r, uint32_t ins)   /* a row of ui.inst with a note or a command */
{
    uint8_t k[NFX];
    const row_t *w = &S.phrases[ph][r];
    return ins == ui.inst && (w->n != EMPTY || q_cmds(w, k));
}
static void q_build_row(uint32_t ch, uint32_t sr, uint32_t cr, uint32_t ph, uint32_t r, uint32_t t, uint32_t ins)
{
    uint8_t k[NFX];
    const row_t *w = &S.phrases[ph][r];
    uint32_t at = ph << 4 | r, i;
    if (w->n == EMPTY && !q_cmds(w, k)) return;
    if (ins != EMPTY) q_played |= 1u << ins;
    if (ins != ui.inst || (q_filt && !q_has(w))) return;
    i = qh.q.at[at];
    if (i == EMPTY) {
        if (q_n == Q_MAX) return;
        i = q_n++;
        qh.q.at[at] = (uint8_t)i;
        qh.q.it[i].key = (uint16_t)at; qh.q.it[i].n = 0; qh.q.it[i].chs = 0; qh.q.it[i].t = 0xFFFFu;
    }
    qh.q.it[i].n++;
    qh.q.it[i].chs |= (uint8_t)(1u << ch);
    if (t < qh.q.it[i].t) { qh.q.it[i].t = (uint16_t)t; qh.q.it[i].at = (uint16_t)(ch << 10 | sr << 4 | cr); qh.q.it[i].e = (uint8_t)q_e; }
}
/* events.js listItems 'where': in time order, each row at its phrase's first pass: that pass's start + the rows past its
 * entry row (rows above the entry at the start, in row order), so a row the instrument only gets on a later pass or that
 * a HOP skipped keeps its place (unit feedback #43); same time: channel, row, phrase */
static uint16_t q_p0[NPHR] __attribute__((section(".pool")));   /* a phrase's first pass: its start (0xFFFF = none) */
static uint8_t q_pe[NPHR] __attribute__((section(".pool")));    /* ... and its entry row */
static uint32_t q_place(uint32_t i)             /* -> its time << 16 | channel << 12 | row << 8 | phrase */
{
    uint32_t ph = qh.q.it[i].key >> 4, r = qh.q.it[i].key & 15u;
    return (q_p0[ph] + (r > q_pe[ph] ? r - q_pe[ph] : 0u)) << 16 | (uint32_t)(qh.q.it[i].at >> 10) << 12 | r << 8 | ph;
}
static uint32_t q_len;                          /* the song's length in rows (the longest channel), set by q_build */
static void q_build(void)
{
    uint32_t a, b;
    q_n = q_played = 0;
    memset(qh.q.at, EMPTY, sizeof qh.q.at);
    q_len = q_walk(q_build_row);
    memset(q_p0, 0xFF, sizeof q_p0);
    for (a = 0; a < q_n; a++) {
        uint32_t ph = qh.q.it[a].key >> 4, t0 = qh.q.it[a].t - (qh.q.it[a].key & 15u) + qh.q.it[a].e;
        if (t0 < q_p0[ph]) { q_p0[ph] = (uint16_t)t0; q_pe[ph] = qh.q.it[a].e; }
    }
    for (a = 1; a < q_n; a++)
        for (b = a; b > 0 && q_place(b) < q_place(b - 1); b--) { qitem_t x = qh.q.it[b]; qh.q.it[b] = qh.q.it[b - 1]; qh.q.it[b - 1] = x; }
}
static row_t *q_row(uint32_t i) { return &S.phrases[qh.q.it[i].key >> 4][qh.q.it[i].key & 15u]; }
static uint32_t q_cell_cmd(void)                /* the cursor cell's command, 0 = the note cell or no line */
{
    uint8_t k[NFX];
    if (!ui.qc || ui.qr >= q_n) return 0;
    q_cmds(q_row(ui.qr), k);
    return q_row(ui.qr)->fx[k[ui.qc - 1u]].c;
}
static uint32_t q_cells(void) { uint8_t k[NFX]; return ui.qr < q_n ? 1u + q_cmds(q_row(ui.qr), k) : 1u; }
static void q_clamp(void)                       /* app.js qClamp: the cursor on a line and a cell that exist; the list
                                                 * scrolls only within a line of its edges */
{
    int32_t top = (int32_t)ui.qt, n = (int32_t)q_n;
    if (ui.qr >= q_n) ui.qr = q_n ? q_n - 1u : 0u;
    if (ui.qc >= q_cells()) ui.qc = q_cells() - 1u;
    if ((int32_t)ui.qr < top + 1) top = (int32_t)ui.qr - 1;
    if ((int32_t)ui.qr > top + (int32_t)Q_LINES - 2) top = (int32_t)ui.qr - (int32_t)Q_LINES + 2;
    if (top > n - (int32_t)Q_LINES) top = n - (int32_t)Q_LINES;
    ui.qt = (uint32_t)(top < 0 ? 0 : top);
}
static void open_q(uint32_t i)                  /* app.js openQ: another instrument's list from the top */
{
    if (i != ui.inst) {
        ui.qr = ui.qc = ui.qw = ui.qt = 0;
        ui.inst = i;
        if (q_want[0]) {                        /* the filter follows by name (another engine's CUT); one without it:
                                                 * every line, and it comes back on the next one that has it (user) */
            q_filt = cmd_by_name(q_want, i);
            if (q_filt) q_build();
            if (!q_filt || !q_n) {
                char t[40] = "no ";
                q_filt = 0;
                sa(t, q_want); sa(t, " here: every line");
                say(t);
            }
        }
    }
    set_view(V_Q);
    ui.on_row = ui.on_head = 0;
    sel.on = mark.on = 0;
    q_build();
    q_clamp();
}
/* the playhead in song time (app.js liveState qPlayT, events.js tOf): the row t of the first channel whose position
 * the song walk meets; NONE32 = none (stopped, or a chain / phrase playing) */
#define NONE32 0xFFFFFFFFu
static uint32_t q_pt = NONE32;
static struct { uint32_t key, t, at; } qn;      /* q_next's search */
static void q_pt_row(uint32_t ch, uint32_t sr, uint32_t cr, uint32_t ph, uint32_t r, uint32_t t, uint32_t ins)
{
    uint32_t p = shown_pos[ch];
    (void)ph; (void)ins;
    if (q_pt == NONE32 && POS_ON(p) && POS_SROW(p) == sr && POS_CROW(p) == cr && POS_ROW(p) == r) q_pt = t;
}
static void q_next_row(uint32_t ch, uint32_t sr, uint32_t cr, uint32_t ph, uint32_t r, uint32_t t, uint32_t ins)
{
    if ((ph << 4 | r) == qn.key && ins == ui.inst && t >= q_pt && t < qn.t) { qn.t = t; qn.at = ch << 10 | sr << 4 | cr; }
}
static uint32_t q_next(uint32_t *t)             /* events.js nextPlay: the cursor line's first play at or after the
                                                 * playhead, else its first (time, then channel): ch << 10 | sr << 4 | cr */
{
    qn.key = qh.q.it[ui.qr].key; qn.t = NONE32;
    if (q_pt != NONE32) q_walk(q_next_row);    /* shortcut: a song walk per main-loop pass while the song plays (the
                                                 * bottom line); cache it if the device load item shows it */
    if (qn.t == NONE32) { qn.t = qh.q.it[ui.qr].t; qn.at = qh.q.it[ui.qr].at; }
    *t = qn.t;
    return qn.at;
}
static uint32_t q_from(void)                    /* PLAY in Q: the song row where the cursor's line plays first */
{
    return ui.qr < q_n ? qh.q.it[ui.qr].at >> 4 & 63u : ui.song_row;
}
static void q_where(char *s)                    /* the bottom line: where the cursor's line plays next (events.js draw) */
{
    uint32_t t, at;
    s[0] = 0;
    if (ui.qr >= q_n) return;
    at = q_next(&t);
    sa(s, "bar "); sn(s, (int32_t)(t / 16u + 1u));
    sa(s, " S"); fmt_hex(s + str_len(s), at >> 4 & 63u, 2);
    sa(s, " CH"); sn(s, (int32_t)(at >> 10) + 1);
    if (qh.q.it[ui.qr].n > 1u) { sa(s, ", plays "); sn(s, qh.q.it[ui.qr].n); sa(s, " times"); }
}
static void q_help(char *s)                     /* EDIT held (app.js helpState): the note's place, or the value explained */
{
    uint8_t k[NFX];
    row_t *w;
    s[0] = 0;
    if (ui.qr >= q_n) return;
    w = q_row(ui.qr);
    q_cmds(w, k);
    sa(s, "SEQ: ");                             /* the filter's cue first (principles-review.md #22); the value's
                                                 * `CUT 40 = ` and the note's plays (the bottom line's) make room */
    if (q_filt) sa(s, "all again");
    else if (ui.qc == 0u) sa(s, "only notes");
    else { char nm[4]; cmd_name(w->fx[k[ui.qc - 1u]].c, ui.inst, nm); sa(s, "only "); sa(s, nm); }
    if (ui.qc == 0u) {
        sa(s, ", note of P"); fmt_hex(s + str_len(s), qh.q.it[ui.qr].key >> 4, 2);
        sa(s, " row "); fmt_hex(s + str_len(s), qh.q.it[ui.qr].key & 15u, 1);
    } else {
        fx_t *f = &w->fx[k[ui.qc - 1u]];
        char t[40];
        uint32_t j;
        cmd_value_text(f->c, ui.inst, f->v, t);
        for (j = 0; t[j] && !(t[j] == '=' && t[j + 1] == ' '); j++) ;
        if (t[j]) { sa(s, ", "); sa(s, t + j + 2); }   /* (cut at 39) */
    }
}
static void q_seq(void)                         /* SEQ in Q (app.js seqButton): the filter, or every line again; the
                                                 * cursor stays on its line */
{
    uint32_t key = ui.qr < q_n ? qh.q.it[ui.qr].key : 0xFFFFu, i;
    sel.on = 0;                                 /* (SEQ drops the line selection) */
    if (q_filt || q_want[0]) q_filt = q_want[0] = 0;
    else if (ui.qr < q_n) {
        q_filt = ui.qc ? q_cell_cmd() : Q_NOTE;
        if (ui.qc) cmd_name(q_filt, ui.inst, q_want);
    } else return;
    q_build();
    for (i = 0; i < q_n && qh.q.it[i].key != key; i++) ;
    ui.qr = i < q_n ? i : 0;
    q_clamp();
}
/* the timeline over the list (events.js draw, final: layout 'focus', ruler 'bars'; step Q-2), biggest scale first:
 * SONG = the whole song squeezed into 128 px, a tick every 8 bars (a page), where the instrument plays, this page
 * framed; BARS = the 8-bar page (1 px a row, never stretched: a page ends where the song does, the SONG strip is as long
 * as the song up to 128 px; user, 2026-10-11); then three lanes over
 * the page: NOTE (a dot at its pitch), the focus parameter's curve (the cursor cell's sound command, else the line's
 * first, else the first the song uses; held, ramps followed), REST (a tick per other command, a ramp's span dim). The
 * cursor line's plays at full channel colour, the rest at 35 %. The playhead and the page following it come with
 * PLAY in Q (step Q-3) */
#define TL_X (MAP_X - 8 - 128)
#define TL_SY (TOP + 1 + 4)                     /* the SONG strip's line */
#define TL_LANE(n) (TOP + 1 + (2 + (n)) * LH)   /* NOTE, the focus curve, REST */
/* the channel colours and the dim grey at 35 %, as the prototype computes them (its 565 colour x 0.35, rounded in
 * floating point: 90 x 0.35 = 31.4999.. rounds down) */
static const uint16_t P_CH35[NCH] = {RGB(0x1a, 0x39, 0x59), RGB(0x1f, 0x4a, 0x25), RGB(0x56, 0x4a, 0x1a), RGB(0x17, 0x4b, 0x51)};
#define P_DIM35 RGB(0x1d, 0x1e, 0x1d)
static struct { uint32_t t0, span, len, slen, lo, hi, focus, ut, uc, pt; uint16_t sel, pkey; uint8_t pk; } tl;   /* slen:
                                                 * the rows SONG's 128 px stand for */
static struct { uint16_t key, first; uint8_t k, ch; } tl_set[128] __attribute__((section(".pool")));   /* the focus
                                                 * command set on each row of the page: the last one (it wins) and the
                                                 * first channel's (its colour); key 0xFFFF = none */
static int32_t tl_x(uint32_t t) { return TL_X + (int32_t)(t - tl.t0); }
static int tl_in(uint32_t t) { return t >= tl.t0 && t < tl.t0 + tl.span; }
static int tl_cha(const row_t *w) { uint32_t j; for (j = 0; j < NFX; j++) if (w->fx[j].c == CMD_CHA) return 1; return 0; }
static uint16_t tl_hue(uint32_t key, uint32_t ch, int cha)   /* events.js hue: the cursor line's plays at full colour */
{
    int mine = key == tl.sel;
    return cha ? (mine ? P_DIM : P_DIM35) : mine ? P_CH[ch] : P_CH35[ch];
}
static void tl_scan(uint32_t ch, uint32_t sr, uint32_t cr, uint32_t ph, uint32_t r, uint32_t t, uint32_t ins)
{
    const row_t *w = &S.phrases[ph][r];
    uint32_t j;
    (void)ch; (void)sr; (void)cr;
    if (!q_event(ph, r, ins)) return;
    if (w->n < NOTE_OFF) { uint32_t n = q_pitch(w); if (n < tl.lo) tl.lo = n; if (n > tl.hi) tl.hi = n; }
    if (tl.uc && t >= tl.ut) return;            /* the first sound command the song uses (time, then channel) */
    for (j = 0; j < NFX; j++) if (w->fx[j].c >= CMD_SOUND) { tl.uc = w->fx[j].c; tl.ut = t; return; }
}
static void tl_draw(uint32_t ch, uint32_t sr, uint32_t cr, uint32_t ph, uint32_t r, uint32_t t, uint32_t ins)
{                                               /* the SONG strip's marks, the notes, the focus sets, the ramps' spans */
    const row_t *w = &S.phrases[ph][r];
    uint8_t k[NFX];
    uint32_t j, n, key = ph << 4 | r, r1;
    int endless, cha = tl_cha(w);
    (void)sr; (void)cr;
    if (!q_event(ph, r, ins)) return;
    cv_rect(TL_X + (int32_t)(t * 128u / tl.slen), TL_SY + 1, 1, 1, P_LABEL);
    if (w->n != EMPTY && tl_in(t)) {
        int32_t y = TL_LANE(0);
        if (w->n == NOTE_OFF) cv_rect(tl_x(t), y + LH - 2, 1, 1, P_LABEL);
        else cv_rect(tl_x(t), y + 1 + (tl.hi == tl.lo ? 4 : rdiv((int32_t)(tl.hi - q_pitch(w)) * (LH - 4), (int32_t)(tl.hi - tl.lo))), 2, 2,
                     tl_hue(key, ch, cha));
    }
    n = q_cmds(w, k);
    for (j = 0; j < n; j++) {
        uint32_t c = w->fx[k[j]].c;
        if (c == tl.focus) {
            if (t < tl.t0) { if (tl.pkey == 0xFFFFu || t >= tl.pt) { tl.pt = t; tl.pkey = (uint16_t)key; tl.pk = k[j]; } }
            else if (tl_in(t)) {
                if (tl_set[t - tl.t0].key == 0xFFFFu) { tl_set[t - tl.t0].first = (uint16_t)key; tl_set[t - tl.t0].ch = (uint8_t)ch; }
                tl_set[t - tl.t0].key = (uint16_t)key; tl_set[t - tl.t0].k = k[j];
            }
        } else if (ramp_span(S.phrases[ph], r, k[j], &r1, &endless)) {
            uint32_t a = t > tl.t0 ? t : tl.t0, b = endless ? tl.len : t + r1 - r;
            if (b > tl.t0 + tl.span) b = tl.t0 + tl.span;
            if (b > a) { int32_t xa = tl_x(a), wd = tl_x(b) - xa; cv_rect(xa, TL_LANE(2) + LH / 2, wd < 1 ? 1 : wd, 1, P_DIM); }
        }
    }
}
static void tl_marks(uint32_t ch, uint32_t sr, uint32_t cr, uint32_t ph, uint32_t r, uint32_t t, uint32_t ins)
{                                               /* REST: a tick per command other than the focus one (over the spans) */
    const row_t *w = &S.phrases[ph][r];
    uint8_t k[NFX];
    uint32_t j, n;
    (void)sr; (void)cr;
    if (!tl_in(t) || !q_event(ph, r, ins)) return;
    n = q_cmds(w, k);
    for (j = 0; j < n; j++)
        if (w->fx[k[j]].c != tl.focus) cv_rect(tl_x(t), TL_LANE(2) + 3, 1, LH - 6, tl_hue(ph << 4 | r, ch, tl_cha(w)));
}
static int32_t tl_py(uint32_t v, int32_t top)    /* events.js curve's norm: a list's item / its last, else v / 255 */
{
    uint32_t items, den = 255;
    if (cmd_desc(tl.focus, ui.inst, &items) && items) { den = items > 1u ? items - 1u : 1u; if (v > den) v = den; }
    return top + rdiv((int32_t)(den - v) * (LH - 3), (int32_t)den);
}
static int tl_ramp(uint32_t key, uint32_t k, uint32_t t0, uint32_t t, uint32_t *v)   /* a set on row t0, at row t: ramped
                                                 * up to where a command below stops it (U43); -1 no ramp, 0 its last value */
{
    const row_t *rows = S.phrases[key >> 4];
    uint32_t n = ramp_stop(rows, key & 15u, k) - (key & 15u), dt = t - t0 < n ? t - t0 : n;
    ramp_t sp;
    int done;
    if (!ramp_spec(rows, key & 15u, k, &sp)) { *v = rows[key & 15u].fx[k].v; return -1; }
    sp.ticks = dt * TICKS;
    *v = (uint32_t)ramp_value(&sp, &done);
    return !done && dt < n;
}
static uint32_t tl_val(uint32_t key, uint32_t k, uint32_t t0, uint32_t t) { uint32_t v; tl_ramp(key, k, t0, t, &v); return v; }
static void q_timeline(void)
{
    char s[16];
    uint32_t i, bar, every = 8, a, b, mt, len, v, rk = 0xFFFFu, rkk = 0, rt = 0;   /* rk: the ramp moving the curve */
    int32_t x, sw, py0 = -1, top = TL_LANE(1) + 1;
    uint8_t k[NFX];
    tl.len = len = q_len;
    tl.slen = len > 128u ? len : 128u;
    tl.t0 = (q_pt != NONE32 ? q_pt : q_n ? qh.q.it[ui.qr].t : 0u) / 128u * 128u;   /* the page: the playhead's, else the
                                                 * cursor line's first play */
    tl.span = len - tl.t0 < 128u ? len - tl.t0 : 128u;
    tl.sel = q_n ? qh.q.it[ui.qr].key : 0xFFFFu;
    tl.lo = 999; tl.hi = 0; tl.uc = 0; tl.pkey = 0xFFFFu;
    q_walk(tl_scan);
    tl.focus = tl.uc;                           /* the cursor cell's sound command, else the line's first, else the song's */
    if (q_n) {
        row_t *w = q_row(ui.qr);
        uint32_t n = q_cmds(w, k), c = ui.qc ? w->fx[k[ui.qc - 1u]].c : 0u;
        for (i = n; i-- > 0;) if (w->fx[k[i]].c >= CMD_SOUND) tl.focus = w->fx[k[i]].c;
        if (c >= CMD_SOUND) tl.focus = c;
    }
    /* SONG: the range in bars (review #12), the line, a tick a page (or 2, 4 ... on a long song), this page framed */
    str_cpy(s, "SONG 1-", sizeof s); sn(s, (int32_t)((len + 15u) / 16u));
    cv_str(X0, TOP + 1, s, P_LABEL);
    sw = (int32_t)((len * 128u + tl.slen - 1u) / tl.slen);
    cv_rect(TL_X, TL_SY + 1, sw, 1, P_LINE);
    while (every * 16u * 128u < 8u * tl.slen) every *= 2u;
    for (bar = 0; bar * 16u <= len; bar += every) { x = TL_X + (int32_t)(bar * 16u * 128u / tl.slen); cv_rect(x > TL_X + sw - 1 ? TL_X + sw - 1 : x, TL_SY, 1, 3, P_DIM); }
    /* BARS: the page's range and its ruler, a tick a bar, a number every 4 */
    str_cpy(s, "BARS ", sizeof s); sn(s, (int32_t)(tl.t0 / 16u + 1u)); sa(s, "-"); sn(s, (int32_t)(tl.t0 / 16u + (tl.span + 15u) / 16u));
    cv_str(X0, TOP + 1 + LH, str_len(s) <= (uint32_t)((TL_X - X0) / CW - 1) ? s : s + 5, P_LABEL);
    for (bar = 0; bar < (tl.span + 15u) / 16u; bar++) {   /* the page's bars: 8, fewer where the song ends */
        x = TL_X + (int32_t)bar * 16;
        cv_rect(x, TOP + 1 + 2 * LH - 5, 1, 2, P_DIM);
        if (bar % 4u == 0u) { s[0] = 0; sn(s, (int32_t)(tl.t0 / 16u + 1u + bar)); cv_str(x, TOP + LH, s, P_LABEL); }
    }
    cv_str(X0, TL_LANE(0), "NOTE", P_LABEL);
    if (tl.focus) cmd_name(tl.focus, ui.inst, s); else str_cpy(s, "---", sizeof s);
    cv_str(X0, TL_LANE(1), s, P_TEXT);
    cv_str(X0, TL_LANE(2), "REST", P_LABEL);
    for (i = 0; i < 128u; i++) tl_set[i].key = 0xFFFFu;
    q_walk(tl_draw);
    q_walk(tl_marks);
    mt = tl.t0 + tl.span;                       /* the page framed on SONG, over its marks */
    a = (uint32_t)TL_X + tl.t0 * 128u / tl.slen; b = (uint32_t)TL_X + (mt * 128u + tl.slen - 1u) / tl.slen - 1u;
    b = b - a + 1u < 3u ? a + 2u : b;
    cv_frame((int32_t)a, TL_SY - 2, (int32_t)(b - a + 1u), 7, P_ACCENT);
    if (q_pt != NONE32) cv_rect(TL_X + (int32_t)(q_pt * 128u / tl.slen), TL_SY - 1, 1, 5, P_PLAY);   /* the playhead on SONG */
    v = tl.focus ? cmd_encode(tl.focus, ui.inst) : 0u;   /* the focus curve: the instrument's own value, then the sets;
                                                 * as the player, a ramp goes on until a set in its own column (U43) */
    if (tl.pkey != 0xFFFFu && tl_ramp(tl.pkey, tl.pk, tl.pt, tl.pt, &v) >= 0) { rk = tl.pkey; rkk = tl.pk; rt = tl.pt; }
    for (x = TL_X; tl.focus && x < TL_X + (int32_t)tl.span; x++) {
        uint32_t t = tl.t0 + (uint32_t)(x - TL_X);
        int32_t py;
        i = t - tl.t0;
        if (tl_set[i].key != 0xFFFFu) {
            if (tl_ramp(tl_set[i].key, tl_set[i].k, t, t, &v) >= 0) { rk = tl_set[i].key; rkk = tl_set[i].k; rt = t; }
            else if (rk != 0xFFFFu && tl_set[i].k == rkk) rk = 0xFFFFu;
        }
        if (rk != 0xFFFFu && t > rt && tl_ramp(rk, rkk, rt, t, &v) <= 0) rk = 0xFFFFu;
        py = tl_py(v, top);
        if (py0 >= 0) cv_rect(x, py < py0 ? py : py0, 1, (py < py0 ? py0 - py : py - py0) + 1, P_TEXT);
        else cv_rect(x, py, 1, 1, P_ROW4);
        py0 = py;
    }
    for (i = 0; tl.focus && i < tl.span; i++)   /* the sets, in their channel's colour */
        if (tl_set[i].key != 0xFFFFu)
            cv_rect(tl_x(tl.t0 + i), tl_py(tl_val(tl_set[i].key, tl_set[i].k, tl.t0 + i, tl.t0 + i), top) - 1, 2, 2,
                    tl_hue(tl_set[i].first, tl_set[i].ch, 0));
    if (q_pt != NONE32 && tl_in(q_pt)) cv_rect(tl_x(q_pt), TOP + 1 + LH, 1, 4 * LH, P_PLAY);   /* the playhead through
                                                 * the page, from its ruler */
}
static void q_band(int32_t y0, int32_t h)       /* events.js draw (layout 'focus', list 'where') between rows y0, y0 + h */
{
    uint32_t l;
    if (y0 < Q_Y - 2) q_timeline();
    cv_rect(X0, Q_Y - 3, MAP_X - 8 - X0, 1, P_LINE);   /* under the timeline */
    if (!q_n && q_filt && q_played >> ui.inst & 1u) {   /* (the filter left nothing: an edit emptied its lines) */
        char t[16] = "no ";
        if (q_filt == Q_NOTE) sa(t, "NOTE"); else cmd_name(q_filt, ui.inst, t + 3);
        sa(t, " here");
        cv_str(X0, Q_Y, t, P_DIM);
    } else if (!q_n) cv_str(X0, Q_Y, "not played in the song", P_DIM);
    for (l = 0; l < Q_LINES && ui.qt + l < q_n; l++) {
        uint32_t i = ui.qt + l, ph = qh.q.it[i].key >> 4, r = qh.q.it[i].key & 15u, j, n, ch;
        int32_t yy = Q_Y + (int32_t)l * LH, x = X0 + 6 * CW;
        const row_t *w = &S.phrases[ph][r];
        uint16_t col = P_TEXT;
        uint8_t k[NFX];
        char s[12];
        if (yy + LH <= y0 || yy >= y0 + h) continue;
        if (sel.on && (i >= sel.r || i >= ui.qr) && (i <= sel.r || i <= ui.qr)) cv_rect(X0 - 2, yy, MAP_X - 8 - X0 + 2, LH, P_SEL);   /* selected */
        else if (i == ui.qr) cv_rect(X0 - 2, yy, MAP_X - 8 - X0 + 2, LH, P_BAND);
        for (ch = 0; ch < NCH; ch++) if (qh.q.it[i].chs >> ch & 1u) cv_rect(X0 + (int32_t)ch * 3, yy + 5, 2, 2, P_CH[ch]);
        s[0] = 'P'; fmt_hex(s + 1, ph, 2);
        cv_str(X0 + 2 * CW, yy, s, P_LABEL);
        s[0] = HEX[r]; s[1] = 0;
        cv_str(x, yy, s, P_LABEL);
        n = q_cmds(w, k);
        for (j = 0; j < NFX; j++) if (w->fx[j].c == CMD_CHA) col = P_LABEL;   /* a chance row: dimmer */
        for (j = 0; j <= n; j++) {              /* the cells: the note, then the commands */
            int32_t cx = x + (j ? 6 + 6 * ((int32_t)j - 1) : 2) * CW;
            uint16_t c = j == 0u && w->n == EMPTY ? P_DIM : col;
            ramp_t sp;
            if (j == 0u) fmt_note(s, w->n);
            else {
                cmd_name(w->fx[k[j - 1u]].c, ui.inst, s);
                fmt_hex(s + 3, w->fx[k[j - 1u]].v, 2);
                if (ramp_spec(S.phrases[ph], r, k[j - 1u], &sp)) { s[5] = '~'; s[6] = 0; }
            }
            if (i == ui.qr && j == ui.qc) { cv_rect(cx - 1, yy, (int32_t)str_len(s) * CW + 2, LH, P_ACCENT); cv_str(cx, yy, s, P_BG); }
            else cv_str(cx, yy, s, c);
        }
        if (qh.q.it[i].n > 1u) {                   /* x12, right-aligned in 4 */
            char t[8] = "x";
            fmt_int(t + 1, qh.q.it[i].n);
            cv_str(MAP_X - 12 - (int32_t)str_len(t) * CW, yy, t, P_LABEL);
        }
    }
    if (q_n > Q_LINES) {                        /* the scroll position: a thin bar right of the list */
        int32_t hh = (int32_t)Q_LINES * LH, bh = hh * (int32_t)Q_LINES / (int32_t)q_n, by;
        bh = bh < 4 ? 4 : bh;
        by = rdiv((hh - bh) * (int32_t)ui.qt, (int32_t)(q_n - Q_LINES));
        cv_rect(MAP_X - 10, Q_Y, 1, hh, P_LINE);
        cv_rect(MAP_X - 10, Q_Y + by, 1, bh, P_LABEL);
    }
}
static void draw_q(int top)                     /* top: only the band holding the timeline (the playhead moved) */
{
    q_build();
    q_clamp();
    draw_bands(TOP, top ? (Q_Y - 2 - TOP + CV_ROWS - 1) / CV_ROWS * CV_ROWS : STRIP_Y - TOP, q_band);   /* top: the bands
                                                 * that start above the list */
}
static void q_jump(void)                        /* app.js qJump: the line's row in the phrase view, the same cell, where
                                                 * it plays first (playing: the next play after the playhead) */
{
    uint8_t k[NFX];
    uint32_t at, t;
    if (ui.qr >= q_n) return;
    at = q_next(&t);
    q_cmds(q_row(ui.qr), k);
    ui.ch = at >> 10; ui.song_row = at >> 4 & 63u; ui.chain_row = at & 15u; ui.row = qh.q.it[ui.qr].key & 15u;
    ui.col = ui.qc ? 4u + 2u * k[ui.qc - 1u] : 0u;
    set_view(V_PHRASE);
}
/* EDIT + arrows on selected lines (app.js qChangeBlock): the cursor cell's command (or the note) moves on every
 * selected line that has it, wherever it sits in the line (user, 2026-10-10); empty stays empty; reset = OPTION + EDIT:
 * back to 00 / empty */
static void q_block(int32_t d, int32_t big, int reset)
{
    uint8_t k[NFX];
    uint32_t i, j, n, a = sel.r < ui.qr ? sel.r : ui.qr, b = sel.r < ui.qr ? ui.qr : sel.r, target = q_cell_cmd();
    if (ui.qr >= q_n) return;
    for (i = a; i <= b && i < q_n; i++) {
        row_t *w = q_row(i);
        if (!target) {
            int32_t v = (int32_t)w->n + d * (big ? 12 : 1);
            if (w->n == EMPTY || w->n == NOTE_OFF) continue;
            if (reset) w->n = EMPTY;
            else if (v >= (int32_t)NOTE_MIN && v <= (int32_t)NOTE_MAX) w->n = (uint8_t)v;   /* (notes keep their name at
                                                 * the range ends) */
        } else {
            uint32_t mx = cmd_value_max(target, ui.inst);
            int32_t v;
            fx_t *f;
            n = q_cmds(w, k);
            for (j = 0; j < n && w->fx[k[j]].c != target; j++) ;
            if (j == n) continue;
            f = &w->fx[k[j]];
            v = reset ? 0 : (int32_t)f->v + d * (big && mx == 255u ? 16 : 1);
            f->v = (uint8_t)clamp_i(v, 0, (int32_t)mx);
        }
    }
}
static uint32_t q_desel;                        /* a plain OPTION with lines selected (Q) or a mark (S C P): they drop
                                                 * when it is let go, unless it was a modifier (OPTION + arrows keep a
                                                 * mark: unit-feedback #22; app.js deselect) */
static uint32_t q_input(uint32_t keys, uint32_t down, int32_t dx, int32_t dy, uint32_t edit_up)   /* -> dirty bits */
{
    uint32_t edit = keys >> PAD_EDIT & 1u, dirty = 0;
    uint8_t k[NFX];
    q_build();
    if (down & PAD_KEYS & ~(1u << PAD_OPTION)) q_desel = 0;
    if (down >> PAD_OPTION & 1u) {              /* SHIFT + OPTION selects lines (to change them together); no copy /
                                                 * paste: the lines are rows of different phrases */
        if (keys >> PAD_SHIFT & 1u) { if (sel.on) sel.on = 0; else { sel.on = 1; sel.r = ui.qr; say("select lines: EDIT+arrows change them"); } }
        else if (sel.on) q_desel = 1;
        dirty = DIRTY_ALL;
    }
    if (q_desel && !(keys >> PAD_OPTION & 1u)) { q_desel = sel.on = 0; say("deselected"); dirty = DIRTY_ALL; }
    if (down >> PAD_EDIT & 1u) {
        q_pending = !(keys >> PAD_SHIFT & 1u) && !(keys >> PAD_OPTION & 1u) && q_n && !sel.on;
        if (keys >> PAD_OPTION & 1u && sel.on) q_block(0, 0, 1);
        else if (keys >> PAD_OPTION & 1u && ui.qr < q_n) {   /* OPTION + EDIT: the default, every play of the row */
            row_t *w = q_row(ui.qr);
            if (ui.qc == 0u) w->n = EMPTY;
            else { q_cmds(w, k); w->fx[k[ui.qc - 1u]].v = 0; last.val = 0; }
        }
        dirty = DIRTY_ALL;
    }
    if (dx || dy) {
        q_pending = 0;
        if (edit && sel.on) q_block(dx ? dx : -dy, !dx, 0);
        else if (edit && ui.qr < q_n) {         /* EDIT + arrows: the row, every play of it (app.js GRID.q); the value
                                                 * is stored as the column's last (edit-defaults.md) */
            row_t *w = q_row(ui.qr);
            int32_t d = dx ? dx : -dy, big = !dx;
            if (ui.qc == 0u) {
                int32_t v = w->n;
                if (v == EMPTY) v = last.note;  /* (app.js insertLast: the last note) */
                else if (v == NOTE_OFF || (big && (v + d * 12 < (int32_t)NOTE_MIN || v + d * 12 > (int32_t)NOTE_MAX))) v = -1;
                else { v += d * (big ? 12 : 1); v = clamp_i(v, (int32_t)NOTE_MIN, (int32_t)NOTE_MAX); }
                if (v >= 0) { w->n = (uint8_t)v; last.note = (uint8_t)v; last.ni = NONE; blip(ui.ch, (uint32_t)v, ui.inst); }
            } else {
                fx_t *f;
                uint32_t mx;
                int32_t v;
                q_cmds(w, k);
                f = &w->fx[k[ui.qc - 1u]];
                mx = cmd_value_max(f->c, ui.inst);
                v = (int32_t)f->v + d * (big && mx == 255u ? 16 : 1);
                f->v = (uint8_t)clamp_i(v, 0, (int32_t)mx);
                last.val = f->v;
            }
        } else if (keys >> PAD_OPTION & 1u && !sel.on) {   /* app.js optionMove: left / right = the previous / next instrument
                                                 * the song plays (unused ones skipped), up / down = the previous /
                                                 * next phrase in the list */
            if (dx) {
                int32_t i;
                for (i = (int32_t)ui.inst + dx; i >= 0 && i < (int32_t)NINS; i += dx)
                    if (q_played >> i & 1u) { open_q((uint32_t)i); break; }
            } else if (ui.qr < q_n) {
                uint32_t i = ui.qr, ph = qh.q.it[i].key >> 4;
                if (dy > 0) { while (i < q_n && qh.q.it[i].key >> 4 == ph) i++; }
                else {
                    while (i > 0 && qh.q.it[i - 1u].key >> 4 == ph) i--;
                    if (i > 0) { i--; ph = qh.q.it[i].key >> 4; while (i > 0 && qh.q.it[i - 1u].key >> 4 == ph) i--; }
                }
                if (i < q_n) ui.qr = i;
                q_clamp();
            }
        } else {                                /* the cursor: a shorter line pulls it back, the next line long
                                                 * enough puts it on the cell picked with left / right again (user) */
            if (dy) { int32_t r = (int32_t)ui.qr + dy; ui.qr = (uint32_t)(r < 0 ? 0 : r); ui.qc = ui.qw; }
            else if (dx > 0 || ui.qc) ui.qc += (uint32_t)dx;
            q_clamp();
            if (dx) ui.qw = ui.qc;
        }
        dirty = DIRTY_ALL;
    }
    if (edit_up && q_pending) {                 /* an EDIT tap: go there */
        q_pending = 0;
        q_jump();
        rows_clear();
        dirty = DIRTY_ALL;
    }
    if (ui.view == V_Q) { q_build(); q_clamp(); }   /* (an edit can empty a line) */
    return dirty;
}

static void save_button(void)                   /* the panel SAVE (app.js 'save'; HANDOFF section 0 "Projects") */
{
    char t[12];
    if (playing || play_req) { say("stop to save"); return; }   /* (the flash write would stop the sound) */
    undo_close();                               /* (the changes not yet seen) */
    if (proj_save(pj_slot)) { say("save failed"); return; }
    if (ui.view == V_Q) q_build();              /* (the save used Q's tables) */
    proj_dirty = 0;
    slots_read();
    str_cpy(t, "saved to ", sizeof t); fmt_hex(t + 9, pj_slot, 2);
    say(t);
}

/* ---------------------------------------------------------------- the main loop's parts (app_main) --- */
/* S / C / P: selections, EDIT and its doubles (EDIT EDIT, SHIFT + EDIT again), EDIT + d-pad, OPTION + d-pad, the
 * cursor (app.js GRID, keyDown). -> dirty bits */
static uint32_t grid_input(uint32_t keys, uint32_t down, int32_t dx, int32_t dy)
{
    static uint32_t edit_t, clone_t, edit_cell = 0xFFFFFFFFu;   /* EDIT EDIT: the first EDIT, its cell; the last clone */
    uint32_t dirty = 0;
    uint32_t cell = ui.on_row << 28 | ui.view << 24 | ui.ch << 20 | ui.song_row << 12 | ui.chain_row << 8 | ui.row << 4 | cur_col();
    bounds_t bk;
    int has_block;
    if (down & PAD_KEYS & ~(1u << PAD_OPTION)) q_desel = 0;
    if (down >> PAD_OPTION & 1u) {              /* SHIFT + OPTION selects; OPTION copies it; a plain OPTION drops the mark */
        if (keys >> PAD_SHIFT & 1u) sel_start();
        else if (sel.on) { sel_copy(); opt_copy = 1; }
        else if (mark.on) q_desel = 1;
        dirty |= DIRTY_ALL;
    }
    if (q_desel && !(keys >> PAD_OPTION & 1u)) { q_desel = mark.on = 0; say("deselected"); dirty |= DIRTY_ALL; }
    has_block = block(&bk);
    if (!(keys >> PAD_OPTION & 1u)) opt_copy = 0;
    if (down >> PAD_EDIT & 1u) {
        uint32_t quick = fm1_ms - clone_t < 300u, again = quick ? n_cloned : 0, ramp = quick && interp.on;
        n_cloned = interp.on = 0;               /* (deep clone, ramp: only right after the clone / interpolation) */
        if (keys >> PAD_SHIFT & 1u) {
            if (ramp) rampify();                /* (the ramp is still selected: before the selection) */
            else if (sel.on) {
                if (interpolable(&bk)) interpolate(&bk); else clone_sel(&bk);
                clone_t = fm1_ms;
            } else if (again) deep_clone(again);
            else paste();
        } else if (keys >> PAD_OPTION & 1u) {
            if (has_block || opt_copy) block_delete();
            else if (ui.on_head) column_clear();
            else if (ui.on_row) row_delete();
            else edit_clear();
        } else if (ui.on_row) {                 /* EDIT EDIT on a row number: an empty row here */
            if (fm1_ms - edit_t < 300u && cell == edit_cell) { row_insert(); edit_t = 0; }
            else { edit_t = fm1_ms; edit_cell = cell; }
        } else if (!ui.on_head && !has_block) {   /* (a column label, a block: EDIT is the modifier for EDIT + d-pad) */
            if (fm1_ms - edit_t < 300u && cell == edit_cell) { edit_new(); edit_t = 0; }   /* EDIT EDIT */
            else { edit_press(); fx_audition(); edit_t = fm1_ms; edit_cell = cell; }
        }
        dirty |= DIRTY_ALL;                     /* (a new chain / phrase changes the preview and the crumbs) */
    }
    if (dx || dy) {
        int32_t d = dx ? dx : -dy, big = !dx;   /* EDIT + right / up = more; up / down = a big step */
        if (keys >> PAD_EDIT & 1u && has_block) {
            /* one column: its values change; several (or whole rows): up / down move the cells, left / right
             * nothing (unit-feedback #25) */
            if (bk.rows || bk.c0 != bk.c1) { if (dy) rows_move(bk.r0, bk.r1, dy, bk.rows ? 0u : bk.c0, bk.rows ? ~0u : bk.c1); }
            else if (!(ui.view == V_PHRASE && bk.c0 >= 3u && (bk.c0 - 3u) % 2u == 0u)) block_change(&bk, d, big);   /* (FX
                                                 * command names: nothing, user 2026-10-12) */
        } else if (keys >> PAD_EDIT & 1u && ui.on_row && !sel.on) {
            if (dy) rows_move(*cur_r(), *cur_r(), dy, 0u, ~0u);
        } else if (keys >> PAD_EDIT & 1u && ui.on_head && !sel.on) {   /* every value of the column */
            bounds_t col = {0, 15, ui.col, ui.col, 0};
            if (!(ui.col >= 3u && (ui.col - 3u) % 2u == 0u)) block_change(&col, d, big);
        } else if (keys >> PAD_EDIT & 1u && cmd_pick()) {
            cmd_grid_move(0, dx, dy); fx_audition();   /* the FX grid is open: the arrows move in it */
        } else if (keys >> PAD_EDIT & 1u && popup_kind(keys) == POP_LIST) {
            pop_move(dx, dy);                   /* a list is open: down / right = on */
        } else if (keys >> PAD_EDIT & 1u) {
            edit_change(d, big); fx_audition();
        } else if (keys >> PAD_OPTION & 1u && !sel.on) {
            if (ui.view != V_SONG) option_move(dx, dy);
        } else {                                /* the cursor; the row number and the column labels are cells too */
            uint32_t *R = cur_r(), *C = cur_c();
            if (ui.on_head) {
                if (dy > 0) ui.on_head = 0;
                else if (dx) { int32_t c = (int32_t)*C + dx; *C = (uint32_t)clamp_i(c, 0, 8); }
            } else if (dy < 0 && *R == 0u && ui.view == V_PHRASE && !ui.on_row && !key_rep) ui.on_head = 1;
            else if (dx < 0 && *C == 0u && !ui.on_row && !key_rep) ui.on_row = 1;
            else if (dx > 0 && ui.on_row) ui.on_row = 0;
            else if (!(ui.on_row && dx)) {
                int32_t c = (int32_t)*C + dx, w = (int32_t)*R + dy, nc = (int32_t)grid_cols(), nr = (int32_t)grid_rows();
                *C = (uint32_t)clamp_i(c, 0, nc - 1);
                *R = (uint32_t)clamp_i(w, 0, nr - 1);
            }
        }
        dirty |= DIRTY_ALL;
    }
    return dirty;
}
/* the instrument page: EDIT (a parameter copied when let go, a letter), EDIT + d-pad, OPTION + d-pad, the cursor,
 * KNOB 1-4 on the cursor's row. -> dirty bits */
static uint8_t copy_pending;                    /* EDIT went down on a parameter: let go untouched, it is copied */
/* Quick assign (decisions.md "Knobs, take 3" (2), app.js assignKnob): the keybed EDIT held + KNOB n on I E L V puts the
 * cursor's parameter in slot n (on the KNOBS rows: that slot's parameter); the hold's first detent assigns, the rest
 * do nothing, letting EDIT go then copies nothing; ENGINE / PRESET / NAME and the other screens: a message */
static uint8_t kn_assigned;                     /* a knob turned in this EDIT hold */
static void knob_assign(uint32_t k)
{
    uint32_t key = IF[ui.field].key;
    char s[40];
    copy_pending = 0;
    if (kn_assigned) return;
    kn_assigned = 1;
    if (!IS_PAGE(ui.view)) { say("EDIT + KNOB assigns on I E L V"); return; }
    key = IS_KNOB(key) ? slot_key(ui.inst, key - FK_KNOB) : fkey(ui.field);
    if (key >= P_COUNT || cmd_index(CMD_SOUND | key, ui.inst) < 0) {
        str_cpy(s, key == FK_ENGINE ? "ENGINE" : key == FK_PRESET ? "PRESET" : key == FK_NAME ? "NAME" : "it", sizeof s);
        sa(s, " takes no knob"); say(s);
        return;
    }
    undo_mark(0, 0);                            /* (its own undo step) */
    S.knob[ui.inst][k] = (uint8_t)(key & 0x7Fu);
    str_cpy(s, KNOB_NAME, sizeof s); sn(s, KNOB_NO(k)); sa(s, " = ");
    cmd_name(CMD_SOUND | key, ui.inst, s + str_len(s));
    say(s);
}
static uint32_t page_input(uint32_t keys, uint32_t down, int32_t dx, int32_t dy, uint32_t edit_up, uint32_t field_was)
{
    static nhold_t nh;
    uint32_t k, dirty = 0;
    if (down >> PAD_EDIT & 1u && keys >> PAD_OPTION & 1u) { inst_reset(ui.field); dirty |= DIRTY_ALL; }
    if (down >> PAD_EDIT & 1u && !(keys >> PAD_OPTION & 1u)) {
        if (IF[ui.field].key != FK_NAME) {
            copy_pending = 1;
            if (!IS_KNOB(IF[ui.field].key)) blip(ui.ch, last.note, ui.inst);   /* heard while EDIT is held; not a KNOBS
                                                 * name, a choice of parameter (unit-feedback #30) */
        } else if (!(keys >> PAD_SHIFT & 1u)) {   /* a letter: remembered, so letting EDIT go can move on */
            name_down(&nh);
            dirty |= DIRTY_ALL;
        }
    }
    if (dx || dy) copy_pending = 0;             /* EDIT + arrows edits: no copy */
    if (edit_up && copy_pending) { copy_pending = 0; param_copy(); }
    if (edit_up && name_up(&nh, IF[ui.field].key == FK_NAME)) dirty |= DIRTY_ALL;   /* written or changed: the next letter */
    if (dx || dy) {
        if (keys >> PAD_EDIT & 1u && IF[ui.field].key == FK_NAME) {
            name_move(dx, dy);
            dirty |= DIRTY_ALL;
        } else if (keys >> PAD_EDIT & 1u && IS_KNOB(IF[ui.field].key)) {
            cmd_grid_move(1, dx, dy);           /* a knob slot: the grid is open, the arrows move in it */
            dirty |= DIRTY_ALL;
        } else if (keys >> PAD_EDIT & 1u && popup_kind(keys) == POP_LIST) {
            pop_move(dx, dy);
            dirty |= DIRTY_ALL;
        } else if (keys >> PAD_EDIT & 1u) {
            inst_change(ui.field, dx ? dx : -dy * inst_big(ui.field));
            dirty |= DIRTY_ALL;                 /* (an engine change relabels the whole page) */
        } else if (keys >> PAD_OPTION & 1u) {   /* OPTION + left/right: instrument -1 / +1, up/down: +4 / -4 */
            int32_t i = (int32_t)ui.inst + (dx ? dx : -dy * 4);
            ui.inst = (uint32_t)clamp_i(i, 0, (int32_t)NINS - 1);
            if (!field_usable(ui.field)) ui.field = 0;
            dirty |= DIRTY_ALL;
        } else {
            inst_move(dx, dy);
            dirty |= 1u << field_was | 1u << FROW(ui.field);
        }
    }
    for (k = 0; k < 4u; k++) {                  /* KNOB 1..4: the cursor's row of four, left to right (on the KNOBS
                                                 * names: their values; a knob always turns a value, user 2026-10-10) */
        int32_t t = knob_take(k);
        uint32_t f, n = 0;
        if (!t) continue;
        if (ss.on) { ss.on = ss.stuck = 0; dirty |= DIRTY_ALL; }   /* (a knob turn closes the scene strip) */
        if (keys >> PAD_EDIT & 1u) { knob_assign(k); dirty |= DIRTY_ALL; continue; }
        for (f = 0; f < NIF; f++)
            if (IF[f].row == IF[ui.field].row + IS_KNOB(IF[ui.field].key) && IF[f].key != FK_NAME && field_usable(f) && n++ == k) {
                if (kb == KB_REC && playing) {   /* REC: written into the phrase, the title in the bottom line */
                    const char *why = rec_knob(ui.inst, fkey(f), t);
                    char tt[40];
                    if (!why) { str_cpy(tt, "REC I", sizeof tt); tt[6] = 0; tt[5] = HEX[ui.inst & 15u]; sa(tt, " "); sa(tt, ins[ui.inst].name); }
                    say(why ? why : tt);
                    dirty |= DIRTY_ALL;
                    break;
                }
                undo_mark(1u << 31 | ui.view << 24 | ui.inst << 8 | f, 0);   /* (one turn: one step) */
                inst_change(f, fast(field_desc(f), t));
                dirty |= DIRTY_ALL;
                break;
            }
    }
    return dirty;
}
static uint32_t knob_input(uint32_t keys)       /* away from the pages: KNOB 1-4 turn the focus instrument's knob slots,
                                                 * the knob strip shows them. -> dirty bits */
{
    uint32_t k, dirty = 0;
    for (k = 0; k < 4u; k++) {                  /* KNOB 1..4: the focus instrument's knob slots, the strip shows them */
        int32_t t = knob_take(k), v;
        uint32_t i = focus_inst(), key = slot_key(i, k);
        if (!t) continue;
        if (keys >> PAD_EDIT & 1u) { knob_assign(k); dirty |= DIRTY_ALL; continue; }
        ss.on = ss.stuck = 0;   /* (the knob strip takes its place) */
        if (kb != KB_REC) undo_mark(1u << 31 | 0x7Fu << 24 | i << 8 | k, 0);   /* (one turn: one step; a take: its own) */
        ks.rec = kb == KB_REC && playing;
        ks.scn = 0;
        if (ks.rec) { ks.why = rec_knob(i, key, t); dirty |= 0xFFFFu; }   /* REC: written into the phrase */
        else {
            v = param_turn(ins_desc(i, key), sc_value(i, key, ins[i].p[key]), fast(ins_desc(i, key), t));
            ks.scn = (uint8_t)(sc_editing(i, key) ? near_scene() + 1u : 0u);
            if (ks.scn) sc_set(i, key, v);   /* (into the near scene: SEL, or it holds it) */
            else if (v != ins[i].p[key]) { ins[i].p[key] = (int16_t)v; ins_gen[i]++; }   /* heard at once (the parts reload) */
        }
        ks.on = 1; ks.hot = (uint8_t)k; ks.inst = (uint8_t)i; ks.drawn = 0; ks.until = fm1_ms + 1500u;
    }
    return dirty;
}
/* F and X, rows of four (decisions.md "Knobs, take 3" (1), app.js knobFields): on F KNOB n = field n of the cursor's
 * frame row (past its last field: nothing), not recorded in REC (F has no commands); on X KNOB n = track n's TVn from
 * any row, in REC recorded as TVn on that track. Through the near scene as EDIT + arrows; no knob strip. -> dirty bits */
static uint32_t row_knobs(uint32_t keys)
{
    uint32_t k, f, n, dirty = 0;
    for (k = 0; k < 4u; k++) {
        int32_t t = knob_take(k);
        if (!t) continue;
        ss.on = ss.stuck = 0;
        dirty = DIRTY_ALL;
        if (keys >> PAD_EDIT & 1u) { knob_assign(k); continue; }
        if (ui.view == V_MIX && kb == KB_REC && playing) { rec_tvl(k, t * kn_mul); continue; }
        if (kb != KB_REC) undo_mark(1u << 31 | ui.view << 24 | 0x80u << 8 | k, 0);   /* (one turn: one step) */
        if (ui.view == V_MIX) mix_set(k, 0, sc_value(SC_MIX, k, S.lvl[k]) + t * kn_mul);
        else for (f = n = 0; f < 9u; f++)
            if (FXF[f].row == FXF[ui.ff].row && n++ == k) { fx_change(f, fast(&GP[FXF[f].g], t)); break; }
    }
    return dirty;
}
/* what is drawn over the rows (the FX grid, the character grid, a popup, the knob or scene strip) and the bottom line's
 * help while EDIT is held (app.js helpState). -> the dirty bits left to redraw */
static uint32_t overlay_draw(uint32_t keys, uint32_t dirty)
{
    static uint32_t grid_was;                   /* the overlay drawn last (OV_*) */
    uint32_t k;
    bounds_t bk2;
    uint32_t ek = keys >> PAD_EDIT & 1u && !(keys >> PAD_SHIFT & 1u) && !(keys >> PAD_OPTION & 1u);   /* EDIT alone */
    uint32_t sel_on = ui.view <= V_PHRASE && block(&bk2);
    int held = ek && ui.view == V_PHRASE && !ui.on_row && !ui.on_head && !sel_on;
    uint32_t kgrid = IS_PAGE(ui.view) && IS_KNOB(IF[ui.field].key) && ek;   /* a knob slot's grid */
    uint32_t grid = kgrid || (held && cmd_pick());
    if (!grid) map_lit = EMPTY;                 /* (the rows come back, the map with them) */
    uint32_t on_name = ui.view == V_PROJ ? ui.prow == PR_NAME : ui.view == V_INST && IF[ui.field].key == FK_NAME;
    uint32_t chars = on_name && ek && name_get() != NONE;   /* the character grid */
    uint32_t pop = grid || chars ? POP_NONE : popup_kind(keys);
    sc_t skey[2u * SCENE_MAX];
    uint32_t nk = ss.on ? scene_keys(skey) : 0u;
    uint32_t open = grid ? OV_GRID : chars ? OV_CHARS : pop ? OV_CHARS + pop : ks.on ? OV_KSTRIP + !kstrip_top() : ss.on ? OV_SSTRIP + (uint32_t)(sstrip_top(nk) + 1)
                  + 32u * nk + 1024u * ss.stuck : OV_NONE;   /* (what is drawn over the rows; the knob and scene strips: where
                                                 * they are, the scene strip's size and focus) */
    char h[40];
    h[0] = 0;
    if (held && (on_cmd() || on_val()) && get() != NONE) {
        fx_t *f = cur_fx();
        if (on_cmd()) cmd_help(f->c, row_inst(ui.row), h);
        else cmd_value_text(f->c, row_inst(ui.row), f->v, h);
    }
    if (ek && ui.view <= V_PHRASE && !h[0])   /* the plain cells (gen/help.h, from app.js) */
        str_cpy(h, ui.on_row ? ROW_HELP : ui.on_head ? HEAD_HELP : sel_on ? SEL_HELP : ui.view == V_SONG ? SONG_HELP
                   : ui.view == V_CHAIN ? CHAIN_HELP[ui.ccol] : ui.col < 3u ? PHRASE_HELP[ui.col] : cmd_pick() ? "arrows: pick a command"
                   : FX_EMPTY_HELP, sizeof h);
    if (IS_PAGE(ui.view) && !msg[0]) live_line(h);   /* a held value: set and now */
    if (!(keys >> PAD_EDIT & 1u) && !sel_held && !msg[0] && pop == POP_NONE) { char sl[40]; if (scene_line(sl)) str_cpy(h, sl, sizeof h); }   /* (over
                                                 * live_line: the morph wins) */
    if (ui.view == V_MIX && ek)
        str_cpy(h, ui.mf == 0u ? "channel level, 0dB = full" : ui.mf == 1u ? "silence this channel" : "hear only the soloed channels", sizeof h);   /* (app.js CELL_HELP) */
    if (ui.view == V_FX && ek)
        str_cpy(h, FXP_HELP[ui.ff], sizeof h);
    if (ui.view == V_Q) { if (ek && sel.on) str_cpy(h, SEL_HELP, sizeof h); else if (ek) q_help(h); else if (!msg[0]) q_where(h); }   /* (where the line plays) */
    if (kgrid) cmd_help(CMD_SOUND | slot_key(ui.inst, IF[ui.field].key - FK_KNOB), ui.inst, h);
    if (ui.view == V_PROJ && ek) str_cpy(h, PROJ_HELP[ui.prow], sizeof h);   /* (its NAME: NAME_HELP) */
    if (ui.view == V_PROJ && ui.prow == PR_TEMPO && !ek && !msg[0] && tempo_live()) {   /* `TEMPO set 124, now 90` + the
                                                 * ramp's triangle (param-values.md 4) */
        int d = ramp_dir(tempo_ch & 3u, CMD_TPO);
        char t[2] = {(char)(d > 0 ? 1 : 2), 0};
        str_cpy(h, "TEMPO set ", sizeof h); sn_n(h, S.tempo, sizeof h); sa_n(h, ", now ", sizeof h); sn_n(h, tempo, sizeof h);
        if (d) sa_n(h, t, sizeof h);
    }
    if (on_name && ek) str_cpy(h, NAME_HELP, sizeof h);
    if (pop == POP_LIST && ui.view == V_INST && IF[ui.field].key == FK_ENGINE)   /* the engine's line */
        for (k = 0; k < NELEM(ENGINE_HELP); k++)
            if (str_eq(ENGINE_HELP[k].engine, ENGINES[ins[ui.inst].eng]->name)) str_cpy(h, ENGINE_HELP[k].help, sizeof h);
    if (pop != POP_KEYS) kb_base = EMPTY;
    if (ss.on && ss.stuck && nk) {              /* the stuck scene strip: what EDIT does on its line (app.js helpState) */
        char nm[16];
        sc_name(skey[ss.line < nk ? ss.line : nk - 1u].t, skey[ss.line < nk ? ss.line : nk - 1u].p, nm);
        str_cpy(h, "EDIT: go to ", sizeof h); sa(h, nm); sa(h, ", OPTION: close");
    }
    if (!str_eq(h, help)) { str_cpy(help, h, sizeof help); msg_dirty = 1; }
    if (grid || chars || !(pop || ks.on || ss.on)) ov_clip.y0 = ov_clip.y1 = 0;   /* (no overlay that keeps the rows off) */
    if (grid) {
        if (dirty || grid_was != open) { if (dirty & DIRTY_BAR) draw_bar(); draw_cmd_grid((int)kgrid); }
        dirty = 0;
    } else if (chars) {                         /* the E rows under it stay as they are */
        if (dirty || grid_was != open) { redraw(dirty & ~CHARS_UNDER); draw_char_grid(); }
        dirty = 0;
    } else if (pop) {                           /* the rows (not under the popup: clip), then the popup; it flickered on the
                                                 * device before the clip (decisions.md firmware review #7) */
        if (dirty || grid_was != open || msg_dirty) {   /* (the bottom line first: a 16-line list's frame
                                                 * closes over it, as render.js draws it) */
            redraw(dirty);
            if (msg_dirty) { draw_strip(); msg_dirty = 0; }
            ov_moved(pop == POP_KEYS ? draw_keys : draw_list);
        }
        dirty = 0;
    } else if (ks.on) {                         /* as the popups: the rows, then the strip */
        if (dirty || grid_was != open || !ks.drawn) { redraw(grid_was != open && grid_was ? DIRTY_ALL : dirty); ov_moved(draw_kstrip); }
        dirty = 0;
    } else if (ss.on) {                         /* the scene strip: as the knob strip */
        if (dirty || grid_was != open || !ss.drawn) { redraw(grid_was != open && grid_was ? DIRTY_ALL : dirty); ov_moved(draw_sstrip); }
        dirty = 0;
    } else if (grid_was)
        dirty = DIRTY_ALL;                      /* the grid closes: the rows come back */
    if (grid_was == OV_LIST && open != grid_was) msg_dirty = 1;   /* a 16-line list's frame reaches the strip's top line */
    grid_was = open;
    return dirty;
}

#if BOARD_SMK37
#include "smk_keys.c"                           /* the SMK-37's pads, keys, faders, wheels and LEDs */
#endif

static void app_main(void)
{
    uint32_t prev = 0, dirty = DIRTY_ALL, moved, rec_row_was = 0, lfo_t = 0, shown_tick = 0, shown_swing = 0, was_playing = 0;
    uint32_t prev_edit = 0, edit_was = 0, prv_on = 0, shown_xf = 0, xf_t = 0, sel_was = 0, kb_was = 0;
    song_new();
    instruments_new();
    sound_init();
    sound_ready = 1;
    if (settings_load(&pj_slot, 1) || pj_slot >= PJ_SLOTS) pj_slot = 0;
    proj_load(pj_slot);                         /* the project opened last, if it is there */
    undo_reset();
    slots_read();
    memset((void *)ramp_ph, EMPTY, sizeof ramp_ph);
    memset(held_ch, EMPTY, sizeof held_ch);
    memset(entry_row, EMPTY, sizeof entry_row);
    lcd_fill(0, 0, 240, 240, P_BG);
    msg[0] = 0;
    draw_strip();

    for (;;) {
        uint32_t keys, down, kup, rel, btn, k, field_was = FROW(ui.field);
        int32_t dx, dy;
        platform_poll();
#if BOARD_SMK37
        keys = smk_poll(&dirty);                /* the gamepad on the pads (smk_keys.c) */
#else
        keys = fm1_in.notes;
#endif
        aud_hold = (uint8_t)(keys >> PAD_EDIT & 1u);
        down = keys & ~prev;
        kup = prev & ~keys;
        prev = keys;
        {   /* a held arrow repeats, like the M8: the cursor and EDIT + arrow (unit-feedback.md #1); not with SHIFT, so
             * a held SHIFT + arrow changes screen once */
            static uint32_t rep_t, rep_k;
            uint32_t d = down & (1u << PAD_LEFT | 1u << PAD_DOWN | 1u << PAD_UP | 1u << PAD_RIGHT);
            key_rep = 0;
            if (d) { rep_k = d & -d; rep_t = fm1_ms + REP_WAIT; }
            else if (!(keys & rep_k)) rep_k = 0;
            else if (!(keys >> PAD_SHIFT & 1u) && fm1_ms - rep_t < 0x80000000u) { down |= rep_k; rep_t = fm1_ms + REP_EVERY; key_rep = 1; }
        }
        btn = fm1_input_edges(&rel);
        if (sys_open && (down || btn)) { sys_open = 0; down = 0; btn = 0; dirty |= DIRTY_ALL; }   /* any key or button
                                                 * closes G's SYSTEM popup, and does nothing else */
        prev_edit = edit_was;
        if (down >> PAD_EDIT & 1u) grid_col = -1;   /* (the grids' remembered place) */
        edit_was = keys >> PAD_EDIT & 1u;
        loop_next = (uint8_t)(ui.view == V_PHRASE || IS_PAGE(ui.view) ? cur_phrase() : EMPTY);
        { static uint32_t shape; uint32_t h = song_shape(); if (h != shape) { shape = h; memset(entry_row, EMPTY, sizeof entry_row); } }
        song.g[G_TUNE] = S.pj.tune;             /* TUNE: Felucca's own, in cents */
        for (k = 0; k < 9u; k++)                /* F: Felucca's delay, reverb, chorus, through the scenes */
            song.g[FXF[k].g] = (int16_t)morph(SC_FX, k, S.fx[k], is_list(&GP[FXF[k].g]));
        sel_held = (uint8_t)(fm1_in.buttons >> BTN_SEL & 1u);

        /* PLAY (the pad's, or the panel's); SHIFT + PLAY: the song */
        if (keys >> PAD_OPTION & 1u && (down >> PAD_SHIFT & 1u || down >> PAD_PLAY & 1u)) {   /* the M8's mute / solo */
            char t[28];
            int clear = down >> PAD_PLAY & 1u && keys >> PAD_SHIFT & 1u, solo = down >> PAD_PLAY & 1u;
            if (clear) { memset(S.mute, 0, NCH); memset(S.solo, 0, NCH); mix_req = 0xFFu; str_cpy(t, "mutes and solos cleared", sizeof t); }
            else {
                int on = !(solo ? heard_solo(ui.ch) : heard_mute(ui.ch));   /* (toggles what is heard) */
                mix_flag(ui.ch, solo, on);      /* (with SEL held too, unlike X's mix_set) */
                str_cpy(t, on ? "" : "un", sizeof t);
                sa_n(t, solo ? "solo CH" : "mute CH", sizeof t);
                t[str_len(t) + 1] = 0; t[str_len(t)] = (char)('1' + ui.ch);
            }
            say(t);
            dirty |= DIRTY_ALL;
            down &= ~(1u << PAD_SHIFT | 1u << PAD_PLAY);   /* (not a screen change, not a play) */
        }
        if (down >> PAD_PLAY & 1u && keys >> PAD_EDIT & 1u && IS_PAGE(ui.view)) {   /* EDIT + PLAY: I E L V and Q only,
                                                 * not P (its preview is EDIT's audition; picks review #4) */
            prv_ch = (uint8_t)ui.ch;
            prv_note = (uint8_t)last.note;
            prv_ins = (uint8_t)ui.inst;
            prv_ph = EMPTY;
            prv_req = 1;
            prv_on = 1;
        }
        if (down >> PAD_PLAY & 1u && keys >> PAD_EDIT & 1u && ui.view == V_Q && ui.qr < q_n) {   /* EDIT + PLAY in Q (app.js
                                                 * previewQ): the line's note, the instrument shown, its row's sound
                                                 * commands, on the channel of its first play */
            const row_t *w = q_row(ui.qr);
            q_pending = 0;                      /* (EDIT was a modifier, not a tap) */
            if (w->n != EMPTY && w->n != NOTE_OFF) {
                prv_ch = (uint8_t)(qh.q.it[ui.qr].at >> 10);
                prv_note = w->n;
                prv_ins = (uint8_t)ui.inst;
                prv_ph = (uint8_t)(qh.q.it[ui.qr].key >> 4);
                prv_r = (uint8_t)(qh.q.it[ui.qr].key & 15u);
                prv_req = 1;
                prv_on = 1;
            }
        }
        if (kup >> PAD_PLAY & 1u && prv_on) { prv_on = 0; prv_req = 2; }   /* PLAY let go: the preview ends */
        if ((down >> PAD_PLAY & 1u && !(keys >> PAD_EDIT & 1u)) || btn >> BTN_PLAY & 1u) {
            play(keys >> PAD_SHIFT & 1u);
        }
        moved = 0;
        if (ui.view == V_MIX && memcmp(shown_tvl, (const void *)ch_tvl, sizeof shown_tvl)) {   /* a TVL moved a level */
            memcpy(shown_tvl, (const void *)ch_tvl, sizeof shown_tvl);
            dirty |= DIRTY_ALL;
        }
        {   /* G: the tempo heard now moved (param-values.md 4) */
            uint32_t g = ui.view == V_PROJ && tempo_live() ? (uint32_t)tempo | (uint32_t)tempo_ch << 8 | (uint32_t)(ramp_dir(tempo_ch & 3u, CMD_TPO) + 1) << 16 | 1u << 24 : 0u;
            if (g != shown_tempo) { shown_tempo = g; dirty |= DIRTY_ALL; }
        }
        for (k = 0; k < NCH; k++) {             /* a MUT / SOL heard: X, S's marks, the grey cells follow */
            uint8_t a = (uint8_t)(heard_mute(k) | heard_solo(k) << 1);
            if (a != shown_mix[k]) { shown_mix[k] = a; dirty |= DIRTY_ALL; }
        }
        for (k = 0; k < NCH; k++) {             /* the ISR moved on: redraw what shows a playhead */
            uint32_t p = playing ? ch_pos[k] : 0, o = shown_pos[k];
            if (p == o) continue;
            shown_pos[k] = p;
            moved = 1;
            if (ui.view == V_SONG) {
                uint32_t t = song_top(), a = POS_SROW(o), b = POS_SROW(p);
                if (POS_ON(o) && a >= t && a < t + 16u) { dirty |= 1u << (a - t); rows_old |= 1u << (a - t); }
                if (POS_ON(p) && b >= t && b < t + 16u) dirty |= 1u << (b - t);
                if (POS_ON(o)) dirty |= smap_line(a);   /* the song map's red rows */
                if (POS_ON(p)) dirty |= smap_line(b);
            } else if (ui.view == V_PHRASE && k == ui.ch) {
                uint32_t r;
                int ramps = 0;
                for (r = 0; r < NFX; r++) if (ramp_ph[k][r] != EMPTY) ramps = 1;
                if (ramps || (o >> 4) != (p >> 4) || !POS_ON(p)) dirty |= 0xFFFFu | DIRTY_LABELS;   /* ramps, HOP, CHA */
                if (POS_ON(o)) { dirty |= 1u << POS_ROW(o); rows_old |= 1u << POS_ROW(o); }
                if (POS_ON(p)) dirty |= 1u << POS_ROW(p);
            } else if (ui.view == V_CHAIN && k == ui.ch) {
                if (POS_ON(o)) { dirty |= 1u << POS_CROW(o); rows_old |= 1u << POS_CROW(o); }
                if (POS_ON(p)) dirty |= 1u << POS_CROW(p);
            } else if (ui.view == V_MIX) dirty |= 1u << 15;   /* the notes line */
        }
        if (moved) {                            /* Q's playhead in song time (the song playing only): its timeline */
            uint32_t pt = q_pt;
            q_pt = NONE32;
            if (playing && mode == M_SONG) q_walk(q_pt_row);
            if (ui.view == V_Q && q_pt != pt) dirty |= DIRTY_GRAPH;
        }
        if (ui.follow && ui.view <= V_PHRASE && POS_ON(shown_pos[ui.ch])) {   /* HOME: the cursor on what its channel plays */
            uint32_t p = shown_pos[ui.ch], sr = POS_SROW(p), cr = POS_CROW(p), r = ui.view == V_PHRASE ? POS_ROW(p) : cr;
            uint32_t *at = ui.view == V_PHRASE ? &ui.row : &ui.chain_row;
            if (ui.song_row != sr || (ui.view == V_PHRASE && ui.chain_row != cr)) {   /* another chain / phrase */
                ui.song_row = sr;
                if (ui.view != V_SONG) ui.chain_row = cr;
                if (ui.view == V_PHRASE) ui.row = r;
                dirty |= DIRTY_ALL;
            } else if (ui.view != V_SONG && *at != r) { dirty |= 1u << *at | 1u << r; rows_old |= 1u << *at; *at = r; }
        }
        if (ui.view == V_PHRASE && playing && ui_tick != shown_tick) {   /* RET's ticks light up as they play */
            shown_tick = ui_tick;
            if (POS_ON(shown_pos[ui.ch])) dirty |= 1u << POS_ROW(shown_pos[ui.ch]);
        }
        if (ch_swing[ui.ch] != shown_swing) { shown_swing = ch_swing[ui.ch]; dirty |= DIRTY_BAR; }
        if (S.scn.xf != shown_xf && fm1_ms - xf_t >= 100u) {   /* the fader moved (SCN): its mark (the bar and the line under
                                                 * it), the strips, and the values the morph moves (I E L V X F), at most every
                                                 * 100 ms; shortcut: G X F Q and the pages redraw whole, measure on the device */
            shown_xf = S.scn.xf; xf_t = fm1_ms;
            dirty |= DIRTY_BAR | DIRTY_LABELS | (IS_PAGE(ui.view) || ui.view == V_MIX || ui.view == V_FX ? DIRTY_ALL : 0u);
            ss.drawn = ks.drawn = 0;
        }
        if (sel_held != sel_was) { sel_was = sel_held; dirty |= DIRTY_ALL; ks.drawn = 0; }
        dirty |= touch_state();
        if (kb != kb_was) { kb_was = kb; if (kb != KB_REC) memset(rc, 0, sizeof rc); dirty |= DIRTY_ALL; }   /* the take's red
                                                 * cells: drawn as usual again when it ends */   /* SEL: the scene's values, the others dim */
        if (playing != was_playing) { was_playing = playing; dirty |= DIRTY_ALL; }   /* stop: the dims go */

        dx = (down >> PAD_RIGHT & 1u) ? 1 : (down >> PAD_LEFT & 1u) ? -1 : 0;
        dy = (down >> PAD_DOWN & 1u) ? 1 : (down >> PAD_UP & 1u) ? -1 : 0;
        if (ss.on && !sel_held && down & PAD_KEYS) {   /* the scene strip's keys (app.js sceneStripKey; SEL held: the page's) */
            sc_t key[2u * SCENE_MAX];
            uint32_t n = scene_keys(key), plain = !(keys & ~down & (1u << PAD_SHIFT | 1u << PAD_OPTION | 1u << PAD_EDIT));
            if (dy && plain && n) {             /* up / down: it keeps the focus, a line cursor walks it */
                ss.line = (uint8_t)(!ss.stuck ? 0u : dy < 0 ? (ss.line ? ss.line - 1u : 0u) : ss.line + 1u < n ? ss.line + 1u : n - 1u);
                ss.stuck = 1; ss.drawn = 0;
                down &= ~(1u << PAD_UP | 1u << PAD_DOWN);
                dy = 0;
                dirty |= DIRTY_ALL;             /* (the screen's cursor dims) */
            } else if (ss.stuck && down & PAD_KEYS & ~(1u << PAD_PLAY | 1u << PAD_SHIFT)) {   /* PLAY, SHIFT leave it */
                ss.on = ss.stuck = 0;
                if (down >> PAD_EDIT & 1u && plain && n) {   /* EDIT: that value's page */
                    uint32_t j = ss.line < n ? ss.line : n - 1u;
                    scene_jump(key[j].t, key[j].p);
                    down &= ~(1u << PAD_EDIT);
                } else if (down >> PAD_OPTION & 1u) down &= ~(1u << PAD_OPTION);   /* OPTION: closes it, nothing else */
                dirty |= DIRTY_ALL;             /* (other keys close it and act) */
            }
        }
        if ((down >> PAD_EDIT & 1u || dx || dy) && msg[0]) { msg[0] = 0; msg_dirty = 1; }
        if (ui.follow && dy && !(keys >> PAD_SHIFT & 1u) && !(keys >> PAD_EDIT & 1u)) { ui.follow = 0; say("follow off"); }   /* looking elsewhere */
        if (pend.act && !pend.wait && down & PAD_KEYS) {   /* a slot question (G): EDIT confirms, any other key cancels */
            if ((down & PAD_KEYS) == 1u << PAD_EDIT && !(keys >> PAD_SHIFT & 1u) && !(keys >> PAD_OPTION & 1u)) slot_confirm();
            else { pend.act = 0; say("cancelled"); }
            down &= ~(1u << PAD_EDIT);          /* (the EDIT does nothing else) */
            dirty |= DIRTY_ALL;
        }
        if (pend.wait && !playing && !play_req) {   /* stopped (its phrase ended, or PLAY): the slot action */
            uint32_t a = pend.act;
            pend.act = pend.wait = 0;
            end_ch = 0;
            slot_do(a, pend.slot);
            dirty |= DIRTY_ALL;
        } else if (pend.wait && !msg[0]) say_held(pend.t);
        {   /* undo: each input closes a step, but EDIT + d-pad on one cell goes on (app.js snapshot groups) */
            uint32_t ukey = ui.view == V_PROJ ? ui.prow : ui.view == V_MIX ? ui.ch << 2 | ui.mf : ui.view == V_FX ? ui.ff : ui.view == V_Q ? ui.inst << 16 | ui.qr << 4 | ui.qc : IS_PAGE(ui.view) ? ui.inst << 8 | ui.field
                          : ui.on_row << 20 | ui.ch << 18 | ui.song_row << 12 | ui.chain_row << 8 | cur_col() << 4 | ui.row;
            if (kb == KB_REC) undo_mark(U_TAKE, 0);   /* (the take, and any edit during it) */
            else if ((dx || dy) && keys >> PAD_EDIT & 1u) {   /* a block (a selection, Q's plays): one step
                                                 * while EDIT stays held, the cursor moving with it (picks review #23) */
                int blk = sel.on || ui.view == V_Q;
                undo_mark(1u << 31 | ui.view << 24 | (blk ? 0u : ukey), blk);
            }
            else if (down || btn) undo_mark(0, 0);
            if (fm1_in.buttons >> BTN_EDIT & 1u && btn >> BTN_OCT_DOWN & 1u) { undo_step(-1); dirty |= DIRTY_ALL; }
            if (fm1_in.buttons >> BTN_EDIT & 1u && btn >> BTN_OCT_UP & 1u) { undo_step(1); dirty |= DIRTY_ALL; }
            if (u_fl_n && fm1_ms - u_fl_t >= 1200u) { u_fl_n = 0; dirty |= DIRTY_ALL; }   /* the flash is over */
        }
        if (btn >> BTN_SEL & 1u) {              /* SEL: edits go into the near scene (scenes.md 4: the bottom line names
                                                 * it; my pick: said when SEL is pressed, as the prototype's mockup) */
            char t[40];
            str_cpy(t, "scene ", sizeof t); sn(t, (int32_t)near_scene() + 1);
            sa(t, S.scn.xf * 2u > XF_MAX ? " (B)" : " (A)"); sa(t, ": edits go into the scene");
            say(t);
        }
        if (sel_held) {                         /* SEL held: the middle white keys B3..B4 pick the scenes (app.js sceneKey):
                                                 * a tap puts it on the side the fader is away from, SHIFT + key on its side */
            char t[40];
            for (k = 0; k < NSCENE; k++)
                if (down >> SCENE_KEYS[k] & 1u && keys >> PAD_OPTION & 1u) {   /* OPTION + key: empty it, asks first */
                    str_cpy(t, "empty scene ", sizeof t); sn(t, (int32_t)k + 1); sa(t, "? EDIT to confirm");
                    pend.act = SA_SCENE; pend.slot = (uint8_t)k; pend.wait = 0;
                    say_held(t);
                } else if (down >> SCENE_KEYS[k] & 1u) {
                    uint32_t b = S.scn.xf * 2u > XF_MAX ? (keys >> PAD_SHIFT & 1u) : !(keys >> PAD_SHIFT & 1u);
                    if (b) S.scn.b = (uint8_t)k; else S.scn.a = (uint8_t)k;
                    scn_changed();
                    str_cpy(t, "scene ", sizeof t); sn(t, (int32_t)k + 1); sa(t, b ? " on B" : " on A");
                    say(t);
                    ss.on = 1; ss.until = fm1_ms + 1500u; ss.drawn = 0;   /* (the strip: a stuck one stays stuck) */
                    dirty |= DIRTY_ALL;
                }
            for (k = 0; k < NSCENE; k++) down &= ~(1u << SCENE_KEYS[k]);   /* (not notes) */
        }
        {   /* the crossfader on SELECT (scenes.md 3, app.js crossfade): a detent = 1/32 of the way; SHIFT + a turn snaps
             * to that end. Moved by hand it is saved with the song (not undone: SCN plays it too). Only while SEL is held:
             * a crossfade is always meant (user, 2026-10-11, decisions.md "Scenes over commands") */
            int32_t t = fm1_enc_take(ENC_SELECT), x = (int32_t)S.scn.xf + t * 4;
            if (t && !sel_held) say("hold SEL to crossfade");
            else if (t) {
                S.scn.xf = (uint8_t)(keys >> PAD_SHIFT & 1u ? (t < 0 ? 0u : XF_MAX) : (uint32_t)clamp_i(x, 0, (int32_t)XF_MAX));
                proj_dirty = 1;
                if (kb == KB_REC && playing) rec_scn();   /* REC: SCN xx (touch) */
                ss.on = 1; ss.until = fm1_ms + 1500u; ss.drawn = 0;
                dirty |= DIRTY_ALL;
            }
        }
        if (ss.on && !ss.stuck && fm1_ms - ss.until < 0x80000000u) ss.on = 0;   /* (over: the rows come back) */
        if (btn >> BTN_SAVE & 1u) { save_button(); dirty |= DIRTY_ALL; }
        if (btn >> BTN_HOME & 1u) { ui.follow ^= 1u; say(ui.follow ? "follow on" : "follow off"); }   /* (app.js 'follow') */
        if (btn >> BTN_GLO & 1u) {              /* GLO: the G page; again: back (app.js 'view') */
            if (ui.view == V_PROJ) set_view(ui.back);
            else { ui.back = ui.view; set_view(V_PROJ); }
            screen_change();
            dirty |= DIRTY_ALL;
        }
        if (btn >> BTN_SEQ & 1u) {              /* SEQ: Q of the focus instrument (the one on I E L V; app.js
                                                 * seqButton); in Q: the filter */
            if (ui.view == V_Q) q_seq();
            else open_q(IS_PAGE(ui.view) ? ui.inst : focus_inst());
            screen_change();
            dirty |= DIRTY_ALL;
        }
        if (btn >> BTN_REC & 1u) {              /* REC: gamepad -> notes -> armed -> gamepad; recording -> gamepad, */
            if (kb == KB_REC) rec_stop();       /* playback goes on (app.js recButton) */
            else {
                kb = kb == KB_ARMED ? KB_PAD : kb + 1u;
                say(kb == KB_NOTES ? "keys: notes (practise)" : kb == KB_ARMED ? "armed: PLAY to record" : "keys: gamepad");
            }
            if (!kb) key_want = 0;
            dirty |= DIRTY_BAR;
        }
        if (kb == KB_REC && rec_wait && POS_ROW(ch_pos[ui.ch]) == 0u && rec_row_was != 0u) rec_wait = 0;   /* a phrase starts */
        rec_row_was = POS_ROW(ch_pos[ui.ch]);
        if (kb == KB_REC && mode == M_CHAIN && (rec_spare == EMPTY || phrase_used(rec_spare) || !phrase_empty(rec_spare)))
            rec_spare = (uint8_t)next_free_phrase(EMPTY);   /* (ready for the ISR when the chain reaches an empty row) */
        if (kb == KB_REC && !playing && !play_req && !count_left) rec_stop();   /* (stopped some other way) */
        if (!(fm1_in.buttons >> BTN_EDIT & 1u) && btn & (1u << BTN_OCT_DOWN | 1u << BTN_OCT_UP)) {   /* the octave */
            char t[12];
            oct += btn >> BTN_OCT_UP & 1u ? 1 : -1;
            oct = clamp_i(oct, -3, 3);
            str_cpy(t, "octave ", sizeof t); sn_n(t, 4 + oct, sizeof t);
            say(t);
        }
#if BOARD_SMK37
        smk_notes(&dirty);                      /* every key a note, OFF on a pad (smk_keys.c) */
#else
        if (kb) {                               /* notes mode (app.js keyDown): A#3..C#5 notes, F#3 a note off; the
                                                 * gamepad keys work as ever. Stopped, on the phrase: written at the
                                                 * cursor, which moves down (step entry) */
            for (k = 1; k <= 20u; k++) {
                uint32_t b = 1u << k, note = k == 1u ? NOTE_OFF : KEY_BASE + k + (uint32_t)(12 * oct);
                if (PAD_KEYS >> k & 1u) continue;   /* (the d-pad's G3 G#3 A3) */
                if (kup & b) key_want &= ~b;
                if (!(down & b) || (note != NOTE_OFF && (note < NOTE_MIN || note > NOTE_MAX))) continue;
                if (kb == KB_REC) rec_write(note);
                else if (kb == KB_NOTES && !playing && ui.view == V_PHRASE && !ui.on_row && !ui.on_head && ensure_phrase() != EMPTY) {
                    row_t *w = &cur_phrase_rows()[ui.row];
                    if (note != NOTE_OFF && (w->n == EMPTY || w->n == NOTE_OFF) && w->vol == EMPTY) w->vol = last.vol;
                    w->n = (uint8_t)note;
                    if (w->i == EMPTY && note != NOTE_OFF) w->i = last.inst;
                    if (note != NOTE_OFF) { last.note = (uint8_t)note; last.inst = w->i; last.ni = NONE; }
                    ui.row = (ui.row + S.pj.step) % 16u;   /* STEP (G page): 0 stays */
                    dirty |= DIRTY_ALL;
                }
                if (note == NOTE_OFF) continue;
                key_note[k] = (uint8_t)note;
                key_ins[k] = (uint8_t)(IS_PAGE(ui.view) ? ui.inst : last.inst);
                key_ch[k] = (uint8_t)ui.ch;
                key_want |= b;
            }
        }
#endif
        if (!(fm1_in.buttons >> BTN_EDIT & 1u) && btn & (1u << BTN_ENV | 1u << BTN_LFO | 1u << BTN_FX)) {
            /* panel ENV / LFO / FX: the focus instrument's E / L / V page (app.js 'jump'); FX again on V: F */
            uint32_t v = btn >> BTN_ENV & 1u ? V_ENV : btn >> BTN_LFO & 1u ? V_LFO : V_VOX;
            if (v == V_VOX && ui.view == V_VOX) { ui.back = V_VOX; v = V_FX; }   /* (F then hangs off P) */
            if (!IS_PAGE(ui.view)) ui.inst = focus_inst();
            set_view(v);
            screen_change();
            dirty |= DIRTY_ALL;
        }

        if (!IS_PAGE(ui.view)) copy_pending = 0;   /* (a copy only from the instrument pages) */
        if (down >> PAD_EDIT & 1u) kn_assigned = 0;   /* (a new EDIT hold: a knob assigns again) */
        if ((dx || dy) && keys >> PAD_SHIFT & 1u) {
            if (dx > 0 && param_jump()) { rows_clear(); dirty |= DIRTY_ALL; }
            else dirty |= go_screen(dx, dy);
        } else if (ui.view == V_PROJ) {
            dirty |= proj_input(keys, down, dx, dy, prev_edit && !(keys >> PAD_EDIT & 1u));
        } else if (ui.view == V_MIX) {
            dirty |= mix_input(keys, down, dx, dy);
        } else if (ui.view == V_FX) {
            dirty |= fx_input(keys, down, dx, dy);
        } else if (ui.view == V_Q) {
            dirty |= q_input(keys, down, dx, dy, prev_edit && !(keys >> PAD_EDIT & 1u));
        } else if (!IS_PAGE(ui.view)) {         /* the song, chain and phrase screens */
            dirty |= grid_input(keys, down, dx, dy);
        } else {                                /* the instrument page */
            dirty |= page_input(keys, down, dx, dy, prev_edit && !(keys >> PAD_EDIT & 1u), field_was);
        }

        if (ui.view == V_FX || ui.view == V_MIX) dirty |= row_knobs(keys);
        else if (!IS_PAGE(ui.view)) dirty |= knob_input(keys);
        if (ks.on && (IS_PAGE(ui.view) || fm1_ms - ks.until < 0x80000000u)) ks.on = 0;   /* (over: the rows come back) */
        if (ks.on && playing && held_gen != ks.gen && fm1_ms - ks.t >= 100u) { ks.gen = held_gen; ks.t = fm1_ms; ks.drawn = 0; }

        if (ui.view == V_LFO && fm1_ms - lfo_t >= 50u) { lfo_t = fm1_ms; dirty |= DIRTY_GRAPH; }   /* (20 a second) */
        if (IS_PAGE(ui.view)) {                 /* live values: when they change (at most every 100 ms), and EDIT */
            static uint32_t shown_held, t_held;
            uint8_t e = (uint8_t)(keys >> PAD_EDIT & 1u);
            if (e != edit_down) { edit_down = e; dirty |= DIRTY_ALL; }
            if (held_gen != shown_held && fm1_ms - t_held >= 100u) { shown_held = held_gen; t_held = fm1_ms; dirty |= DIRTY_ALL; }
        }
        if (IS_PAGE(ui.view) && dirty) live_snapshot(ui.inst);   /* the page and its bottom line: the same values */
        {   /* the battery (every 100 ms, a slow average: Felucca's main.c) and its mark; MIDI in, drained (the tracker plays
             * no MIDI yet; the SYSTEM popup shows it arrive); the popup's values every 250 ms */
            static uint32_t batt_t, shown_batt;
            uint8_t a, b, c;
            if (fm1_ms - batt_t >= 100u) {
                int32_t v = fm1_adc_read(FM1_ADC_BATT);
                batt_t = fm1_ms;
                if (v > 0) { batt_acc = batt_acc ? batt_acc + v - (batt_acc >> 5) : v << 5; batt_raw = batt_acc >> 5; }
            }
            if (batt_state() != shown_batt) { shown_batt = batt_state(); dirty |= DIRTY_BAR; }
            while (midi_get(&a, &b, &c)) midi_in_ms = fm1_ms | 1u;
            if (sys_open && cpu_pm > sys_pk) sys_pk = cpu_pm;
            if (sys_open && ui.view == V_PROJ && fm1_ms - sys_shown >= 250u) { sys_shown = fm1_ms; draw_bands(SYS_Y, SYS_H, proj_band); }
        }
        dirty = overlay_draw(keys, dirty);
        leds_update();
        if (dirty && mark_check()) dirty |= DIRTY_ALL;   /* (an edit, a take, undo: each sets dirty) */
        if (dirty) { redraw(dirty); dirty = 0; }
        if (msg_dirty) { draw_strip(); msg_dirty = 0; }
    }
}
