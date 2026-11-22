/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument on the host: the generator (every style makes a valid song, the same seed the same song,
 * different seeds different songs, the tune's strong beats on its chords, every note in the key's scale), the
 * scale lock (no key plays a note outside the scale), the safety (the output never above the volume cap, at
 * any MASTER, on the heaviest song; the resonance held back in the safe range), every friend and every pad
 * sounds and goes quiet (the black keys over the song, even with its drums off; the white keys on a muted
 * layer, and a tap shorter than an audio block), no jumps or flattened tops
 * in the sound, the tricks, Loop me and Make's recording (quantized, no double hits), the MIDI clock; no NaN.
 * With an argument (a directory) it also writes a WAV of every style and every friend, to listen to, and
 * times the render. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "bop.h"

static int fails;
#define CHECK(c, ...)                                                                                              \
    do {                                                                                                           \
        if (!(c)) {                                                                                                \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                            \
            printf(__VA_ARGS__);                                                                                   \
            printf("\n");                                                                                          \
            fails++;                                                                                               \
        }                                                                                                          \
    } while (0)

static uint32_t midi_on, midi_clock;
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2)
{
    (void)d1;
    if ((st & 0xF0u) == 0x90u && d2)
        midi_on++;
    if (st == 0xF8u)
        midi_clock++;
}

static int32_t buf[2 * 256];
static float peak, sum2;
static long frames;
static int bad;
static FILE *wav;
static uint32_t wav_n;
static void render(uint32_t n, uint32_t gain)
{
    while (n) {
        uint32_t k = n > 256u ? 256u : n, i;
        en_render(buf, k, gain);
        for (i = 0; i < 2 * k; i++) {
            float x = (float)buf[i] / 8388608.0f;
            if (x != x || x > 1.01f || x < -1.01f)
                bad++;
            if (fabsf(x) > peak)
                peak = fabsf(x);
            sum2 += x * x;
            if (wav) {
                int16_t s = (int16_t)(x * 32767.0f);
                fwrite(&s, 2, 1, wav);
            }
        }
        if (wav)
            wav_n += k;
        frames += (long)k;
        n -= k;
    }
}
static void run(float secs) { render((uint32_t)(secs * 44100.0f), 4096); }
static void meter_reset(void)
{
    peak = sum2 = 0.0f;
    frames = 0;
    bad = 0;
}
static float rms_db(void) { return 10.0f * log10f(sum2 / (float)(2 * (frames ? frames : 1)) + 1e-12f); }

static void wav_open(const char *dir, const char *name)
{
    char p[512];
    static const uint8_t h[44] = {0};
    if (!dir)
        return;
    snprintf(p, sizeof p, "%s/%s.wav", dir, name);
    wav = fopen(p, "wb");
    if (wav)
        fwrite(h, 1, 44, wav);
    wav_n = 0;
}
static void wav_close(void)
{
    uint32_t bytes = wav_n * 4, v;
    uint16_t a[2] = {1, 2}, b[2] = {4, 16};
    uint32_t r[2] = {44100, 44100 * 4};
    if (!wav)
        return;
    fseek(wav, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, wav);
    v = 36 + bytes;
    fwrite(&v, 4, 1, wav);
    fwrite("WAVEfmt ", 1, 8, wav);
    v = 16;
    fwrite(&v, 4, 1, wav);
    fwrite(a, 2, 2, wav);
    fwrite(r, 4, 2, wav);
    fwrite(b, 2, 2, wav);
    fwrite("data", 1, 4, wav);
    fwrite(&bytes, 4, 1, wav);
    fclose(wav);
    wav = 0;
}

static void fresh_style(int style, uint32_t seed)
{
    en_glob_defaults();
    en_generate(&en_song, style, seed);
    en_init();
    run(0.02f);
}

static int in_scale(int note, int key)                /* C major pentatonic, moved to the key */
{
    int pc = (((note - key) % 12) + 12) % 12;
    return pc == 0 || pc == 2 || pc == 4 || pc == 7 || pc == 9;
}

