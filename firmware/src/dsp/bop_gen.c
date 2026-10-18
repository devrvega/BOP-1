/* SPDX-License-Identifier: GPL-3.0-only */
/* BOP-1's songs: the friends (eight sounds), the scales, the styles, the progressions, and the generator that
 * makes a whole song from a style and a seed. Included once, by bop.c.
 *
 * The generator: a seeded random number drives each choice, so the same seed always gives the same song.
 * The chords come first (one of the style's progressions, a chord a bar), then the drums (the style's beat,
 * part B a little different, part D with a fill), the bass (the style's figure on roots, fifths and
 * octaves, written over C: it follows the chords), the sparkle (an arpeggio or chord stabs, written over C
 * too), and the tune. The tune is phrased as a call and an answer over the four bars: a one-bar motif (a
 * rhythm and a contour) over the first chord; an answer over the second; the motif again over the third,
 * moved so that it starts on one of its chord's notes (a sequence); and an answer that comes home, a long
 * note on the tonic. Strong beats land on the chord's notes; the rest walks the pentatonic scale in small
 * steps, so it sounds like a song and not like noise. */

/* --------------------------------------------------------------- friends --- */
/* WAVE1 WAVE2 TUNE2 DETUNE MIX SUB  CUT RES FTYPE FENV KTRACK  A D S R  LWAVE LRATE LDEPTH LTARGET
 * ARP ARATE AOCT GLIDE  ECHO REVERB CRUSH LEVEL */
const en_friend_t EN_FRIEND[EN_NFRIEND] = {
    {"Bloop", {0, 0, 24, 0, 0, 0, 62, 10, 0, 15, 50, 4, 45, 70, 30, 0, 40, 0, 0, 0, 2, 1, 0, 15, 20, 0, 82},
     0x4D7Fu, 0},                                                   /* sky blue: a round blob */
    {"Pointy", {25, 0, 36, 0, 15, 0, 70, 15, 0, 10, 50, 18, 50, 80, 30, 0, 52, 22, 0, 0, 2, 1, 5, 20, 25, 0, 80},
     0x5F2Au, 1},                                                   /* green: a flute with a vibrato */
    {"Zig", {50, 50, 24, 25, 45, 0, 55, 35, 0, 40, 50, 2, 40, 65, 25, 0, 50, 8, 0, 0, 2, 1, 8, 25, 15, 0, 70},
     0xFC84u, 0},                                                   /* orange: a saw lead */
    {"Bit", {75, 75, 36, 0, 25, 0, 72, 20, 0, 20, 30, 0, 30, 55, 15, 2, 60, 0, 0, 0, 2, 1, 0, 20, 10, 25, 70},
     0x9E7Bu, 0},                                                   /* silver: a square robot */
    {"Wub", {50, 75, 12, 20, 30, 70, 30, 45, 0, 45, 20, 0, 45, 60, 20, 0, 35, 0, 1, 0, 2, 1, 12, 0, 5, 0, 86},
     0x99DFu, -1},                                                  /* purple: the bass monster */
    {"Twinkle", {0, 25, 31, 0, 45, 0, 75, 10, 0, 20, 50, 0, 55, 0, 55, 0, 40, 0, 0, 0, 2, 1, 0, 35, 45, 0, 72},
     0xFF0Cu, 1},                                                   /* yellow: two oscillators a fifth apart */
    {"Bubbles", {50, 75, 24, 10, 30, 0, 22, 72, 0, 65, 60, 0, 28, 0, 25, 0, 40, 0, 1, 0, 2, 1, 0, 25, 25, 0, 80},
     0x2F5Du, 0},                                                   /* aqua: a resonant pluck */
    {"Fizz", {100, 100, 24, 0, 0, 0, 45, 78, 1, 20, 100, 30, 60, 50, 55, 3, 25, 20, 1, 0, 2, 1, 0, 20, 40, 0, 90},
     0xFCD9u, 0},                                                   /* pink: noise (wind, waves, rain) */
};
enum { F_BLOOP, F_POINTY, F_ZIG, F_BIT, F_WUB, F_TWINKLE, F_BUBBLES, F_FIZZ };

