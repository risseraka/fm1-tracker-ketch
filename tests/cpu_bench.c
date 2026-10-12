/* SPDX-License-Identifier: GPL-3.0-only */
/* The audio interrupt's cost on a computer, for the device estimate in docs/research/cpu-budget.md (Q5).
 * firmware/src/tracker.c as the host build has it (tests/host.c; its main renamed), driven here instead of by a
 * script: per half buffer (HALF_FRAMES = 256 frames, the device's) the platform ISR's work (platform.c
 * fm1_alnk0_irq less the USB tap) and app_audio, counted in user-space instructions by the kernel (Linux
 * perf_event_open; Felucca's regress.c counts the same way on a Mac). 1 s to settle, then 2 s counted.
 *
 *   tests/cpu_bench.sh                       (builds and runs this in Docker; see there)
 *
 * Scenarios: the player playing a phrase with a note on every row (16ths at 150 BPM) on all 4 channels, each
 * instrument at its worst: every send (DIST CHO DLY REV) 127 and the delay's MIX 127, the chorus and reverb on.
 *   4v  each instrument MONO (LEGATO): one voice a channel, 4 in all (the tracker's usual case)
 *   8v  each instrument POLY with RELEASE 127: the notes' tails overlap until Felucca's 8-voice budget is full
 *   anchor/C4 held  what tools/emu-device counts on the device build (one key held on the new song's channel 1)
 *   felucca  Felucca's own CPU scenario (regress.c job_cpu: part 1, POLY SUS 127, 8 notes held, no player, the
 *       preset's own sends), to compare this computer's counts with tests/cpu_baseline.txt (the Mac's) */
#define main host_main
#include "../firmware/src/tracker.c"
#undef main
#include <time.h>
#define HALF 256u                               /* platform.c HALF_FRAMES (sound.c undefines it, core.h has Felucca's 128) */
#ifdef __linux__
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <unistd.h>
static int pe_fd = -1;
static void instr_init(void)
{
    struct perf_event_attr a;
    memset(&a, 0, sizeof a);
    a.type = PERF_TYPE_HARDWARE;
    a.size = sizeof a;
    a.config = PERF_COUNT_HW_INSTRUCTIONS;
    a.exclude_kernel = 1;
    a.exclude_hv = 1;
    pe_fd = (int)syscall(SYS_perf_event_open, &a, 0, -1, -1, 0);
}
static uint64_t instr_now(void)
{
    uint64_t c = 0;
    return pe_fd >= 0 && read(pe_fd, &c, sizeof c) == (ssize_t)sizeof c ? c : 0;
}
#else
static void instr_init(void) {}
static uint64_t instr_now(void) { return 0; }
#endif

static int32_t hb[2u * HALF];
static uint64_t half(void)                      /* platform.c fm1_alnk0_irq's half, without the hardware and USB */
{
    uint64_t i0 = instr_now();
    uint32_t i;
    for (i = 0; i < 2u * HALF; i++) hb[i] = 0;
    app_audio(hb, HALF);
    for (i = 0; i < 2u * HALF; i++) {
        int32_t v = (hb[i] * 2048) >> 12;          /* (the MASTER knob at half: platform.c master_q12) */
        hb[i] = v > 32767 ? 32767 : v < -32767 ? -32767 : v;
    }
    for (i = 0; i < 2u * HALF; i++) hb[i] <<= 7;
    __asm__ volatile("" ::"r"(hb) : "memory");
    return instr_now() - i0;
}

static uint32_t voices(void)
{
    uint32_t p, v, n = 0;
    for (p = 0; p < NPART; p++)
        for (v = 0; v < NVOICE; v++) n += trk[p].v[v].active;
    return n;
}