static void t_generator(void)
{
    int st, p, s, k;
    for (st = 0; st < EN_NSTYLE; st++) {
        uint32_t seed;
        for (seed = 1; seed < 40; seed++) {
            static en_song_t a, b;
            en_generate(&a, st, seed);
            en_generate(&b, st, seed);
            CHECK(en_song_valid(&a), "style %d seed %u: not valid", st, seed);
            CHECK(en_song_hash(&a) == en_song_hash(&b), "style %d seed %u: not the same twice", st, seed);
            if (seed > 1) {
                en_generate(&b, st, seed - 1);
                CHECK(memcmp(a.part, b.part, sizeof a.part) != 0, "style %d: seeds %u and %u alike", st, seed, seed - 1);
            }
            for (p = 0; p < EN_NPART; p++) {
                int r, m, notes = 0;
                en_prog_chord(a.prog, p, &r, &m);
                for (s = 0; s < EN_NSTEP; s++) {
                    const en_note_t *e = &a.part[p].syn[1][s];
                    if (!e->note)
                        continue;
                    notes++;
                    CHECK(in_scale(e->note, 0), "style %d seed %u part %d step %d: tune note %d off the scale", st, seed, p, s,
                          e->note);
                    if (s == 0) {
                        int pc = e->note % 12;
                        CHECK(pc == r || pc == (r + (m ? 3 : 4)) % 12 || pc == (r + 7) % 12,
                              "style %d seed %u part %d: the downbeat %d not on the chord", st, seed, p, e->note);
                    }
                }
                CHECK(notes >= 1, "style %d seed %u part %d: no tune", st, seed, p);
            }
            {
                int home = a.prog == 1 ? 9 : 0;
                CHECK(a.part[3].syn[1][12].note % 12 == home, "style %d seed %u: does not end at home (%d)", st, seed,
                      a.part[3].syn[1][12].note);
            }
            for (k = 0; k < EN_NPART; k++) {
                int hits = 0, i;
                for (i = 0; i < EN_NPAD; i++)
                    hits += __builtin_popcount(a.part[k].hit[i]);
                CHECK(hits > 0, "style %d seed %u part %d: no beats", st, seed, k);
            }
        }
    }
    /* the song heard: every note of the tune in the key's pentatonic, whatever the key */
    for (st = 0; st < EN_NSTYLE; st++) {
        en_generate(&en_song, st, 7u);
        for (k = 0; k < 12; k++) {
            en_song.key = (uint8_t)k;
            for (p = 0; p < EN_NPART; p++)
                for (s = 0; s < EN_NSTEP; s++)
                    if (en_song.part[p].syn[1][s].note) {
                        int h = en_follow(L_TUNE, en_song.part[p].syn[1][s].note);
                        CHECK(in_scale(h, k), "style %d key %d: heard %d off the scale", st, k, h);
                    }
        }
    }
}

static void t_scale_lock(void)
{
    int k, i, sc;
    for (k = 0; k < 12; k++)
        for (i = 0; i < 16; i++) {
            int n = en_scale_note(0, k, i);
            CHECK(in_scale(n, k), "key %d white key %d plays %d: off the pentatonic", k, i, n);
            if (i)
                CHECK(n > en_scale_note(0, k, i - 1), "key %d white key %d: not higher than the last", k, i);
        }
    for (sc = 0; sc < EN_NSCALE; sc++) {
        int n = en_scale_len(sc);
        CHECK(n >= 5 && n <= 12, "scale %d: %d notes", sc, n);
        CHECK(en_scale_note(sc, 0, n) == 60, "scale %d: no octave", sc);
    }
}

