/* SPDX-License-Identifier: GPL-3.0-only */
/* The LAB door (ages 9-12+): the real synth, by its real names. Its first screen offers the synth or the
 * lessons (lessons.c).
 *
 * The top of the screen is the signal flow, OSC 1 and OSC 2 into the FILTER, into the AMP, into the FX, with
 * the ENVELOPE and the LFO drawn as wires to what they move; the page open is lit. Under it, a live scope
 * shows the wave at that stage (after the oscillators, after the filter, after the amp), so the filter can be
 * seen rounding off a saw; the envelope's page draws its shape, the LFO's its wave. Each knob shows its real
 * name with the toy's word under it ("CUTOFF / bright").
 *
 *   SEL OSC   SEQ FILTER   ENV ENVELOPE   LFO LFO   ARP ARP + GLIDE   FX EFFECTS   (OSC and FILTER again: more)
 *   ALGORITHM the layer (TUNE, BASS, SPARKLE)      PRESETS  a friend, or one of My Friends, onto it
 *   keys      a piano (the keys as they are, their names shown), or the scale: GLO switches
 *   SAVE      into My Friends (a white key picks the place, K1 the colour): they then appear in Play and Make
 *   EDIT      oops; held: the friend's sound as it was
 *
 * Part of ui.c. */

enum { LP_OSC, LP_OSC2, LP_FILTER, LP_FILTER2, LP_ENV, LP_LFO, LP_ARP, LP_FX, NLP };
static const int8_t LAB_K[NLP][4] = {
    {PK_WAVE1, PK_WAVE2, PK_TUNE2, PK_MIX},  {PK_SUB, PK_DETUNE, PK_GLIDE, PK_LEVEL},
    {PK_CUT, PK_RES, PK_FTYPE, PK_FENV},     {PK_KTRACK, PK_FENV, PK_CUT, PK_RES},
    {PK_A, PK_D, PK_S, PK_R},                {PK_LWAVE, PK_LRATE, PK_LDEPTH, PK_LTARGET},
    {PK_ARP, PK_ARATE, PK_AOCT, PK_GLIDE},   {-1, -2, PK_REVERB, PK_CRUSH},   /* -1, -2: the song's echo */
};
static const char *const LAB_PAGE[NLP] = {"OSCILLATORS", "OSCILLATORS 2", "FILTER", "FILTER 2", "ENVELOPE", "LFO",
                                          "ARPEGGIATOR", "EFFECTS"};
static struct {
    uint8_t page, layer, menu, menu_sel, saving, colour;
    float scope[3][EN_SCOPE_N];
    uint8_t have;
    uint8_t last_note;
    uint32_t note_t;
} lab = {LP_OSC, L_TUNE, 1, 0, 0, 0, {{0}}, 0, 60, 0};

static void lessons_start(void);
static int lesson_on(void);
static void lesson_key(int k, int down);
static int lesson_button(int b);
static void lesson_check(void);
static void lesson_draw(void);
static void lesson_stop(void);
static void lab_play_key(int k, int down);

static int lab_stage(void)                       /* the scope's stage: 0 the oscillators, 1 the filter, 2 the amp */
{
    return lab.page <= LP_OSC2 ? 0 : lab.page <= LP_FILTER2 ? 1 : 2;
}

static void lab_enter(void)
{
    lab.menu = 1;                                 /* the first screen: the synth or the lessons */
    lab.saving = 0;
    lesson_stop();
    undo_save();
}

static int lab_key_note(int k)                   /* a key as a note: the piano, or the scale */
{
    int w = white_of(k);
    if (glob.piano)
        return 53 + k + 12 * en_song.lay[lab.layer].oct;
    return w >= 0 ? lay_note_of_white(lab.layer, w) : -1;
}