/* ---------------------------------------------------------------- scales --- */
const char *const EN_SCALE_NAME[EN_NSCALE] = {"Pentatonic", "Major", "Minor penta", "Minor", "Blues", "Mystery",
                                              "Space", "Piano"};
static const uint8_t SC_LEN[EN_NSCALE] = {5, 7, 5, 7, 6, 7, 6, 12};
static const uint8_t SC_IV[EN_NSCALE][12] = {
    {0, 2, 4, 7, 9},          {0, 2, 4, 5, 7, 9, 11}, {0, 3, 5, 7, 10},   {0, 2, 3, 5, 7, 8, 10},
    {0, 3, 5, 6, 7, 10},      {0, 2, 3, 5, 7, 9, 10}, {0, 2, 4, 6, 8, 10}, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11},
};
int en_scale_len(int sc) { return SC_LEN[(unsigned)sc % EN_NSCALE]; }
int en_scale_note(int sc, int key, int i)
{
    int n = SC_LEN[(unsigned)sc % EN_NSCALE];
    if (i < 0)
        i = 0;
    return 48 + (key % 12) + 12 * (i / n) + SC_IV[(unsigned)sc % EN_NSCALE][i % n];
}

/* --------------------------------------------------------- progressions --- */
const char *const EN_PROG_NAME[EN_NPROG] = {"Happy", "Sad", "Brave", "Sleepy"};
static const uint8_t PROG[EN_NPROG][4][2] = {  /* root over C, minor */
    {{0, 0}, {7, 0}, {9, 1}, {5, 0}},          /* I V vi IV */
    {{9, 1}, {5, 0}, {0, 0}, {7, 0}},          /* vi IV I V */
    {{0, 0}, {5, 0}, {7, 0}, {0, 0}},          /* I IV V I */
    {{0, 0}, {9, 1}, {5, 0}, {7, 0}},          /* I vi IV V */
};
void en_prog_chord(int prog, int bar, int *root, int *minor)
{
    const uint8_t *c = PROG[(unsigned)prog % EN_NPROG][bar & 3];
    *root = c[0];
    *minor = c[1];
}

const char *const EN_TRICK_NAME[EN_NTRICK] = {"Chipmunk", "Giant", "Robot", "Stutter", "Slow-mo", "Wah", "Rise",
                                              "Drop"};

/* ---------------------------------------------------------------- styles --- */
enum { BM_ROOTS, BM_BOUNCE, BM_OOMPAH, BM_LONG, BM_DISCO, BM_JUNGLE, BM_STOMP, BM_ROBOT };
enum { SM_ARP16, SM_ARP8, SM_STABS, SM_SLOW };
typedef struct {
    uint8_t tmin, tmax, swing;
    uint8_t progs;                     /* the progressions it may use (bits) */
    uint8_t density;                   /* the tune's notes in a bar, about */
    uint8_t tune, bass, spark;         /* the friends */
    uint8_t bmode, smode;
    uint8_t room;                      /* echo and reverb */
    uint8_t drums;                     /* the beats' level */
    const char *beat[EN_NPAD];         /* the main beat ("x..." a step each), each pad; 0 none */
    const char *acc;
    uint8_t funny;                     /* the pad that ends part B */
} style_t;
const char *const EN_STYLE_NAME[EN_NSTYLE] = {"Bounce", "March", "Robot", "Space", "Lullaby", "Dino Stomp",
                                              "Jungle Party", "Disco"};