/* the safety: the output's peak never above the cap, whatever MASTER, on the heaviest song with every trick */
static void t_safety(void)
{
    static const int CAPS[4] = {10, 40, 70, 100};
    static const uint32_t GAINS[3] = {1024, 2600, 4096};
    int c, g, t;
    for (c = 0; c < 4; c++)
        for (g = 0; g < 3; g++) {
            en_glob_defaults();
            en_glob.v[G_VOLCAP] = (int16_t)CAPS[c];
            en_heavy_song();
            en_init();
            en_play(1);
            meter_reset();
            for (t = 0; t <= EN_NTRICK; t++) {
                en_trick(t);
                render((uint32_t)(0.6f * 44100.0f), GAINS[g]);
                en_note(L_TUNE, 96, 1);
                en_pad(PAD_KICK, 1);
            }
            en_trick(0);
            CHECK(!bad, "cap %d gain %u: NaN or out of range", CAPS[c], GAINS[g]);
            CHECK(peak <= (float)CAPS[c] * 0.01f + 0.002f, "cap %d%% gain %u: peak %.3f above the cap", CAPS[c], GAINS[g],
                  (double)peak);
            CHECK(peak > (float)CAPS[c] * 0.004f, "cap %d%% gain %u: hardly any sound (%.3f)", CAPS[c], GAINS[g],
                  (double)peak);
        }
    /* the fade in: quiet at first */
    en_glob_defaults();
    en_generate(&en_song, 0, 3u);
    en_init();
    en_play(1);
    meter_reset();
    render(441, 4096);
    CHECK(peak < 0.05f, "it starts loud (%.3f)", (double)peak);
}

static int sounds(int layer, int note, float *db)
{
    meter_reset();
    en_note(layer, note, 1);
    run(0.3f);
    en_note(layer, note, 0);
    run(0.1f);
    *db = rms_db();
    return peak > 0.005f && !bad;
}

static void t_friends(const char *dir)
{
    int f, l, p;
    for (f = 0; f < EN_NFRIEND; f++)
        for (l = L_BASS; l <= L_SPARK; l++) {
            float db;
            fresh_style(0, 1u);
            en_layer_friend(&en_song.lay[l], f, 0);
            en_song.lay[l].oct = 0;
            CHECK(sounds(l, 60, &db), "%s on %s: silent or bad", EN_FRIEND[f].name, EN_LAYER_NAME[l]);
            run(6.0f);
            meter_reset();
            run(0.2f);
            CHECK(peak < 0.002f, "%s on %s: not quiet after (%.4f)", EN_FRIEND[f].name, EN_LAYER_NAME[l], (double)peak);
        }
    for (p = 0; p < EN_NPAD; p++) {
        fresh_style(0, 1u);
        meter_reset();
        en_pad(p, 1);
        run(0.4f);
        CHECK(peak > 0.01f && !bad, "pad %s: silent or bad (%.4f)", EN_PAD_NAME[p], (double)peak);
        run(3.0f);
        meter_reset();
        run(0.2f);
        CHECK(peak < 0.002f, "pad %s: not quiet after", EN_PAD_NAME[p]);
    }
    {   /* the black keys are heard over the song (Play: about as loud as its loudest), and with its drums off */
        float mus;
        fresh_style(0, 1u);
        en_duck = 1;
        en_play(1);
        render(44100, 1024);
        meter_reset();
        render(4 * 44100, 1024);
        mus = peak;
        for (p = 0; p < EN_NPAD; p++) {
            int d;
            for (d = 1; d >= 0; d--) {
                fresh_style(0, 1u);
                en_duck = (uint8_t)d;
                en_song.lay[L_BEATS].mute = 1;
                render(35000, 1024);                  /* past the fade in */
                meter_reset();
                en_pad(p, 0);
                render(11025, 1024);
                if (d)
                    CHECK(peak >= 0.75f * mus, "Play: the %s key is lost under the song (%.2f, the song %.2f)",
                          EN_PAD_NAME[p], (double)peak, (double)mus);
                else
                    CHECK(peak > 0.02f, "the %s key is silent with the song's drums off (%.3f)", EN_PAD_NAME[p], (double)peak);
            }
        }
        en_duck = 1;
    }
    if (dir) {                                    /* every friend saying a little tune, to listen to */
        for (f = 0; f < EN_NFRIEND; f++) {
            static const int TUNE[8] = {60, 64, 67, 72, 67, 64, 60, 55};
            int i;
            char n[64];
            fresh_style(0, 1u);
            en_layer_friend(&en_song.lay[L_TUNE], f, 0);
            snprintf(n, sizeof n, "friend_%d_%s", f, EN_FRIEND[f].name);
            wav_open(dir, n);
            for (i = 0; i < 8; i++) {
                en_note(L_TUNE, TUNE[i] + 12 * EN_FRIEND[f].oct, 1);
                run(0.22f);
                en_note(L_TUNE, TUNE[i] + 12 * EN_FRIEND[f].oct, 0);
                run(0.06f);
            }
            run(1.0f);
            wav_close();
        }
        wav_open(dir, "pads");
        fresh_style(0, 1u);
        for (p = 0; p < EN_NPAD; p++) {
            en_pad(p, 0);
            run(0.5f);
        }
        run(1.0f);
        wav_close();
    }
}

