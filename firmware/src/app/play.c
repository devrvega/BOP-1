/* SPDX-License-Identifier: GPL-3.0-only */
/* The PLAY door (ages 3+, no reading needed). The music is already playing; everything is a toy:
 *
 *   white keys     the friend sings, in the scale (no wrong notes)    black keys   drums and funny sounds
 *   ALGORITHM      the friend         PRESETS   the style (the place changes)     SELECT   turtle .. rabbit
 *   K1 shape   K2 bright   K3 wobble   K4 echo (the room grows: bedroom, cave, canyon, space)
 *   ENV            short / medium / long notes (it hops, walks, floats)
 *   OCT- OCT+      lower (bigger) / higher (smaller)
 *   SEQ  drums on / off      ARP  sparkle on / off (and the keys held run up and down)      LFO  wobble on / off
 *   FX             Surprise! a new song in the same style;  FX held + a white key: a magic trick while held
 *   REC            Loop me: what you play goes round over the song (two bars); REC again to stop
 *   EDIT           oops (undo the last loop);  held: clear my loop      SAVE  keep this song
 *
 * The screen: the friend in its place, the band (the bass at the left, the sparkle at the right), the floor
 * lit by the drums, the knobs as pictures. The key lights show the song's tune, to play along with. Part of
 * ui.c. */

#define FLOOR_Y 112
static uint8_t play_wob_keep = 40;               /* LFO off keeps the wobble it had */

static int friends_n(void)                       /* the friends to pick from: the eight, then My Friends kept */
{
    int i, n = EN_NFRIEND;
    for (i = 0; i < NMYF; i++)
        n += myf.f[i].used;
    return n;
}
static int friend_at(int i)                      /* the i-th of them, as a who */
{
    int k;
    if (i < EN_NFRIEND)
        return i;
    i -= EN_NFRIEND;
    for (k = 0; k < NMYF; k++)
        if (myf.f[k].used && i-- == 0)
            return EN_NFRIEND + k;
    return 0;
}
static int friend_index(int who)
{
    int i, n = friends_n();
    for (i = 0; i < n; i++)
        if (friend_at(i) == who)
            return i;
    return 0;
}
static void put_friend(int layer, int who)       /* a friend (its sound and look) onto a layer */
{
    en_layer_t L = en_song.lay[layer];
    if (who >= EN_NFRIEND) {
        const myfriend_t *m = &myf.f[(who - EN_NFRIEND) % NMYF];
        en_layer_friend(&L, 0, m->p);
        L.who = (uint8_t)who;
        L.colour = m->colour;
        L.oct = 0;
    } else {
        en_layer_friend(&L, who, 0);
    }
    if (layer == L_BASS)
        L.oct = (int8_t)(L.oct - 1);
    L.mute = en_song.lay[layer].mute;
    if (layer != L_TUNE) {                        /* (the band's levels and sends stay as the song set them) */
        L.p[PK_LEVEL] = en_song.lay[layer].p[PK_LEVEL];
        L.p[PK_ECHO] = en_song.lay[layer].p[PK_ECHO];
    }
    en_song.lay[layer] = L;
    mark_work();
}