static const style_t STYLE[EN_NSTYLE] = {
    {112, 124, 12, 0x9, 6, F_BLOOP, F_WUB, F_TWINKLE, BM_BOUNCE, SM_ARP8, 25, 80,
     {"x.......x.x.....", 0, "....x.......x...", "x.x.x.x.x.x.x.x.", 0, 0, 0, 0, 0, 0, 0}, "x...x...x...x...", PAD_BOING},
    {104, 116, 0, 0x5, 5, F_POINTY, F_BLOOP, F_BIT, BM_OOMPAH, SM_STABS, 15, 78,
     {"x...x...x...x...", "....x.x.....x.xx", 0, 0, 0, 0, 0, 0, 0, 0, 0}, "x.......x.......", PAD_TADA},
    {116, 126, 0, 0x5, 7, F_BIT, F_BIT, F_BUBBLES, BM_ROBOT, SM_ARP16, 20, 80,
     {"x.....x...x.....", "....x.......x...", 0, "xxxxxxxxxxxxxxxx", 0, 0, 0, 0, 0, 0, 0}, "..x...x...x...x.", PAD_LASER},
    {84, 96, 0, 0xA, 4, F_TWINKLE, F_WUB, F_TWINKLE, BM_LONG, SM_SLOW, 80, 60,
     {"x..........x....", 0, 0, "..x...x...x...x.", 0, 0, 0, 0, 0, 0, 0}, "x...............", PAD_WHOOSH},
    {66, 76, 0, 0x9, 3, F_POINTY, F_BLOOP, F_TWINKLE, BM_LONG, SM_SLOW, 60, 45,
     {0, 0, 0, "x...x...x...x...", "x...............", 0, 0, 0, 0, 0, 0}, "", PAD_POP},
    {88, 100, 0, 0x6, 5, F_ZIG, F_WUB, F_BUBBLES, BM_STOMP, SM_ARP8, 30, 85,
     {"x...x...x...x...", 0, "....x.......x...", 0, "x.....x...x.....", 0, 0, 0, 0, 0, 0}, "x...x...x...x...", PAD_BOING},
    {120, 132, 34, 0x5, 6, F_BUBBLES, F_WUB, F_POINTY, BM_JUNGLE, SM_ARP8, 30, 82,
     {"x..x..x...x..x..", 0, "....x.......x...", "x.x.x.x.x.x.x.x.", "..x...x.x..x..x.", "x...x.x...x.x...", 0, 0, 0, 0, 0},
     "x.....x.........", PAD_POP},
    {116, 124, 0, 0x3, 6, F_ZIG, F_WUB, F_BIT, BM_DISCO, SM_STABS, 30, 82,
     {"x...x...x...x...", 0, "....x.......x...", "xxxxxxxxxxxxxxxx", 0, 0, 0, 0, 0, 0, 0}, "..x...x...x...x.", PAD_TADA},
};

/* ----------------------------------------------------------------- the dice --- */
static uint32_t g_rng;
static uint32_t grnd(void)
{
    uint32_t x = g_rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_rng = x;
    return x;
}
static int gpick(int n) { return n > 0 ? (int)(grnd() % (uint32_t)n) : 0; }
static int gchance(int pct) { return gpick(100) < pct; }
static void gseed(uint32_t seed, uint32_t salt)
{
    uint32_t x = seed * 0x9E3779B9u + salt * 0x85EBCA6Bu;   /* mixed, so that seeds one apart are far apart */
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    g_rng = x ? x : 0x2545F491u;
    grnd();
}
static uint16_t mask_of(const char *s)
{
    uint16_t m = 0;
    int i;
    for (i = 0; s && i < 16 && s[i]; i++)
        if (s[i] == 'x')
            m |= (uint16_t)(1u << i);
    return m;
}

void en_layer_friend(en_layer_t *l, int who, const uint8_t *p)
{
    int k;
    const en_friend_t *f = &EN_FRIEND[(unsigned)who % EN_NFRIEND];
    if (!p)
        p = f->p;
    for (k = 0; k < PK_N; k++)
        l->p[k] = p[k];
    l->who = (uint8_t)who;
    l->colour = 0;
    l->oct = who < EN_NFRIEND ? f->oct : 0;
}

