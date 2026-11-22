/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument (bop.h). One compilation unit, -O2: this file includes the building blocks, the voice, the
 * pads, the effects and the generator, which are only ever part of it. */
#include "bop.h"
#include "fastmath.h"

#ifdef EN_HOST
#define EN_POOL
#else
#define EN_POOL __attribute__((section(".pool")))
#endif
#define BARRIER() __asm__ volatile("" ::: "memory")

static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* ------------------------------------------------------------ shared state --- */
en_song_t en_song;
en_glob_t en_glob;
volatile uint8_t en_safe = 1, en_arp_live, en_duck = 1;
volatile uint8_t en_playing, en_ext, en_recording, en_step, en_tick, en_part_playing, en_chain_pos, en_bar;
volatile uint8_t en_chord_root, en_chord_minor, en_tune_next, en_live_on, en_trick_now;
volatile uint32_t en_steps, en_rec_count;
volatile uint8_t en_lay_on[EN_NLAYER], en_lay_note[EN_NLAYER], en_pad_on[EN_NPAD];
volatile float en_level[EN_NLAYER], en_env[EN_NLAYER], en_lfo[EN_NLAYER];
volatile uint8_t en_scope_req;
float en_scope[3][EN_SCOPE_N];
volatile float en_scope_hz;
uint32_t (*en_time)(void);
volatile uint32_t en_prof[PR_N];
#ifdef BOP_BENCH
volatile uint32_t en_prof_worst[PR_N + 1];
#endif
static uint32_t prof_t;
static inline void prof(int k)
{
    if (en_time) {
        uint32_t t = en_time();
        en_prof[k] += t - prof_t;
        prof_t = t;
    }
}
static float tunefac = 1.0f;

const char *const EN_LAYER_NAME[EN_NLAYER] = {"Beats", "Bass", "Tune", "Sparkle"};

#include "bop_dsp.h"
#include "bop_voice.c"
#include "bop_drums.c"
#include "bop_fx.c"
#include "bop_gen.c"

/* ------------------------------------------------------------- the patch --- */
static const en_pk_t PK[PK_N] = {
    {"Wave", "shape", 0, 100, 0},        {"Wave 2", "shape 2", 0, 100, 0}, {"Tune 2", "tune 2", 0, 48, 24},
    {"Detune", "fat", 0, 50, 0},         {"Mix", "1 or 2", 0, 100, 0}, {"Sub", "deep", 0, 100, 0},
    {"Cutoff", "bright", 0, 100, 60},    {"Resonance", "squeak", 0, 100, 10}, {"Type", "kind", 0, 2, 0},
    {"Env amt", "wah", 0, 100, 0},    {"Key track", "follow", 0, 100, 50}, {"Attack", "start", 0, 100, 0},
    {"Decay", "fall", 0, 100, 40},       {"Sustain", "hold", 0, 100, 70}, {"Release", "end", 0, 100, 30},
    {"LFO wave", "shape", 0, 3, 0}, {"Rate", "speed", 0, 100, 40}, {"Depth", "wobble", 0, 100, 0},
    {"Target", "where", 0, 2, 0},        {"Arp", "pattern", 0, 4, 0},     {"Arp rate", "speed", 0, 3, 2},
    {"Octaves", "how far", 1, 3, 1},     {"Glide", "slide", 0, 100, 0},   {"Echo", "echo", 0, 100, 0},
    {"Reverb", "room", 0, 100, 0},       {"Robot", "robot", 0, 100, 0},   {"Level", "loud", 0, 100, 80},
};
const en_pk_t *en_pk_info(int k) { return &PK[(unsigned)k < PK_N ? k : 0]; }

static char *itoa_s(int v, char *b)
{
    char t[12];
    int n = 0;
    if (v < 0) {
        *b++ = '-';
        v = -v;
    }
    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    while (n)
        *b++ = t[--n];
    *b = 0;
    return b;
}
static char *scopy(char *d, const char *s)
{
    while (*s)
        *d++ = *s++;
    *d = 0;
    return d;
}
static char *fixed1(char *b, float x)            /* "1.2" */
{
    int v = (int)(x * 10.0f + 0.5f);
    b = itoa_s(v / 10, b);
    *b++ = '.';
    *b++ = (char)('0' + v % 10);
    *b = 0;
    return b;
}
static void time_text(char *b, float secs)
{
    if (secs < 1.0f)
        scopy(itoa_s((int)(secs * 1000.0f + 0.5f), b), " ms");
    else
        scopy(fixed1(b, secs), " s");
}

static const char *const WAVE_NAME[5] = {"Sine", "Triangle", "Saw", "Square", "Noise"};
const char *en_wave_name(int v) { return WAVE_NAME[clampi((v + 12) / 25, 0, 4)]; }

