/* SPDX-License-Identifier: GPL-3.0-only */
/* The GAMES door (all ages: stars, and no timers for the youngest), and the sticker book.
 *
 *   FOLLOW THE LIGHTS  a key lights: press it, and a tune comes out (public-domain tunes; a staff for those
 *                      who read)
 *   COPY ME            the friend sings notes; sing them back; one more each time
 *   HIGH OR LOW?       two notes: OCT+ if the second is higher, OCT- if lower (the big and small friends say so)
 *   BEAT COPY          hear a rhythm, tap it back on a black key
 *   SOUND MATCH        turn the knobs until your friend sounds like the other one (warm, warmer, hot!)
 *   NAME THE FRIEND    whose sound was that?
 *
 * Stars go into the sticker book; each few stars a new sticker. Nothing is ever taken away. White keys 1-7
 * pick (7 the book) and open (again, or PLAY); PLAY in a game hears it again; HOME goes home. The song stops
 * while the games play (they use the friends) and its sounds come back after. Part of ui.c. */

enum { GM_FOLLOW, GM_COPY, GM_HIGHLOW, GM_BEAT, GM_MATCH, GM_NAME, GM_BOOK };
static const char *const GAME_NAME[NGAME + 1] = {"Follow the lights", "Copy me", "High or low?", "Beat copy",
                                                 "Sound match", "Name the friend", "Sticker book"};
enum { GS_MENU, GS_LISTEN, GS_TURN, GS_RESULT };

/* the tunes (public domain), in C, on the white keys: notes and their lengths in eighths */
typedef struct {
    const char *name;
    uint8_t n;
    uint8_t note[28], len[28];
} tune_t;
static const tune_t TUNES[8] = {
    {"Twinkle Twinkle", 14, {60, 60, 67, 67, 69, 69, 67, 65, 65, 64, 64, 62, 62, 60},
     {2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 4}},
    {"Hot Cross Buns", 17, {64, 62, 60, 64, 62, 60, 60, 60, 60, 60, 62, 62, 62, 62, 64, 62, 60},
     {2, 2, 4, 2, 2, 4, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 4}},
    {"Mary Had a Little Lamb", 26, {64, 62, 60, 62, 64, 64, 64, 62, 62, 62, 64, 67, 67, 64, 62, 60, 62, 64, 64, 64,
                                    64, 62, 62, 64, 62, 60},
     {2, 2, 2, 2, 2, 2, 4, 2, 2, 4, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4}},
    {"Frere Jacques", 14, {60, 62, 64, 60, 60, 62, 64, 60, 64, 65, 67, 64, 65, 67}, {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 2, 2, 4}},
    {"Row Row Row Your Boat", 11, {60, 60, 60, 62, 64, 64, 62, 64, 65, 67, 67}, {3, 3, 2, 1, 3, 2, 1, 2, 1, 6, 2}},
    {"Old MacDonald", 13, {60, 60, 60, 55, 57, 57, 55, 64, 64, 62, 62, 60, 60}, {2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 4, 2}},
    {"Ode to Joy", 15, {64, 64, 65, 67, 67, 65, 64, 62, 60, 60, 62, 64, 64, 62, 62}, {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 1, 4}},
    {"Happy Birthday", 12, {55, 55, 57, 55, 60, 59, 55, 55, 57, 55, 62, 60}, {1, 1, 2, 2, 2, 4, 1, 1, 2, 2, 2, 4}},
};
static const uint8_t COPY_NOTE[4] = {60, 64, 67, 72};

static struct {
    uint8_t sel, game, state;
    uint32_t t0;
    uint8_t round, score, mistakes, stars, won;
    uint8_t tune, pos;
    uint8_t seq[24], n, i;
    uint8_t a, b;
    uint8_t pat[8], taps_n;
    uint16_t tap_ms[16];
    uint32_t bar_t;
    uint8_t target[4], mine[4];
    uint8_t choices[3], right;
    uint32_t wrong_t, right_t;
    uint32_t demo_t;
    uint8_t demo_i;
    en_layer_t keep[EN_NLAYER];
    uint8_t kept;
} gm;
static int key_of_note(int note) { return note - 53; }        /* the physical key (F3 = 0) */