/* ---------------------------------------------------------------- the song --- */
static void part_clear(en_part_t *p)
{
    int t, s, k;
    for (k = 0; k < EN_NPAD; k++)
        p->hit[k] = 0;
    p->acc = 0;
    for (t = 0; t < EN_NSYN; t++)
        for (s = 0; s < EN_NSTEP; s++) {
            p->syn[t][s].note = 0;
            p->syn[t][s].len = 1;
            p->syn[t][s].fl = 0;
        }
}

static void layers_for(en_song_t *s, int style)
{
    const style_t *st = &STYLE[(unsigned)style % EN_NSTYLE];
    int k;
    for (k = 0; k < PK_N; k++)
        s->lay[L_BEATS].p[k] = 0;
    s->lay[L_BEATS].p[PK_LEVEL] = st->drums;
    s->lay[L_BEATS].p[PK_REVERB] = (uint8_t)(st->room / 3);
    s->lay[L_BEATS].who = 0;
    s->lay[L_BEATS].oct = 0;
    s->lay[L_BEATS].mute = 0;
    en_layer_friend(&s->lay[L_BASS], st->bass, 0);
    en_layer_friend(&s->lay[L_TUNE], st->tune, 0);
    en_layer_friend(&s->lay[L_SPARK], st->spark, 0);
    s->lay[L_BASS].oct = -1;                      /* the bass under everything, whoever plays it */
    s->lay[L_BASS].p[PK_LEVEL] = 80;
    s->lay[L_BASS].p[PK_ECHO] = 0;
    s->lay[L_SPARK].p[PK_LEVEL] = 52;             /* the sparkle a little back */
    s->lay[L_SPARK].p[PK_ECHO] = (uint8_t)(20 + st->room / 2);
    s->lay[L_TUNE].p[PK_ECHO] = (uint8_t)(st->room / 2 + 5);
    s->lay[L_TUNE].p[PK_REVERB] = (uint8_t)(st->room / 2 + 10);
    if (st->bass == F_WUB)
        s->lay[L_BASS].p[PK_GLIDE] = 0;
}

void en_song_empty(en_song_t *s, int style)
{
    const style_t *st = &STYLE[(unsigned)style % EN_NSTYLE];
    uint8_t *b = (uint8_t *)s;
    uint32_t i;
    int p, k;
    for (i = 0; i < sizeof *s; i++)
        b[i] = 0;
    s->style = (uint8_t)(style % EN_NSTYLE);
    s->prog = 0;
    s->key = 0;
    s->scale = SC_PENTA;
    s->tempo = (uint8_t)((st->tmin + st->tmax) / 2);
    s->swing = st->swing;
    s->echo_time = 50;
    s->echo_fb = 40;
    s->chain[0] = 0;
    s->chain_n = 1;
    for (k = 0; k < EN_NPAD; k++)
        s->dk[k][0] = s->dk[k][1] = 50;
    layers_for(s, style);
    for (p = 0; p < EN_NPART; p++)
        part_clear(&s->part[p]);
    for (k = 0; k < EN_LOOP; k++)
        s->loop[k].len = 1;
}

/* the chord of bar b, as pitch classes over C */
static int chord_pcs(const en_song_t *s, int bar, int *pc)
{
    int r, m;
    en_prog_chord(s->prog, bar, &r, &m);
    pc[0] = r;
    pc[1] = (r + (m ? 3 : 4)) % 12;
    pc[2] = (r + 7) % 12;
    return 3;
}
static int in_chord(int note, const int *pc)
{
    int k = ((note % 12) + 12) % 12;
    return k == pc[0] || k == pc[1] || k == pc[2];
}