static void t_resonance(void)
{
    /* the safe range: Bubbles at full resonance stays well under a squeal; Lab lets it ring */
    float a, b, db;
    fresh_style(0, 1u);
    en_layer_friend(&en_song.lay[L_TUNE], 6, 0);
    en_song.lay[L_TUNE].p[PK_RES] = 100;
    en_song.lay[L_TUNE].p[PK_CUT] = 40;
    en_safe = 1;
    sounds(L_TUNE, 60, &db);
    a = db;
    en_safe = 0;
    sounds(L_TUNE, 60, &db);
    b = db;
    en_safe = 1;
    CHECK(b > a + 1.0f, "the safe range does not hold the resonance back (%.1f vs %.1f dB)", (double)a, (double)b);
}

/* how much a trick changes a second of the song (the difference over the song, by power); trick 0: the
 * song again, which must come out the same */
static float trick_diff(int t)
{
    static int32_t a[2 * 44100];
    double d = 0.0, p = 0.0;
    uint32_t i, k;
    for (k = 0; k < 2; k++) {
        fresh_style(7, 5u);
        en_play(1);
        run(1.0f);
        if (k)
            en_trick(t);
        for (i = 0; i < 44100; i += 256) {
            uint32_t m = 44100 - i < 256 ? 44100 - i : 256, j;
            en_render(buf, m, 2048);
            for (j = 0; j < 2 * m; j++) {
                if (!k) {
                    a[2 * i + j] = buf[j];
                } else {
                    double x = (double)a[2 * i + j], y = (double)buf[j];
                    d += (y - x) * (y - x);
                    p += x * x;
                }
            }
        }
        en_trick(0);
    }
    return (float)(d / (p + 1.0));
}

static void t_play(const char *dir)
{
    int st;
    for (st = 0; st < EN_NSTYLE; st++) {
        uint32_t s0;
        fresh_style(st, 11u);
        en_play(1);
        wav_open(dir, EN_STYLE_NAME[st]);
        run(0.01f);
        meter_reset();
        s0 = en_steps;
        run(16.0f * 60.0f / (float)en_song.tempo + 0.2f);   /* four bars */
        wav_close();
        CHECK(!bad, "style %s: NaN or out of range", EN_STYLE_NAME[st]);
        CHECK(rms_db() > -40.0f && rms_db() < -6.0f, "style %s: level %.1f dB", EN_STYLE_NAME[st], (double)rms_db());
        CHECK(en_steps - s0 >= 63, "style %s: %u steps in four bars", EN_STYLE_NAME[st], en_steps - s0);
        CHECK(en_bar >= 3, "style %s: bar %d", EN_STYLE_NAME[st], en_bar);
    }
    /* the tricks, each heard */
    {
        int t;
        for (t = 1; t <= EN_NTRICK; t++) {
            float plain, with;
            if (t == TR_SLOWMO) {                 /* slow-mo: the song slows */
                uint32_t a, b;
                fresh_style(7, 5u);
                en_play(1);
                run(0.5f);
                a = en_steps;
                run(2.0f);
                a = en_steps - a;
                fresh_style(7, 5u);
                en_play(1);
                run(0.5f);
                en_trick(t);
                b = en_steps;
                run(2.0f);
                b = en_steps - b;
                CHECK(b * 10 < a * 8, "slow-mo: %u steps against %u", b, a);
                continue;
            }
            plain = trick_diff(0);
            with = trick_diff(t);
            CHECK(plain < 1e-3f && with > 0.01f, "trick %s does nothing (%.3f of the song changed; plain again %.3f)",
                  EN_TRICK_NAME[t - 1], (double)with, (double)plain);
        }
    }
}

