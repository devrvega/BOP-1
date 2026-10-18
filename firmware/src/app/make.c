/* SPDX-License-Identifier: GPL-3.0-only */
/* The MAKE door (ages 6-8): beats, loops, songs. Four layers, like four friends in a band: BEATS, BASS, TUNE,
 * SPARKLE; four one-bar parts, A B C D, played in a chain; each part over its chord (the progression's).
 *
 *   ALGORITHM      the layer           PRESETS   its friend (BEATS: the drum)       SELECT   the tempo
 *   K1-K4          shape, bright, wobble, echo (BEATS: the drum's tune and length, the swing, the echo)
 *   BEATS          a black key picks the drum (and plays it), the white keys are its 16 steps; ENV + a step:
 *                  a loud one
 *   BASS TUNE SPARKLE   the keys play (in the scale); REC records them as you play, in time; SEQ turns the
 *                  white keys into the 16 steps: a tap puts the last note played there (or takes it away);
 *                  hold a step and turn K1 for its note, K2 for its length
 *   ARP held       1-4 pick the part to work on; press several in a row: a song (the chain)
 *   GLO held       1-4 pick the chords: happy, sad, brave, sleepy (GLO tapped: this part / the whole song)
 *   FX             help me: a new idea for this layer      SEL   this layer quiet / back
 *   EDIT           oops; held: this layer cleared in this part
 *
 * Part of ui.c. */

static struct {
    uint8_t layer, part, pad, steps, song;        /* song: the chain plays (else the part alone) */
    uint8_t hand[EN_NLAYER];                      /* the last note played, as written */
    uint16_t held;                                /* the steps held */
    uint8_t seq[EN_CHAIN_MAX], seq_n;             /* a chain being pressed in, ARP held */
    uint32_t seed;
} mk = {L_TUNE, 0, 0, 0, 0, {0, 48, 72, 72}, 0, {0}, 0, 77};

static int ui_part_shown(void) { return ui.door == D_MAKE ? mk.part : en_part_playing; }

static int root_of_part(int p)
{
    int r, m;
    en_prog_chord(en_song.prog, p, &r, &m);
    return r <= 6 ? r : r - 12;
}
static int key_of_song(void) { return en_song.key <= 6 ? en_song.key : en_song.key - 12; }
static int written_of(int layer, int heard)       /* a note as heard, written as the step keeps it */
{
    int n = heard - key_of_song() - 12 * en_song.lay[layer].oct;
    if (layer == L_BASS || layer == L_SPARK)
        n -= root_of_part(mk.part);
    return n < 1 ? 1 : n > 127 ? 127 : n;
}
static en_part_t *mk_p(void) { return &en_song.part[mk.part]; }

static void mk_listen(void)
{
    if (mk.song)
        en_chain_play();
    else
        en_part_now(mk.part);
}

static void make_enter(void)
{
    mk.held = 0;
    mk.seq_n = 0;
    mk_listen();
}

static int step_of_white(int k) { return white_of(k); }