/* a layer's friend as the engine plays it now */
static void friend_of(fr_t *f, int layer, float x, float y, float r, uint16_t bg)
{
    const en_layer_t *L = &en_song.lay[layer];
    uint32_t now = plat_ms();
    float env = en_env[layer], lfo = en_lfo[layer] * (float)L->p[PK_LDEPTH] * 0.01f;
    memset(f, 0, sizeof *f);
    f->x = x;
    f->y = y;
    f->r = r * (1.0f - 0.13f * (float)(L->oct + (layer == L_BASS ? 1 : 0)));
    f->wave = (float)L->p[PK_WAVE1] * 0.04f;
    f->mouth = (float)L->p[PK_CUT] * 0.009f + (float)L->p[PK_FENV] * 0.004f * env;
    f->lift = env * (1.0f - (float)L->p[PK_S] * 0.004f);
    f->squash = fm_clampf((env - 0.5f) * 0.8f, -0.6f, 0.6f);
    switch (L->p[PK_LTARGET]) {
    case 0: f->bob = lfo * r * 0.22f; break;
    case 1: f->flap = lfo * 0.35f; break;
    default: f->fade = (float)L->p[PK_LDEPTH] * 0.01f * (0.5f + 0.5f * en_lfo[layer]) * 0.6f; break;
    }
    f->t = (float)now * 0.001f;
    f->look = fm_clampf((float)((int)en_lay_note[layer] - 64) * (1.0f / 18.0f), -1.0f, 1.0f);
    f->blink = (now + (uint32_t)layer * 1300u) % 3700u < 130u;
    f->col = who_colour(L->who, L->colour);
    f->bg = bg;
}

/* the scene: a place, the floor, the band, the notes, the echo's rings (y0: the floor's line) */
static void band_scene(int show_band)
{
    const en_layer_t *T = &en_song.lay[L_TUNE];
    uint32_t now = plat_ms();
    int pl = place_of(T->p[PK_ECHO]), sh = now - ui.kick_t < 70u ? 2 : 0;
    float ago[EN_NPAD], dt = (float)(now - ui.last_ms) * 0.001f;
    int i;
    fr_t f;
    if (dt > 0.1f)
        dt = 0.1f;
    for (i = 0; i < EN_NPAD; i++)
        ago[i] = (float)(now - ui.pad_t[i]) * 0.001f;
    place_draw(pl, (float)now * 0.001f, FLOOR_Y, (int)(en_steps / 4u) % 4);
    floor_draw(pl, FLOOR_Y + sh, BAND_H0, ago);
    {   /* the echo's rings, after each note of the tune */
        float age = (float)(now - ui.lay_t[L_TUNE]) * 0.001f, gap = 0.12f + (float)en_song.echo_time * 0.004f;
        int n = 1 + T->p[PK_ECHO] / 30;
        for (i = 0; i < n; i++) {
            float a = age - (float)i * gap;
            if (a > 0.0f && a < 0.7f)
                ring(120.0f, 82.0f, 30.0f + a * 110.0f, 2.5f, mix565(CREAM, PLACE_BG[pl], (int)(a * 360.0f)));
        }
    }
    if (show_band) {
        if (!en_song.lay[L_BASS].mute) {
            friend_of(&f, L_BASS, 36.0f, (float)(FLOOR_Y + 20 + sh), 20.0f, PLACE_FLOOR[pl]);
            friend_draw(&f);
        }
        if (!en_song.lay[L_SPARK].mute) {
            friend_of(&f, L_SPARK, 204.0f, (float)(FLOOR_Y + 20 + sh), 15.0f, PLACE_FLOOR[pl]);
            friend_draw(&f);
        }
    }
    friend_of(&f, L_TUNE, 120.0f, (float)(FLOOR_Y + 24 + sh), 38.0f, PLACE_FLOOR[pl]);
    {
        int slide = ui.ov == OV_FRIEND && now < ui.ov_until ? (int)(ui.ov_until - now) : 0;
        f.x += (float)slide * 0.3f * (ui.ov_arg ? 1.0f : -1.0f);
    }
    friend_draw(&f);
    notes_draw(dt);
    ui.last_ms = now;
}

