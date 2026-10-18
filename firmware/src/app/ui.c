/* SPDX-License-Identifier: GPL-3.0-only */
/* BOP-1 UI: the panel and the screen. Part of the unity build (after gfx.c and project.c); the host
 * simulator includes it too.
 *
 * Four doors: PLAY (ages 3+: music is already playing, everything is a toy), MAKE (6-8: four layers, steps,
 * loops, parts and a song), LAB (9-12+: the real synth, by its real names, with a scope) and GAMES. HOME opens
 * the doors (a grown-up picks which show; with only Play, HOME does nothing and nobody gets lost). Each door
 * gives the buttons its own jobs and the screen always shows them: hold any button for a second and a card
 * says what it does now. Some things are the same everywhere:
 *
 *   PLAY           the music on / off                 HOME        the doors
 *   EDIT           oops (undo); held 2 s: clear (a ring fills while it is held)
 *   SAVE           keep this song (a white key picks one of eight places)
 *   GLO + SAVE     held 3 s: the grown-ups' page      OCT- + OCT+ held 5 s: update mode
 *
 * Nothing destructive happens on a tap; no screen strands anyone (HOME always goes home). */
#include "../dsp/fastmath.h"

static const uint8_t WHITE_KEY[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t BLACK_KEY[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
static int white_of(int k)
{
    int i;
    for (i = 0; i < 16; i++)
        if (WHITE_KEY[i] == k)
            return i;
    return -1;
}
static int black_of(int k)
{
    int i;
    for (i = 0; i < 11; i++)
        if (BLACK_KEY[i] == k)
            return i;
    return -1;
}

/* the overlays: a picture for a moment, after a knob or a button */
enum { OV_NONE, OV_TEMPO, OV_STYLE, OV_FRIEND, OV_DICE, OV_KEPT, OV_ENV, OV_OCT, OV_TOGGLE, OV_UNDO, OV_CLEARED,
       OV_SAY };
enum { PK_NONE, PK_KEEP, PK_OPEN };               /* the slot picker */

typedef struct {
    void (*enter)(void);
    void (*key)(int k, int down);                 /* a key */
    void (*button)(int b);                        /* a button tapped (let go before its card) */
    void (*enc)(int role, int d);
    void (*draw)(int band);                       /* the screen's top (0) or bottom (1) */
    void (*leds)(uint32_t *b, uint32_t *k, uint32_t *db, uint32_t *dk);
    void (*clear)(void);                          /* EDIT held */
    const struct card_s *cards;                   /* each button's card */
} door_t;
typedef struct card_s {
    uint8_t icon;
    const char *title;                            /* the kid's words */
    const char *line;                             /* one sentence (Make, Lab) */
    const char *term;                             /* the real term (Lab) */
} card_t;

static struct {
    uint8_t door, home, home_sel;
    uint32_t btn, keys, btn_used, keys_used;
    uint32_t down_t[NB];
    uint32_t enc_t[NE];
    uint8_t enc_run[NE];
    uint32_t act_t;                               /* the last touch */
    uint32_t turn_t;
    uint8_t help;                                 /* a card is up: the button + 1 */
    uint8_t ov, ov_arg;
    uint32_t ov_t, ov_until;
    char msg[2][28];
    uint8_t picker;                               /* PK_* */
    uint32_t frame, last_ms;
    uint32_t sig[2];
    uint8_t fx_trick;                             /* the trick held + 1 */
    uint8_t key_note[NKEYS], key_lay[NKEYS];      /* what each held key plays (0 none) */
    uint8_t gu;                                   /* the grown-ups' page */
    uint8_t dimmed, asleep;
    uint32_t sleep_t0;
    uint32_t chase_t;                             /* a rainbow on the keys, since */
    uint32_t flash_t;                             /* the last whole-panel flash */
    uint8_t seen_lay[EN_NLAYER], seen_pad[EN_NPAD], seen_live;
    uint32_t lay_t[EN_NLAYER], pad_t[EN_NPAD];
    uint32_t kick_t;
    float env_prev;
    uint8_t has_undo;
} ui;
static en_song_t undo_song;
static const uint16_t SLEEP_MIN[5] = {0, 5, 10, 20, 30};   /* the sleep timer's choices */

static int slen(const char *s)
{
    int n = 0;
    while (s[n])
        n++;
    return n;
}
static void scpy(char *d, const char *s, int n)
{
    int i;
    for (i = 0; i < n - 1 && s && s[i]; i++)
        d[i] = s[i];
    d[i] = 0;
}
static void itoa_u(uint32_t v, char *b)
{
    char t[12];
    int n = 0;
    do {
        t[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n)
        *b++ = t[--n];
    *b = 0;
}
static void overlay(int ov, int arg, uint32_t ms)
{
    ui.ov = (uint8_t)ov;
    ui.ov_arg = (uint8_t)arg;
    ui.ov_t = plat_ms();
    ui.ov_until = plat_ms() + ms;
}
static void say(const char *a, const char *b)
{
    scpy(ui.msg[0], a, sizeof ui.msg[0]);
    scpy(ui.msg[1], b, sizeof ui.msg[1]);
    overlay(OV_SAY, 0, 1500u);
}
void ui_say(const char *a, const char *b) { say(a, b); }
int ui_dirty(void) { return project_is_dirty(); }
static void mark_work(void) { project_dirty(PD_WORK); }
static void mark_glob(void) { project_dirty(PD_GLOB); }

static void undo_save(void)
{
    undo_song = en_song;
    ui.has_undo = 1;
}
static void undo(void)                            /* oops: the song as it was before the last big change */
{
    en_song_t t;
    if (!ui.has_undo)
        return;
    t = en_song;
    en_song = undo_song;
    undo_song = t;
    mark_work();
    overlay(OV_UNDO, 0, 900u);
}

/* knob acceleration by speed (X0X's): a few clicks one step each; a long quick turn up to 8 a detent */
static uint8_t accel_off;                         /* the host simulator's "spin": exact steps */
static int32_t accel(int role, int32_t s, int range)
{
    uint32_t now = plat_ms(), dt = now - ui.enc_t[role], a = (uint32_t)(s < 0 ? -s : s), m;
    ui.enc_t[role] = now;
    if (!a)
        return s;
    if (a > 1)
        dt /= a;
    ui.enc_run[role] = (uint8_t)(dt < 60u ? (ui.enc_run[role] + a > 255u ? 255u : ui.enc_run[role] + a) : a);
    if (range <= 24 || accel_off || ui.enc_run[role] < 4u)
        return s;
    m = dt < 12u ? 6u : dt < 25u ? 4u : dt < 45u ? 2u : 1u;
    return s * (int32_t)m;
}
static int bump(int v, int d, int lo, int hi)
{
    v += d;
    return v < lo ? lo : v > hi ? hi : v;
}

/* the photosensitivity rule: whole-panel flashes at most three a second (WCAG 2.3.1), none when calm */
static int flash_gate(void)
{
    uint32_t now = plat_ms();
    if (glob.lights != 0 || now - ui.flash_t < 340u)
        return 0;
    ui.flash_t = now;
    return 1;
}

extern void (*ui_mark)(int part);
#define UI_MARK_(p)                                                                                                   \
    do {                                                                                                              \
        if (ui_mark)                                                                                                  \
            ui_mark(p);                                                                                               \
    } while (0)

#include "draw.c"
#include "friends.c"

/* the screen in two bands, drawn by turns (each at 30 frames a second) */
#define BAND_H0 144
#define BAND_H1 96
static void band(int32_t y, int32_t h, uint16_t bg)   /* a band of the screen, drawn in screen co-ordinates */
{
    cv_begin(240, (uint32_t)h, bg);
    cv_oy = -y;
    cv_y0 = 0;
    cv_y1 = h;
}
static uint32_t hash(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

/* --------------------------------------------------------------- icons --- */
enum { IC_NONE, IC_PLAY, IC_REC, IC_UNDO, IC_HOP, IC_BIG, IC_SMALL, IC_DOORS, IC_DRUM, IC_SPARK, IC_WOBBLE, IC_DICE,
       IC_SAVE, IC_SHAPE, IC_BRIGHT, IC_ECHO, IC_TEMPO, IC_STYLE, IC_FRIEND, IC_STEPS, IC_PARTS, IC_CHORDS, IC_MUTE,
       IC_OSC, IC_FILTER, IC_ENV, IC_LFO, IC_ARP, IC_FX, IC_PIANO, IC_MAGIC, IC_LAYER, IC_STAR };
static void dice(float x, float y, float s, int face)
{
    static const uint16_t PIPS[6] = {0x010, 0x044, 0x054, 0x145, 0x155, 0x16D};
    int i;
    uint32_t m = PIPS[face % 6];
    cv_round((int32_t)(x - s), (int32_t)(y - s), (int32_t)(2 * s), (int32_t)(2 * s), (int32_t)(s * 0.4f), CREAM);
    for (i = 0; i < 9; i++)
        if (m >> i & 1u)
            disc(x + (float)(i % 3 - 1) * s * 0.55f, y + (float)(i / 3 - 1) * s * 0.55f, s * 0.17f, INK);
}
static void icon(int id, float x, float y, float s, uint16_t c)
{
    switch (id) {
    case IC_PLAY: {
        float xy[6] = {x - s * 0.6f, y - s * 0.8f, x + s * 0.8f, y, x - s * 0.6f, y + s * 0.8f};
        poly(xy, 3, c);
        break;
    }
    case IC_REC: disc(x, y, s * 0.75f, RGB(255, 70, 70)); break;
    case IC_UNDO:
        ring(x, y, s * 0.7f, s * 0.22f, c);
        {
            float xy[6] = {x - s * 1.0f, y - s * 0.1f, x - s * 0.4f, y - s * 0.1f, x - s * 0.7f, y + s * 0.4f};
            poly(xy, 3, c);
        }
        break;
    case IC_HOP:
        disc(x, y - s * 0.2f, s * 0.5f, c);
        thick(x - s * 0.8f, y + s * 0.8f, x + s * 0.8f, y + s * 0.8f, 2.5f, c);
        thick(x - s * 0.9f, y + s * 0.2f, x - s * 0.6f, y - s * 0.4f, 2.0f, c);
        break;
    case IC_BIG:
    case IC_SMALL: {
        float r = id == IC_BIG ? s * 0.85f : s * 0.4f;
        disc(x, y, r, c);
        disc(x - r * 0.35f, y - r * 0.2f, r * 0.15f, INK);
        disc(x + r * 0.35f, y - r * 0.2f, r * 0.15f, INK);
        break;
    }
    case IC_DOORS: {
        int i;
        for (i = 0; i < 4; i++)
            cv_round((int32_t)(x - s + (float)(i % 2) * s * 1.05f), (int32_t)(y - s + (float)(i / 2) * s * 1.05f),
                     (int32_t)(s * 0.9f), (int32_t)(s * 0.9f), 2, c);
        break;
    }
    case IC_DRUM: pad_icon(PAD_KICK, x, y, s * 0.8f, c); break;
    case IC_SPARK:
    case IC_STAR: star(x, y, s * 0.9f, 0.0f, c); break;
    case IC_WOBBLE:
    case IC_LFO: knob_icon(2, x, y, s * 0.9f, 0.8f, c); break;
    case IC_DICE: dice(x, y, s * 0.8f, 4); break;
    case IC_SAVE: heart(x, y - s * 0.15f, s * 0.75f, RGB(255, 90, 140)); break;
    case IC_SHAPE:
    case IC_OSC: knob_icon(0, x, y, s * 0.8f, 0.5f, c); break;
    case IC_BRIGHT:
    case IC_FILTER: knob_icon(1, x, y, s * 0.8f, 0.7f, c); break;
    case IC_ECHO:
    case IC_FX:
        ring(x, y, s * 0.4f, 2.5f, c);
        ring(x, y, s * 0.75f, 2.5f, c);
        ring(x, y, s * 1.1f, 2.5f, mix565(c, NAVY, 128));
        break;
    case IC_TEMPO:
        ellipse(x, y, s * 0.8f, s * 0.5f, RGB(90, 200, 110));
        disc(x + s * 0.8f, y - s * 0.1f, s * 0.25f, RGB(90, 200, 110));
        break;
    case IC_STYLE: style_icon(glob.style, x, y, s * 0.8f); break;
    case IC_FRIEND: {
        fr_t f;
        memset(&f, 0, sizeof f);
        f.x = x;
        f.y = y + s;
        f.r = s * 0.7f;
        f.mouth = 0.5f;
        f.col = c;
        f.bg = NAVY;
        friend_draw(&f);
        break;
    }
    case IC_STEPS:
    case IC_PIANO: {
        int i;
        for (i = 0; i < 4; i++)
            cv_round((int32_t)(x - s + (float)i * s * 0.52f), (int32_t)(y - s * 0.6f), (int32_t)(s * 0.42f),
                     (int32_t)(s * 1.2f), 2, (i & 1) ? mix565(c, NAVY, 140) : c);
        break;
    }
    case IC_PARTS: {
        static const char *const L[4] = {"A", "B", "C", "D"};
        int i;
        for (i = 0; i < 4; i++)
            cv_text((int32_t)(x - s + (float)i * s * 0.5f), (int32_t)(y - 9), &FONT_S, L[i], c);
        break;
    }
    case IC_CHORDS:
        quaver(x - s * 0.4f, y + s * 0.4f, s * 0.35f, c);
        quaver(x + s * 0.3f, y + s * 0.2f, s * 0.35f, c);
        break;
    case IC_MUTE:
        ring(x, y, s * 0.75f, 3.0f, c);
        thick(x - s * 0.5f, y + s * 0.5f, x + s * 0.5f, y - s * 0.5f, 3.0f, c);
        break;
    case IC_ENV: {
        float px[5] = {x - s, x - s * 0.5f, x, x + s * 0.5f, x + s}, py[5] = {y + s * 0.6f, y - s * 0.7f, y - s * 0.1f,
                                                                                 y - s * 0.1f, y + s * 0.6f};
        int i;
        for (i = 0; i < 4; i++)
            thick(px[i], py[i], px[i + 1], py[i + 1], 2.5f, c);
        break;
    }
    case IC_ARP: {
        int i;
        for (i = 0; i < 4; i++)
            disc(x - s * 0.75f + (float)i * s * 0.5f, y + s * 0.5f - (float)i * s * 0.35f, s * 0.18f, c);
        break;
    }
    case IC_MAGIC: star(x, y, s * 0.6f, 0.05f, RGB(255, 220, 90)); thick(x - s, y + s, x - s * 0.3f, y + s * 0.3f, 3.0f, c); break;
    case IC_LAYER: {
        int i;
        for (i = 0; i < 3; i++)
            cv_round((int32_t)(x - s), (int32_t)(y - s * 0.8f + (float)i * s * 0.6f), (int32_t)(2 * s), (int32_t)(s * 0.4f),
                     2, mix565(c, NAVY, i * 60));
        break;
    }
    default: break;
    }
}

/* ------------------------------------------------------- the doors' parts --- */
static void door_go(int d);
static void picker_open(int kind);
static int lay_note_of_white(int layer, int i);
static void live_keys_off(void);
#include "play.c"
#include "make.c"
#include "lab.c"
#include "lessons.c"
#include "games.c"
#include "grownups.c"
static const door_t *const DOORS[ND] = {&DOOR_PLAY, &DOOR_MAKE, &DOOR_LAB, &DOOR_GAMES};
static const door_t *dp(void) { return DOORS[ui.door % ND]; }

/* the white key i as a note of the layer: the scale's in Play and Make (no wrong notes), the keys as they
 * are in the Lab's piano */
static int lay_note_of_white(int layer, int i)
{
    return en_scale_note(en_song.scale, en_song.key, i) + 12 * en_song.lay[layer].oct;
}
static void live_keys_off(void)                   /* every key's note let go (a door changes under them) */
{
    int k;
    for (k = 0; k < NKEYS; k++)
        if (ui.key_note[k]) {
            en_note(ui.key_lay[k], ui.key_note[k], 0);
            ui.key_note[k] = 0;
        }
    if (ui.fx_trick) {
        ui.fx_trick = 0;
        en_trick(0);
    }
}

static int doors_n(void)
{
    int d, n = 0;
    for (d = 0; d < ND; d++)
        n += (glob.doors >> d) & 1;
    return n;
}
static void door_go(int d)
{
    if (!(glob.doors >> d & 1u))
        d = D_PLAY;
    if (ui.door == D_GAMES && d != D_GAMES)
        games_leave();
    live_keys_off();
    en_rec(REC_OFF, 0);
    ui.home = 0;
    ui.picker = PK_NONE;
    ui.help = 0;
    ui.door = (uint8_t)d;
    if (glob.door != d) {
        glob.door = (uint8_t)d;
        mark_glob();
    }
    en_safe = (uint8_t)(d != D_LAB || en_glob.v[G_SAFE]);
    en_arp_live = 0;
    en_duck = d == D_PLAY;
    en_chain_play();
    if (project_is_dirty())                       /* a change of door: a natural break to keep the song */
        project_save();
    ui.sig[0] = ui.sig[1] = 0;
    if (dp()->enter)
        dp()->enter();
}

/* --------------------------------------------------------- the slot picker --- */
static uint8_t slot_c_used[NSLOT], slot_c_who[NSLOT], slot_c_col[NSLOT];   /* the kept songs' pictures */
static void slots_read(void)
{
    int i;
    for (i = 0; i < NSLOT; i++)
        slot_c_used[i] = (uint8_t)slot_look(i, &slot_c_who[i], &slot_c_col[i]);
}
static void picker_open(int kind)
{
    live_keys_off();
    slots_read();
    ui.picker = (uint8_t)kind;
}
static void picker_key(int i)
{
    if (i < 0 || i >= NSLOT)
        return;
    if (ui.picker == PK_KEEP) {
        en_preview(L_TUNE, 72);
        if (slot_keep(i) == 0) {
            slots_read();
            en_pad(PAD_TADA, 1);
            overlay(OV_KEPT, i, 1600u);
            ui.chase_t = plat_ms();
        }
    } else if (slot_c_used[i]) {
        undo_save();
        slot_open(i);
        en_pad(PAD_POP, 0);
        en_chain_play();
    } else {
        en_pad(PAD_POP, 0);
        return;
    }
    ui.picker = PK_NONE;
}
static void picker_draw(void)
{
    int i;
    cv_round(6, 6, 228, 132, 12, RGB(36, 34, 86));
    icon(ui.picker == PK_KEEP ? IC_SAVE : IC_PLAY, 22, 22, 12, ui.picker == PK_KEEP ? CREAM : RGB(90, 220, 110));
    if (glob.door != D_PLAY) {
        cv_text(40, 12, &FONT_S, ui.picker == PK_KEEP ? "Keep it here" : "Open a song", CREAM);
        cv_text(232 - text_w(&FONT_XS, "turn: keep / open"), 15, &FONT_XS, "turn: keep / open", RGB(150, 150, 210));
    }
    for (i = 0; i < NSLOT; i++) {
        int x = 16 + (i % 4) * 54, y = 40 + (i / 4) * 48;
        uint8_t who = slot_c_who[i], col = slot_c_col[i];
        int used = slot_c_used[i];
        cv_round(x, y, 48, 42, 8, used ? RGB(60, 58, 130) : RGB(46, 44, 100));
        if (used) {
            fr_t f;
            memset(&f, 0, sizeof f);
            f.x = (float)x + 24.0f;
            f.y = (float)y + 38.0f;
            f.r = 13.0f;
            f.wave = who < EN_NFRIEND ? (float)EN_FRIEND[who].p[PK_WAVE1] * 0.04f : 1.0f;
            f.mouth = 0.4f;
            f.col = who_colour(who, col);
            f.bg = RGB(60, 58, 130);
            friend_draw(&f);
        } else {
            ring((float)x + 24.0f, (float)y + 21.0f, 9.0f, 2.0f, RGB(80, 78, 150));
        }
        if (glob.slot == i)
            ring((float)x + 41.0f, (float)y + 8.0f, 4.0f, 2.0f, RGB(255, 220, 90));
    }
}

/* -------------------------------------------------------------- the cards --- */
static const char *const BTN_LABEL[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                          "PLAY", "REC", "OCT-", "OCT+"};
#define HELP_HOLD 900u
static void card_draw(void)
{
    const card_t *c = &dp()->cards[ui.help - 1];
    if (ui.home)
        c = &dp()->cards[B_HOME];
    cv_round(10, 10, 220, 124, 14, CREAM);
    cv_round(14, 14, 212, 116, 12, RGB(40, 38, 96));
    icon(c->icon, 120, 46, 20, CREAM);
    text_c(120, 70, &FONT_XS, BTN_LABEL[ui.help - 1], RGB(160, 160, 220));
    text_c(120, 84, &FONT_B, c->title ? c->title : "", CREAM);
    if (ui.door != D_PLAY && c->line)
        text_c(120, 106, &FONT_XS, c->line, RGB(200, 200, 240));
    if (ui.door == D_LAB && c->term)
        text_c(120, 118, &FONT_XS, c->term, RGB(255, 210, 120));
}

/* --------------------------------------------------------- the doors' screen --- */
static const char *const DOOR_NAME[ND] = {"Play", "Make", "Lab", "Games"};
static const uint16_t DOOR_COL[ND] = {RGB(255, 110, 90), RGB(90, 200, 120), RGB(90, 150, 255), RGB(255, 200, 70)};
static void home_draw(int b)
{
    int d, n = 0;
    if (b == 1) {
        text_c(120, 160, &FONT_M, DOOR_NAME[ui.home_sel], DOOR_COL[ui.home_sel]);
        text_c(120, 204, &FONT_XS, "PLAY opens it", RGB(150, 150, 200));
        return;
    }
    for (d = 0; d < ND; d++) {
        int x = 14 + (d % 2) * 112, y = 8 + (d / 2) * 68, on = glob.doors >> d & 1u, sel = d == ui.home_sel;
        uint16_t c = on ? DOOR_COL[d] : RGB(50, 50, 80);
        if (sel)
            cv_round(x - 4, y - 4, 108, 66, 12, CREAM);
        cv_round(x, y, 100, 58, 10, shade(c, 140));
        cv_round(x + 30, y + 6, 40, 52, 8, c);    /* a door */
        disc((float)x + 62.0f, (float)y + 34.0f, 3.0f, INK);
        if (!on)
            continue;
        n++;
        switch (d) {
        case D_PLAY: quaver((float)x + 50.0f, (float)y + 36.0f, 8.0f, INK); break;
        case D_MAKE: {
            int i;
            for (i = 0; i < 3; i++)
                cv_round(x + 38 + (i % 2) * 12, y + 22 + (i / 2) * 12, 11, 11, 2, INK);
            break;
        }
        case D_LAB: knob_icon(2, (float)x + 50.0f, (float)y + 30.0f, 14.0f, 0.9f, INK); break;
        default: star((float)x + 50.0f, (float)y + 30.0f, 12.0f, 0.0f, INK); break;
        }
    }
    (void)n;
}
static void home_key(int i)
{
    if (i < 0 || i >= ND || !(glob.doors >> i & 1u))
        return;
    if (ui.home_sel == i)
        door_go(i);
    else {
        ui.home_sel = (uint8_t)i;
        en_pad(PAD_POP, 0);
    }
}
static void home_move(int d)
{
    int i, s = ui.home_sel;
    for (i = 0; i < ND; i++) {
        s = (s + (d > 0 ? 1 : ND - 1)) % ND;
        if (glob.doors >> s & 1u)
            break;
    }
    ui.home_sel = (uint8_t)s;
}

/* --------------------------------------------------------------- MIDI in --- */
/* (USB and the TRS jack) notes play the friend, channel 10 the pads (General MIDI's drums); clock in */
static const uint8_t GM_PAD[EN_NPAD] = {36, 38, 39, 42, 45, 56, 49, 51, 37, 46, 57};
static uint32_t midi_rx_count;
static void midi_in(void)
{
    uint32_t pkt;
    while (plat_midi_in(&pkt)) {
        uint32_t st = (pkt >> 8) & 0xFFu, n = (pkt >> 16) & 0x7Fu, v = pkt >> 24, ch = st & 15u;
        midi_rx_count++;
        if (st == 0xF8u || st == 0xFAu || st == 0xFBu || st == 0xFCu) {
            en_clock(st == 0xF8u ? EN_CLK_TICK : st == 0xFAu ? EN_CLK_START : st == 0xFBu ? EN_CLK_CONTINUE : EN_CLK_STOP);
        } else if ((st & 0xE0u) == 0x80u) {
            int on = (st & 0xF0u) == 0x90u && v;
            if (ch == 9) {
                int p;
                for (p = 0; p < EN_NPAD; p++)
                    if (GM_PAD[p] == n && on)
                        en_pad(p, v > 100);
            } else {
                en_note(L_TUNE, (int)n, on);
            }
            ui.act_t = plat_ms();
        }
    }
}

/* ------------------------------------------------------------------ input --- */
static void wake(void)                            /* a touch: lit again (after the sleep timer, its time again) */
{
    if (ui.asleep) {
        ui.sleep_t0 = plat_ms();
        en_fade_in();
    }
    ui.dimmed = ui.asleep = 0;
}

static void input(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), ch, i, now = plat_ms();
    const uint32_t octs = 1u << B_OCTDN | 1u << B_OCTUP, gs = 1u << B_GLO | 1u << B_SAVE;
    int32_t e;
    ch = btn ^ ui.btn;
    ui.btn = btn;
    if ((btn & octs) == octs)
        ui.btn_used |= octs;
    if ((btn & gs) == gs) {                       /* GLO + SAVE, 3 s: the grown-ups */
        ui.btn_used |= gs;
        if (now - ui.down_t[B_GLO] > 3000u && now - ui.down_t[B_SAVE] > 3000u && !ui.gu)
            gu_open();
    }
    for (i = 0; i < NB; i++) {
        uint32_t m = 1u << i;
        if (!(ch & m))
            continue;
        if (btn & m) {
            ui.btn_used &= ~m;
            ui.down_t[i] = now;
            wake();
            if (i == B_PLAY && !ui.gu && !ui.home && ui.door != D_GAMES) {   /* PLAY acts at once */
                en_play(!en_playing);
                en_fade_in();
                ui.btn_used |= m;
            }
            if (i == B_FX && !ui.gu)
                ui.fx_trick = 0;
        } else {
            if (i == B_FX && ui.fx_trick) {       /* FX let go: the trick too */
                ui.fx_trick = 0;
                en_trick(0);
                ui.btn_used |= m;
            }
            if (ui.help == i + 1) {
                ui.help = 0;
                continue;
            }
            if (ui.btn_used & m)
                continue;
            if (now - ui.down_t[i] > 450u && i != B_HOME)
                continue;                         /* held a while (to see, to hold): not a tap */
            if (ui.gu) {
                gu_button((int)i);
            } else if (i == B_HOME) {
                if (ui.home)
                    ui.home = 0;
                else if (ui.picker)
                    ui.picker = PK_NONE;
                else if (doors_n() > 1) {
                    live_keys_off();
                    ui.home = 1;
                    ui.home_sel = ui.door;
                }
            } else if (ui.home) {
                if (i == B_PLAY)
                    door_go(ui.home_sel);
            } else if (ui.picker) {
                ui.picker = PK_NONE;
            } else if (i == B_SAVE && ui.door != D_GAMES && ui.door != D_LAB) {
                picker_open(PK_KEEP);
            } else if (i == B_EDIT && ui.door != D_GAMES) {
                undo();
            } else if (dp()->button) {
                dp()->button((int)i);
            }
        }
    }
    /* held: the card (a single button, a second), EDIT's clear */
    if (!ui.help && btn && !(btn & (btn - 1u)) && !keys && !ui.gu) {
        for (i = 0; i < NB; i++)
            if (btn == 1u << i && i != B_PLAY && i != B_EDIT && !(ui.btn_used & btn) && now - ui.down_t[i] >= HELP_HOLD &&
                (int32_t)(ui.turn_t - ui.down_t[i]) <= 0 && !(i == B_FX && ui.fx_trick)) {
                ui.help = (uint8_t)(i + 1);
                ui.btn_used |= btn;
            }
    }
    if ((btn >> B_EDIT & 1u) && !(ui.btn_used >> B_EDIT & 1u) && now - ui.down_t[B_EDIT] >= 2000u && !ui.gu) {
        ui.btn_used |= 1u << B_EDIT;
        undo_save();
        if (dp()->clear)
            dp()->clear();
        overlay(OV_CLEARED, 0, 1000u);
    }
    ch = keys ^ ui.keys;
    ui.keys = keys;
    for (i = 0; i < NKEYS; i++)
        if (ch >> i & 1u) {
            int down = (keys >> i & 1u) != 0, w = white_of((int)i);
            if (down)
                wake();
            if (ui.gu) {
                if (down)
                    gu_key((int)i);
            } else if (ui.home) {
                if (down && w >= 0)
                    home_key(w);
            } else if (ui.picker) {
                if (down && w >= 0)
                    picker_key(w);
            } else if (dp()->key) {
                if (down && w >= 0 && (btn >> B_FX & 1u) && ui.door != D_GAMES && ui.door != D_LAB) {
                    if (w < EN_NTRICK) {          /* FX held + a white key: a trick while it is held */
                        ui.fx_trick = (uint8_t)(w + 1);
                        en_trick(w + 1);
                        ui.btn_used |= 1u << B_FX;
                        ui.keys_used |= 1u << i;
                    }
                } else if (!down && (ui.keys_used >> i & 1u)) {
                    ui.keys_used &= ~(1u << i);
                    if (ui.fx_trick == w + 1) {
                        ui.fx_trick = 0;
                        en_trick(0);
                    }
                } else {
                    dp()->key((int)i, down);
                }
            }
            if (btn)
                ui.btn_used |= btn & ~(1u << B_FX | 1u << B_PLAY);
        }
    if (btn || keys || ch)
        ui.act_t = now;
    midi_in();
    for (i = 0; i < NE; i++) {
        if ((e = plat_enc((int)i)) == 0)
            continue;
        wake();
        ui.act_t = ui.turn_t = now;
        ui.btn_used |= btn;
        ui.help = 0;
        if (ui.gu)
            gu_enc((int)i, e);
        else if (ui.home) {
            if (i == EN_ALGO || i == EN_SELECT)
                home_move(e);
        } else if (ui.picker) {                   /* the picker: keep here, or open */
            if (i == EN_ALGO || i == EN_PRESET || i == EN_SELECT)
                ui.picker = ui.picker == PK_KEEP ? PK_OPEN : PK_KEEP;
        } else if (dp()->enc)
            dp()->enc((int)i, e);
    }
}

/* ------------------------------------------------------------- the lights --- */
static void leds(void)
{
    uint32_t b = 0, k = 0, db = 0, dk = 0, now = plat_ms();
    int i;
    if (ui.gu) {
        b = 1u << B_GLO | 1u << B_SAVE;
    } else if (ui.home) {
        for (i = 0; i < ND; i++)
            if (glob.doors >> i & 1u)
                *(i == ui.home_sel ? &k : &dk) |= 1u << WHITE_KEY[i];
        b = 1u << B_HOME | 1u << B_PLAY;
    } else if (ui.picker) {
        for (i = 0; i < NSLOT; i++)
            *(slot_c_used[i] ? &k : &dk) |= 1u << WHITE_KEY[i];
        b = 1u << B_SAVE;
    } else if (dp()->leds) {
        dp()->leds(&b, &k, &db, &dk);
    }
    if (en_playing && ui.door != D_GAMES)
        b |= 1u << B_PLAY;
    if (glob.lights == 0 && now - ui.chase_t < 900u) {   /* a rainbow across the keys (a show moment) */
        int x = (int)((now - ui.chase_t) / 50u);
        if (x < 18) {
            k |= 1u << WHITE_KEY[x < 16 ? x : 15];
            if (x > 0 && x - 1 < 16)
                dk |= 1u << WHITE_KEY[x - 1];
        }
    }
    if (glob.lights == 2) {                       /* off: PLAY alone */
        b &= 1u << B_PLAY;
        k = db = dk = 0;
    }
    plat_glow(glob.lights == 0);
    plat_play_red(en_recording);
    plat_leds_dim(db, dk);
    plat_leds(b, k);
}

/* ---------------------------------------------------------------- sleep --- */
static void sleep_tick(void)
{
    uint32_t now = plat_ms();
    if (glob.sleep && !ui.asleep && now - ui.sleep_t0 > 60000u * SLEEP_MIN[glob.sleep] && now - ui.act_t > 30000u) {
        ui.asleep = 1;                            /* bedtime: the music goes, the screen dims */
        en_play(0);
        live_keys_off();
    }
    if (glob.idle && !ui.dimmed && now - ui.act_t > 120000u)
        ui.dimmed = 1;
    if (glob.idle && en_playing && now - ui.act_t > 600000u) {
        en_play(0);
        ui.asleep = 1;
    }
    plat_backlight(ui.asleep ? 1 : ui.dimmed ? (glob.bright > 2 ? 2 : 1) : glob.bright);
}

/* AUTOSAVE: when stopped and quiet a few seconds after a change; in Make (where work is made) also while it
 * plays, twenty seconds after the last change and at most once a minute, at a bar's start (a flash erase
 * silences the audio for a moment); SAVE and a change of door save too */
static void autosave(void)
{
    static uint32_t last;
    uint32_t now = plat_ms();
    if (!ui_dirty() || ui.btn || ui.keys)
        return;
    if (!en_playing && now - ui.act_t >= 4000u && en_level[L_TUNE] <= 0.01f) {
        project_save();
        last = now;
    } else if (en_playing && ui.door == D_MAKE && now - ui.act_t >= 20000u && now - last >= 60000u && en_step == 0) {
        project_save();
        last = now;
    }
}

/* ---------------------------------------------------------------- frames --- */
static void draw_overlay(void)                    /* (on the top band) */
{
    uint32_t now = plat_ms();
    float u = (float)(now - ui.ov_t) / (float)(ui.ov_until - ui.ov_t + 1u);
    if (now >= ui.ov_until)
        return;
    switch (ui.ov) {
    case OV_TEMPO: {
        float v = (float)(en_song.tempo - 60) / 120.0f;
        cv_round(20, 8, 200, 30, 12, RGB(36, 34, 86));
        ellipse(36, 23, 10, 6, RGB(90, 200, 110));             /* the turtle ... */
        disc(46, 22, 3, RGB(90, 200, 110));
        ellipse(203, 24, 9, 6, CREAM);                         /* ... the rabbit */
        ellipse(198, 15, 2, 6, CREAM);
        ellipse(204, 15, 2, 6, CREAM);
        cv_round(58, 20, 124, 6, 3, RGB(70, 68, 140));
        disc(60.0f + v * 120.0f, 23, 7, RGB(255, 220, 90));
        if (ui.door != D_PLAY) {
            char b[8];
            itoa_u(en_song.tempo, b);
            text_c(120, 40, &FONT_S, b, CREAM);
        }
        break;
    }
    case OV_DICE: {
        float r = 18.0f + 6.0f * fm_sin_turns(u * 2.0f);
        dice(120.0f + 40.0f * (1.0f - u) * fm_sin_turns(u * 3.0f), 40.0f, r, (int)(u * 12.0f) % 6);
        break;
    }
    case OV_KEPT: {
        int i;
        for (i = 0; i < 6; i++)
            star(120.0f + fm_sin_turns((float)i / 6.0f + u * 0.3f) * 60.0f * u, 60.0f - fm_sin_turns((float)i / 6.0f + 0.25f) * 50.0f * u,
                 8.0f, u, RGB(255, 220, 90));
        heart(120, 60, 16, RGB(255, 90, 140));
        break;
    }
    case OV_UNDO: icon(IC_UNDO, 120, 40, 18, CREAM); break;
    case OV_CLEARED: ring(120, 40, 18.0f + u * 30.0f, 3.0f, mix565(CREAM, NAVY, (int)(u * 256.0f))); break;
    case OV_SAY:
        cv_round(14, 6, 212, 46, 12, RGB(36, 34, 86));
        text_c(120, 10, &FONT_B, ui.msg[0], CREAM);
        text_c(120, 32, &FONT_XS, ui.msg[1], RGB(200, 200, 240));
        break;
    default:
        break;
    }
}

static void edit_ring(void)                       /* EDIT held: a ring fills toward the clear */
{
    uint32_t now = plat_ms(), held;
    if (!(ui.btn >> B_EDIT & 1u) || (ui.btn_used >> B_EDIT & 1u) || ui.gu)
        return;
    held = now - ui.down_t[B_EDIT];
    if (held < 300u)
        return;
    cv_round(80, 20, 80, 80, 20, RGB(36, 34, 86));
    arc(120, 60, 30, 7.0f, (float)(held - 300u) / 1700.0f, RGB(70, 68, 140), RGB(255, 110, 90));
    icon(IC_UNDO, 120, 60, 12, CREAM);
}

void ui_init(void)
{
    memset(&ui, 0, sizeof ui);
    ui.sleep_t0 = ui.act_t = plat_ms();
    ui.chase_t = plat_ms();
    notes_clear();
    lcd_fill(0, 0, 240, 240, NAVY);
    door_go(glob.door);
    if (glob.startup && ui.door != D_GAMES) {     /* switching on starts the music (Blipblox's way) */
        en_play(1);
        en_preview(L_TUNE, en_follow(L_TUNE, 72));
    }
    en_fade_in();
}

void (*ui_mark)(int part);

void ui_frame(void)
{
    int b = (int)(ui.frame & 1u);
    UI_MARK_(0);
    input();
    autosave();
    sleep_tick();
    UI_MARK_(1);
    {   /* what moved: a layer's note, a pad's hit (counted by the engine; the times kept here) */
        uint32_t now = plat_ms();
        int i;
        for (i = 0; i < EN_NLAYER; i++)
            if (en_lay_on[i] != ui.seen_lay[i]) {
                ui.seen_lay[i] = en_lay_on[i];
                ui.lay_t[i] = now;
                if (i == L_TUNE && ui.door <= D_MAKE)
                    notes_spawn(120.0f, 80.0f, en_lay_note[i]);
            }
        for (i = 0; i < EN_NPAD; i++)
            if (en_pad_on[i] != ui.seen_pad[i]) {
                ui.seen_pad[i] = en_pad_on[i];
                ui.pad_t[i] = now;
                if (i == PAD_KICK)
                    ui.kick_t = now;
            }
    }
    if (b == 0) {
        band(0, BAND_H0, NAVY);
        if (ui.gu)
            gu_draw(0);
        else if (ui.home)
            home_draw(0);
        else if (ui.picker)
            picker_draw();
        else {
            dp()->draw(0);
            draw_overlay();
            edit_ring();
            if (ui.help)
                card_draw();
        }
        UI_MARK_(2);
        if (ui.asleep)
            cv_rect(0, 0, 240, BAND_H0, RGB(4, 4, 12));
        cv_blit(0, 0);
    } else {
        band(BAND_H0, BAND_H1, NAVY);
        if (ui.gu)
            gu_draw(1);
        else if (ui.home)
            home_draw(1);
        else
            dp()->draw(1);
        UI_MARK_(3);
        if (ui.asleep)
            cv_rect(0, BAND_H0, 240, BAND_H1, RGB(4, 4, 12));
        cv_blit(0, BAND_H0);
    }
    leds();
    UI_MARK_(4);
    ui.frame++;
}

void ui_input_only(void) { input(); }