void en_pk_text(int k, int v, char *b)
{
    switch (k) {
    case PK_WAVE1:
    case PK_WAVE2: {
        static const char *const SH[5] = {"Sin", "Tri", "Saw", "Sqr", "Nse"};
        int i = v / 25, f = v % 25;
        if (f <= 3 || f >= 22 || i >= 4)
            scopy(b, en_wave_name(v));
        else
            scopy(scopy(scopy(b, SH[i]), "+"), SH[i + 1]);
        break;
    }
    case PK_TUNE2: {
        int st = v - 24;
        if (st > 0)
            *b++ = '+';
        scopy(itoa_s(st, b), " st");
        break;
    }
    case PK_DETUNE: scopy(itoa_s(v, b), " ct"); break;
    case PK_CUT: {
        float hz = 40.0f * fm_exp2f(8.64f * (float)v * 0.01f);
        if (hz >= 1000.0f)
            scopy(fixed1(b, hz * 0.001f), " kHz");
        else
            scopy(itoa_s((int)hz, b), " Hz");
        break;
    }
    case PK_FTYPE: {
        static const char *const T[3] = {"Low pass", "Band pass", "High pass"};
        scopy(b, T[clampi(v, 0, 2)]);
        break;
    }
    case PK_A: time_text(b, exp01(v, 0.001f, 2.5f)); break;
    case PK_D: time_text(b, exp01(v, 0.02f, 4.0f)); break;
    case PK_R: time_text(b, exp01(v, 0.015f, 5.0f)); break;
    case PK_LWAVE: {
        static const char *const T[4] = {"Sine", "Triangle", "Square", "Random"};
        scopy(b, T[clampi(v, 0, 3)]);
        break;
    }
    case PK_LRATE: scopy(fixed1(b, exp01(v, 0.1f, 12.0f)), " Hz"); break;
    case PK_LTARGET: {
        static const char *const T[3] = {"Pitch", "Filter", "Volume"};
        scopy(b, T[clampi(v, 0, 2)]);
        break;
    }
    case PK_ARP: {
        static const char *const T[5] = {"Off", "Up", "Down", "Up+down", "Random"};
        scopy(b, T[clampi(v, 0, 4)]);
        break;
    }
    case PK_ARATE: {
        static const char *const T[4] = {"1/4", "1/8", "1/16", "1/32"};
        scopy(b, T[clampi(v, 0, 3)]);
        break;
    }
    case PK_AOCT: scopy(itoa_s(v, b), v == 1 ? " oct" : " octs"); break;
    default: itoa_s(v, b); break;
    }
}

/* ---------------------------------------------------------------- settings --- */
static const int16_t GLOB_LO[G_N] = {10, 0, 0, 1, 0, -50}, GLOB_HI[G_N] = {100, 1, 1, 16, 1, 50},
                     GLOB_DEF[G_N] = {70, 0, 0, 1, 0, 0};
void en_glob_defaults(void)
{
    int i;
    for (i = 0; i < G_N; i++)
        en_glob.v[i] = GLOB_DEF[i];
}
int en_glob_ok(int i, int v) { return i >= 0 && i < G_N && v >= GLOB_LO[i] && v <= GLOB_HI[i]; }

int en_song_valid(const en_song_t *s)
{
    int l, k, p, t, i;
    if (s->style >= EN_NSTYLE || s->prog >= EN_NPROG || s->key > 11 || s->scale >= EN_NSCALE || s->tempo < 50 ||
        s->tempo > 200 || s->swing > 100 || s->echo_time > 100 || s->echo_fb > 100 || s->chain_n < 1 ||
        s->chain_n > EN_CHAIN_MAX)
        return 0;
    for (i = 0; i < s->chain_n; i++)
        if (s->chain[i] >= EN_NPART)
            return 0;
    for (k = 0; k < EN_NPAD; k++)
        if (s->dk[k][0] > 100 || s->dk[k][1] > 100)
            return 0;
    for (l = 0; l < EN_NLAYER; l++) {
        const en_layer_t *L = &s->lay[l];
        if (L->who >= 2 * EN_NFRIEND || L->oct < -2 || L->oct > 2 || L->mute > 1)
            return 0;
        for (k = 0; k < PK_N; k++)
            if (L->p[k] > PK[k].hi)
                return 0;
    }
    for (p = 0; p < EN_NPART; p++)
        for (t = 0; t < EN_NSYN; t++)
            for (i = 0; i < EN_NSTEP; i++) {
                const en_note_t *e = &s->part[p].syn[t][i];
                if (e->note > 127 || e->len < 1 || e->len > 16 || e->fl > 3)
                    return 0;
            }
    for (i = 0; i < EN_LOOP; i++)
        if (s->loop[i].note[0] > 127 || s->loop[i].note[1] > 127 || s->loop[i].len < 1 || s->loop[i].len > 16 ||
            s->loop[i].pads_hi > 7)
            return 0;
    return 1;
}