static void t_record(void)
{
    int i, n = 0, hits = 0;
    float step = 44100.0f * 60.0f / (120.0f * 4.0f);
    /* Loop me: two notes and a pad, on the grid, heard again the next time round, once */
    fresh_style(0, 2u);
    en_song.tempo = 120;
    en_song.lay[L_TUNE].mute = 0;
    en_play(1);
    run(0.01f);
    en_rec(REC_LOOP, 0);
    render((uint32_t)(step * 4.1f), 4096);           /* just after step 4 */
    en_note(L_TUNE, 64, 1);
    en_pad(PAD_POP, 0);
    render((uint32_t)(step * 1.5f), 4096);
    en_note(L_TUNE, 64, 0);
    render((uint32_t)(step * 2.0f), 4096);
    en_rec(REC_OFF, 0);
    run(0.02f);
    for (i = 0; i < EN_LOOP; i++) {
        if (en_song.loop[i].note[0])
            n++;
        if ((en_song.loop[i].pads_lo | en_song.loop[i].pads_hi << 8) >> PAD_POP & 1u)
            hits++;
    }
    CHECK(en_song.loop_on, "Loop me: nothing kept");
    CHECK(n == 1 && hits == 1, "Loop me: %d notes, %d pops (want 1, 1)", n, hits);
    CHECK(en_song.loop[4].note[0] != 0, "Loop me: not on step 4");
    CHECK(en_song.loop[4].len == 2, "Loop me: length %d (want 2)", en_song.loop[4].len);
    CHECK(en_song.loop[4].note[0] == 64 - 12 * en_song.lay[L_TUNE].oct - (en_song.key <= 6 ? en_song.key : en_song.key - 12),
          "Loop me: written %d", en_song.loop[4].note[0]);
    {   /* heard again two bars later, once */
        uint8_t c0 = en_pad_on[PAD_POP];
        render((uint32_t)(step * 32.0f), 4096);
        CHECK((uint8_t)(en_pad_on[PAD_POP] - c0) == 1, "Loop me: the pop played %d times in two bars",
              (uint8_t)(en_pad_on[PAD_POP] - c0));
    }
    /* Make: a pad into the beats of the part playing; a bass note written as if over C */
    fresh_style(0, 2u);
    en_song.tempo = 120;
    en_part_now(1);
    en_play(1);
    run(0.01f);
    en_rec(REC_LAYER, L_BASS);
    render((uint32_t)(step * 8.05f), 4096);
    {
        int r, m, heard;
        uint16_t before = en_song.part[1].hit[PAD_COWBELL];
        en_pad(PAD_COWBELL, 0);
        en_prog_chord(en_song.prog, 1, &r, &m);
        heard = 50;
        en_note(L_BASS, heard, 1);
        render((uint32_t)(step * 0.5f), 4096);
        en_note(L_BASS, heard, 0);
        run(0.01f);
        CHECK((en_song.part[1].hit[PAD_COWBELL] & ~before) == 1u << 8, "Make: cowbell at %04x", en_song.part[1].hit[PAD_COWBELL]);
        CHECK(en_follow(L_BASS, en_song.part[1].syn[0][8].note) % 12 == heard % 12 || m,
              "Make: the bass written %d is heard %d, not %d", en_song.part[1].syn[0][8].note,
              en_follow(L_BASS, en_song.part[1].syn[0][8].note), heard);
    }
    en_rec(REC_OFF, 0);
    CHECK(en_part_playing == 1, "Make: part %d plays, not B", en_part_playing);
}

