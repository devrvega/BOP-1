/* SPDX-License-Identifier: GPL-3.0-only */
/* The grown-ups' page: hold GLO + SAVE for three seconds (hard to hit by chance). SELECT or ALGORITHM moves,
 * K1 (or PRESETS) changes; the resets want SAVE held for three seconds, each on its own; HOME closes (and
 * keeps it). Part of ui.c. */

enum { GU_DOORS, GU_VOLCAP, GU_LIGHTS, GU_SCREEN, GU_SLEEP, GU_IDLE, GU_START, GU_SAFE, GU_MIDIOUT, GU_MIDICH,
       GU_CLOCK, GU_TUNE, GU_RSONGS, GU_RFRIENDS, GU_RBOOK, GU_ABOUT, NGU };
static const char *const GU_NAME[NGU] = {"Doors shown", "Volume cap", "Lights", "Screen", "Sleep timer", "Idle",
                                         "At power-on", "Lab squeals", "Keys to USB MIDI", "MIDI channel",
                                         "MIDI clock", "Tune", "Reset: songs", "Reset: My Friends",
                                         "Reset: sticker book", "About"};
static uint8_t gu_sel, gu_top, gu_done;

static void gu_open(void)
{
    live_keys_off();
    ui.gu = 1;
    ui.home = 0;
    ui.picker = PK_NONE;
    ui.help = 0;
    gu_sel = gu_top = 0;
    gu_done = 0;
}
static int doors_level(void)
{
    return glob.doors == 1u ? 0 : glob.doors == (1u | 1u << D_GAMES) ? 1 : glob.doors == (1u | 1u << D_GAMES | 1u << D_MAKE) ? 2 : 3;
}
static void gu_value(int row, char *b)
{
    switch (row) {
    case GU_DOORS: {
        static const char *const T[4] = {"Play only", "+ Games", "+ Make", "all four"};
        scpy(b, T[doors_level()], 16);
        break;
    }
    case GU_VOLCAP: itoa_u((uint32_t)en_glob.v[G_VOLCAP], b); scpy(b + slen(b), " %", 3); break;
    case GU_LIGHTS: {
        static const char *const T[3] = {"full", "calm", "off"};
        scpy(b, T[glob.lights % 3u], 16);
        break;
    }
    case GU_SCREEN: itoa_u(glob.bright, b); scpy(b + slen(b), " of 4", 6); break;
    case GU_SLEEP:
        if (!glob.sleep)
            scpy(b, "off", 16);
        else {
            itoa_u(SLEEP_MIN[glob.sleep], b);
            scpy(b + slen(b), " min", 5);
        }
        break;
    case GU_IDLE: scpy(b, glob.idle ? "dim, then stop" : "always on", 16); break;
    case GU_START: scpy(b, glob.startup ? "music" : "quiet", 16); break;
    case GU_SAFE: scpy(b, en_glob.v[G_SAFE] ? "never" : "allowed", 16); break;
    case GU_MIDIOUT: scpy(b, en_glob.v[G_MIDIOUT] ? "on" : "off", 16); break;
    case GU_MIDICH: itoa_u((uint32_t)en_glob.v[G_MIDICH], b); break;
    case GU_CLOCK: scpy(b, en_glob.v[G_CLOCK] ? "lead (send)" : "follow", 16); break;
    case GU_TUNE: {
        int v = en_glob.v[G_TUNE];
        if (v < 0) {
            b[0] = '-';
            itoa_u((uint32_t)-v, b + 1);
        } else
            itoa_u((uint32_t)v, b);
        scpy(b + slen(b), " ct", 4);
        break;
    }
    case GU_RSONGS:
    case GU_RFRIENDS:
    case GU_RBOOK: scpy(b, "hold SAVE", 16); break;
    default: scpy(b, "v" EN_VERSION, 16); break;
    }
}
static void gu_change(int row, int d)
{
    switch (row) {
    case GU_DOORS: {
        static const uint8_t M[4] = {1u, 1u | 1u << D_GAMES, 1u | 1u << D_GAMES | 1u << D_MAKE, 15u};
        glob.doors = M[bump(doors_level(), d, 0, 3)];
        break;
    }
    case GU_VOLCAP: en_glob.v[G_VOLCAP] = (int16_t)bump(en_glob.v[G_VOLCAP], 5 * d, 10, 100); break;
    case GU_LIGHTS: glob.lights = (uint8_t)bump(glob.lights, d, 0, 2); break;
    case GU_SCREEN: glob.bright = (uint8_t)bump(glob.bright, d, 1, 4); break;
    case GU_SLEEP:
        glob.sleep = (uint8_t)bump(glob.sleep, d, 0, 4);
        ui.sleep_t0 = plat_ms();
        break;
    case GU_IDLE: glob.idle = (uint8_t)bump(glob.idle, d, 0, 1); break;
    case GU_START: glob.startup = (uint8_t)bump(glob.startup, d, 0, 1); break;
    case GU_SAFE: en_glob.v[G_SAFE] = (int16_t)bump(en_glob.v[G_SAFE], d, 0, 1); break;
    case GU_MIDIOUT: en_glob.v[G_MIDIOUT] = (int16_t)bump(en_glob.v[G_MIDIOUT], d, 0, 1); break;
    case GU_MIDICH: en_glob.v[G_MIDICH] = (int16_t)bump(en_glob.v[G_MIDICH], d, 1, 16); break;
    case GU_CLOCK: en_glob.v[G_CLOCK] = (int16_t)bump(en_glob.v[G_CLOCK], d, 0, 1); break;
    case GU_TUNE: en_glob.v[G_TUNE] = (int16_t)bump(en_glob.v[G_TUNE], d, -50, 50); break;
    default: return;
    }
    mark_glob();
}
static void gu_button(int b)
{
    if (b == B_HOME) {                            /* closed, kept */
        ui.gu = 0;
        glob.glob = en_glob;
        mark_glob();
        project_save();
        door_go(glob.doors >> ui.door & 1u ? ui.door : D_PLAY);
    }
}
static void gu_key(int k) { (void)k; }
static void gu_enc(int r, int d)
{
    if (r == EN_SELECT || r == EN_ALGO) {
        gu_sel = (uint8_t)bump(gu_sel, d > 0 ? 1 : -1, 0, NGU - 1);
        if (gu_sel < gu_top)
            gu_top = gu_sel;
        if (gu_sel >= gu_top + 10)
            gu_top = (uint8_t)(gu_sel - 9);
        gu_done = 0;
    } else if (r == EN_K1 || r == EN_PRESET) {
        gu_change(gu_sel, d > 0 ? 1 : -1);
    }
}
static void gu_reset(int row)
{
    int i;
    if (row == GU_RSONGS) {
        slot_clear_all();
        en_generate(&en_song, glob.style, glob.seed);
        mark_work();
    } else if (row == GU_RFRIENDS) {
        for (i = 0; i < NMYF; i++)
            myf.f[i].used = 0;
        for (i = L_BASS; i <= L_SPARK; i++)
            if (en_song.lay[i].who >= EN_NFRIEND)
                put_friend(i, 0);
        project_dirty(PD_FRIENDS);
    } else {
        memset(book.stars, 0, sizeof book.stars);
        memset(book.level, 0, sizeof book.level);
        book.lessons = 0;
        book.stickers = 0;
        project_dirty(PD_BOOK);
    }
    project_save();
    en_pad(PAD_POP, 0);
}
static void gu_draw(int b)
{
    int i, first = b == 0 ? 0 : 6, last = b == 0 ? 6 : 10;
    uint32_t now = plat_ms();
    if (b == 0) {
        cv_text(8, 2, &FONT_B, "For grown-ups", RGB(255, 210, 120));
        cv_text(232 - text_w(&FONT_XS, "HOME: done"), 6, &FONT_XS, "HOME: done", RGB(160, 160, 220));
    }
    for (i = first; i < last && gu_top + i < NGU; i++) {
        int row = gu_top + i, y = (b == 0 ? 24 : BAND_H0) + (i - first) * 20, sel = row == gu_sel;
        char v[24];
        if (sel)
            cv_round(2, y, 236, 19, 5, RGB(70, 66, 150));
        cv_text(8, y + 1, &FONT_XS, GU_NAME[row], CREAM);
        gu_value(row, v);
        cv_text(232 - text_w(&FONT_XS, v), y + 1, &FONT_XS, v, sel ? RGB(255, 220, 90) : RGB(170, 170, 230));
    }
    if (b == 1) {
        int y = BAND_H0 + 82;
        if (gu_sel >= GU_RSONGS && gu_sel <= GU_RBOOK && (ui.btn >> B_SAVE & 1u)) {   /* the reset: held */
            uint32_t held = now - ui.down_t[B_SAVE];
            cv_round(20, y - 2, 200, 12, 5, RGB(44, 42, 96));
            cv_round(20, y - 2, (int)(200.0f * fm_minf((float)held / 3000.0f, 1.0f)), 12, 5, RGB(255, 110, 90));
            if (held >= 3000u && !gu_done) {
                gu_done = 1;
                gu_reset(gu_sel);
            }
        } else if (gu_done) {
            text_c(120, y - 4, &FONT_XS, "done", RGB(170, 230, 170));
        } else if (gu_sel == GU_ABOUT) {
            char t[48];
            scpy(t, "battery ", sizeof t);
            itoa_u(plat_battery(), t + slen(t));
            scpy(t + slen(t), "  update: OCT- + OCT+ 5 s", (int)(sizeof t) - slen(t));
            text_c(120, y - 4, &FONT_XS, t, RGB(170, 170, 230));
        }
    }
}