static void lab_key(int k, int down)
{
    int w = white_of(k);
    if (lesson_on()) {
        lesson_key(k, down);
        return;
    }
    if (lab.menu) {
        if (down && w == 0) {
            lab.menu = 0;
            en_pad(PAD_POP, 0);
        } else if (down && w == 1) {
            lab.menu = 0;
            lessons_start();
        }
        return;
    }
    if (lab.saving) {                             /* into My Friends */
        if (down && w >= 0 && w < NMYF) {
            myfriend_t *m = &myf.f[w];
            const en_layer_t *L = &en_song.lay[lab.layer];
            memcpy(m->p, L->p, PK_N);
            m->used = 1;
            m->colour = lab.colour;
            project_dirty(PD_FRIENDS);
            en_song.lay[lab.layer].who = (uint8_t)(EN_NFRIEND + w);
            en_song.lay[lab.layer].colour = lab.colour;
            mark_work();
            lab.saving = 0;
            en_pad(PAD_TADA, 1);
            overlay(OV_KEPT, w, 1600u);
            ui.chase_t = plat_ms();
        }
        return;
    }
    lab_play_key(k, down);
}
static void lab_play_key(int k, int down)
{
    int n;
    if (!glob.piano && black_of(k) >= 0) {
        if (down)
            en_pad(black_of(k), 0);
        return;
    }
    if (down) {
        n = lab_key_note(k);
        if (n < 0)
            return;
        ui.key_note[k] = (uint8_t)n;
        ui.key_lay[k] = lab.layer;
        en_note(lab.layer, n, 1);
        lab.last_note = (uint8_t)n;
        lab.note_t = plat_ms();
    } else if (ui.key_note[k]) {
        en_note(ui.key_lay[k], ui.key_note[k], 0);
        ui.key_note[k] = 0;
    }
}

static void lab_button(int b)
{
    int pg = -1;
    if (lesson_on() && lesson_button(b))
        return;
    if (lab.menu)
        return;
    if (lab.saving && b != B_SAVE) {
        lab.saving = 0;
        return;
    }
    switch (b) {
    case B_SEL: pg = lab.page == LP_OSC ? LP_OSC2 : LP_OSC; break;
    case B_SEQ: pg = lab.page == LP_FILTER ? LP_FILTER2 : LP_FILTER; break;
    case B_ENV: pg = LP_ENV; break;
    case B_LFO: pg = LP_LFO; break;
    case B_ARP: pg = LP_ARP; break;
    case B_FX: pg = LP_FX; break;
    case B_GLO:
        glob.piano ^= 1u;
        mark_glob();
        say(glob.piano ? "Piano" : "Scale", glob.piano ? "The keys as they are" : "Only the scale's notes");
        break;
    case B_SAVE:
        lab.saving = 1;
        lab.colour = (uint8_t)(lab.colour & 15u);
        live_keys_off();
        break;
    case B_OCTDN:
    case B_OCTUP:
        en_song.lay[lab.layer].oct = (int8_t)bump(en_song.lay[lab.layer].oct, b == B_OCTUP ? 1 : -1, -2, 2);
        mark_work();
        break;
    default: break;
    }
    if (pg >= 0) {
        lab.page = (uint8_t)pg;
        lab.have = 0;
    }
}

static void lab_enc(int role, int d)
{
    en_layer_t *L = &en_song.lay[lab.layer];
    if (lab.menu)
        return;
    if (lab.saving) {
        if (role == EN_K1)
            lab.colour = (uint8_t)((lab.colour + (d > 0 ? 1 : 15)) & 15u);
        return;
    }
    switch (role) {
    case EN_ALGO:
        live_keys_off();
        lab.layer = (uint8_t)(lab.layer == L_SPARK && d > 0 ? L_BASS : lab.layer == L_BASS && d < 0 ? L_SPARK
                                                                                                  : lab.layer + (d > 0 ? 1 : -1));
        lab.have = 0;
        return;
    case EN_PRESET: {
        int n = friends_n(), i = (friend_index(L->who) + (d > 0 ? 1 : n - 1)) % n;
        put_friend(lab.layer, friend_at(i));
        en_preview(lab.layer, lab.last_note);
        return;
    }
    case EN_SELECT:
        en_song.tempo = (uint8_t)bump(en_song.tempo, (int)accel(role, d, 120), 50, 200);
        overlay(OV_TEMPO, 0, 1200u);
        mark_work();
        return;
    default: break;
    }
    if (role >= EN_K1) {
        int k = LAB_K[lab.page][role - EN_K1];
        if (k == -1)
            en_song.echo_time = (uint8_t)bump(en_song.echo_time, (int)accel(role, d, 100), 0, 100);
        else if (k == -2)
            en_song.echo_fb = (uint8_t)bump(en_song.echo_fb, (int)accel(role, d, 100), 0, 100);
        else {
            const en_pk_t *pi = en_pk_info(k);
            int range = pi->hi - pi->lo;
            L->p[k] = (uint8_t)bump(L->p[k], range > 10 ? (int)accel(role, d, range) : (d > 0 ? 1 : -1), pi->lo, pi->hi);
        }
        mark_work();
    }
}