static void make_key(int k, int down)
{
    int w = white_of(k), bk = black_of(k);
    en_part_t *P = mk_p();
    if ((ui.btn >> B_ARP & 1u) && w >= 0) {       /* the parts: one picks, several make a song */
        ui.btn_used |= 1u << B_ARP;
        if (down && w < EN_NPART) {
            if (mk.seq_n < EN_CHAIN_MAX)
                mk.seq[mk.seq_n++] = (uint8_t)w;
            mk.part = (uint8_t)w;
            en_pad(PAD_POP, 0);
        }
        return;
    }
    if ((ui.btn >> B_GLO & 1u) && w >= 0) {       /* the chords */
        ui.btn_used |= 1u << B_GLO;
        if (down && w < EN_NPROG) {
            undo_save();
            en_song.prog = (uint8_t)w;
            mark_work();
        }
        return;
    }
    if (mk.layer == L_BEATS) {
        if (bk >= 0) {
            if (down) {
                mk.pad = (uint8_t)bk;
                en_pad(bk, 0);
            }
            return;
        }
        if (!down)
            return;
        if (ui.btn >> B_ENV & 1u) {               /* ENV + a step: a loud one */
            P->acc ^= (uint16_t)(1u << w);
            ui.btn_used |= 1u << B_ENV;
        } else {
            P->hit[mk.pad] ^= (uint16_t)(1u << w);
            if (P->hit[mk.pad] >> w & 1u)
                en_pad(mk.pad, 0);
        }
        mark_work();
        return;
    }
    if (mk.steps && w >= 0) {                     /* the steps */
        en_note_t *e = &P->syn[mk.layer - 1][w];
        if (down) {
            mk.held |= (uint16_t)(1u << w);
            ui.keys_used &= ~(1u << k);
        } else {
            mk.held &= (uint16_t)~(1u << w);
            if (ui.keys_used >> k & 1u) {         /* (a knob turned while it was held: no toggle) */
                ui.keys_used &= ~(1u << k);
                return;
            }
            if (e->note) {
                e->note = 0;
            } else {
                e->note = mk.hand[mk.layer];
                e->len = (uint8_t)(mk.layer == L_BASS ? 2 : 1);
                e->fl = 0;
                en_preview(mk.layer, en_follow(mk.layer, e->note));
            }
            mark_work();
        }
        return;
    }
    if (bk >= 0 && mk.layer != L_BEATS && !mk.steps) {   /* black keys: the drums, to play along */
        if (down)
            en_pad(bk, 0);
        return;
    }
    if (w < 0)
        return;
    if (down) {
        int n = lay_note_of_white(mk.layer, w);
        ui.key_note[k] = (uint8_t)n;
        ui.key_lay[k] = mk.layer;
        en_note(mk.layer, n, 1);
        mk.hand[mk.layer] = (uint8_t)written_of(mk.layer, n);
        if (en_recording)
            mark_work();
    } else if (ui.key_note[k]) {
        en_note(ui.key_lay[k], ui.key_note[k], 0);
        ui.key_note[k] = 0;
    }
}

static void make_button(int b)
{
    en_layer_t *L = &en_song.lay[mk.layer];
    switch (b) {
    case B_FX:
        undo_save();
        mk.seed = hash32(mk.seed + 1u);
        en_generate_layer(&en_song, mk.layer, mk.seed);
        mark_work();
        overlay(OV_DICE, 0, 700u);
        break;
    case B_REC:
        if (en_recording) {
            en_rec(REC_OFF, 0);
        } else {
            undo_save();
            en_rec(REC_LAYER, mk.layer);
            if (!en_playing)
                en_play(1);
        }
        break;
    case B_SEQ:
        if (mk.layer != L_BEATS)
            mk.steps ^= 1u;
        live_keys_off();
        break;
    case B_SEL:
        L->mute ^= 1u;
        mark_work();
        break;
    case B_ENV:
        if (mk.layer != L_BEATS) {
            static const uint8_t E[3][4] = {{0, 25, 0, 15}, {3, 45, 65, 30}, {45, 70, 80, 65}};
            int now = L->p[PK_A] >= 30 ? 2 : L->p[PK_S] < 20 ? 0 : 1, nx = (now + 1) % 3;
            L->p[PK_A] = E[nx][0];
            L->p[PK_D] = E[nx][1];
            L->p[PK_S] = E[nx][2];
            L->p[PK_R] = E[nx][3];
            en_preview(mk.layer, en_follow(mk.layer, mk.hand[mk.layer]));
            mark_work();
        }
        break;
    case B_LFO:
        if (mk.layer != L_BEATS) {
            L->p[PK_LDEPTH] = (uint8_t)(L->p[PK_LDEPTH] ? 0 : 40);
            L->p[PK_LRATE] = (uint8_t)(30 + L->p[PK_LDEPTH] / 2);
            mark_work();
        }
        break;
    case B_OCTDN:
    case B_OCTUP:
        if (mk.layer != L_BEATS) {
            L->oct = (int8_t)bump(L->oct, b == B_OCTUP ? 1 : -1, -2, 2);
            en_preview(mk.layer, en_follow(mk.layer, mk.hand[mk.layer]));
            mark_work();
        }
        break;
    case B_ARP:                                   /* (tapped: the parts, as a card would show them) */
        break;
    case B_GLO:
        mk.song ^= 1u;
        mk_listen();
        say(mk.song ? "The whole song" : "This part", mk.song ? "A chain of parts" : "Round and round");
        break;
    default: break;
    }
}