/* ---------------------------------------------------------------- commands --- */
enum { C_PLAY, C_NOTE, C_PAD, C_REC, C_TRICK, C_PART, C_CHAIN, C_PANIC, C_FADE, C_PREVIEW, C_CLOCK };
#define QN 128u
static uint32_t q[QN];
static volatile uint32_t q_w, q_r;
static void post(uint32_t c, int a, int b)
{
    uint32_t w = q_w;
    if (w - q_r >= QN)
        return;
    q[w % QN] = c << 24 | ((uint32_t)a & 0xFFu) << 16 | ((uint32_t)b & 0xFFFFu);
    BARRIER();
    q_w = w + 1;
}
void en_play(int on) { post(C_PLAY, on, 0); }
void en_note(int layer, int note, int on) { post(C_NOTE, layer, (note & 0x7F) | (on ? 0x80 : 0)); }
void en_pad(int pad, int acc) { post(C_PAD, pad, acc); }
void en_rec(int mode, int layer) { post(C_REC, mode, layer); }
void en_trick(int t) { post(C_TRICK, t, 0); }
void en_part_now(int p) { post(C_PART, p, 0); }
void en_chain_play(void) { post(C_CHAIN, 0, 0); }
void en_panic(void) { post(C_PANIC, 0, 0); }
void en_fade_in(void) { post(C_FADE, 0, 0); }
void en_preview(int layer, int note) { post(C_PREVIEW, layer, note); }
void en_clock(int msg) { post(C_CLOCK, msg, 0); }

/* ================================================================ the sequencer === */
#define TPS 24
static float step_samples = 5512.5f, tick_len = 229.7f, tick_left, tempo_fac = 1.0f;
static int vtick, part_force = -1, chain_pos, chain_restart, started;
static int rec_mode, rec_layer;
static int duck_steps;                            /* the song's tune makes room for the child's */
static float pad_duck, pad_duck_g = 1.0f;         /* ... and the whole song for a moment, for the child's pads */
static int tune_left;                             /* the song's tune: steps its note lasts (play along) */
static int stut_s0 = -1;                          /* the stutter: the eighth it holds */
static int skip_pos = -1;                         /* a step live recording already played (this pass) */
static uint16_t skip_pads;
static uint8_t skip_syn;
static float mg[EN_NLAYER] = {1.0f, 1.0f, 1.0f, 1.0f};
static int moff_n[8], moff_left[8];

static int key_off(void)
{
    int k = clampi(en_song.key, 0, 11);
    return k <= 6 ? k : k - 12;
}
static int part_now(void) { return part_force >= 0 ? part_force : en_song.chain[clampi(chain_pos, 0, EN_CHAIN_MAX - 1)] % EN_NPART; }
static int root_off(void)                         /* the chord's root as a move from C, -5 .. +6 */
{
    int r, m;
    en_prog_chord(en_song.prog, part_now(), &r, &m);
    return r <= 6 ? r : r - 12;
}
static int chord_minor(void)
{
    int r, m;
    en_prog_chord(en_song.prog, part_now(), &r, &m);
    return m;
}

int en_follow(int layer, int note)
{
    int n = note;
    if (layer == L_BASS || layer == L_SPARK) {
        if (chord_minor()) {                      /* the scale bent to the chord: C's third, sixth, seventh down */
            int pc = ((n % 12) + 12) % 12;
            if (pc == 4 || pc == 9 || pc == 11)
                n--;
        }
        n += root_off();
    }
    if (layer >= L_BASS)
        n += key_off() + 12 * en_song.lay[layer].oct;
    return n;
}
static int unfollow(int layer, int heard)         /* what a heard note is written as */
{
    int n = heard;
    if (layer == L_BASS || layer == L_SPARK)
        n -= root_off();
    if (layer >= L_BASS)
        n -= key_off() + 12 * en_song.lay[layer].oct;
    return clampi(n, 1, 127);
}

static void tempo_set(void)
{
    step_samples = EN_SR * 60.0f / ((float)clampi(en_song.tempo, 50, 200) * 4.0f) / tempo_fac;
    tick_len = step_samples * (1.0f / (float)TPS);
}

static void midi_note_out(int note, int on)
{
    int ch = clampi(en_glob.v[G_MIDICH], 1, 16) - 1;
    if (en_glob.v[G_MIDIOUT])
        en_midi_out((on ? 0x90u : 0x80u) | (uint32_t)ch, (uint32_t)clampi(note, 0, 127), on ? 100u : 0u);
}