static void lab_clear(void)                       /* EDIT held: the friend's sound as it was */
{
    int who = en_song.lay[lab.layer].who;
    put_friend(lab.layer, who);
}

/* ----------------------------------------------------------- the screen --- */
static void flow_box(int x, int y, int w, const char *name, int lit, uint16_t c)
{
    cv_round(x, y, w, 20, 5, lit ? c : RGB(44, 42, 96));
    text_c(x + w / 2, y + 2, &FONT_XS, name, lit ? INK : RGB(180, 180, 230));
}
static void wire(float x0, float y0, float x1, float y1, uint16_t c, float t)   /* a wire, its dashes moving */
{
    int i;
    float dx = x1 - x0, dy = y1 - y0, l = fm_sqrtf(dx * dx + dy * dy), n = l / 6.0f;
    for (i = 0; i < (int)n; i++) {
        float u0 = ((float)i + t) / n, u1 = u0 + 0.5f / n;
        if (u1 > 1.0f)
            continue;
        thick(x0 + dx * u0, y0 + dy * u0, x0 + dx * u1, y0 + dy * u1, 2.0f, c);
    }
}
static void lab_flow(void)
{
    const en_layer_t *L = &en_song.lay[lab.layer];
    float t = (float)(plat_ms() % 1000u) * 0.001f;
    static const uint16_t PC[5] = {RGB(255, 150, 90), RGB(255, 220, 80), RGB(120, 220, 140), RGB(120, 170, 255),
                                   RGB(230, 140, 255)};
    int osc = lab.page <= LP_OSC2, fil = lab.page == LP_FILTER || lab.page == LP_FILTER2;
    flow_box(2, 2, 40, "OSC 1", osc, PC[0]);
    flow_box(2, 24, 40, "OSC 2", osc, PC[0]);
    wire(42, 12, 66, 22, CREAM, t);
    wire(42, 34, 66, 24, CREAM, t);
    flow_box(66, 13, 52, "FILTER", fil, PC[1]);
    wire(118, 23, 132, 23, CREAM, t);
    flow_box(132, 13, 40, "AMP", 0, PC[2]);
    wire(172, 23, 186, 23, CREAM, t);
    flow_box(186, 13, 52, "FX", lab.page == LP_FX, PC[4]);
    flow_box(96, 36, 36, "ENV", lab.page == LP_ENV, PC[3]);       /* the envelope moves the amp, and the filter */
    wire(114, 36, 144, 33, RGB(120, 170, 255), t);
    if (L->p[PK_FENV])
        wire(108, 36, 96, 33, RGB(120, 170, 255), t);
    flow_box(150, 36, 34, "LFO", lab.page == LP_LFO, PC[3]);
    {
        int tg = L->p[PK_LTARGET];
        float tx = tg == 0 ? 22.0f : tg == 1 ? 92.0f : 152.0f;
        if (L->p[PK_LDEPTH])
            wire(167, 36, tx, tg == 0 ? 44.0f : 33.0f, RGB(230, 140, 255), t);
    }
    if (lab.page == LP_ARP)
        flow_box(2, 46, 50, "ARP", 1, PC[2]);
}