/* the drums: the style's beat in every part; B a little different, D with a fill */
static void gen_beats(en_song_t *s)
{
    const style_t *st = &STYLE[s->style % EN_NSTYLE];
    int p, k;
    for (p = 0; p < EN_NPART; p++) {
        en_part_t *P = &s->part[p];
        for (k = 0; k < EN_NPAD; k++)
            P->hit[k] = mask_of(st->beat[k]);
        P->acc = mask_of(st->acc);
        if (p == 1) {                             /* B: a hat moved, a kick added, the funny sound at the end */
            if (P->hit[PAD_HAT] && gchance(60))
                P->hit[PAD_HAT] ^= (uint16_t)(1u << (1 + 2 * gpick(8)));
            if (P->hit[PAD_KICK] && gchance(50))
                P->hit[PAD_KICK] |= (uint16_t)(1u << (gchance(50) ? 14 : 7));
            P->hit[st->funny] |= (uint16_t)(1u << (st->funny == PAD_WHOOSH ? 0 : 14));
        }
        if (p == 3 && s->style != 4) {            /* D: a fill (not in a lullaby) */
            int f = gpick(3), i;
            uint16_t fill = f == 0 ? 0xA000u : f == 1 ? 0xE000u : 0xF000u;
            for (i = 0; i < EN_NPAD; i++)
                if (i != PAD_KICK && i != PAD_HAT)
                    P->hit[i] &= (uint16_t)~fill;
            P->hit[gchance(50) ? PAD_TOM : PAD_SNARE] |= fill;
            P->acc |= 0x8000u;
        }
        if (s->style == 3 && p == 0)
            P->hit[PAD_WHOOSH] |= 1u;             /* space: a whoosh as it starts */
    }
}

/* the bass, over C (it follows the chords) */
static void bass_put(en_part_t *P, int step, int note, int len, int acc)
{
    en_note_t *e = &P->syn[0][step & 15];
    e->note = (uint8_t)note;
    e->len = (uint8_t)(len < 1 ? 1 : len > 16 ? 16 : len);
    e->fl = (uint8_t)(acc ? EN_N_ACC : 0);
}
static void gen_bass(en_song_t *s)
{
    const style_t *st = &STYLE[s->style % EN_NSTYLE];
    int p, i;
    const int R = 48, F = 55, O = 60;            /* root, fifth, octave (the layer sits an octave down) */
    for (p = 0; p < EN_NPART; p++) {
        en_part_t *P = &s->part[p];
        for (i = 0; i < EN_NSTEP; i++)
            P->syn[0][i].note = 0;
        switch (st->bmode) {
        case BM_ROOTS:
            for (i = 0; i < 16; i += 4)
                bass_put(P, i, R, 3, i == 0);
            break;
        case BM_BOUNCE:
            for (i = 0; i < 16; i += 2)
                bass_put(P, i, (i & 2) ? O : R, 1, i == 0);
            if (gchance(40))
                bass_put(P, 14, F, 2, 0);
            break;
        case BM_OOMPAH:
            bass_put(P, 0, R, 3, 1);
            bass_put(P, 4, F - 12, 3, 0);
            bass_put(P, 8, R, 3, 0);
            bass_put(P, 12, F - 12, 3, 0);
            break;
        case BM_LONG:
            bass_put(P, 0, R, 12, 1);
            if (gchance(50))
                bass_put(P, 12, F - 12, 4, 0);
            break;
        case BM_DISCO:
            for (i = 0; i < 16; i += 2)
                bass_put(P, i, (i & 2) ? O : R, 1, (i & 2) != 0);
            break;
        case BM_JUNGLE: {
            static const uint8_t AT[6] = {0, 3, 6, 8, 11, 14};
            for (i = 0; i < 6; i++)
                bass_put(P, AT[i], (i == 2 || i == 5) ? F : R, 2, i == 0);
            break;
        }
        case BM_STOMP:
            for (i = 0; i < 16; i += 4)
                bass_put(P, i, R, 2, 1);
            if (gchance(60))
                bass_put(P, 15, R + 10, 1, 0);    /* a grace into the next */
            break;
        default:                                  /* robot: sixteenths, a bleep of octaves and fifths */
            for (i = 0; i < 16; i++)
                if ((0x5D5Du >> i) & 1u)
                    bass_put(P, i, (i % 8 == 6) ? O : (i % 8 == 4) ? F : R, 1, i % 4 == 0);
            break;
        }
    }
}