static void friend_mini(float x, float y, float r, uint16_t c);
static void gm_tone(int layer, int note) { en_preview(layer, note); }
static void gm_good(void)
{
    gm.right_t = plat_ms();
    en_pad(PAD_POP, 0);
}
static void gm_oops(void)
{
    gm.wrong_t = plat_ms();
    en_pad(PAD_BOING, 0);
}

static void gm_finish(int stars)                  /* the stars; a sticker each five */
{
    uint32_t total = 0, i;
    if (stars < 1)
        stars = 1;
    if (stars > 3)
        stars = 3;
    gm.stars = (uint8_t)stars;
    gm.state = GS_RESULT;
    gm.t0 = plat_ms();
    book.stars[gm.game] = (uint16_t)(book.stars[gm.game] + stars);
    if (book.level[gm.game] < 250)
        book.level[gm.game]++;
    for (i = 0; i < NGAME; i++)
        total += book.stars[i];
    for (i = 0; i < 16 && i * 5u + 3u <= total; i++)
        book.stickers |= 1u << i;
    project_dirty(PD_BOOK);
    en_pad(PAD_TADA, 1);
    ui.chase_t = plat_ms();
}

/* the sound match: the knobs' four, onto a layer */
static void match_apply(int layer, const uint8_t *v)
{
    static const uint8_t E[3][4] = {{0, 25, 0, 15}, {3, 45, 65, 30}, {45, 70, 80, 65}};
    en_layer_t *L = &en_song.lay[layer];
    en_layer_friend(L, 0, 0);
    L->oct = 0;
    L->mute = 0;
    L->p[PK_WAVE1] = L->p[PK_WAVE2] = v[0];
    L->p[PK_CUT] = v[1];
    L->p[PK_A] = E[v[2]][0];
    L->p[PK_D] = E[v[2]][1];
    L->p[PK_S] = E[v[2]][2];
    L->p[PK_R] = E[v[2]][3];
    L->p[PK_LDEPTH] = v[3];
    L->p[PK_LRATE] = 55;
    L->p[PK_ECHO] = 0;
    L->p[PK_LEVEL] = 80;
}
static int match_knobs(void) { int l = book.level[GM_MATCH]; return l < 1 ? 1 : l < 3 ? 2 : l < 5 ? 3 : 4; }
static float match_close(void)                    /* 0 cold .. 1 the same */
{
    int n = match_knobs(), k;
    float d = 0.0f;
    for (k = 0; k < n; k++) {
        float x = k == 2 ? (gm.mine[2] == gm.target[2] ? 0.0f : 1.0f)
                         : fm_fabsf((float)gm.mine[k] - (float)gm.target[k]) * (1.0f / 70.0f);
        d += fm_minf(x, 1.0f);
    }
    return 1.0f - d / (float)n;
}

