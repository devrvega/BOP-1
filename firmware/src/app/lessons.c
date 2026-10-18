/* SPDX-License-Identifier: GPL-3.0-only */
/* The lessons (behind the Lab door): ten short ones, each a few picture cards and a "try it" that the
 * firmware checks ("Turn the cutoff all the way down... hear it?"). Finishing one puts a sticker in the book.
 * The synth stays playable under them: the keys play, the knobs turn the page the lesson needs. OCT+ the
 * next card, OCT- the one before, HOME out. Part of ui.c (after lab.c). */

enum { CK_NONE, CK_KEY, CK_HIGHLOW, CK_SQUARE, CK_DARK, CK_RES, CK_SLOW, CK_WOBBLE, CK_DETUNE, CK_FOURSTEPS,
       CK_FIVE, CK_THREE };
enum { PIC_NONE, PIC_SCOPE, PIC_FAST, PIC_SHAPES, PIC_FILTER, PIC_ADSR, PIC_LFO, PIC_TWO, PIC_STEPS, PIC_SCALE,
       PIC_CHORD };
typedef struct {
    uint8_t pic, check, page;          /* the picture; the try-it (0: a card); the Lab page it opens */
    const char *a, *b;                 /* two short lines */
} lstep_t;
typedef struct {
    const char *title;
    lstep_t s[5];
} lesson_t;
static const lesson_t LESSON[NLESSON] = {
    {"What is sound?", {{PIC_SCOPE, 0, LP_OSC, "Sound is a wiggle", "in the air."},
                        {PIC_SCOPE, 0, LP_OSC, "Your ear feels the wiggle", "and you hear it."},
                        {PIC_SCOPE, CK_KEY, LP_OSC, "Try it! Play a key", "and watch the wiggle."}}},
    {"High and low", {{PIC_FAST, 0, LP_OSC, "Fast wiggles sound high.", "Slow wiggles sound low."},
                      {PIC_FAST, 0, LP_OSC, "How fast: the frequency,", "in hertz (Hz) a second."},
                      {PIC_FAST, CK_HIGHLOW, LP_OSC, "Try it! Play a high key", "then a low one."}}},
    {"Shapes of sound", {{PIC_SHAPES, 0, LP_OSC, "Same note, other shape:", "it sounds different."},
                         {PIC_SHAPES, 0, LP_OSC, "Sine, triangle, saw,", "square, noise."},
                         {PIC_SHAPES, CK_SQUARE, LP_OSC, "Try it! Turn K1 (Wave)", "to Square. A robot!"}}},
    {"The filter", {{PIC_FILTER, 0, LP_FILTER, "A filter takes", "the fizz out."},
                    {PIC_FILTER, 0, LP_FILTER, "Cutoff: how much", "bright gets through."},
                    {PIC_FILTER, CK_DARK, LP_FILTER, "Try it! Turn K1 (Cutoff)", "all the way down."},
                    {PIC_FILTER, CK_RES, LP_FILTER, "Now K2 (Resonance) up:", "it squeaks at the edge."}}},
    {"Envelopes", {{PIC_ADSR, 0, LP_ENV, "How a sound starts", "and how it stops."},
                   {PIC_ADSR, 0, LP_ENV, "Attack, Decay,", "Sustain, Release."},
                   {PIC_ADSR, CK_SLOW, LP_ENV, "Try it! Turn K1 (Attack)", "up: a slow start."}}},
    {"The LFO", {{PIC_LFO, 0, LP_LFO, "An LFO is a hand", "that keeps turning a knob."},
                 {PIC_LFO, 0, LP_LFO, "Pitch: vibrato. Filter: wah.", "Volume: tremolo."},
                 {PIC_LFO, CK_WOBBLE, LP_LFO, "Try it! Turn K3 (Depth)", "up and play."}}},
    {"Two oscillators", {{PIC_TWO, 0, LP_OSC2, "Two a little apart", "make it fat."},
                         {PIC_TWO, 0, LP_OSC, "A fifth apart", "makes it bright."},
                         {PIC_TWO, CK_DETUNE, LP_OSC2, "Try it! K2 (Detune) up,", "then listen."}}},
    {"Beats and steps", {{PIC_STEPS, 0, LP_OSC, "A bar has 16 steps.", "The kick on 1..."},
                         {PIC_STEPS, 0, LP_OSC, "...the snare on 5", "and on 13."},
                         {PIC_STEPS, CK_FOURSTEPS, LP_OSC, "Try it! Press white keys", "1, 5, 9 and 13."}}},
    {"Scales", {{PIC_SCALE, 0, LP_OSC, "Some notes go together:", "a scale."},
                {PIC_SCALE, 0, LP_OSC, "Pentatonic: five notes,", "no wrong ones."},
                {PIC_SCALE, 0, LP_OSC, "Major sounds happy.", "Minor sounds sad."},
                {PIC_SCALE, CK_FIVE, LP_OSC, "Try it! Play five", "different notes."}}},
    {"Chords and songs", {{PIC_CHORD, 0, LP_OSC, "Three notes at once:", "a chord."},
                          {PIC_CHORD, 0, LP_OSC, "Parts A and B,", "one after another: a song."},
                          {PIC_CHORD, CK_THREE, LP_OSC, "Try it! Hold three keys", "at once."}}},
};
static const char *const LESSON_SHORT[NLESSON] = {"Sound", "High, low", "Shapes", "The filter", "Envelopes",
                                                  "The LFO", "Two oscs", "Beats", "Scales", "Chords"};