static void make_enc(int role, int d)
{
    en_layer_t *L = &en_song.lay[mk.layer];
    int a = (int)accel(role, d, 100);
    if (mk.held && role == EN_K1 && mk.layer != L_BEATS) {   /* a held step's note, along the scale */
        int s, i, n = en_scale_len(en_song.scale);
        for (s = 0; s < 16; s++)
            if (mk.held >> s & 1u) {
                en_note_t *e = &mk_p()->syn[mk.layer - 1][s];
                int pc = 0, best = 0, nn = e->note ? e->note : mk.hand[mk.layer];
                for (i = 0; i < 64; i++) {        /* the nearest scale note, then the next one along */
                    int sn = en_scale_note(en_song.scale, 0, i) - 48 + 12;
                    if (sn >= nn) {
                        best = i;
                        break;
                    }
                }
                best += d > 0 ? 1 : -1;
                if (best < 0)
                    best = 0;
                pc = en_scale_note(en_song.scale, 0, best) - 48 + 12;
                e->note = (uint8_t)(pc < 1 ? 1 : pc > 120 ? 120 : pc);
                if (!e->len)
                    e->len = 1;
                mk.hand[mk.layer] = e->note;
                en_preview(mk.layer, en_follow(mk.layer, e->note));
                ui.keys_used |= 1u << WHITE_KEY[s];
                (void)n;
            }
        mark_work();
        return;
    }
    if (mk.held && role == EN_K2 && mk.layer != L_BEATS) {   /* ... its length */
        int s;
        for (s = 0; s < 16; s++)
            if (mk.held >> s & 1u) {
                en_note_t *e = &mk_p()->syn[mk.layer - 1][s];
                e->len = (uint8_t)bump(e->len, d > 0 ? 1 : -1, 1, 16);
                ui.keys_used |= 1u << WHITE_KEY[s];
            }
        mark_work();
        return;
    }
    switch (role) {
    case EN_ALGO:
        live_keys_off();
        en_rec(REC_OFF, 0);
        mk.layer = (uint8_t)((mk.layer + (d > 0 ? 1 : EN_NLAYER - 1)) % EN_NLAYER);
        mk.held = 0;
        return;
    case EN_PRESET:
        if (mk.layer == L_BEATS) {
            mk.pad = (uint8_t)((mk.pad + (d > 0 ? 1 : EN_NPAD - 1)) % EN_NPAD);
            en_pad(mk.pad, 0);
        } else {
            int n = friends_n(), i = (friend_index(L->who) + (d > 0 ? 1 : n - 1)) % n;
            put_friend(mk.layer, friend_at(i));
            en_preview(mk.layer, en_follow(mk.layer, mk.hand[mk.layer]));
        }
        return;
    case EN_SELECT:
        en_song.tempo = (uint8_t)bump(en_song.tempo, (int)accel(role, d, 120), 50, 200);
        overlay(OV_TEMPO, 0, 1200u);
        mark_work();
        return;
    default: break;
    }
    if (mk.layer == L_BEATS) {
        switch (role) {
        case EN_K1: en_song.dk[mk.pad][0] = (uint8_t)bump(en_song.dk[mk.pad][0], a, 0, 100); break;
        case EN_K2: en_song.dk[mk.pad][1] = (uint8_t)bump(en_song.dk[mk.pad][1], a, 0, 100); break;
        case EN_K3: en_song.swing = (uint8_t)bump(en_song.swing, a, 0, 100); break;
        case EN_K4:
            L->p[PK_REVERB] = (uint8_t)bump(L->p[PK_REVERB], a, 0, 100);
            L->p[PK_ECHO] = (uint8_t)(L->p[PK_REVERB] / 2);
            break;
        default: break;
        }
        if (role == EN_K1 || role == EN_K2)
            en_pad(mk.pad, 0);
    } else {
        switch (role) {
        case EN_K1: {
            int w2 = L->p[PK_WAVE2] - L->p[PK_WAVE1];
            L->p[PK_WAVE1] = (uint8_t)bump(L->p[PK_WAVE1], a, 0, 100);
            L->p[PK_WAVE2] = (uint8_t)bump(L->p[PK_WAVE1], w2, 0, 100);
            break;
        }
        case EN_K2: L->p[PK_CUT] = (uint8_t)bump(L->p[PK_CUT], a, 8, 96); break;
        case EN_K3:
            L->p[PK_LDEPTH] = (uint8_t)bump(L->p[PK_LDEPTH], a, 0, 100);
            L->p[PK_LRATE] = (uint8_t)(30 + L->p[PK_LDEPTH] / 2);
            break;
        case EN_K4:
            L->p[PK_ECHO] = (uint8_t)bump(L->p[PK_ECHO], a, 0, 100);
            L->p[PK_REVERB] = (uint8_t)(L->p[PK_ECHO] * 4 / 5);
            break;
        default: break;
        }
    }
    mark_work();
}