static void gm_round(void)                        /* a new round of the game running */
{
    uint32_t r = hash32(plat_ms() ^ (uint32_t)gm.round * 977u);
    gm.state = GS_LISTEN;
    gm.t0 = gm.demo_t = plat_ms();
    gm.demo_i = 0;
    switch (gm.game) {
    case GM_FOLLOW:
        gm.state = GS_TURN;                       /* (no listening first: the lights lead) */
        break;
    case GM_COPY:
        if (gm.n < 24)
            gm.seq[gm.n++] = (uint8_t)(r % 4u);
        gm.i = 0;
        break;
    case GM_HIGHLOW: {
        int lv = book.level[GM_HIGHLOW], gap = lv < 2 ? 7 : lv < 5 ? 4 : 2, base = 57 + (int)(r % 12u);
        gm.a = (uint8_t)base;
        gm.b = (uint8_t)(base + ((r >> 8) & 1u ? gap + (int)((r >> 9) % 3u) : -gap - (int)((r >> 9) % 3u)));
        break;
    }
    case GM_BEAT: {
        static const uint8_t PATS[6] = {0x55, 0x99, 0x5A, 0xB1, 0xD5, 0x6D};
        int i, p = PATS[(r % 6u)];
        for (i = 0; i < 8; i++)
            gm.pat[i] = (uint8_t)(p >> (7 - i) & 1);
        gm.pat[0] = 1;
        gm.taps_n = 0;
        break;
    }
    case GM_MATCH: {
        static const uint8_t SH[5] = {0, 25, 50, 75, 100}, CU[4] = {20, 45, 70, 95}, WB[3] = {0, 45, 90};
        int n = match_knobs();
        gm.target[0] = SH[r % 5u];
        gm.target[1] = n > 1 ? CU[(r >> 4) % 4u] : 70;
        gm.target[2] = (uint8_t)(n > 2 ? (r >> 8) % 3u : 1);
        gm.target[3] = n > 3 ? WB[(r >> 12) % 3u] : 0;
        gm.mine[0] = (uint8_t)((gm.target[0] + 50) % 125);
        gm.mine[1] = 70;
        gm.mine[2] = 1;
        gm.mine[3] = 0;
        match_apply(L_SPARK, gm.target);
        match_apply(L_TUNE, gm.mine);
        break;
    }
    case GM_NAME: {
        gm.choices[0] = (uint8_t)(r % 8u);                         /* three different friends */
        gm.choices[1] = (uint8_t)((gm.choices[0] + 1u + (r >> 4) % 3u) % 8u);
        gm.choices[2] = (uint8_t)((gm.choices[1] + 1u + (r >> 7) % 3u) % 8u);
        if (gm.choices[2] == gm.choices[0])
            gm.choices[2] = (uint8_t)((gm.choices[2] + 1u) % 8u);
        gm.right = (uint8_t)((r >> 11) % 3u);
        {
            en_layer_t *L = &en_song.lay[L_SPARK];
            en_layer_friend(L, gm.choices[gm.right], 0);
            L->mute = 0;
            L->p[PK_LEVEL] = 80;
        }
        break;
    }
    default: break;
    }
}

static void gm_start(int g)
{
    gm.game = (uint8_t)g;
    gm.round = gm.score = gm.mistakes = gm.n = 0;
    gm.pos = 0;
    gm.tune = (uint8_t)(book.level[GM_FOLLOW] % 8u);
    gm.state = GS_MENU;
    if (g == GM_BOOK) {
        gm.state = GS_RESULT + 1;                 /* (the book: a page of its own) */
        return;
    }
    if (!gm.kept) {
        memcpy(gm.keep, en_song.lay, sizeof gm.keep);
        gm.kept = 1;
    }
    gm_round();
}
static void games_leave(void)                     /* the song's sounds back as they were */
{
    if (gm.kept) {
        memcpy(en_song.lay, gm.keep, sizeof gm.keep);
        gm.kept = 0;
    }
}

static void games_enter(void)
{
    en_play(0);
    gm.state = GS_MENU;
    gm.game = 0;
}