static void scope_take(void)                      /* the scope: ask, and keep what came */
{
    if (!en_scope_req) {
        if (en_scope_hz > 0.0f && lab.have < 255) {
            memcpy(lab.scope, en_scope, sizeof lab.scope);
            lab.have = 1;
        }
        en_scope_req = (uint8_t)(lab.layer + 1);
    }
}
static void scope_draw(int y0, int h, int stage, uint16_t c)
{
    const float *s = lab.scope[stage];
    int i, start = 0;
    float mid = (float)y0 + (float)h * 0.5f, k = (float)h * 0.42f, pk = 0.02f, px = 0.0f, py = 0.0f;
    for (i = 0; i < EN_SCOPE_N; i++)
        pk = fm_maxf(pk, fm_fabsf(s[i]));
    if (stage < 2)
        k /= pk;                                  /* the shape, as big as it fits (the amp's stays to scale) */
    else
        k *= 3.0f;
    for (i = 1; i < EN_SCOPE_N / 2; i++)          /* a rising zero: the picture holds still */
        if (s[i - 1] < 0.0f && s[i] >= 0.0f) {
            start = i;
            break;
        }
    for (i = 0; i < 232 && start + i * (EN_SCOPE_N / 2) / 232 < EN_SCOPE_N; i++) {
        float y = mid - fm_clampf(s[start + i * (EN_SCOPE_N / 2) / 232] * k, -(float)h * 0.48f, (float)h * 0.48f), x = 4.0f + (float)i;
        if (i)
            thick(px, py, x, y, 2.2f, c);
        px = x;
        py = y;
    }
}
static void adsr_draw(int y0, int h)
{
    const en_layer_t *L = &en_song.lay[lab.layer];
    float a = (float)L->p[PK_A] * 0.6f, d = (float)L->p[PK_D] * 0.6f, s = (float)L->p[PK_S] * 0.01f,
          r = (float)L->p[PK_R] * 0.6f, x0 = 6.0f, yb = (float)(y0 + h - 4), yt = (float)(y0 + 6), sus = 40.0f;
    float sc = fm_minf(1.0f, 220.0f / (a + d + sus + r + 1.0f)), ys = yb - (yb - yt) * s;
    float x1 = x0 + a * sc, x2 = x1 + d * sc, x3 = x2 + sus, x4 = x3 + r * sc;
    thick(x0, yb, x1, yt, 3.0f, RGB(120, 170, 255));
    thick(x1, yt, x2, ys, 3.0f, RGB(120, 170, 255));
    thick(x2, ys, x3, ys, 3.0f, RGB(120, 170, 255));
    thick(x3, ys, x4, yb, 3.0f, RGB(120, 170, 255));
    cv_text((int32_t)((x0 + x1) * 0.5f) - 3, (int32_t)yb - 14, &FONT_XS, "A", CREAM);
    cv_text((int32_t)((x1 + x2) * 0.5f) - 3, (int32_t)yt + 2, &FONT_XS, "D", CREAM);
    cv_text((int32_t)((x2 + x3) * 0.5f) - 3, (int32_t)ys - 16, &FONT_XS, "S", CREAM);
    cv_text((int32_t)((x3 + x4) * 0.5f), (int32_t)yb - 14, &FONT_XS, "R", CREAM);
    disc(x0 + (x4 - x0) * 0.0f + fm_minf(en_env[lab.layer], 1.0f) * 0.0f, yb - (yb - yt) * en_env[lab.layer], 4.0f,
         RGB(255, 220, 90));                      /* where it is now */
}
static void lfo_draw(int y0, int h)
{
    const en_layer_t *L = &en_song.lay[lab.layer];
    int i, w = L->p[PK_LWAVE];
    float px = 0.0f, py = 0.0f, mid = (float)y0 + (float)h * 0.5f, amp = (float)h * 0.4f;
    for (i = 0; i <= 116; i++) {
        float u = (float)i / 58.0f, ph = u - fm_floorf(u), v;
        v = w == 1 ? fm_fabsf(ph * 4.0f - 2.0f) - 1.0f : w == 2 ? (ph < 0.5f ? 1.0f : -1.0f)
                 : w == 3 ? (float)((int)(hash32((uint32_t)(u * 4.0f)) % 200u) - 100) * 0.01f : fm_sin_turns(ph);
        if (i)
            thick(px, py, 4.0f + (float)i * 2.0f, mid - v * amp, 2.5f, RGB(230, 140, 255));
        px = 4.0f + (float)i * 2.0f;
        py = mid - v * amp;
    }
    disc(120.0f, mid - en_lfo[lab.layer] * amp, 5.0f, RGB(255, 220, 90));
}

static void lab_menu_draw(int b)
{
    if (b == 1) {
        text_c(120, BAND_H0 + 20, &FONT_S, "1: the synth    2: lessons", CREAM);
        return;
    }
    cv_round(14, 20, 100, 104, 14, RGB(70, 110, 220));
    knob_icon(2, 64, 56, 22, 0.9f, CREAM);
    text_c(64, 90, &FONT_B, "Synth", CREAM);
    cv_round(126, 20, 100, 104, 14, RGB(220, 140, 60));
    star(176, 56, 20, 0.0f, CREAM);
    text_c(176, 90, &FONT_B, "Lessons", CREAM);
    {
        int i, done = 0;
        for (i = 0; i < NLESSON; i++)
            done += book.lessons >> i & 1u;
        if (done) {
            char b2[12];
            itoa_u((uint32_t)done, b2);
            text_c(176, 108, &FONT_XS, b2, CREAM);
        }
    }
}