static struct {
    uint8_t on, n, st, done, seen_live;
    uint16_t keys_seen;
    uint8_t lo, hi;
} les;

static int lesson_on(void) { return les.on; }
static void lesson_stop(void) { les.on = 0; }
static int lesson_steps(int n)
{
    int k = 0;
    while (k < 5 && LESSON[n].s[k].a)
        k++;
    return k;
}
static void lesson_open_step(void)
{
    const lstep_t *s = &LESSON[les.n].s[les.st];
    lab.page = s->page;
    les.done = 0;
    les.keys_seen = 0;
    les.lo = 127;
    les.hi = 0;
    les.seen_live = en_live_on;
}
static void lessons_start(void)
{
    int i;
    les.on = 2;                                   /* the list */
    for (i = 0; i < NLESSON && (book.lessons >> i & 1u); i++)
        ;
    les.n = (uint8_t)(i < NLESSON ? i : 0);
}
static void lesson_check(void)                    /* each frame: is the try-it done? */
{
    const lstep_t *s = &LESSON[les.n].s[les.st];
    const en_layer_t *L = &en_song.lay[lab.layer];
    int ok = 0, i, held = 0;
    for (i = 0; i < NKEYS; i++)
        if (ui.key_note[i]) {
            held++;
            les.lo = ui.key_note[i] < les.lo ? ui.key_note[i] : les.lo;
            les.hi = ui.key_note[i] > les.hi ? ui.key_note[i] : les.hi;
            les.keys_seen |= (uint16_t)(1u << (ui.key_note[i] % 16u));
        }
    switch (s->check) {
    case CK_KEY: ok = en_live_on != les.seen_live; break;
    case CK_HIGHLOW: ok = les.hi >= 72 && les.lo <= 60; break;
    case CK_SQUARE: ok = L->p[PK_WAVE1] >= 68 && L->p[PK_WAVE1] <= 82; break;
    case CK_DARK: ok = L->p[PK_CUT] <= 12; break;
    case CK_RES: ok = L->p[PK_RES] >= 65; break;
    case CK_SLOW: ok = L->p[PK_A] >= 60; break;
    case CK_WOBBLE: ok = L->p[PK_LDEPTH] >= 50; break;
    case CK_DETUNE: ok = L->p[PK_DETUNE] >= 20; break;
    case CK_FOURSTEPS: {
        uint16_t want = 1u | 1u << 4 | 1u << 8 | 1u << 12;
        ok = (les.keys_seen & want) == want;
        break;
    }
    case CK_FIVE: ok = __builtin_popcount(les.keys_seen) >= 5; break;
    case CK_THREE: ok = held >= 3; break;
    default: break;
    }
    if (ok && !les.done) {
        les.done = 1;
        en_pad(PAD_TADA, 0);
        ui.chase_t = plat_ms();
    }
}
/* the steps lesson: the white keys mark steps (seen by position, not by note) */
static void lesson_key(int k, int down)
{
    int w = white_of(k);
    if (les.on == 2) {                            /* the list: a white key opens a lesson */
        if (down && w >= 0 && w < NLESSON) {
            les.n = (uint8_t)w;
            les.st = 0;
            les.on = 1;
            lesson_open_step();
        }
        return;
    }
    if (LESSON[les.n].s[les.st].check == CK_FOURSTEPS) {
        if (down && w >= 0) {
            les.keys_seen |= (uint16_t)(1u << w);
            en_pad(w % 4 == 0 ? PAD_KICK : PAD_HAT, 0);
        }
        return;
    }
    lab_play_key(k, down);
    if (down && LESSON[les.n].s[les.st].check == CK_FIVE && ui.key_note[k])
        les.keys_seen |= (uint16_t)(1u << (ui.key_note[k] % 12u));
}
static int lesson_button(int b)                   /* 1: used */
{
    int n = lesson_steps(les.n);
    if (les.on == 2) {
        if (b == B_OCTDN) {                       /* back to the Lab's first screen */
            les.on = 0;
            lab.menu = 1;
        }
        return b == B_OCTUP || b == B_OCTDN;
    }
    if (b == B_OCTUP) {
        if (LESSON[les.n].s[les.st].check && !les.done)
            return 1;                             /* the try-it first */
        if (les.st + 1 < n) {
            les.st++;
            lesson_open_step();
        } else {                                  /* done: the sticker */
            if (!(book.lessons >> les.n & 1u)) {
                book.lessons |= (uint16_t)(1u << les.n);
                book.stickers |= 1u << (16 + les.n);
                project_dirty(PD_BOOK);
            }
            en_pad(PAD_TADA, 1);
            overlay(OV_KEPT, 0, 1600u);
            les.on = 2;
        }
        return 1;
    }
    if (b == B_OCTDN) {
        if (les.st > 0) {
            les.st--;
            lesson_open_step();
        } else {
            les.on = 2;
        }
        return 1;
    }
    return 0;
}