/* each frame: the friend singing its part (the demos) */
static void gm_tick(void)
{
    uint32_t now = plat_ms(), dt = now - gm.demo_t;
    if (gm.state != GS_LISTEN)
        return;
    switch (gm.game) {
    case GM_COPY:
        if (gm.demo_i < gm.n && dt >= 650u) {
            gm_tone(L_TUNE, COPY_NOTE[gm.seq[gm.demo_i]]);
            gm.demo_i++;
            gm.demo_t = now;
        } else if (gm.demo_i >= gm.n && dt >= 500u) {
            gm.state = GS_TURN;
        }
        break;
    case GM_HIGHLOW:
        if (gm.demo_i == 0 && dt >= 500u) {
            gm_tone(L_TUNE, gm.a);
            gm.demo_i = 1;
            gm.demo_t = now;
        } else if (gm.demo_i == 1 && dt >= 800u) {
            gm_tone(L_TUNE, gm.b);
            gm.demo_i = 2;
            gm.state = GS_TURN;
        }
        break;
    case GM_BEAT: {                               /* the rhythm (eighths at 90), four clicks, then your turn */
        const uint32_t E = 333u;
        uint32_t slot = dt / E;
        if (dt >= 400u + E * 16u) {
            gm.state = GS_TURN;
            gm.bar_t = gm.demo_t + 400u + E * 16u;
            gm.t0 = now;
        } else if (dt >= 400u && (dt - 400u) / E != gm.demo_i) {
            slot = (dt - 400u) / E;
            gm.demo_i = (uint8_t)slot;
            if (slot < 8u && gm.pat[slot])
                en_pad(PAD_COWBELL, 0);
            if (slot >= 8u && (slot & 1u) == 0u)
                en_pad(PAD_HAT, 1);               /* the count-in */
        }
        break;
    }
    case GM_MATCH:
        if (dt >= 300u) {
            gm_tone(L_SPARK, 64);
            gm.state = GS_TURN;
        }
        break;
    case GM_NAME:
        if (dt >= 400u) {
            gm_tone(L_SPARK, 64);
            gm.state = GS_TURN;
        }
        break;
    default: gm.state = GS_TURN; break;
    }
}
static void beat_score(void)                      /* the bar is over: how many hits were near */
{
    int i, j, hits = 0, want = 0, extra;
    for (i = 0; i < 8; i++) {
        if (!gm.pat[i])
            continue;
        want++;
        for (j = 0; j < gm.taps_n; j++) {
            int d = (int)gm.tap_ms[j] - (int)(i * 333);
            if (d > -140 && d < 160) {
                hits++;
                break;
            }
        }
    }
    extra = gm.taps_n > want ? gm.taps_n - want : 0;
    if (hits == want && extra <= 1) {
        gm.score++;
        gm_good();
    } else {
        gm_oops();
    }
    gm.round++;
    if (gm.round >= 4)
        gm_finish(gm.score >= 4 ? 3 : gm.score >= 2 ? 2 : 1);
    else
        gm_round();
}

static void games_key(int k, int down)
{
    int w = white_of(k), bk = black_of(k);
    if (!down && ui.key_note[k]) {                /* a note let go, whatever happened since (the tune's last ends it) */
        en_note(ui.key_lay[k], ui.key_note[k], 0);
        ui.key_note[k] = 0;
        return;
    }
    if (gm.state == GS_MENU) {
        if (down && w >= 0 && w <= GM_BOOK) {
            if (gm.sel == w)
                gm_start(w);
            else {
                gm.sel = (uint8_t)w;
                en_pad(PAD_POP, 0);
            }
        }
        return;
    }
    if (gm.state == GS_RESULT || gm.state == GS_RESULT + 1) {
        if (down && plat_ms() - gm.t0 > 800u) {
            gm.state = GS_MENU;
            games_leave();
        }
        return;
    }
    if (gm.state != GS_TURN) {
        return;
    }
    switch (gm.game) {
    case GM_FOLLOW: {
        const tune_t *T = &TUNES[gm.tune];
        if (!down)
            return;
        ui.key_note[k] = (uint8_t)(53 + k);
        ui.key_lay[k] = L_TUNE;
        en_note(L_TUNE, 53 + k, 1);
        if (k == key_of_note(T->note[gm.pos])) {
            if (++gm.pos >= T->n)
                gm_finish(gm.mistakes <= 1 ? 3 : gm.mistakes <= 4 ? 2 : 1);
        } else {
            gm.mistakes++;
            gm.wrong_t = plat_ms();
        }
        break;
    }
    case GM_COPY: {
        int c = -1, i;
        if (!down)
            return;
        for (i = 0; i < 4; i++)
            if (k == key_of_note(COPY_NOTE[i]))
                c = i;
        if (c < 0)
            return;
        gm_tone(L_TUNE, COPY_NOTE[c]);
        if (c == gm.seq[gm.i]) {
            if (++gm.i >= gm.n) {
                gm.round++;
                gm_good();
                gm_round();
                gm.demo_t = plat_ms() + 400u;
            }
        } else {
            gm_oops();
            gm_finish(gm.n >= 8 ? 3 : gm.n >= 5 ? 2 : 1);
        }
        break;
    }
    case GM_HIGHLOW:
        if (down && w >= 0)
            gm.state = GS_LISTEN, gm.demo_i = 0, gm.demo_t = plat_ms();   /* a key: again */
        break;
    case GM_BEAT:
        if (down && bk >= 0 && gm.taps_n < 16) {
            uint32_t t = plat_ms() - gm.bar_t;
            en_pad(PAD_COWBELL, 1);
            if (t < 3000u)
                gm.tap_ms[gm.taps_n++] = (uint16_t)t;
        }
        break;
    case GM_MATCH:
        if (down && w >= 0)
            gm_tone(L_TUNE, 64);
        break;
    case GM_NAME:
        if (down && w >= 0 && w < 3) {
            if (w == gm.right) {
                gm.score++;
                gm_good();
            } else {
                gm_oops();
            }
            if (++gm.round >= 6)
                gm_finish(gm.score >= 5 ? 3 : gm.score >= 3 ? 2 : 1);
            else
                gm_round();
        }
        break;
    default: break;
    }
}