static void pad_hit(int p, float vel)
{
    if (en_poly_cap < NV_MAX) {                   /* under load: six pads at most, the oldest goes */
        int k, n = 0, old = -1;
        int32_t age = -1;
        for (k = 0; k < EN_NPAD; k++)
            if (pv[k].on && k != p) {
                n++;
                if (pv[k].t > age) {
                    age = pv[k].t;
                    old = k;
                }
            }
        if (n >= 6 && old >= 0)
            pv[old].on = 0;
    }
    pad_fire(p, vel);
    en_pad_on[p]++;
    en_lay_on[L_BEATS]++;
    en_lay_note[L_BEATS] = (uint8_t)p;
}

static void play_note(int layer, const en_note_t *e, float vel_scale)
{
    int n = en_follow(layer, e->note), k;
    float vel = ((e->fl & EN_N_ACC) ? 1.0f : 0.8f) * vel_scale;
    int32_t gate = (int32_t)((float)e->len * step_samples - 0.25f * step_samples);
    if (gate < (int32_t)(0.3f * step_samples))
        gate = (int32_t)(0.3f * step_samples);
    syn_on(layer - 1, n, vel, gate);
    if (e->fl & EN_N_CHORD)                       /* a chord: its third and fifth over C, bent to the chord */
        for (k = 1; k < 3; k++)
            syn_on(layer - 1, en_follow(layer, e->note + (k == 1 ? 3 : 7)), vel * 0.8f, gate);
    en_lay_on[layer]++;
    en_lay_note[layer] = (uint8_t)n;
}

/* the step that plays now: its pads, its notes, the loop's */
static void fire_step(int s)
{
    const en_part_t *P = &en_song.part[part_now()];
    int k, l, pos = (en_bar & 1) * 16 + s;
    float acc = (P->acc >> s & 1u) ? 1.0f : 0.72f;
    if (!en_song.lay[L_BEATS].mute)
        for (k = 0; k < EN_NPAD; k++)
            if (P->hit[k] >> s & 1u && !(pos == skip_pos && (skip_pads >> k & 1u)))
                pad_hit(k, acc);
    for (l = L_BASS; l <= L_SPARK; l++) {
        const en_note_t *e = &P->syn[l - 1][s];
        if (!e->note)
            continue;
        if (l == L_TUNE) {
            en_tune_next = (uint8_t)en_follow(L_TUNE, e->note);
            tune_left = e->len;
            if (duck_steps > 0 && en_duck)
                continue;                         /* the child is singing */
        }
        if (en_song.lay[l].mute || (pos == skip_pos && (skip_syn >> l & 1u)))
            continue;
        play_note(l, e, 1.0f);
    }
    if (en_song.loop_on) {                        /* Loop me */
        const en_loop_t *L = &en_song.loop[pos];
        uint16_t pads = (uint16_t)(L->pads_lo | L->pads_hi << 8);
        int skip = pos == skip_pos;
        for (k = 0; k < EN_NPAD; k++)
            if (pads >> k & 1u && !(skip && (skip_pads >> k & 1u)))
                pad_hit(k, 0.9f);
        if (!(skip && (skip_syn & 1u)))
            for (k = 0; k < 2; k++)
                if (L->note[k]) {
                    en_note_t e;
                    e.note = L->note[k];
                    e.len = L->len;
                    e.fl = 0;
                    play_note(L_TUNE, &e, 0.95f);
                }
    }
}

static void bar_advance(void)
{
    en_bar++;
    if (chain_restart) {
        chain_pos = 0;
        chain_restart = 0;
    } else if (part_force < 0) {
        chain_pos = (chain_pos + 1) % clampi(en_song.chain_n, 1, EN_CHAIN_MAX);
    }
    en_chain_pos = (uint8_t)chain_pos;
}

static void tick(void)
{
    int s = en_step, sw = (s & 1) ? (clampi(en_song.swing, 0, 100) * 12 + 50) / 100 : 0;
    if (vtick == 0) {
        if (s == 0 && started)
            bar_advance();
        started = 1;
        en_part_playing = (uint8_t)part_now();
        if (tune_left > 0 && --tune_left == 0)
            en_tune_next = 0;
        if (duck_steps > 0)
            duck_steps--;
        if (en_trick_now == TR_STUTTER) {         /* the stutter: the eighth it began on, again and again */
            if (stut_s0 < 0)
                stut_s0 = s & ~1;
        } else {
            stut_s0 = -1;
        }
    }
    if (vtick == sw)
        fire_step(stut_s0 >= 0 ? stut_s0 + (s & 1) : s);
    if (vtick % 4 == 0 && !en_ext && en_glob.v[G_CLOCK])
        en_midi_out(0xF8, 0, 0);
    en_tick = (uint8_t)vtick;
    if (++vtick >= TPS) {
        int k;
        vtick = 0;
        if (skip_pos == (en_bar & 1) * 16 + s)
            skip_pos = -1;
        en_steps++;
        en_step = (uint8_t)((s + 1) % EN_NSTEP);
        for (k = 0; k < 8; k++)                   /* MIDI notes out: their offs */
            if (moff_left[k] > 0 && --moff_left[k] == 0)
                midi_note_out(moff_n[k], 0);
    }
}