/* ARP let go: the part, or the song pressed in */
static void make_tick(void)
{
    if (!(ui.btn >> B_ARP & 1u) && mk.seq_n) {
        if (mk.seq_n > 1) {
            undo_save();
            memcpy(en_song.chain, mk.seq, mk.seq_n);
            en_song.chain_n = mk.seq_n;
            mk.song = 1;
            mark_work();
            say("A song!", "");
        } else {
            mk.song = 0;
        }
        mk.seq_n = 0;
        mk_listen();
    }
}

static void make_clear(void)
{
    en_part_t *P = mk_p();
    int s, k;
    en_rec(REC_OFF, 0);
    if (mk.layer == L_BEATS) {
        for (k = 0; k < EN_NPAD; k++)
            P->hit[k] = 0;
        P->acc = 0;
    } else {
        for (s = 0; s < EN_NSTEP; s++)
            P->syn[mk.layer - 1][s].note = 0;
    }
    mark_work();
}

/* ----------------------------------------------------------- the screen --- */
static const char *const PART_L[4] = {"A", "B", "C", "D"};
static void prog_face(int prog, float x, float y, float r)   /* happy, sad, brave, sleepy */
{
    static const uint16_t C[4] = {RGB(255, 210, 70), RGB(110, 150, 255), RGB(255, 110, 80), RGB(170, 140, 230)};
    disc(x, y, r, C[prog & 3]);
    if (prog == 3) {
        thick(x - r * 0.55f, y - r * 0.15f, x - r * 0.2f, y - r * 0.15f, 2.0f, INK);
        thick(x + r * 0.2f, y - r * 0.15f, x + r * 0.55f, y - r * 0.15f, 2.0f, INK);
    } else {
        disc(x - r * 0.35f, y - r * 0.2f, r * 0.13f, INK);
        disc(x + r * 0.35f, y - r * 0.2f, r * 0.13f, INK);
    }
    if (prog == 2) {
        thick(x - r * 0.6f, y - r * 0.55f, x - r * 0.15f, y - r * 0.4f, 2.0f, INK);
        thick(x + r * 0.6f, y - r * 0.55f, x + r * 0.15f, y - r * 0.4f, 2.0f, INK);
    }
    if (prog == 1) {
        thick(x - r * 0.35f, y + r * 0.45f, x, y + r * 0.3f, 2.0f, INK);
        thick(x, y + r * 0.3f, x + r * 0.35f, y + r * 0.45f, 2.0f, INK);
    } else {
        thick(x - r * 0.4f, y + r * 0.3f, x, y + r * 0.48f, 2.0f, INK);
        thick(x, y + r * 0.48f, x + r * 0.4f, y + r * 0.3f, 2.0f, INK);
    }
}

static void layer_tab_icon(int l, float x, float y, int sel)
{
    uint16_t c = sel ? CREAM : RGB(140, 140, 200);
    if (l == L_BEATS) {
        pad_icon(PAD_KICK, x, y, 8.0f, c);
    } else {
        fr_t f;
        const en_layer_t *L = &en_song.lay[l];
        memset(&f, 0, sizeof f);
        f.x = x;
        f.y = y + 9.0f;
        f.r = 8.0f;
        f.wave = (float)L->p[PK_WAVE1] * 0.04f;
        f.mouth = 0.4f;
        f.col = who_colour(L->who, L->colour);
        f.bg = sel ? RGB(70, 66, 150) : RGB(40, 38, 90);
        friend_draw(&f);
    }
}