static void games_button(int b)
{
    if (gm.state == GS_MENU) {
        if (b == B_PLAY)
            gm_start(gm.sel);
        return;
    }
    if (gm.state >= GS_RESULT) {
        gm.state = GS_MENU;
        games_leave();
        return;
    }
    if (b == B_PLAY) {                            /* hear it again */
        if (gm.game == GM_MATCH || gm.game == GM_NAME)
            gm_tone(L_SPARK, 64);
        else if (gm.game == GM_COPY || gm.game == GM_HIGHLOW) {
            gm.state = GS_LISTEN;
            gm.demo_i = 0;
            gm.i = 0;
            gm.demo_t = plat_ms();
        }
        return;
    }
    if (gm.game == GM_HIGHLOW && gm.state == GS_TURN && (b == B_OCTUP || b == B_OCTDN)) {
        int up = gm.b > gm.a;
        if ((b == B_OCTUP) == up) {
            gm.score++;
            gm_good();
        } else {
            gm_oops();
        }
        if (++gm.round >= 8)
            gm_finish(gm.score >= 7 ? 3 : gm.score >= 5 ? 2 : 1);
        else
            gm_round();
    }
}

static void games_enc(int role, int d)
{
    if (gm.state == GS_MENU) {
        if (role == EN_ALGO || role == EN_SELECT)
            gm.sel = (uint8_t)((gm.sel + (d > 0 ? 1 : GM_BOOK)) % (GM_BOOK + 1));
        return;
    }
    if (gm.game == GM_MATCH && gm.state == GS_TURN && role >= EN_K1) {
        int k = role - EN_K1;
        if (k >= match_knobs())
            return;
        if (k == 2)
            gm.mine[2] = (uint8_t)bump(gm.mine[2], d > 0 ? 1 : -1, 0, 2);
        else
            gm.mine[k] = (uint8_t)bump(gm.mine[k], (int)accel(role, d, 100) * 2, 0, k == 1 ? 96 : 100);
        match_apply(L_TUNE, gm.mine);
        if (match_close() > 0.93f) {
            gm_tone(L_TUNE, 64);
            gm_finish(3);
        }
    }
}