/* ------------------------------------------------------------ live input --- */
static int rec_pos(int *step)                     /* where a note played now goes (the nearest sixteenth) */
{
    int s = en_step, bar = en_bar;
    if (vtick >= TPS / 2) {
        s++;
        if (s >= EN_NSTEP) {
            s = 0;
            bar++;
        }
    }
    *step = s;
    return (bar & 1) * 16 + s;
}
static void mark_skip(int pos, int s, int pad, int lay)
{
    if (s == en_step && vtick < TPS / 2)
        return;                                   /* this step: it already fired */
    if (skip_pos != pos) {
        skip_pos = pos;
        skip_pads = 0;
        skip_syn = 0;
    }
    if (pad >= 0)
        skip_pads |= (uint16_t)(1u << pad);
    if (lay >= 0)
        skip_syn |= (uint8_t)(1u << lay);
}

static struct {
    uint8_t on, layer, note;
    int8_t pos;                                   /* the loop's step, or the part's */
    uint8_t part;
    uint32_t t0;
} recn[6];
static uint32_t abs_ticks;

static void live_pad(int p, int acc)
{
    int s, pos;
    if (p < 0 || p >= EN_NPAD)
        return;
    pad_hit(p, acc ? 1.0f : 0.85f);
    pv[p].live = 1;
    if (en_duck) {
        pv[p].g *= PAD_LIVE[p];
        pad_duck = 1.0f;                          /* Play: the song makes room for it */
    }
    en_live_on++;
    if (!en_recording || !en_playing)
        return;
    pos = rec_pos(&s);
    if (rec_mode == REC_LOOP) {
        en_loop_t *L = &en_song.loop[pos];
        if (p < 8)
            L->pads_lo |= (uint8_t)(1u << p);
        else
            L->pads_hi |= (uint8_t)(1u << (p - 8));
        en_song.loop_on = 1;
        mark_skip(pos, s, p, -1);
    } else {
        en_part_t *P = &en_song.part[part_now()];
        P->hit[p] |= (uint16_t)(1u << s);
        if (acc)
            P->acc |= (uint16_t)(1u << s);
        mark_skip(pos, s, p, -1);
    }
    en_rec_count++;
}

static void live_note(int layer, int note, int on)
{
    int l = layer - 1, i;
    if (layer < L_BASS || layer > L_SPARK)
        return;
    if (on) {
        syn_key(l, note, 1);
        en_lay_on[layer]++;
        en_lay_note[layer] = (uint8_t)note;
        en_live_on++;
        if (layer == L_TUNE)
            duck_steps = 32;
        midi_note_out(note, 1);
        if (en_recording && en_playing && (rec_mode == REC_LOOP ? layer == L_TUNE : layer == rec_layer)) {
            int s, pos = rec_pos(&s), w = unfollow(layer, note);
            if (rec_mode == REC_LOOP) {
                en_loop_t *L = &en_song.loop[pos];
                if (!L->note[0] || L->note[0] == w)
                    L->note[0] = (uint8_t)w;
                else
                    L->note[1] = (uint8_t)w;
                L->len = 1;
                en_song.loop_on = 1;
                mark_skip(pos, s, -1, 0);
            } else {
                en_note_t *e = &en_song.part[part_now()].syn[l][s];
                e->note = (uint8_t)w;
                e->len = 1;
                e->fl = 0;
                mark_skip(pos, s, -1, layer);
            }
            for (i = 0; i < 6; i++)
                if (!recn[i].on) {
                    recn[i].on = 1;
                    recn[i].layer = (uint8_t)layer;
                    recn[i].note = (uint8_t)note;
                    recn[i].pos = (int8_t)(rec_mode == REC_LOOP ? pos : s);
                    recn[i].part = (uint8_t)(rec_mode == REC_LOOP ? 0xFF : part_now());
                    recn[i].t0 = abs_ticks;
                    break;
                }
            en_rec_count++;
        }
    } else {
        syn_key(l, note, 0);
        midi_note_out(note, 0);
        for (i = 0; i < 6; i++)                   /* its length, if it was written */
            if (recn[i].on && recn[i].note == note && recn[i].layer == layer) {
                int len = clampi((int)((abs_ticks - recn[i].t0 + TPS / 2) / TPS), 1, 16);
                recn[i].on = 0;
                if (recn[i].part == 0xFF)
                    en_song.loop[recn[i].pos & 31].len = (uint8_t)len;
                else
                    en_song.part[recn[i].part].syn[l][recn[i].pos & 15].len = (uint8_t)len;
            }
    }
}

/* ------------------------------------------------------------- transport --- */
static int ext_pulses;
static float ext_iv = 920.0f, ext_since;
static uint32_t ext_age;