/* the bottom: the pads' tiles (the floor's front) and the knobs as pictures */
static void knob_strip(const int *v, int y)
{
    static const uint16_t KC[4] = {RGB(255, 140, 90), RGB(255, 220, 80), RGB(120, 220, 140), RGB(120, 170, 255)};
    int k;
    for (k = 0; k < 4; k++) {
        int cx = 30 + k * 60;
        arc(cx, y, 25, 6.0f, (float)v[k] * 0.01f, RGB(52, 50, 110), KC[k]);
        knob_icon(k, (float)cx, (float)y, 11.0f, (float)v[k] * 0.01f, KC[k]);
    }
}
static void pad_tiles(int y0, int y1)
{
    uint32_t now = plat_ms();
    int i;
    for (i = 0; i < EN_NPAD; i++) {
        int x = 4 + i * 21;
        float g = (float)(now - ui.pad_t[i]) * 0.001f;
        g = g < 0.3f ? 1.0f - g / 0.3f : 0.0f;
        cv_round(x, y0, 19, y1 - y0, 5, mix565(RGB(44, 42, 96), PAD_COL[i], 60 + (int)(g * 190.0f)));
        pad_icon(i, (float)x + 9.5f, (float)(y0 + y1) * 0.5f, 5.5f + g * 1.5f, g > 0.2f ? INK : PAD_COL[i]);
    }
}

static void play_draw(int b)
{
    const en_layer_t *T = &en_song.lay[L_TUNE];
    if (b == 0) {
        band_scene(1);
        style_icon(glob.style, 16, 14, 8);        /* the style's badge */
        if (en_recording) {                       /* Loop me: a red dot, the two bars going round */
            float u = (float)((en_bar & 1) * 16 + en_step) / 32.0f;
            disc(224, 14, 7, RGB(255, 70, 70));
            arc(224, 14, 11, 2.5f, u, RGB(80, 40, 60), RGB(255, 120, 120));
        } else if (en_song.loop_on) {
            ring(224, 14, 7, 2.5f, RGB(255, 120, 120));
        }
        return;
    }
    {
        int v[4] = {T->p[PK_WAVE1], T->p[PK_CUT], T->p[PK_LDEPTH], T->p[PK_ECHO]};
        pad_tiles(BAND_H0 + 2, BAND_H0 + 26);
        knob_strip(v, BAND_H0 + 62);
    }
}

static void regen(uint32_t seed)                 /* a new song in the style: the friend, its sound, my loop kept */
{
    static en_song_t t;
    en_generate(&t, glob.style, seed);
    t.lay[L_TUNE] = en_song.lay[L_TUNE];
    memcpy(t.loop, en_song.loop, sizeof t.loop);
    t.loop_on = en_song.loop_on;
    t.lay[L_BEATS].mute = en_song.lay[L_BEATS].mute;
    t.lay[L_SPARK].mute = en_song.lay[L_SPARK].mute;
    en_song = t;
    glob.seed = seed;
    mark_glob();
    mark_work();
}

static void play_key(int k, int down)
{
    int w = white_of(k), bk = black_of(k);
    if (bk >= 0) {
        if (down)
            en_pad(bk, 0);
        return;
    }
    if (down) {
        int n = lay_note_of_white(L_TUNE, w);
        ui.key_note[k] = (uint8_t)n;
        ui.key_lay[k] = L_TUNE;
        en_note(L_TUNE, n, 1);
    } else if (ui.key_note[k]) {
        en_note(L_TUNE, ui.key_note[k], 0);
        ui.key_note[k] = 0;
    }
}

static void play_env(int which)                  /* short (a plink), medium, long (a swell) */
{
    static const uint8_t E[3][4] = {{0, 25, 0, 15}, {3, 45, 65, 30}, {45, 70, 80, 65}};
    en_layer_t *T = &en_song.lay[L_TUNE];
    T->p[PK_A] = E[which][0];
    T->p[PK_D] = E[which][1];
    T->p[PK_S] = E[which][2];
    T->p[PK_R] = E[which][3];
    mark_work();
}
static int play_env_now(void)
{
    const en_layer_t *T = &en_song.lay[L_TUNE];
    return T->p[PK_A] >= 30 ? 2 : T->p[PK_S] < 20 ? 0 : 1;
}