/* ----------------------------------------------------------- the screen --- */
static void game_icon(int g, float x, float y, float s)
{
    switch (g) {
    case GM_FOLLOW: {
        int i;
        for (i = 0; i < 4; i++)
            cv_round((int32_t)(x - s + (float)i * s * 0.5f), (int32_t)(y - s * 0.6f), (int32_t)(s * 0.42f), (int32_t)(s * 1.2f),
                     2, i == 2 ? RGB(255, 220, 90) : CREAM);
        break;
    }
    case GM_COPY: friend_mini(x - s * 0.4f, y, s * 0.55f, EN_FRIEND[0].colour); friend_mini(x + s * 0.5f, y, s * 0.55f, EN_FRIEND[5].colour); break;
    case GM_HIGHLOW: icon(IC_BIG, x - s * 0.4f, y, s * 0.6f, CREAM); icon(IC_SMALL, x + s * 0.6f, y, s * 0.6f, CREAM); break;
    case GM_BEAT: pad_icon(PAD_COWBELL, x, y, s * 0.7f, RGB(80, 150, 255)); break;
    case GM_MATCH: knob_icon(1, x, y, s * 0.8f, 0.7f, RGB(255, 220, 80)); break;
    case GM_NAME: quaver(x - s * 0.2f, y + s * 0.4f, s * 0.4f, CREAM); text_c((int32_t)(x + s * 0.5f), (int32_t)(y - s * 0.6f), &FONT_B, "?", CREAM); break;
    default: star(x, y, s * 0.8f, 0.0f, RGB(255, 210, 80)); break;
    }
}
static void friend_mini(float x, float y, float r, uint16_t c)
{
    fr_t f;
    memset(&f, 0, sizeof f);
    f.x = x;
    f.y = y + r;
    f.r = r;
    f.mouth = 0.5f;
    f.col = c;
    f.bg = NAVY;
    friend_draw(&f);
}

static void staff_draw(int y0, const tune_t *T, int pos)   /* a staff: the next notes, coloured by name */
{
    static const int8_t STEP[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};   /* C D E F G A B on the lines */
    int i, x0 = 20;
    for (i = 0; i < 5; i++)
        cv_rect(10, y0 + i * 8, 220, 1, RGB(120, 120, 180));
    for (i = 0; i < 6 && pos + i < T->n; i++) {
        int n = T->note[pos + i], deg = STEP[n % 12] + 7 * (n / 12 - 5);       /* C4 = 0 */
        float y = (float)y0 + 40.0f - (float)deg * 4.0f, x = (float)(x0 + i * 36);
        if (deg <= 0)
            cv_rect((int32_t)x - 8, y0 + 40, 16, 1, RGB(120, 120, 180));   /* middle C's line */
        ellipse(x, y, i == 0 ? 7.0f : 5.0f, i == 0 ? 5.5f : 4.0f, NOTE_COL[n % 12]);
    }
}