static void quiet_all(void)
{
    int l;
    pads_quiet();
    for (l = 0; l < EN_NSYN; l++)
        syn_quiet(l);
}

static void play_start(void)
{
    started = 0;
    vtick = 0;
    en_step = 0;
    chain_pos = 0;
    chain_restart = 0;
    en_chain_pos = 0;
    en_bar = 0;
    tick_left = 0.0f;
    ext_pulses = 0;
    skip_pos = -1;
    en_playing = 1;
    en_steps = 0;
    en_part_playing = (uint8_t)part_now();
    if (!en_ext && en_glob.v[G_CLOCK])
        en_midi_out(0xFA, 0, 0);
}

static void play_stop(void)
{
    int l;
    en_playing = 0;
    en_tune_next = 0;
    for (l = 0; l < EN_NSYN; l++)
        syn_quiet(l);
    if (!en_ext && en_glob.v[G_CLOCK])
        en_midi_out(0xFC, 0, 0);
}

static void drain(void)
{
    while (q_r != q_w) {
        uint32_t c = q[q_r % QN];
        int a = (int)(c >> 16 & 0xFFu), b = (int)(c & 0xFFFFu);
        BARRIER();
        q_r++;
        switch (c >> 24) {
        case C_PLAY:
            if (a && !en_playing)
                play_start();
            else if (!a && en_playing)
                play_stop();
            break;
        case C_NOTE: live_note(a, b & 0x7F, (b & 0x80) != 0); break;
        case C_PAD: live_pad(a, b); break;
        case C_REC:
            rec_mode = a;
            rec_layer = b;
            en_recording = (uint8_t)(a != REC_OFF);
            break;
        case C_TRICK: en_trick_now = (uint8_t)(a <= EN_NTRICK ? a : 0); break;
        case C_PART: part_force = a < EN_NPART ? a : -1; break;
        case C_CHAIN:
            part_force = -1;
            chain_restart = 1;
            break;
        case C_PANIC: quiet_all(); break;
        case C_FADE: out_fade = 0.0f; break;
        case C_PREVIEW:
            if (a >= L_BASS && a <= L_SPARK) {
                syn_on_live(a - 1, b, 0.8f, (int32_t)(0.4f * EN_SR));
                en_lay_on[a]++;
                en_lay_note[a] = (uint8_t)b;
            }
            break;
        case C_CLOCK:
            if (en_glob.v[G_CLOCK])
                break;                            /* our own clock: the incoming one is not followed */
            en_ext = 1;
            ext_age = 0;
            if (a == EN_CLK_TICK) {
                if (en_playing) {
                    if (ext_pulses > 0)
                        while (ext_pulses-- > 0)
                            tick();
                    ext_pulses = 4;
                    if (ext_since > 50.0f)
                        ext_iv += (ext_since - ext_iv) * 0.3f;
                    ext_since = 0.0f;
                    tick_left = 0.0f;
                }
            } else if (a == EN_CLK_START) {
                play_start();
            } else if (a == EN_CLK_CONTINUE) {
                if (!en_playing)
                    play_start();
            } else if (en_playing) {
                play_stop();
            }
            break;
        }
    }
}

/* ------------------------------------------------------------------ render --- */
#define BLK 256
static float ml[BLK], mr[BLK], dl[BLK], dr[BLK], lb[BLK], es[BLK], rs[BLK], pl[BLK], pr[BLK];
static const float LAY_PAN[EN_NSYN] = {0.0f, 0.0f, 0.3f};
#define LIVE_PAD_G 3.0f                           /* Play: the child's pads, over the song */