static void play_button(int b)
{
    en_layer_t *T = &en_song.lay[L_TUNE];
    switch (b) {
    case B_FX:                                    /* Surprise! */
        undo_save();
        regen(hash32(glob.seed + 0x9E37u));
        overlay(OV_DICE, 0, 900u);
        ui.chase_t = plat_ms();
        en_chain_play();
        if (!en_playing)
            en_play(1);
        break;
    case B_REC:
        if (en_recording) {
            en_rec(REC_OFF, 0);
        } else {
            undo_save();
            en_rec(REC_LOOP, 0);
            if (!en_playing)
                en_play(1);
        }
        break;
    case B_ENV:
        play_env((play_env_now() + 1) % 3);
        en_preview(L_TUNE, en_follow(L_TUNE, 67));
        break;
    case B_OCTDN:
    case B_OCTUP:
        T->oct = (int8_t)bump(T->oct, b == B_OCTUP ? 1 : -1, -2, 2);
        en_preview(L_TUNE, en_follow(L_TUNE, 67));
        mark_work();
        break;
    case B_SEQ:
        en_song.lay[L_BEATS].mute ^= 1u;
        mark_work();
        break;
    case B_ARP:
        en_song.lay[L_SPARK].mute ^= 1u;
        mark_work();
        break;
    case B_LFO:
        if (T->p[PK_LDEPTH]) {
            play_wob_keep = T->p[PK_LDEPTH];
            T->p[PK_LDEPTH] = 0;
        } else {
            T->p[PK_LDEPTH] = play_wob_keep ? play_wob_keep : 40;
        }
        mark_work();
        break;
    default: break;
    }
    en_arp_live = !en_song.lay[L_SPARK].mute;
}

static void play_enc(int role, int d)
{
    en_layer_t *T = &en_song.lay[L_TUNE];
    switch (role) {
    case EN_ALGO: {
        int n = friends_n(), i = (friend_index(T->who) + (d > 0 ? 1 : n - 1)) % n;
        uint8_t keep[PK_N];
        int k;
        memcpy(keep, T->p, PK_N);
        put_friend(L_TUNE, friend_at(i));
        for (k = PK_ECHO; k <= PK_REVERB; k++)    /* (the room stays the child's) */
            T->p[k] = keep[k];
        overlay(OV_FRIEND, d > 0, 500u);
        en_preview(L_TUNE, en_follow(L_TUNE, 72));
        break;
    }
    case EN_PRESET:
        undo_save();
        glob.style = (uint8_t)((glob.style + (d > 0 ? 1 : EN_NSTYLE - 1)) % EN_NSTYLE);
        regen(glob.seed);
        overlay(OV_STYLE, glob.style, 900u);
        break;
    case EN_SELECT:
        en_song.tempo = (uint8_t)bump(en_song.tempo, (int)accel(role, d, 120) * 2, 60, 180);
        overlay(OV_TEMPO, 0, 1200u);
        mark_work();
        break;
    case EN_K1: {                                 /* both oscillators' shapes, together */
        int dd = (int)accel(role, d, 100) * 2, w2 = T->p[PK_WAVE2] - T->p[PK_WAVE1];
        T->p[PK_WAVE1] = (uint8_t)bump(T->p[PK_WAVE1], dd, 0, 100);
        T->p[PK_WAVE2] = (uint8_t)bump(T->p[PK_WAVE1], w2, 0, 100);
        break;
    }
    case EN_K2: T->p[PK_CUT] = (uint8_t)bump(T->p[PK_CUT], (int)accel(role, d, 100) * 2, 8, 96); break;
    case EN_K3:
        T->p[PK_LDEPTH] = (uint8_t)bump(T->p[PK_LDEPTH], (int)accel(role, d, 100) * 2, 0, 100);
        T->p[PK_LRATE] = (uint8_t)(30 + T->p[PK_LDEPTH] / 2);   /* the rate follows */
        break;
    case EN_K4:
        T->p[PK_ECHO] = (uint8_t)bump(T->p[PK_ECHO], (int)accel(role, d, 100) * 2, 0, 100);
        T->p[PK_REVERB] = (uint8_t)(T->p[PK_ECHO] * 4 / 5);
        break;
    default: break;
    }
    if (role >= EN_K1)
        mark_work();
}