static void lesson_pic(int pic, int y0)
{
    const en_layer_t *L = &en_song.lay[lab.layer];
    int i;
    switch (pic) {
    case PIC_SCOPE:
    case PIC_FILTER:
        if (lab.have)
            scope_draw(y0, 56, pic == PIC_FILTER ? 1 : 0, pic == PIC_FILTER ? RGB(255, 220, 80) : RGB(255, 150, 90));
        else
            knob_icon(2, 120, (float)y0 + 28.0f, 30.0f, 0.6f, RGB(255, 150, 90));
        break;
    case PIC_FAST: {
        float px = 0, py = 0;
        for (i = 0; i <= 110; i++) {
            float x = 4.0f + (float)i, y = (float)y0 + 28.0f - fm_sin_turns((float)i / 36.0f) * 18.0f;
            if (i)
                thick(px, py, x, y, 2.5f, RGB(120, 170, 255));
            px = x;
            py = y;
        }
        for (i = 0; i <= 110; i++) {
            float x = 126.0f + (float)i, y = (float)y0 + 28.0f - fm_sin_turns((float)i / 9.0f) * 18.0f;
            if (i)
                thick(px, py, x, y, 2.5f, RGB(255, 150, 90));
            px = x;
            py = y;
        }
        text_c(60, y0 + 46, &FONT_XS, "low", CREAM);
        text_c(180, y0 + 46, &FONT_XS, "high", CREAM);
        break;
    }
    case PIC_SHAPES:
        for (i = 0; i < 5; i++)
            knob_icon(0, 28.0f + (float)i * 46.0f, (float)y0 + 26.0f, 15.0f, (float)i * 0.25f,
                      i == (L->p[PK_WAVE1] + 12) / 25 ? RGB(255, 220, 90) : RGB(140, 140, 210));
        break;
    case PIC_ADSR: adsr_draw(y0, 56); break;
    case PIC_LFO: lfo_draw(y0, 56); break;
    case PIC_TWO: {
        float px = 0, py = 0, d = 1.0f + (float)L->p[PK_DETUNE] * 0.004f;
        int j;
        for (j = 0; j < 2; j++)
            for (i = 0; i <= 232; i++) {
                float x = 4.0f + (float)i, y = (float)y0 + 28.0f - fm_sin_turns((float)i / 40.0f * (j ? d : 1.0f)) * 16.0f;
                if (i)
                    thick(px, py, x, y, 2.0f, j ? RGB(255, 150, 90) : RGB(120, 170, 255));
                px = x;
                py = y;
            }
        break;
    }
    case PIC_STEPS:
        for (i = 0; i < 16; i++) {
            int on = les.keys_seen >> i & 1u, mark = i % 4 == 0;
            cv_round(6 + i * 14, y0 + 14, 12, 28, 3, on ? RGB(255, 110, 90) : mark ? RGB(80, 78, 160) : RGB(50, 48, 110));
        }
        break;
    case PIC_SCALE:
        for (i = 0; i < 8; i++) {
            static const uint8_t PC[8] = {0, 2, 4, 5, 7, 9, 11, 12};
            int penta = i != 3 && i != 6;
            cv_round(10 + i * 28, y0 + 6, 24, 44, 4, penta ? NOTE_COL[PC[i] % 12] : RGB(60, 60, 110));
        }
        break;
    case PIC_CHORD:
        quaver(96, (float)y0 + 44.0f, 6.0f, NOTE_COL[0]);
        quaver(120, (float)y0 + 34.0f, 6.0f, NOTE_COL[4]);
        quaver(144, (float)y0 + 24.0f, 6.0f, NOTE_COL[7]);
        break;
    default: break;
    }
}