static void render_chunk(int32_t *out, uint32_t n, float gain)
{
    uint32_t j;
    int l, pads;
    float lpk = 0.0f;
    for (j = 0; j < n; j++)
        ml[j] = mr[j] = es[j] = rs[j] = 0.0f;
    prof(PR_OTHER);
    pads = pads_run(n, dl, dr, pl, pr);
    {   /* the beats */
        const en_layer_t *B = &en_song.lay[L_BEATS];
        float lv = (float)B->p[PK_LEVEL] * 0.01f, g = lv * lv * mg[0] * 1.3f, ec = (float)B->p[PK_ECHO] * 0.005f,
              rv = (float)B->p[PK_REVERB] * 0.005f, pk = 0.0f;
        if (pads & 1) {
            for (j = 0; j < n; j++) {
                float a = dl[j] * g, b = dr[j] * g;
                ml[j] += a;
                mr[j] += b;
                es[j] += (a + b) * ec;
                rs[j] += (a + b) * rv;
            }
            for (j = 0; j < n; j += 8)
                pk = fm_maxf(pk, fm_fabsf(dl[j] * g) + fm_fabsf(dr[j] * g));
        }
        if (pads & 2) {                           /* the child's: never muted with the song's drums */
            float lg = en_duck ? LIVE_PAD_G : lv * lv * 1.3f;
            for (j = 0; j < n; j++) {
                pl[j] *= lg;
                pr[j] *= lg;
            }
            for (j = 0; j < n; j += 8)
                lpk = fm_maxf(lpk, fm_fabsf(pl[j]) + fm_fabsf(pr[j]));
        }
        en_level[0] = fm_maxf(fm_maxf(pk, lpk) * 0.7f, en_level[0] * 0.96f);
        en_env[0] = en_level[0];
    }
    prof(PR_DRUMS);
    for (l = 0; l < EN_NSYN; l++) {
        const en_layer_t *L = &en_song.lay[l + 1];
        float m = syn_live_on(l) ? 1.0f : mg[l + 1];     /* a child's key sounds on a muted layer too */
        float pan = LAY_PAN[l], gl = (pan > 0.0f ? 1.0f - pan * 0.7f : 1.0f) * m,
              gr = (pan < 0.0f ? 1.0f + pan * 0.7f : 1.0f) * m;
        float ec = (float)L->p[PK_ECHO] * 0.009f * m, rv = (float)L->p[PK_REVERB] * 0.009f * m, pk = 0.0f;
        if (syn_run(l, n, lb, step_samples)) {
            for (j = 0; j < n; j++) {
                float x = lb[j];
                ml[j] += x * gl;
                mr[j] += x * gr;
                es[j] += x * ec;
                rs[j] += x * rv;
            }
            for (j = 0; j < n; j += 8)
                pk = fm_maxf(pk, fm_fabsf(lb[j]));
        }
        en_level[l + 1] = fm_maxf(pk, en_level[l + 1] * 0.96f);
        prof(PR_BASS + l);
    }
    {   /* the song dips for the child's pads (a straight line over the chunk to where the dip is now) */
        float g0 = pad_duck_g, g1, dg;
        pad_duck *= fm_expf(-(float)n / (0.35f * EN_SR));
        g1 = 1.0f - 0.33f * pad_duck;
        if (g0 < 0.999f || g1 < 0.999f) {
            dg = (g1 - g0) / (float)n;
            for (j = 0; j < n; j++) {
                ml[j] *= g0;
                mr[j] *= g0;
                es[j] *= g0;
                rs[j] *= g0;
                g0 += dg;
            }
        }
        pad_duck_g = g1;
    }
    if (pads & 2) {
        const en_layer_t *B = &en_song.lay[L_BEATS];
        float ec = (float)B->p[PK_ECHO] * 0.005f, rv = (float)B->p[PK_REVERB] * 0.005f;
        for (j = 0; j < n; j++) {
            ml[j] += pl[j];
            mr[j] += pr[j];
            es[j] += (pl[j] + pr[j]) * ec;
            rs[j] += (pl[j] + pr[j]) * rv;
        }
    }
    sends_run(es, rs, ml, mr, n);
    prof(PR_SENDS);
    tricks_run(ml, mr, n, en_trick_now, step_samples * 16.0f / EN_SR);
    {
        float cap = (float)clampi(en_glob.v[G_VOLCAP], 10, 100) * 0.01f;
        limiter(ml, mr, out, n, gain * cap * 1.6f, cap);
    }
    prof(PR_MASTER);
}

static void control(uint32_t n)                   /* once a chunk: the slow things */
{
    int l, t = en_trick_now;
    float k = fm_minf((float)n / (0.015f * EN_SR), 1.0f), rv = 0.0f;
    for (l = 0; l < EN_NLAYER; l++)
        mg[l] += ((en_song.lay[l].mute ? 0.0f : 1.0f) - mg[l]) * k;
    bend_t = t == TR_CHIPMUNK ? 12.0f : t == TR_GIANT ? -12.0f : t == TR_SLOWMO ? -12.0f : 0.0f;
    bend_semi += (bend_t - bend_semi) * fm_minf((float)n / ((t == TR_SLOWMO ? 0.9f : 0.06f) * EN_SR), 1.0f);
    tempo_fac += ((t == TR_SLOWMO ? 0.5f : 1.0f) - tempo_fac) * fm_minf((float)n / (0.9f * EN_SR), 1.0f);
    tunefac = fm_exp2f((float)clampi(en_glob.v[G_TUNE], -50, 50) * (1.0f / 1200.0f));
    pad_key = (float)key_off();
    for (l = 0; l < EN_NLAYER; l++)
        rv = fm_maxf(rv, (float)en_song.lay[l].p[PK_REVERB] * 0.01f);
    pl_decay = 0.45f + 0.45f * rv;                /* the room grows with the reverb */
    dly_fb = (float)clampi(en_song.echo_fb, 0, 100) * 0.0085f;
    tempo_set();
    delay_time(step_samples / EN_SR);
    {
        int r, m;
        en_prog_chord(en_song.prog, part_now(), &r, &m);
        en_chord_root = (uint8_t)((r + clampi(en_song.key, 0, 11)) % 12);
        en_chord_minor = (uint8_t)m;
    }
    if (en_scope_req && scope_lay < 0) {          /* the scope: two turns of the newest voice */
        int sl = clampi(en_scope_req - 1, L_BASS, L_SPARK) - 1;
        float f = en_scope_hz > 20.0f ? en_scope_hz : 220.0f;
        scope_stride = clampi((int)(4.4f * EN_SR / (f * (float)EN_SCOPE_N) + 0.5f), 1, 16);
        scope_pos = 0;
        scope_lay = sl;
    } else if (scope_lay >= 0 && SCOPE_FULL) {
        scope_lay = -1;
        en_scope_req = 0;
    }
}