static void t_clock(void)
{
    int i;
    fresh_style(0, 1u);
    en_glob.v[G_CLOCK] = 1;
    midi_clock = 0;
    en_play(1);
    run(1.0f);
    CHECK(midi_clock > 30 && midi_clock < 60, "clock out: %u in a second", midi_clock);
    /* MIDI clock in: the song follows it */
    fresh_style(0, 1u);
    en_clock(EN_CLK_START);
    for (i = 0; i < 96; i++) {                    /* 96 ticks = 16 steps at 24 a quarter */
        en_clock(EN_CLK_TICK);
        render(441, 4096);
    }
    CHECK(en_ext && en_steps >= 15 && en_steps <= 17, "clock in: %u steps for a bar of ticks", en_steps);
    en_clock(EN_CLK_STOP);
    run(0.01f);
    CHECK(!en_playing, "clock in: stop");
}

static void t_bench(void)
{
    struct timespec a, b;
    double ns;
    en_glob_defaults();
    en_heavy_song();
    en_init();
    en_play(1);
    run(0.5f);
    clock_gettime(CLOCK_MONOTONIC, &a);
    run(4.0f);
    clock_gettime(CLOCK_MONOTONIC, &b);
    ns = (double)(b.tv_sec - a.tv_sec) * 1e9 + (double)(b.tv_nsec - a.tv_nsec);
    printf("  bench: heavy song %.1f us a 256-frame block (host), %.2f%% of real time\n",
           ns / (4.0 * 44100.0 / 256.0) / 1000.0, 100.0 * ns / 4e9);
    en_generate(&en_song, 0, 1u);
    en_init();
    en_play(1);
    run(0.5f);
    clock_gettime(CLOCK_MONOTONIC, &a);
    run(4.0f);
    clock_gettime(CLOCK_MONOTONIC, &b);
    ns = (double)(b.tv_sec - a.tv_sec) * 1e9 + (double)(b.tv_nsec - a.tv_nsec);
    printf("  bench: a Bounce song %.1f us a block (host)\n", ns / (4.0 * 44100.0 / 256.0) / 1000.0);
}

/* no jumps in the sound: a held sine (Bloop, still) is as smooth with the clock running as stopped (the
 * sequencer cuts the render at its ticks: a voice that ran a whole sub-block there jumped ahead), and the
 * limiter brings a loud chord under the cap without flattening its tops (equal samples in a row up there) */
static void smooth_song(void)
{
    en_glob_defaults();
    en_song_empty(&en_song, 0);
    en_layer_friend(&en_song.lay[L_TUNE], 0, 0);
    en_song.lay[L_TUNE].p[PK_LDEPTH] = 0;
    en_song.lay[L_TUNE].p[PK_ECHO] = en_song.lay[L_TUNE].p[PK_REVERB] = 0;
    en_init();
}
static float smooth_worst(int playing)
{
    float x0 = 0.0f, x1 = 0.0f, worst = 0.0f, pk = 0.0f;
    int i, j, k = 0;
    smooth_song();
    if (playing)
        en_play(1);
    render(30000, 2048);                          /* past the fade in */
    en_note(L_TUNE, 60, 1);
    for (i = 0; i < 300; i++) {
        en_render(buf, 256, 2048);
        for (j = 0; j < 256; j++) {
            float x = (float)buf[2 * j] / 8388608.0f, d = fabsf(x - 2.0f * x1 + x0);
            if (i >= 40 && k++ > 2) {             /* (the attack done) */
                worst = fmaxf(worst, d);
                pk = fmaxf(pk, fabsf(x));
            }
            x0 = x1;
            x1 = x;
        }
    }
    return worst / (pk + 1e-9f);
}
static int flat_tops(void)
{
    const int32_t high = (int32_t)(0.7f * 0.94f * 0.9f * 8300000.0f);   /* near the cap (70%) */
    int32_t prev = 0;
    int i, j, same = 0, flat = 0;
    smooth_song();
    render(30000, 4096);
    en_note(L_TUNE, 48, 1);
    en_note(L_TUNE, 55, 1);
    en_note(L_TUNE, 60, 1);
    for (i = 0; i < 200; i++) {
        en_render(buf, 256, 4096);
        for (j = 0; j < 256; j++) {
            same = buf[2 * j] == prev && abs(buf[2 * j]) > high ? same + 1 : 0;
            flat += same >= 2;
            prev = buf[2 * j];
        }
    }
    return flat;
}
static void t_smooth(void)
{
    float stopped = smooth_worst(0), running = smooth_worst(1);
    int flat = flat_tops();
    CHECK(stopped < 0.01f, "a held sine is not smooth (%.4f)", (double)stopped);
    CHECK(running < 0.01f, "a held sine jumps while the clock runs (%.4f, stopped %.4f)", (double)running, (double)stopped);
    CHECK(!flat, "the limiter flattens a loud chord's tops (%d samples)", flat);
}