static void play_leds(uint32_t *b, uint32_t *k, uint32_t *db, uint32_t *dk)
{
    static uint32_t pulse_seen;
    static int pulse_on;
    uint32_t now = plat_ms();
    int i;
    if (en_tune_next)                             /* the song's tune: play along with the lights */
        for (i = 0; i < 16; i++)
            if (lay_note_of_white(L_TUNE, i) == en_tune_next)
                *k |= 1u << WHITE_KEY[i];
    for (i = 0; i < NKEYS; i++)
        if (ui.key_note[i])
            *k |= 1u << i;
    if (en_playing && glob.lights == 0)
        *dk |= 1u << WHITE_KEY[en_step];          /* the playhead, dimly */
    if (!en_song.lay[L_BEATS].mute)
        *b |= 1u << B_SEQ;
    if (!en_song.lay[L_SPARK].mute)
        *b |= 1u << B_ARP;
    if (en_song.lay[L_TUNE].p[PK_LDEPTH])
        *b |= 1u << B_LFO;
    if (en_recording)
        *b |= 1u << B_REC;
    if (en_song.lay[L_TUNE].oct > 0)
        *b |= 1u << B_OCTUP;
    if (en_song.lay[L_TUNE].oct < 0)
        *b |= 1u << B_OCTDN;
    if (ui.fx_trick)
        *b |= 1u << B_FX;
    if (ui.kick_t != pulse_seen) {                /* the buttons pulse with the kick (three a second at most) */
        pulse_seen = ui.kick_t;
        pulse_on = en_playing && flash_gate();
    }
    if (pulse_on && now - ui.kick_t < 90u)
        *db |= (1u << NB) - 1u;
}

static void play_clear(void)                      /* EDIT held: my loop goes */
{
    int i;
    en_rec(REC_OFF, 0);
    for (i = 0; i < EN_LOOP; i++) {
        en_song.loop[i].note[0] = en_song.loop[i].note[1] = 0;
        en_song.loop[i].pads_lo = en_song.loop[i].pads_hi = 0;
        en_song.loop[i].len = 1;
    }
    en_song.loop_on = 0;
    mark_work();
}

static void play_enter(void)
{
    en_arp_live = !en_song.lay[L_SPARK].mute;
    en_part_now(0xFF);
}

static const card_t PLAY_CARD[NB] = {
    [B_FX] = {IC_DICE, "Surprise!", 0, 0},        [B_SEL] = {IC_NONE, "", 0, 0},
    [B_ENV] = {IC_HOP, "Short or long", 0, 0},    [B_LFO] = {IC_WOBBLE, "Wobble", 0, 0},
    [B_EDIT] = {IC_UNDO, "Oops!", 0, 0},          [B_GLO] = {IC_NONE, "", 0, 0},
    [B_HOME] = {IC_DOORS, "Doors", 0, 0},         [B_SAVE] = {IC_SAVE, "Keep it", 0, 0},
    [B_ARP] = {IC_SPARK, "Sparkle", 0, 0},        [B_SEQ] = {IC_DRUM, "Drums", 0, 0},
    [B_PLAY] = {IC_PLAY, "Music", 0, 0},          [B_REC] = {IC_REC, "Loop me", 0, 0},
    [B_OCTDN] = {IC_BIG, "Lower, bigger", 0, 0},  [B_OCTUP] = {IC_SMALL, "Higher, smaller", 0, 0},
};
static const door_t DOOR_PLAY = {play_enter, play_key, play_button, play_enc, play_draw, play_leds, play_clear, PLAY_CARD};