static void games_draw(int b)
{
    uint32_t now = plat_ms();
    int i;
    gm_tick();
    if (gm.game == GM_BEAT && gm.state == GS_TURN && now - gm.bar_t > 3000u)
        beat_score();
    if (gm.state == GS_MENU) {
        if (b == 1) {
            text_c(120, BAND_H0 + 8, &FONT_B, GAME_NAME[gm.sel], CREAM);
            text_c(120, BAND_H0 + 40, &FONT_XS, "press it again, or PLAY", RGB(160, 160, 220));
            if (gm.sel < NGAME) {
                char t[12];
                itoa_u(book.stars[gm.sel], t);
                star(110, BAND_H0 + 70, 9.0f, 0.0f, RGB(255, 210, 80));
                cv_text(124, BAND_H0 + 61, &FONT_S, t, CREAM);
            }
            return;
        }
        for (i = 0; i <= GM_BOOK; i++) {
            int x = 6 + (i % 4) * 58, y = 8 + (i / 4) * 66;
            cv_round(x, y, 54, 60, 10, i == gm.sel ? RGB(90, 86, 190) : RGB(44, 42, 96));
            game_icon(i, (float)x + 27.0f, (float)y + 26.0f, 16.0f);
            {
                char t[4];
                itoa_u((uint32_t)(i + 1), t);
                cv_text(x + 5, y + 42, &FONT_XS, t, RGB(170, 170, 230));
            }
            if (i < NGAME && book.stars[i])
                star((float)x + 44.0f, (float)y + 50.0f, 6.0f, 0.0f, RGB(255, 210, 80));
        }
        return;
    }
    if (gm.state == GS_RESULT + 1) {              /* the sticker book */
        if (b == 1) {
            int n = __builtin_popcount(book.stickers);
            char t[16];
            itoa_u((uint32_t)n, t);
            text_c(120, BAND_H0 + 20, &FONT_M, t, RGB(255, 210, 80));
            text_c(120, BAND_H0 + 56, &FONT_XS, "stickers", CREAM);
            return;
        }
        for (i = 0; i < 26; i++) {
            int x = 8 + (i % 7) * 33, y = 6 + (i / 7) * 34, has = book.stickers >> i & 1u;
            cv_round(x, y, 30, 31, 7, has ? RGB(60, 58, 130) : RGB(30, 30, 66));
            if (!has)
                disc((float)x + 15.0f, (float)y + 15.0f, 8.0f, RGB(40, 40, 84));
            else if (i < 8)
                friend_mini((float)x + 15.0f, (float)y + 6.0f, 9.0f, EN_FRIEND[i].colour);
            else if (i < 16)
                style_icon(i - 8, (float)x + 15.0f, (float)y + 15.0f, 8.0f);
            else
                star((float)x + 15.0f, (float)y + 15.0f, 10.0f, 0.0f, NOTE_COL[(i - 16) % 12]);
        }
        return;
    }
    if (gm.state == GS_RESULT) {
        float u = fm_minf((float)(now - gm.t0) / 900.0f, 1.0f);
        if (b == 1) {
            text_c(120, BAND_H0 + 30, &FONT_B, gm.stars == 3 ? "Wonderful!" : gm.stars == 2 ? "Great!" : "Well done!", CREAM);
            return;
        }
        for (i = 0; i < 3; i++)
            star(60.0f + (float)i * 60.0f, 70.0f, (i < gm.stars ? 22.0f : 16.0f) * (i < gm.stars ? u : 1.0f), 0.0f,
                 i < gm.stars ? RGB(255, 210, 80) : RGB(60, 58, 110));
        return;
    }
    /* a game */
    if (b == 1) {
        text_c(120, BAND_H0 + 4, &FONT_S, GAME_NAME[gm.game], RGB(170, 170, 230));
        switch (gm.game) {
        case GM_FOLLOW: text_c(120, BAND_H0 + 30, &FONT_XS, TUNES[gm.tune].name, CREAM); break;
        case GM_HIGHLOW:
            icon(IC_BIG, 50, BAND_H0 + 60, 18, CREAM);
            icon(IC_SMALL, 190, BAND_H0 + 60, 18, CREAM);
            text_c(50, BAND_H0 + 80, &FONT_XS, "OCT- lower", CREAM);
            text_c(190, BAND_H0 + 80, &FONT_XS, "OCT+ higher", CREAM);
            break;
        case GM_MATCH: {
            static const char *const KN[4] = {"shape", "bright", "short/long", "wobble"};
            int n = match_knobs();
            for (i = 0; i < n; i++)
                text_c(30 + i * 60, BAND_H0 + 40, &FONT_XS, KN[i], CREAM);
            text_c(120, BAND_H0 + 66, &FONT_XS, "PLAY: hear it again", RGB(160, 160, 220));
            break;
        }
        case GM_NAME:
            for (i = 0; i < 3; i++) {
                char t[4];
                itoa_u((uint32_t)(i + 1), t);
                text_c(50 + i * 70, BAND_H0 + 36, &FONT_B, t, CREAM);
            }
            break;
        case GM_BEAT:
            for (i = 0; i < 8; i++)
                cv_round(14 + i * 27, BAND_H0 + 40, 22, 22, 6, gm.pat[i] ? RGB(80, 150, 255) : RGB(44, 42, 96));
            break;
        default: break;
        }
        {
            char t[12];
            itoa_u(gm.round, t);
            cv_text(220, BAND_H0 + 4, &FONT_XS, t, RGB(120, 120, 170));
        }
        return;
    }
    {   /* the top: the friend (shaking its head after a miss, hopping after a hit) */
        fr_t f;
        float hop = now - gm.right_t < 400u ? 1.0f - (float)(now - gm.right_t) / 400.0f : 0.0f;
        float shake = now - gm.wrong_t < 400u ? fm_sin_turns((float)(now - gm.wrong_t) / 100.0f) * 4.0f : 0.0f;
        friend_of(&f, L_TUNE, 44.0f + shake, 132.0f, 26.0f, NAVY);
        f.lift = fm_maxf(f.lift, hop);
        cv_rect(0, 132, 240, 12, RGB(40, 38, 90));
        if (gm.game == GM_HIGHLOW || gm.game == GM_BEAT) {
            f.x = 120.0f + shake;
        }
        friend_draw(&f);
        switch (gm.game) {
        case GM_FOLLOW: staff_draw(30, &TUNES[gm.tune], gm.pos); break;
        case GM_COPY:
            for (i = 0; i < 4; i++) {
                int lit = gm.state == GS_LISTEN && gm.demo_i > 0 && gm.seq[gm.demo_i - 1] == i && now - gm.demo_t < 400u;
                disc(110.0f + (float)i * 34.0f, 60.0f, lit ? 15.0f : 12.0f, lit ? NOTE_COL[COPY_NOTE[i] % 12] : RGB(60, 58, 120));
            }
            for (i = 0; i < gm.n; i++)
                disc(100.0f + (float)i * 8.0f, 96.0f, 3.0f, i < gm.i ? RGB(255, 210, 80) : RGB(90, 88, 150));
            break;
        case GM_MATCH: {
            float c = match_close();
            fr_t t;
            friend_of(&t, L_SPARK, 190.0f, 132.0f, 24.0f, NAVY);
            friend_draw(&t);
            cv_round(110, 20, 20, 100, 8, RGB(44, 42, 96));       /* warm .. cold */
            cv_round(112, 22 + (int)((1.0f - c) * 96.0f), 16, (int)(c * 96.0f), 6,
                     mix565(RGB(80, 150, 255), RGB(255, 90, 60), (int)(c * 256.0f)));
            break;
        }
        case GM_NAME:
            for (i = 0; i < 3; i++)
                friend_mini(110.0f + (float)i * 44.0f, 70.0f, 15.0f, EN_FRIEND[gm.choices[i]].colour);
            break;
        default: break;
        }
    }
}