static void make_top(void)
{
    int l, p, s;
    const en_part_t *P = mk_p();
    uint32_t now = plat_ms();
    for (l = 0; l < EN_NLAYER; l++) {             /* the layers */
        int x = 4 + l * 36, sel = l == mk.layer;
        cv_round(x, 3, 33, 26, 7, sel ? RGB(70, 66, 150) : RGB(40, 38, 90));
        layer_tab_icon(l, (float)x + 16.5f, 15.0f, sel);
        if (en_song.lay[l].mute)
            thick((float)x + 4.0f, 26.0f, (float)x + 29.0f, 6.0f, 2.5f, RGB(255, 90, 90));
    }
    for (p = 0; p < EN_NPART; p++) {              /* the parts, each with its chord's face */
        int x = 150 + p * 22, sel = p == mk.part, on = en_playing && en_part_playing == p;
        int r, m;
        en_prog_chord(en_song.prog, p, &r, &m);
        cv_round(x, 3, 20, 26, 6, sel ? RGB(255, 210, 90) : RGB(40, 38, 90));
        cv_text(x + 5, 3, &FONT_S, PART_L[p], sel ? INK : CREAM);
        disc((float)x + 10.0f, 24.0f, 3.0f, m ? RGB(110, 150, 255) : RGB(255, 210, 70));
        if (on)
            disc((float)x + 17.0f, 5.0f, 2.5f, RGB(120, 255, 140));
    }
    {   /* the grid */
        const int GX = 22, GY = 34, CW = 13, GH = 104;
        for (s = 0; s < EN_NSTEP; s++) {          /* the beats of the bar, shaded every four */
            int x = GX + s * CW;
            if ((s / 4) & 1)
                cv_rect(x, GY, CW, GH, RGB(26, 30, 70));
            if (en_playing && en_step == s && (mk.song ? en_part_playing == mk.part : 1))
                cv_rect(x, GY, CW, GH, RGB(50, 54, 110));
        }
        if (mk.layer == L_BEATS) {
            int rh = GH / EN_NPAD, k;
            for (k = 0; k < EN_NPAD; k++) {
                int y = GY + k * rh, sel = k == mk.pad;
                float g = (float)(now - ui.pad_t[k]) * 0.001f;
                if (sel)
                    cv_rect(0, y, 240, rh, RGB(56, 50, 120));
                pad_icon(k, 10.0f, (float)y + (float)rh * 0.5f, 3.6f + (g < 0.2f ? 1.0f : 0.0f), PAD_COL[k]);
                for (s = 0; s < EN_NSTEP; s++)
                    if (P->hit[k] >> s & 1u)
                        cv_round(GX + s * CW + 1, y + 1, CW - 2, rh - 2, 2,
                                 (P->acc >> s & 1u) ? shade(PAD_COL[k], 380) : PAD_COL[k]);
            }
        } else {
            const en_note_t *row = P->syn[mk.layer - 1];
            int lo = 127, hi = 0, l2 = mk.layer;
            uint16_t c = who_colour(en_song.lay[l2].who, en_song.lay[l2].colour);
            for (s = 0; s < EN_NSTEP; s++)
                if (row[s].note) {
                    lo = row[s].note < lo ? row[s].note : lo;
                    hi = row[s].note > hi ? row[s].note : hi;
                }
            if (hi < lo) {
                lo = mk.hand[l2] - 6;
                hi = mk.hand[l2] + 6;
            }
            if (hi - lo < 12) {
                int mid = (hi + lo) / 2;
                lo = mid - 6;
                hi = mid + 6;
            }
            for (s = 0; s < EN_NSTEP; s++) {
                const en_note_t *e = &row[s];
                int y, len;
                if (!e->note)
                    continue;
                y = GY + GH - 8 - (e->note - lo) * (GH - 12) / (hi - lo);
                len = e->len > 16 - s ? 16 - s : e->len;
                cv_round(GX + s * CW + 1, y, len * CW - 2, 8, 3, mix565(c, NAVY, 90));
                cv_round(GX + s * CW + 1, y, CW - 2, 8, 3, (e->fl & EN_N_CHORD) ? CREAM : c);
                if (e->fl & EN_N_ACC)
                    disc((float)(GX + s * CW) + 6.5f, (float)y + 4.0f, 2.0f, INK);
            }
            if (mk.steps)                         /* the steps' keys, below */
                for (s = 0; s < EN_NSTEP; s++)
                    cv_round(GX + s * CW + 2, GY + GH - 3, CW - 4, 3, 1, (mk.held >> s & 1u) ? CREAM : RGB(80, 80, 150));
        }
    }
    if (ui.btn >> B_ARP & 1u) {                   /* the parts: press them in */
        cv_round(16, 40, 208, 94, 12, RGB(36, 34, 86));
        for (p = 0; p < EN_NPART; p++) {
            int x = 26 + p * 50;
            cv_round(x, 50, 42, 42, 8, p == mk.part ? RGB(255, 210, 90) : RGB(60, 58, 130));
            text_c(x + 21, 58, &FONT_M, PART_L[p], p == mk.part ? INK : CREAM);
        }
        {
            char b[EN_CHAIN_MAX * 2 + 1];
            int i, n = mk.seq_n ? mk.seq_n : en_song.chain_n;
            const uint8_t *q = mk.seq_n ? mk.seq : en_song.chain;
            for (i = 0; i < n; i++) {
                b[2 * i] = (char)('A' + q[i]);
                b[2 * i + 1] = ' ';
            }
            b[n ? 2 * n - 1 : 0] = 0;
            text_c(120, 104, &FONT_B, b, mk.seq_n > 1 ? RGB(255, 210, 90) : CREAM);
        }
    } else if (ui.btn >> B_GLO & 1u) {            /* the chords */
        cv_round(16, 40, 208, 94, 12, RGB(36, 34, 86));
        for (p = 0; p < EN_NPROG; p++) {
            int x = 26 + p * 50;
            cv_round(x, 48, 42, 70, 8, p == en_song.prog ? RGB(90, 86, 180) : RGB(52, 50, 110));
            prog_face(p, (float)x + 21.0f, 70.0f, 15.0f);
            text_c(x + 21, 94, &FONT_XS, EN_PROG_NAME[p], CREAM);
        }
    }
}