static void lesson_draw(void)
{
    if (les.on == 2) {                            /* the list */
        int i;
        cv_text(8, 2, &FONT_B, "Lessons", CREAM);
        for (i = 0; i < NLESSON; i++) {
            int x = 6 + (i % 2) * 116, y = 26 + (i / 2) * 23, done = book.lessons >> i & 1u;
            char b[4];
            cv_round(x, y, 112, 21, 6, i == les.n ? RGB(80, 76, 170) : RGB(44, 42, 96));
            itoa_u((uint32_t)(i + 1), b);
            cv_text(x + 4, y + 2, &FONT_XS, b, RGB(170, 170, 230));
            cv_text(x + 20, y + 2, &FONT_XS, LESSON_SHORT[i], CREAM);
            if (done)
                star((float)x + 103.0f, (float)y + 10.0f, 6.0f, 0.0f, RGB(255, 210, 80));
        }
        return;
    }
    {
        const lstep_t *s = &LESSON[les.n].s[les.st];
        int i, n = lesson_steps(les.n);
        scope_take();
        cv_round(4, 2, 232, 140, 12, RGB(40, 38, 96));
        cv_text(12, 4, &FONT_S, LESSON[les.n].title, RGB(255, 210, 120));
        for (i = 0; i < n; i++)                   /* where we are */
            disc(228.0f - (float)(n - 1 - i) * 10.0f, 13.0f, 3.0f, i == les.st ? CREAM : RGB(80, 78, 150));
        lesson_pic(s->pic, 26);
        text_c(120, 88, &FONT_S, s->a, CREAM);
        text_c(120, 108, &FONT_S, s->b, CREAM);
        if (s->check) {
            if (les.done) {
                star(212, 126, 9.0f, 0.0f, RGB(255, 210, 80));
                cv_text(150, 120, &FONT_XS, "OCT+ next", RGB(170, 230, 170));
            } else {
                ring(212, 126, 8, 2.0f, RGB(120, 120, 180));
            }
        } else {
            cv_text(168, 124, &FONT_XS, "OCT+ next", RGB(170, 170, 230));
        }
    }
}