static void games_leds(uint32_t *b, uint32_t *k, uint32_t *db, uint32_t *dk)
{
    int i;
    if (gm.state == GS_MENU) {
        for (i = 0; i <= GM_BOOK; i++)
            *(i == gm.sel ? k : dk) |= 1u << WHITE_KEY[i];
        *b |= 1u << B_PLAY;
        return;
    }
    if (gm.state != GS_TURN && gm.state != GS_LISTEN)
        return;
    switch (gm.game) {
    case GM_FOLLOW:
        *k |= 1u << key_of_note(TUNES[gm.tune].note[gm.pos]);
        break;
    case GM_COPY:
        for (i = 0; i < 4; i++)
            *dk |= 1u << key_of_note(COPY_NOTE[i]);
        if (gm.state == GS_LISTEN && gm.demo_i > 0 && plat_ms() - gm.demo_t < 400u)
            *k |= 1u << key_of_note(COPY_NOTE[gm.seq[gm.demo_i - 1]]);
        break;
    case GM_HIGHLOW: *b |= 1u << B_OCTUP | 1u << B_OCTDN; break;
    case GM_BEAT:
        if (gm.state == GS_TURN)
            for (i = 0; i < 11; i++)
                *dk |= 1u << BLACK_KEY[i];
        break;
    case GM_NAME:
        for (i = 0; i < 3; i++)
            *dk |= 1u << WHITE_KEY[i];
        break;
    default: break;
    }
    for (i = 0; i < NKEYS; i++)
        if (ui.key_note[i])
            *k |= 1u << i;
    *db |= 1u << B_PLAY;
}

static const card_t GAMES_CARD[NB] = {
    [B_HOME] = {IC_DOORS, "Doors", "Play, Make, Lab, Games.", 0},
    [B_PLAY] = {IC_PLAY, "Again", "Hear it again.", 0},
    [B_OCTDN] = {IC_BIG, "Lower", "The second note was lower.", 0},
    [B_OCTUP] = {IC_SMALL, "Higher", "The second note was higher.", 0},
    [B_EDIT] = {IC_NONE, "", 0, 0},          [B_SAVE] = {IC_NONE, "", 0, 0},
};
static const door_t DOOR_GAMES = {games_enter, games_key, games_button, games_enc, games_draw, games_leds, 0, GAMES_CARD};