static void make_bottom(void)
{
    static const char *const KL[2][4] = {{"shape", "bright", "wobble", "echo"}, {"tune", "length", "swing", "echo"}};
    const en_layer_t *L = &en_song.lay[mk.layer];
    int v[4], k;
    char line[40];
    if (mk.layer == L_BEATS) {
        v[0] = en_song.dk[mk.pad][0];
        v[1] = en_song.dk[mk.pad][1];
        v[2] = en_song.swing;
        v[3] = L->p[PK_REVERB];
        scpy(line, EN_PAD_NAME[mk.pad], sizeof line);
    } else {
        v[0] = L->p[PK_WAVE1];
        v[1] = L->p[PK_CUT];
        v[2] = L->p[PK_LDEPTH];
        v[3] = L->p[PK_ECHO];
        scpy(line, EN_LAYER_NAME[mk.layer], sizeof line);
    }
    cv_text(8, BAND_H0 + 2, &FONT_B, line, CREAM);
    {
        const char *hint = mk.layer == L_BEATS ? "white keys: steps" : mk.steps ? "tap a step" : "play the keys";
        if (en_recording)
            hint = "recording!";
        cv_text(232 - text_w(&FONT_XS, hint), BAND_H0 + 6, &FONT_XS, hint, en_recording ? RGB(255, 110, 110) : RGB(160, 160, 220));
    }
    if (mk.layer == L_BEATS) {
        static const uint16_t KC[4] = {RGB(255, 140, 90), RGB(255, 220, 80), RGB(120, 220, 140), RGB(120, 170, 255)};
        for (k = 0; k < 4; k++) {
            int cx = 30 + k * 60, y = BAND_H0 + 52;
            arc(cx, y, 22, 5.0f, (float)v[k] * 0.01f, RGB(52, 50, 110), KC[k]);
            if (k < 2)
                pad_icon(mk.pad, (float)cx, (float)y, 7.0f, PAD_COL[mk.pad]);
            else if (k == 2)
                knob_icon(2, (float)cx, (float)y, 9.0f, 0.5f, KC[k]);
            else
                knob_icon(3, (float)cx, (float)y, 9.0f, (float)v[k] * 0.01f, KC[k]);
        }
    } else {
        static const uint16_t KC[4] = {RGB(255, 140, 90), RGB(255, 220, 80), RGB(120, 220, 140), RGB(120, 170, 255)};
        for (k = 0; k < 4; k++) {
            int cx = 30 + k * 60, y = BAND_H0 + 52;
            arc(cx, y, 22, 5.0f, (float)v[k] * 0.01f, RGB(52, 50, 110), KC[k]);
            knob_icon(k, (float)cx, (float)y, 9.0f, (float)v[k] * 0.01f, KC[k]);
        }
    }
    for (k = 0; k < 4; k++)
        text_c(30 + k * 60, BAND_H0 + 76, &FONT_XS, KL[mk.layer == L_BEATS][k], RGB(170, 170, 225));
}