/* the governor: a block that takes most of its time gives the tune (then the sparkle) fewer voices for new
 * notes, and the pads fewer at once (they come back when it is easy again). Only where the app gives a clock (the device). */
static uint32_t gov_t0, gov_easy;
static void governor(uint32_t n)
{
    uint32_t spent, budget = (uint32_t)((float)n * (24000000.0f / EN_SR));
    if (!en_time || !n)
        return;
    spent = en_time() - gov_t0;
    if (spent * 100u > budget * 75u && en_poly_cap > 2) {
        en_poly_cap--;
        gov_easy = 0;
    } else if (spent * 100u < budget * 55u) {
        if (++gov_easy > 400u && en_poly_cap < NV_MAX) {
            en_poly_cap++;
            gov_easy = 0;
        }
    } else {
        gov_easy = 0;
    }
}

void en_render(int32_t *out, uint32_t n, uint32_t gain_q12)
{
    const float gain = (float)gain_q12 * (1.0f / 4096.0f);
    uint32_t n0 = n;
#ifdef BOP_BENCH
    uint32_t p0[PR_N], k0;
    for (k0 = 0; k0 < PR_N; k0++)
        p0[k0] = en_prof[k0];
#endif
    if (en_time)
        prof_t = gov_t0 = en_time();
    drain();
    if (en_ext && ++ext_age > 150u)
        en_ext = 0;
    while (n) {
        uint32_t k = n > BLK ? BLK : n;
        prof(PR_OTHER);
        if (en_playing) {
            if (!en_ext) {
                if (tick_left <= 0.0f) {
                    tick();
                    abs_ticks++;
                    tick_left += tick_len;
                }
                if ((float)k > tick_left)
                    k = (uint32_t)tick_left + 1u;
            } else if (ext_pulses > 0) {
                if (tick_left <= 0.0f) {
                    tick();
                    abs_ticks++;
                    ext_pulses--;
                    tick_left += ext_iv * 0.25f;
                }
                if ((float)k > tick_left)
                    k = (uint32_t)tick_left + 1u;
            }
        }
        if (k > n)
            k = n;
        prof(PR_SEQ);
        if (en_playing)
            tick_left -= (float)k;
        ext_since += (float)k;
        control(k);
        render_chunk(out, k, gain);
        out += 2 * k;
        n -= k;
    }
    governor(n0);
#ifdef BOP_BENCH
    if (en_time) {
        uint32_t all = en_time() - gov_t0;
        if (all > en_prof_worst[PR_N]) {
            for (k0 = 0; k0 < PR_N; k0++)
                en_prof_worst[k0] = en_prof[k0] - p0[k0];
            en_prof_worst[PR_N] = all;
        }
    }
#endif
}

void en_init(void)
{
    int i, l;
    for (i = 0; i <= SN; i++)
        sint[i] = fm_sinf((float)i * (FM_TWO_PI / (float)SN));
    q_r = q_w = 0;
    nz = 22222u;                                  /* (the noise from its start: the same song, the same sound) */
    plate_init();
    for (i = 0; i < DLY_N; i++)
        dly_l[i] = dly_r[i] = 0;
    pads_quiet();
    for (i = 0; i < 6; i++)
        metal_ph[i] = 0.0f;
    for (l = 0; l < EN_NSYN; l++)
        syn_reset(l);
    for (l = 0; l < EN_NLAYER; l++)
        mg[l] = en_song.lay[l].mute ? 0.0f : 1.0f;
    en_trick_now = 0;
    tricks_reset();
    pad_duck = 0.0f;
    pad_duck_g = 1.0f;
    bend_semi = bend_t = 0.0f;
    tempo_fac = 1.0f;
    out_fade = 0.0f;
    out_fade_k = 1.0f / (0.6f * EN_SR);
    limiter_reset();
    en_recording = 0;
    rec_mode = REC_OFF;
    part_force = -1;
    scope_lay = -1;
    en_poly_cap = NV_MAX;
    tempo_set();
    en_playing = 0;
}

#include "bop_demo.c"