/* the sparkle, over C */
static void gen_spark(en_song_t *s)
{
    const style_t *st = &STYLE[s->style % EN_NSTYLE];
    static const uint8_t UP[8] = {60, 64, 67, 72, 76, 79, 84, 79};
    static const uint8_t UP4[4] = {60, 64, 67, 72};
    int p, i, flip = gchance(50);
    for (p = 0; p < EN_NPART; p++) {
        en_part_t *P = &s->part[p];
        for (i = 0; i < EN_NSTEP; i++) {
            en_note_t *e = &P->syn[2][i];
            e->note = 0;
            e->len = 1;
            e->fl = 0;
            switch (st->smode) {
            case SM_ARP16: e->note = UP[(flip ? 7 - (i & 7) : i) & 7]; break;
            case SM_ARP8:
                if (!(i & 1))
                    e->note = UP4[(flip ? 3 - (i >> 1) : (i >> 1)) & 3];
                e->len = 2;
                break;
            case SM_STABS:
                if ((i & 3) == 2) {
                    e->note = 64;
                    e->fl = EN_N_CHORD;
                    e->len = 1;
                }
                break;
            default:
                if (!(i & 3)) {
                    static const uint8_t SL[4] = {60, 67, 64, 72};
                    e->note = SL[(i >> 2) & 3];
                    e->len = 4;
                }
                break;
            }
        }
    }
}

/* the tune: call, answer, call again (moved), answer home */
static void gen_tune(en_song_t *s)
{
    const style_t *st = &STYLE[s->style % EN_NSTYLE];
    static const uint8_t PENT[] = {55, 57, 60, 62, 64, 67, 69, 72, 74, 76, 79, 81, 84};   /* C pentatonic, G3..C6 */
    const int NP = (int)sizeof PENT;
    int rhythm[16], nr = 0, contour[16], len[16], i, p;
    int tonic = s->prog == 1 ? 69 : 72;           /* the sad songs come home to A */
    int dens = st->density + gpick(3) - 1;
    /* the motif's rhythm: always the downbeat; the rest by weight (strong beats first) */
    {
        static const uint8_t W[16] = {0, 2, 5, 3, 8, 2, 5, 3, 9, 2, 5, 3, 7, 2, 4, 2};
        uint16_t on = 1;
        int tries = 0;
        while (__builtin_popcount(on) < dens && tries++ < 200) {
            int k = gpick(16);
            if (gpick(10) < W[k])
                on |= (uint16_t)(1u << k);
        }
        if (s->style == 4)                        /* a lullaby: long notes on the beats */
            on = (uint16_t)(on & 0x1111u) | 1u;
        for (i = 0; i < 16; i++)
            if (on >> i & 1u)
                rhythm[nr++] = i;
        for (i = 0; i < nr; i++) {
            int nx = i + 1 < nr ? rhythm[i + 1] : 16, l = nx - rhythm[i];
            len[i] = l > 4 && s->style != 4 && s->style != 3 ? (l > 6 ? 4 : l - 1) : l;
        }
    }
    /* its contour: steps on the scale, small ones mostly, a leap now and then */
    {
        int d = 0;
        for (i = 0; i < nr; i++) {
            int r = gpick(10);
            contour[i] = d;
            d += r < 3 ? 1 : r < 6 ? -1 : r < 8 ? 2 : r < 9 ? -2 : (s->style == 2 ? 0 : 3);
        }
    }
    for (p = 0; p < EN_NPART; p++) {
        en_part_t *P = &s->part[p];
        int pc[3], start, k, last = 0, answer = p & 1;
        for (i = 0; i < EN_NSTEP; i++) {
            P->syn[1][i].note = 0;
            P->syn[1][i].len = 1;
            P->syn[1][i].fl = 0;
        }
        chord_pcs(s, p, pc);
        /* where it starts: near the middle, on a note of the chord */
        start = 5 + gpick(3);
        for (k = 0; k < NP; k++) {
            int a = start - k, b = start + k;
            if (a >= 0 && in_chord(PENT[a], pc)) {
                start = a;
                break;
            }
            if (b < NP && in_chord(PENT[b], pc)) {
                start = b;
                break;
            }
        }
        for (i = 0; i < nr; i++) {
            int step = rhythm[i], idx, note, l = len[i];
            if (answer && i == nr - 1 && nr > 2 && gchance(40))
                continue;                         /* the answer breathes */
            idx = start + (answer ? -contour[i] : contour[i]);
            if (answer && i > 0)
                idx += gpick(3) - 1;
            while (idx < 0)
                idx += 5;
            while (idx >= NP)
                idx -= 5;
            note = PENT[idx];
            if ((step == 0 || step == 8) && !in_chord(note, pc)) {   /* the strong beats: on the chord */
                if (idx + 1 < NP && in_chord(PENT[idx + 1], pc))
                    note = PENT[idx + 1];
                else if (idx > 0 && in_chord(PENT[idx - 1], pc))
                    note = PENT[idx - 1];
            }
            if (s->style == 2 && i > 0 && gchance(30))
                note = last;                      /* robots say it twice */
            P->syn[1][step].note = (uint8_t)note;
            P->syn[1][step].len = (uint8_t)l;
            P->syn[1][step].fl = (uint8_t)(step == 0 ? EN_N_ACC : 0);
            last = note;
        }
        if (p == 3) {                             /* home: the last note long, on the tonic */
            int k2 = 15;
            for (k2 = 12; k2 < 16; k2++)
                P->syn[1][k2].note = 0;
            P->syn[1][12].note = (uint8_t)(last > tonic + 6 ? tonic + 12 : last < tonic - 9 ? tonic - 12 : tonic);
            if (P->syn[1][12].note > 84)
                P->syn[1][12].note = (uint8_t)tonic;
            P->syn[1][12].len = 4;
        }
    }
}