/* settle s halves, count c: per half the average and the most host instructions, and the voices sounding */
static void measure(const char *name, uint32_t s, uint32_t c)
{
    uint64_t sum = 0, mx = 0, n;
    uint32_t k, vmax = 0, vsum = 0;
    struct timespec t0, t1;
    for (k = 0; k < s; k++) half();
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (k = 0; k < c; k++) {
        n = half();
        sum += n;
        if (n > mx) mx = n;
        vsum += voices();
        if (voices() > vmax) vmax = voices();
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("%-34s avg %8llu max %8llu  voices %.1f max %u  ns/half %.0f\n", name, (unsigned long long)(sum / c),
           (unsigned long long)mx, (double)vsum / c, vmax,
           ((t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec)) / c);
}

static void silence(void)                       /* every voice gone: the player stopped, the parts cut, faded */
{
    uint32_t c, k;
    play_req = 2;
    half();
    for (c = 0; c < NCH; c++) part_cut(c);
    for (k = 0; k < 4u; k++) half();
    for (c = 0; c < NPART; c++) {
        uint32_t v;
        for (v = 0; v < NVOICE; v++) trk[c].v[v].active = 0;
    }
}

#define S_HALVES (FS / HALF)                    /* 1 s settle */
#define C_HALVES (2u * FS / HALF)               /* 2 s counted */

static void fx_worst(void)
{
    song.g[G_DMIX] = 127; song.g[G_DFDBK] = 100; song.g[G_CDEPTH] = 127; song.g[G_RSIZE] = 127;
}

/* the player: phrase c on channel c's song row 0 / chain row 0, a note every row with instrument c, 16 rows looped */
static void player_song(uint32_t bpm)
{
    uint32_t c, r;
    song_new();
    S.tempo = (uint8_t)bpm;
    for (c = 0; c < NCH; c++) {
        S.song[0][c] = (uint8_t)c;
        S.chains[c][0].ph = (uint8_t)c;
        for (r = 0; r < 16u; r++) {
            S.phrases[c][r].n = (uint8_t)(48u + 12u * (c & 1u) + (r * 7u + c * 3u) % 12u);
            S.phrases[c][r].i = (uint8_t)c;
        }
    }
}

static void ins_set(uint32_t i, uint32_t ei, uint32_t pi, int poly)
{
    uint32_t k;
    ins_engine(i, ei);
    ins_preset(i, pi);
    for (k = P_DIST; k <= P_REV; k++) ins[i].p[k] = 127;
    ins[i].p[P_VOICE] = (int16_t)(poly ? V_POLY : V_LEGATO);
    if (poly) { ins[i].p[P_SUS] = 127; ins[i].p[P_REL] = 127; }
    ins_gen[i]++;
}

static void play_4ch(const char *name, const uint8_t ei[4], const uint8_t pi[4], int poly)
{
    uint32_t c;
    silence();
    player_song(150);
    for (c = 0; c < NCH; c++) ins_set(c, ei[c], pi[c], poly);
    fx_worst();
    play_mode = M_SONG; play_from_song = 0; play_count = 0;
    play_req = 1;
    measure(name, S_HALVES, C_HALVES);
}

static void felucca_job(const char *name, uint32_t ei, uint32_t pi)
{
    static const uint8_t NOTES[8] = {48, 52, 55, 59, 60, 64, 67, 71};
    static const uint8_t KIT[8] = {36, 38, 39, 42, 46, 45, 37, 56};
    const engine_t *e = ENGINES[ei];
    uint32_t i, k;
    silence();
    song.g[G_DMIX] = GP[G_DMIX].def; song.g[G_DFDBK] = GP[G_DFDBK].def;
    song.g[G_CDEPTH] = GP[G_CDEPTH].def; song.g[G_RSIZE] = GP[G_RSIZE].def;
    ins_engine(0, ei);
    ins_preset(0, pi);
    part_take(0, 0);
    part_load(&trk[0], 0, 1);
    trk[0].engine = trk[0].eng_req;
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_SUS] = 127;
    trk[0].p[P_AMODE] = 0;
    for (i = 0; i < 8u; i++) trk_note_on(&trk[0], e->oneshot ? KIT[i] : NOTES[i], 100);
    if (!e->oneshot) { measure(name, S_HALVES / 2u, C_HALVES / 2u); return; }
    for (k = 0; k < S_HALVES / 2u; k++) {       /* hits: struck again every 64 blocks of 32 (Felucca's) */
        if (k % 8u == 7u) for (i = 0; i < 8u; i++) trk_note_on(&trk[0], KIT[i], 100);
        half();
    }
    measure(name, 0, C_HALVES / 2u);
}

int main(int argc, char **argv)
{
    uint32_t e, p;
    char name[64];
    instr_init();
    if (!instr_now()) printf("no instruction counter here (perf_event_open): the ns only\n");
    song_new();
    instruments_new();
    sound_init();
    sound_ready = 1;
    memset((void *)ramp_ph, EMPTY, sizeof ramp_ph);
    memset(held_ch, EMPTY, sizeof held_ch);
    memset(entry_row, EMPTY, sizeof entry_row);
    printf("host instructions per half buffer (%u frames; device: %.0f us)\n", HALF, HALF * 1e6 / 44118.0);
    measure("idle", S_HALVES, C_HALVES);
    part_take(0, 0);                            /* tools/emu-device's held key: REC (notes mode), C4 on channel 1 */
    trk_note_on(&trk[0], 60, VOL_MAX);
    measure("anchor/C4 held", S_HALVES / 4u, C_HALVES);
    trk_note_off(&trk[0], 60);
    {
        uint8_t ei[4] = {(uint8_t)engine_by_name("ANALOG"), (uint8_t)engine_by_name("FM6"), (uint8_t)engine_by_name("LOFI"),
                         (uint8_t)engine_by_name("DRUM")};
        uint8_t pi[4] = {(uint8_t)preset_by_name(ei[0], "ACID"), (uint8_t)preset_by_name(ei[1], "PAD"),
                         (uint8_t)preset_by_name(ei[2], "PULSE LD"), 0};
        play_4ch("song/new-song-sounds 4v", ei, pi, 0);
        play_4ch("song/new-song-sounds 8v", ei, pi, 1);
    }
    for (e = 0; e < NENGINES; e++) {
        const engine_t *en = ENGINES[e];
        if (!eng_ok(e) || !en->npresets) continue;
        for (p = 0; p < en->npresets; p++) {
            uint8_t ei[4] = {(uint8_t)e, (uint8_t)e, (uint8_t)e, (uint8_t)e}, pi[4] = {(uint8_t)p, (uint8_t)p, (uint8_t)p, (uint8_t)p};
            if (argc > 1 && strcmp(argv[1], en->name)) continue;
            snprintf(name, sizeof name, "felucca/%s/%02u_%s", en->name, p, en->presets[p].name);
            felucca_job(name, e, p);
            snprintf(name, sizeof name, "4ch/%s/%02u_%s 4v", en->name, p, en->presets[p].name);
            play_4ch(name, ei, pi, 0);
            snprintf(name, sizeof name, "4ch/%s/%02u_%s 8v", en->name, p, en->presets[p].name);
            play_4ch(name, ei, pi, 1);
        }
    }
    return 0;
}