/* the child's keys on the layers (Play's white keys are the tune's; Make plays the layer it is on) */
static float live_peak(int layer, int note, float held_s)   /* press, hold, and the peak heard; then let go */
{
    float pk;
    meter_reset();
    en_note(layer, note, 1);
    if (held_s > 0.0f)
        run(held_s);
    en_note(layer, note, 0);
    run(0.4f);
    pk = peak;
    return pk;
}
static void t_live_keys(void)
{
    int layer, arp;
    for (layer = L_BASS; layer <= L_SPARK; layer++) {
        float pk;
        /* a muted layer: the song's notes stay out, the child's keys are heard (Make's SEL mutes a layer) */
        fresh_style(0, 1u);
        en_song.lay[layer].mute = 1;
        run(0.5f);
        pk = live_peak(layer, 64, 0.3f);
        CHECK(pk > 0.02f, "layer %d muted: its key is silent (%.4f)", layer, (double)pk);
        run(3.0f);
        meter_reset();
        run(0.2f);
        CHECK(peak < 0.002f, "layer %d muted: the key does not go quiet after (%.4f)", layer, (double)peak);
        fresh_style(0, 1u);
        for (arp = 0; arp < 4; arp++)
            en_song.lay[arp].mute = 1;
        en_play(1);
        run(0.5f);
        meter_reset();
        run(6.0f);
        CHECK(peak < 0.002f, "every layer muted: the song is still heard (%.4f)", (double)peak);
        /* a tap shorter than an audio block (down and up before any sound is made) is still heard, and ends */
        for (arp = 0; arp < 3; arp++) {            /* 0 plain, 1 the layer's own arpeggiator, 2 Play's (the tune's keys) */
            fresh_style(0, 1u);
            if (arp == 1)
                en_song.lay[layer].p[PK_ARP] = 1;
            if (arp == 2) {
                if (layer != L_TUNE)
                    continue;
                en_arp_live = 1;
            }
            run(0.3f);
            pk = live_peak(layer, 64, 0.0f);
            CHECK(pk > 0.02f, "layer %d (%s): a tap shorter than a block is lost (%.4f)", layer,
                  arp == 0 ? "plain" : arp == 1 ? "arp" : "play arp", (double)pk);
            run(3.0f);
            meter_reset();
            run(0.2f);
            CHECK(peak < 0.002f, "layer %d (%s): a quick tap does not go quiet (%.4f)", layer,
                  arp == 0 ? "plain" : arp == 1 ? "arp" : "play arp", (double)peak);
            en_arp_live = 0;
        }
        /* a held key is not cut at the minimum: it sustains until let go */
        fresh_style(0, 1u);
        en_song.lay[layer].p[PK_S] = 80;
        en_song.lay[layer].p[PK_D] = 30;
        run(0.3f);
        en_note(layer, 64, 1);
        run(0.5f);
        meter_reset();
        run(0.3f);
        CHECK(peak > 0.02f, "layer %d: a held key stopped sounding (%.4f)", layer, (double)peak);
        en_note(layer, 64, 0);
        run(3.0f);
    }
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : 0;
    t_generator();
    t_scale_lock();
    t_friends(dir);
    t_resonance();
    t_safety();
    t_smooth();
    t_play(dir);
    t_live_keys();
    t_record();
    t_clock();
    t_bench();
    printf("%s (%d failures)\n", fails ? "FAILED" : "all engine tests passed", fails);
    return fails != 0;
}