void en_generate(en_song_t *s, int style, uint32_t seed)
{
    const style_t *st;
    static const uint8_t KEYS[5] = {0, 2, 5, 7, 9};
    int k, n = 0, pick;
    style = ((style % EN_NSTYLE) + EN_NSTYLE) % EN_NSTYLE;
    st = &STYLE[style];
    en_song_empty(s, style);
    s->seed = seed;
    gseed(seed, (uint32_t)style + 1u);
    for (k = 0; k < EN_NPROG; k++)
        n += st->progs >> k & 1u;
    pick = gpick(n);
    for (k = 0; k < EN_NPROG; k++)
        if (st->progs >> k & 1u) {
            if (pick-- == 0) {
                s->prog = (uint8_t)k;
                break;
            }
        }
    s->key = KEYS[gpick(5)];
    s->tempo = (uint8_t)(st->tmin + gpick(st->tmax - st->tmin + 1));
    s->swing = st->swing;
    s->echo_time = (uint8_t)(st->room > 50 ? 70 : 45);
    s->echo_fb = (uint8_t)(25 + st->room / 2);
    for (k = 0; k < 4; k++)
        s->chain[k] = (uint8_t)k;
    s->chain_n = 4;
    gseed(seed, 101u);
    gen_beats(s);
    gseed(seed, 202u);
    gen_bass(s);
    gseed(seed, 303u);
    gen_spark(s);
    gseed(seed, 404u);
    gen_tune(s);
}

void en_generate_layer(en_song_t *s, int layer, uint32_t seed)
{
    gseed(seed, 500u + (uint32_t)layer);
    switch (layer) {
    case L_BEATS: gen_beats(s); break;
    case L_BASS: gen_bass(s); break;
    case L_TUNE: gen_tune(s); break;
    default: gen_spark(s); break;
    }
}

uint32_t en_song_hash(const en_song_t *s)
{
    const uint8_t *b = (const uint8_t *)s;
    uint32_t h = 2166136261u, i;
    for (i = 0; i < sizeof *s; i++)
        h = (h ^ b[i]) * 16777619u;
    return h;
}