static void make_draw(int b)
{
    make_tick();
    if (b == 0)
        make_top();
    else
        make_bottom();
}

static void make_leds(uint32_t *b, uint32_t *k, uint32_t *db, uint32_t *dk)
{
    const en_part_t *P = mk_p();
    int s, here = !mk.song || en_part_playing == mk.part;
    if (ui.btn >> B_ARP & 1u) {
        for (s = 0; s < EN_NPART; s++)
            *(s == mk.part ? k : dk) |= 1u << WHITE_KEY[s];
        *b |= 1u << B_ARP;
        return;
    }
    if (ui.btn >> B_GLO & 1u) {
        for (s = 0; s < EN_NPROG; s++)
            *(s == en_song.prog ? k : dk) |= 1u << WHITE_KEY[s];
        *b |= 1u << B_GLO;
        return;
    }
    if (mk.layer == L_BEATS || mk.steps) {
        for (s = 0; s < EN_NSTEP; s++) {
            int on = mk.layer == L_BEATS ? (P->hit[mk.pad] >> s & 1u) : P->syn[mk.layer - 1][s].note != 0;
            if (on)
                *k |= 1u << WHITE_KEY[s];
        }
        if (en_playing && here) {
            uint32_t m = 1u << WHITE_KEY[en_step];
            if (*k & m)
                *k &= ~m;                         /* the playhead goes through them */
            else
                *dk |= m;
        }
        if (mk.layer == L_BEATS)
            *k |= 1u << BLACK_KEY[mk.pad];
    }
    for (s = 0; s < NKEYS; s++)
        if (ui.key_note[s])
            *k |= 1u << s;
    if (mk.steps)
        *b |= 1u << B_SEQ;
    if (en_recording)
        *b |= 1u << B_REC;
    if (en_song.lay[mk.layer].mute)
        *b |= 1u << B_SEL;
    if (mk.song)
        *db |= 1u << B_GLO;
    if (mk.layer != L_BEATS && en_song.lay[mk.layer].p[PK_LDEPTH])
        *b |= 1u << B_LFO;
    if (en_song.lay[mk.layer].oct > 0)
        *b |= 1u << B_OCTUP;
    if (en_song.lay[mk.layer].oct < 0)
        *b |= 1u << B_OCTDN;
}

static const card_t MAKE_CARD[NB] = {
    [B_FX] = {IC_DICE, "Help me", "A new idea for this layer.", 0},
    [B_SEL] = {IC_MUTE, "Quiet", "This layer quiet, or back.", 0},
    [B_ENV] = {IC_HOP, "Short or long", "Long or short notes. + a step: loud.", 0},
    [B_LFO] = {IC_WOBBLE, "Wobble", "The wobble on or off.", 0},
    [B_EDIT] = {IC_UNDO, "Oops!", "Undo. Hold: clear this layer here.", 0},
    [B_GLO] = {IC_CHORDS, "Chords", "Hold + 1-4: happy sad brave sleepy.", 0},
    [B_HOME] = {IC_DOORS, "Doors", "Play, Make, Lab, Games.", 0},
    [B_SAVE] = {IC_SAVE, "Keep it", "Keep this song in one of eight.", 0},
    [B_ARP] = {IC_PARTS, "Parts", "Hold + 1-4: a part. Several: a song!", 0},
    [B_SEQ] = {IC_STEPS, "Steps", "The white keys become 16 steps.", 0},
    [B_PLAY] = {IC_PLAY, "Music", "Start or stop.", 0},
    [B_REC] = {IC_REC, "Record", "Play along: it goes in, in time.", 0},
    [B_OCTDN] = {IC_BIG, "Lower", "An octave down.", 0},
    [B_OCTUP] = {IC_SMALL, "Higher", "An octave up.", 0},
};
static const door_t DOOR_MAKE = {make_enter, make_key, make_button, make_enc, make_draw, make_leds, make_clear, MAKE_CARD};