static void lab_draw(int b)
{
    const en_layer_t *L = &en_song.lay[lab.layer];
    if (lesson_on() == 2) {
        if (b == 0)
            lesson_draw();
        else
            text_c(120, BAND_H0 + 30, &FONT_S, "A white key opens a lesson", CREAM);
        return;
    }
    if (lesson_on() && b == 0) {
        lesson_check();
        lesson_draw();
        return;
    }
    if (lab.menu) {
        lab_menu_draw(b);
        return;
    }
    if (lab.saving) {
        int i;
        if (b == 1) {
            text_c(120, BAND_H0 + 10, &FONT_S, "A white key keeps it there", CREAM);
            text_c(120, BAND_H0 + 34, &FONT_XS, "K1: its colour", RGB(170, 170, 225));
            disc(120, BAND_H0 + 66, 16, MYF_COL[lab.colour]);
            return;
        }
        cv_text(10, 4, &FONT_B, "My Friends", CREAM);
        for (i = 0; i < NMYF; i++) {
            int x = 10 + (i % 4) * 56, y = 30 + (i / 4) * 54;
            cv_round(x, y, 50, 48, 8, RGB(46, 44, 100));
            if (myf.f[i].used) {
                fr_t f;
                memset(&f, 0, sizeof f);
                f.x = (float)x + 25.0f;
                f.y = (float)y + 44.0f;
                f.r = 14.0f;
                f.wave = (float)myf.f[i].p[PK_WAVE1] * 0.04f;
                f.mouth = (float)myf.f[i].p[PK_CUT] * 0.01f;
                f.col = MYF_COL[myf.f[i].colour & 15];
                f.bg = RGB(46, 44, 100);
                friend_draw(&f);
            }
        }
        return;
    }
    if (b == 0) {
        int st = lab_stage();
        scope_take();
        lab_flow();
        cv_rect(0, 60, 240, 84, RGB(14, 16, 40));
        {
            int y;
            for (y = 64; y < 144; y += 20)
                cv_rect(0, y, 240, 1, RGB(30, 34, 70));
        }
        if (lab.page == LP_ENV)
            adsr_draw(62, 80);
        else if (lab.page == LP_LFO)
            lfo_draw(62, 80);
        else if (lab.have) {
            if (st > 0)
                scope_draw(62, 80, st - 1, RGB(60, 70, 120));   /* the stage before, ghosted */
            scope_draw(62, 80, st, st == 0 ? RGB(255, 150, 90) : st == 1 ? RGB(255, 220, 80) : RGB(120, 220, 140));
        } else {
            text_c(120, 92, &FONT_S, "Play a key to see the wave", RGB(120, 120, 180));
        }
        {   /* the page, the layer, the note */
            char t[32];
            cv_text(4, 63, &FONT_XS, LAB_PAGE[lab.page], RGB(170, 170, 230));
            scpy(t, EN_LAYER_NAME[lab.layer], sizeof t);
            cv_text(236 - text_w(&FONT_XS, t), 63, &FONT_XS, t, who_colour(L->who, L->colour));
            if (plat_ms() - lab.note_t < 2500u) {
                static const char *const NN[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
                char *q = t;
                uint32_t hz = (uint32_t)(en_mtof((float)lab.last_note) + 0.5f);
                q += slen(NN[lab.last_note % 12]);
                scpy(t, NN[lab.last_note % 12], sizeof t);
                *q++ = (char)('0' + (lab.last_note / 12 - 1) % 10);
                *q++ = ' ';
                itoa_u(hz, q);
                q += slen(q);
                scpy(q, " Hz", 4);
                cv_text(236 - text_w(&FONT_XS, t), 128, &FONT_XS, t, CREAM);
            }
        }
        return;
    }
    {   /* the four knobs: the real name, the value, the toy's word */
        static const uint16_t KC[4] = {RGB(255, 150, 90), RGB(255, 220, 80), RGB(120, 220, 140), RGB(120, 170, 255)};
        int i;
        for (i = 0; i < 4; i++) {
            int k = LAB_K[lab.page][i], cx = 30 + i * 60, v, hi;
            const char *name, *kid;
            char val[16];
            if (k == -1) {
                static const char *const T[5] = {"1/16", "1/8", "3/16", "1/4", "3/8"};
                v = en_song.echo_time;
                hi = 100;
                name = "Echo time";
                kid = "how far";
                scpy(val, T[v * 5 / 101], sizeof val);
            } else if (k == -2) {
                v = en_song.echo_fb;
                hi = 100;
                name = "Repeats";
                kid = "how many";
                itoa_u((uint32_t)v, val);
            } else {
                const en_pk_t *pi = en_pk_info(k);
                v = L->p[k] - pi->lo;
                hi = pi->hi - pi->lo;
                name = pi->name;
                kid = pi->kid;
                en_pk_text(k, L->p[k], val);
            }
            arc(cx, BAND_H0 + 24, 19, 5.0f, (float)v / (float)(hi ? hi : 1), RGB(52, 50, 110), KC[i]);
            text_c(cx, BAND_H0 + 46, &FONT_XS, name, CREAM);
            text_c(cx, BAND_H0 + 61, &FONT_S, val, KC[i]);
            text_c(cx, BAND_H0 + 79, &FONT_XS, kid, RGB(150, 150, 210));
        }
    }
}

static void lab_leds(uint32_t *b, uint32_t *k, uint32_t *db, uint32_t *dk)
{
    static const uint8_t PB[NLP] = {B_SEL, B_SEL, B_SEQ, B_SEQ, B_ENV, B_LFO, B_ARP, B_FX};
    int i;
    if (lesson_on() == 2) {
        for (i = 0; i < NLESSON; i++)
            *((book.lessons >> i & 1u) ? k : dk) |= 1u << WHITE_KEY[i];
        return;
    }
    if (lab.menu) {
        *k |= 1u << WHITE_KEY[0] | 1u << WHITE_KEY[1];
        return;
    }
    if (lab.saving) {
        for (i = 0; i < NMYF; i++)
            *(myf.f[i].used ? k : dk) |= 1u << WHITE_KEY[i];
        *b |= 1u << B_SAVE;
        return;
    }
    *b |= 1u << PB[lab.page];
    *db |= 1u << B_SEL | 1u << B_SEQ | 1u << B_ENV | 1u << B_LFO | 1u << B_ARP | 1u << B_FX;
    for (i = 0; i < NKEYS; i++)
        if (ui.key_note[i])
            *k |= 1u << i;
    if (glob.piano)
        *b |= 1u << B_GLO;
    if (en_song.lay[lab.layer].oct > 0)
        *b |= 1u << B_OCTUP;
    if (en_song.lay[lab.layer].oct < 0)
        *b |= 1u << B_OCTDN;
}

static const card_t LAB_CARD[NB] = {
    [B_FX] = {IC_FX, "Effects", "Echo and reverb, and the robot.", "EFFECTS: echo, reverb, bit-crush"},
    [B_SEL] = {IC_OSC, "Oscillators", "Where the sound starts: its shape.", "OSCILLATOR: the waveform"},
    [B_ENV] = {IC_ENV, "Envelope", "How a sound starts and stops.", "ENVELOPE: attack decay sustain release"},
    [B_LFO] = {IC_LFO, "LFO", "A hand that keeps turning a knob.", "LOW-FREQUENCY OSCILLATOR"},
    [B_EDIT] = {IC_UNDO, "Oops!", "Undo. Hold: the friend's sound back.", 0},
    [B_GLO] = {IC_PIANO, "Piano or scale", "The keys as they are, or the scale.", "CHROMATIC or a SCALE"},
    [B_HOME] = {IC_DOORS, "Doors", "Play, Make, Lab, Games.", 0},
    [B_SAVE] = {IC_SAVE, "My Friends", "Keep this sound as a friend.", "a PATCH, saved"},
    [B_ARP] = {IC_ARP, "Arpeggiator", "Held notes, one after another.", "ARPEGGIATOR and GLIDE"},
    [B_SEQ] = {IC_FILTER, "Filter", "Takes the fizz out: bright or dark.", "FILTER: cutoff, resonance"},
    [B_PLAY] = {IC_PLAY, "Music", "The song on or off.", 0},
    [B_REC] = {IC_NONE, "", "", 0},
    [B_OCTDN] = {IC_BIG, "Lower", "An octave down: half the frequency.", "OCTAVE"},
    [B_OCTUP] = {IC_SMALL, "Higher", "An octave up: twice the frequency.", "OCTAVE"},
};
static const door_t DOOR_LAB = {lab_enter, lab_key, lab_button, lab_enc, lab_draw, lab_leds, lab_clear, LAB_CARD};
