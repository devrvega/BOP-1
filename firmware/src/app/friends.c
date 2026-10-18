/* SPDX-License-Identifier: GPL-3.0-only */
/* BOP-1's pictures: the friends, whose bodies are the sound; the places the echo makes; the dance floor the
 * drums light; the notes that fly; the icons of the pads, the styles and the knobs. All drawn from shapes
 * (no bitmaps), so they morph as the knobs turn. Each control has one meaning, the same everywhere:
 *
 *   the waveform     the body's shape: sine a round blob, triangle pointy, saw spiky with a zig-zag back,
 *                    square a boxy robot, noise a fuzzy cloud (and every blend between)
 *   the pitch        the size: low is big, high is small; each note makes it bob
 *   the cutoff       the mouth: closed (dark, "mmm") .. wide open (bright, "aah")
 *   the envelope     it hops up as a note starts (the attack: how fast) and lands (the release: how slowly)
 *   the LFO          it wobbles at the LFO's rate: bobbing (pitch), its mouth flapping (filter), fading (volume)
 *   echo and reverb  the place: a bedroom, a cave, a canyon, outer space; rings go out from it
 *   the drums        the floor: a tile lights for each pad; the kick shakes the ground
 *
 * Part of ui.c. */

#define NAVY RGB(20, 24, 58)
#define INK RGB(16, 12, 34)
#define CREAM RGB(255, 246, 226)
#define PINK RGB(255, 120, 160)

/* the pads' colours (each also has its own icon: meaning never rests on colour alone) */
static const uint16_t PAD_COL[EN_NPAD] = {
    RGB(255, 84, 84), RGB(255, 170, 60), RGB(255, 230, 70), RGB(120, 230, 90), RGB(60, 200, 190), RGB(80, 150, 255),
    RGB(170, 110, 255), RGB(255, 100, 220), RGB(255, 255, 255), RGB(140, 220, 255), RGB(255, 205, 120)};
/* a note's colour by its name (C red, D orange, E yellow, F lime, G green, A blue, B purple: the bells') */
static const uint16_t NOTE_COL[12] = {RGB(255, 70, 70),  RGB(255, 110, 70), RGB(255, 160, 50), RGB(255, 200, 60),
                                      RGB(255, 235, 70), RGB(170, 235, 70), RGB(90, 220, 110), RGB(60, 210, 170),
                                      RGB(70, 170, 255), RGB(90, 120, 255), RGB(160, 100, 255), RGB(230, 100, 230)};
/* My Friends' colours */
static const uint16_t MYF_COL[16] = {
    RGB(255, 90, 90),  RGB(255, 150, 60), RGB(255, 220, 60), RGB(150, 230, 70), RGB(60, 210, 120), RGB(50, 210, 210),
    RGB(70, 160, 255), RGB(120, 110, 255), RGB(180, 100, 255), RGB(255, 100, 210), RGB(255, 170, 200),
    RGB(200, 200, 210), RGB(170, 120, 80), RGB(255, 255, 255), RGB(120, 255, 200), RGB(255, 120, 120)};

static uint16_t who_colour(int who, int colour)
{
    if (who < EN_NFRIEND)
        return EN_FRIEND[who].colour;
    return MYF_COL[colour & 15];
}

/* ------------------------------------------------------------- a friend --- */
typedef struct {
    float x, y;                        /* where its feet are */
    float r;                           /* its size */
    float wave;                        /* 0..4: sine .. noise */
    float mouth;                       /* 0..1 */
    float lift;                        /* 0..1: up off the floor */
    float squash;                      /* -1 (squashed) .. 1 (stretched) */
    float bob, flap, fade;             /* the LFO: up and down (px), the mouth, the volume (0 solid .. 1 gone) */
    float t;                           /* seconds (the fuzz moves) */
    float look;                        /* -1 .. 1: the eyes */
    int blink, happy;
    uint16_t col, bg;
} fr_t;

static float shape_r(int i, float a, float t)     /* the body's radius at turn a (0 the top), for shape i */
{
    float ph = a * FM_TWO_PI;
    switch (i) {
    case 0: return 1.0f + 0.04f * fm_cosf(ph * 2.0f);            /* a blob, a little wide */
    case 1: {                                                    /* a triangle, its point up, rounded */
        float k = ph - fm_floorf(ph / (FM_TWO_PI / 3.0f)) * (FM_TWO_PI / 3.0f) - FM_PI / 3.0f;
        return 0.82f * (0.5f / fm_cosf(k)) * 1.55f * 0.8f + 0.2f;
    }
    case 2: {                                                    /* spiky: a zig-zag along its back */
        float d = a > 0.5f ? 1.0f - a : a, w = fm_clampf(1.0f - (d - 0.05f) * 4.5f, 0.0f, 1.0f), s = a * 16.0f;
        s -= fm_floorf(s);
        return 0.94f + w * (0.32f * (1.0f - s));
    }
    case 3: {                                                    /* a box, its corners round */
        float c = fm_fabsf(fm_cosf(ph)), s = fm_fabsf(fm_sinf(ph));
        return 0.9f * fm_powf(fm_powf(c, 5.0f) + fm_powf(s, 5.0f), -0.2f);
    }
    default: return 0.98f + 0.08f * fm_sinf(ph * 9.0f + t * 3.0f) + 0.05f * fm_sinf(ph * 15.0f - t * 5.0f);
    }
}

#define NPT 56
static void body_pts(const fr_t *f, float cx, float cy, float grow, float *xy)
{
    int i, s = (int)f->wave;
    float fr = f->wave - (float)s, sx = 1.0f + 0.14f * -f->squash, sy = 1.0f + 0.14f * f->squash;
    if (s >= 4) {
        s = 3;
        fr = 1.0f;
    }
    for (i = 0; i < NPT; i++) {
        float a = (float)i / (float)NPT, R = shape_r(s, a, f->t) * (1.0f - fr) + shape_r(s + 1, a, f->t) * fr;
        float dx = fm_sin_turns(a), dy = -fm_sin_turns(a + 0.25f);
        xy[2 * i] = cx + dx * (R * f->r * sx + grow);
        xy[2 * i + 1] = cy + dy * (R * f->r * sy + grow) * (dy > 0.0f ? 0.92f : 1.0f);
    }
}

static void friend_draw(const fr_t *f)
{
    static float xy[2 * NPT];
    float sy = 1.0f + 0.14f * f->squash, cx = f->x, h = f->r * sy, lift = f->lift * f->r * 0.42f + f->bob;
    float cy = f->y - h * 0.92f - lift, w3 = f->wave >= 3.0f ? fm_minf(f->wave - 2.5f, 1.0f) : 0.0f;
    uint16_t body = mix565(f->col, f->bg, (int)(f->fade * 200.0f)), dark = shade(body, 150), light = shade(body, 380);
    uint16_t ink = mix565(INK, f->bg, (int)(f->fade * 150.0f));
    float er = f->r * 0.17f, ex = f->r * 0.33f, ey = cy - f->r * 0.18f;
    /* its shadow, smaller as it rises */
    ellipse(cx, f->y + 1.0f, f->r * (0.85f - 0.3f * f->lift), f->r * 0.16f, shade(f->bg, 170));
    /* the feet */
    ellipse(cx - f->r * 0.4f, cy + h * 0.86f + 2.0f, f->r * 0.26f + 2.0f, f->r * 0.14f + 2.0f, ink);
    ellipse(cx + f->r * 0.4f, cy + h * 0.86f + 2.0f, f->r * 0.26f + 2.0f, f->r * 0.14f + 2.0f, ink);
    ellipse(cx - f->r * 0.4f, cy + h * 0.86f + 2.0f, f->r * 0.26f, f->r * 0.14f, dark);
    ellipse(cx + f->r * 0.4f, cy + h * 0.86f + 2.0f, f->r * 0.26f, f->r * 0.14f, dark);
    if (w3 > 0.05f && f->wave < 3.7f) {           /* the robot's antenna (outlined first) */
        float top = cy - f->r * 0.9f * sy;
        thick(cx, top, cx, top - f->r * 0.4f * w3, 5.0f, ink);
        disc(cx, top - f->r * 0.42f * w3, f->r * 0.12f + 2.5f, ink);
        disc(cx, top - f->r * 0.42f * w3, f->r * 0.12f, PINK);
    }
    body_pts(f, cx, cy, 3.0f, xy);
    poly(xy, NPT, ink);
    body_pts(f, cx, cy, 0.0f, xy);
    poly(xy, NPT, body);
    ellipse(cx, cy + f->r * 0.32f, f->r * 0.5f, f->r * 0.36f, light);                    /* the belly */
    ellipse(cx - f->r * 0.42f, cy - f->r * 0.5f, f->r * 0.16f, f->r * 0.1f, shade(body, 470));   /* a shine */
    if (f->wave > 3.2f) {                         /* fuzz: tufts round the edge */
        int i;
        float k = fm_minf((f->wave - 3.2f) * 1.5f, 1.0f);
        for (i = 0; i < 14; i++) {
            float a = (float)i / 14.0f + 0.02f * fm_sinf(f->t * 2.0f + (float)i);
            disc(cx + fm_sin_turns(a) * f->r * 1.0f, cy - fm_sin_turns(a + 0.25f) * f->r * 0.95f, f->r * 0.13f * k, body);
        }
    }
    /* the eyes */
    if (f->blink) {
        thick(cx - ex - er, ey, cx - ex + er, ey, 3.0f, ink);
        thick(cx + ex - er, ey, cx + ex + er, ey, 3.0f, ink);
    } else {
        disc(cx - ex, ey, er + 1.5f, ink);
        disc(cx + ex, ey, er + 1.5f, ink);
        disc(cx - ex, ey, er, CREAM);
        disc(cx + ex, ey, er, CREAM);
        disc(cx - ex + f->look * er * 0.4f, ey + er * 0.15f, er * 0.55f, ink);
        disc(cx + ex + f->look * er * 0.4f, ey + er * 0.15f, er * 0.55f, ink);
        disc(cx - ex + f->look * er * 0.4f - er * 0.2f, ey - er * 0.1f, er * 0.18f, CREAM);
        disc(cx + ex + f->look * er * 0.4f - er * 0.2f, ey - er * 0.1f, er * 0.18f, CREAM);
    }
    if (w3 > 0.3f) {                              /* the robot's bolts */
        disc(cx - f->r * 0.78f, cy + f->r * 0.1f, f->r * 0.07f, dark);
        disc(cx + f->r * 0.78f, cy + f->r * 0.1f, f->r * 0.07f, dark);
    }
    /* the cheeks, the mouth */
    ellipse(cx - f->r * 0.56f, ey + er * 1.6f, f->r * 0.12f, f->r * 0.07f, mix565(PINK, body, 90));
    ellipse(cx + f->r * 0.56f, ey + er * 1.6f, f->r * 0.12f, f->r * 0.07f, mix565(PINK, body, 90));
    {
        float m = fm_clampf(f->mouth + f->flap, 0.0f, 1.0f), my = cy + f->r * 0.2f, mw = f->r * (0.18f + 0.24f * m);
        float mh = f->r * 0.42f * m;
        if (mh < 2.5f) {                          /* closed: a smile ("mmm") */
            thick(cx - mw, my - 1.0f, cx - mw * 0.4f, my + 2.0f, 3.0f, ink);
            thick(cx - mw * 0.4f, my + 2.0f, cx + mw * 0.4f, my + 2.0f, 3.0f, ink);
            thick(cx + mw * 0.4f, my + 2.0f, cx + mw, my - 1.0f, 3.0f, ink);
        } else {                                  /* open ("aah") */
            ellipse(cx, my + mh * 0.5f, mw, mh * 0.55f + 1.0f, ink);
            if (mh > 6.0f)
                ellipse(cx, my + mh * 0.75f, mw * 0.55f, mh * 0.22f, PINK);
        }
    }
}

/* the notes that fly from its mouth, coloured by their names */
#define NPART 10
static struct {
    float x, y, vx, vy, age;
    uint16_t c;
} pt[NPART];
static void notes_spawn(float x, float y, int note)
{
    int i, o = 0;
    for (i = 0; i < NPART; i++)
        if (pt[i].age > pt[o].age)
            o = i;
    for (i = 0; i < NPART; i++)
        if (pt[i].age >= 1.2f) {
            o = i;
            break;
        }
    pt[o].x = x;
    pt[o].y = y;
    pt[o].vx = 30.0f + (float)(note % 7) * 9.0f;
    pt[o].vy = -50.0f - (float)(note % 5) * 6.0f;
    if (note & 1)
        pt[o].vx = -pt[o].vx;
    pt[o].age = 0.0f;
    pt[o].c = NOTE_COL[note % 12];
}
static void notes_draw(float dt)
{
    int i;
    for (i = 0; i < NPART; i++) {
        if (pt[i].age >= 1.2f)
            continue;
        pt[i].age += dt;
        pt[i].x += pt[i].vx * dt;
        pt[i].y += pt[i].vy * dt;
        pt[i].vy += 25.0f * dt;
        if (pt[i].age < 1.2f)
            quaver(pt[i].x, pt[i].y, 5.5f * (1.0f - pt[i].age * 0.4f), pt[i].c);
    }
}
static void notes_clear(void)
{
    int i;
    for (i = 0; i < NPART; i++)
        pt[i].age = 9.0f;
}

/* ---------------------------------------------------------------- places --- */
enum { PL_BEDROOM, PL_CAVE, PL_CANYON, PL_SPACE };
static int place_of(int echo) { return echo < 25 ? PL_BEDROOM : echo < 50 ? PL_CAVE : echo < 75 ? PL_CANYON : PL_SPACE; }
static const uint16_t PLACE_BG[4] = {RGB(44, 40, 104), RGB(30, 24, 52), RGB(250, 150, 90), RGB(8, 8, 30)};
static const uint16_t PLACE_FLOOR[4] = {RGB(170, 100, 70), RGB(78, 66, 104), RGB(224, 160, 96), RGB(64, 52, 128)};
static uint32_t hash32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

static void place_draw(int pl, float t, int floor_y, int beat)
{
    int i;
    switch (pl) {
    case PL_BEDROOM:
        cv_rect(0, 0, 240, floor_y, PLACE_BG[0]);
        for (i = 0; i < 24; i++) {                /* the wallpaper's little stars */
            uint32_t h = hash32((uint32_t)i * 7u + 1u);
            star((float)(h % 240u), (float)((h >> 8) % (uint32_t)(floor_y - 8)), 3.0f, 0.0f, RGB(70, 66, 140));
        }
        cv_round(18, 14, 62, 54, 8, RGB(26, 22, 70));          /* the window: the night, the moon */
        cv_round(22, 18, 54, 46, 6, RGB(40, 70, 160));
        disc(60, 32, 9, RGB(255, 240, 180));
        disc(64, 29, 8, RGB(40, 70, 160));
        cv_rect(47, 18, 4, 46, RGB(26, 22, 70));
        cv_rect(22, 39, 54, 4, RGB(26, 22, 70));
        cv_rect(160, 46, 64, 6, RGB(120, 70, 50));             /* a shelf with toys */
        cv_round(166, 30, 14, 16, 3, RGB(255, 90, 90));
        disc(192, 38, 8, RGB(80, 200, 120));
        cv_round(204, 26, 14, 20, 3, RGB(255, 210, 70));
        break;
    case PL_CAVE:
        cv_rect(0, 0, 240, floor_y, PLACE_BG[1]);
        for (i = 0; i < 9; i++) {                 /* stalactites */
            float x = 8.0f + (float)i * 28.0f, xy[6] = {x - 10.0f, 0.0f, x + 10.0f, 0.0f, x + (float)(i % 3) * 2.0f,
                                                       18.0f + (float)(hash32((uint32_t)i) % 26u)};
            poly(xy, 3, RGB(56, 46, 86));
        }
        for (i = 0; i < 4; i++) {                 /* crystals, glowing with the beat */
            float x = i < 2 ? 18.0f + (float)i * 22.0f : 186.0f + (float)(i - 2) * 26.0f, y = (float)floor_y - 4.0f;
            float g = beat == i ? 1.0f : 0.0f, xy[8] = {x, y - 30.0f, x + 8.0f, y - 12.0f, x, y, x - 8.0f, y - 12.0f};
            poly(xy, 4, mix565(i & 1 ? RGB(80, 220, 220) : RGB(230, 110, 230), CREAM, (int)(g * 120.0f)));
        }
        break;
    case PL_CANYON: {
        static const uint16_t SKY[4] = {RGB(255, 120, 110), RGB(255, 150, 100), RGB(255, 182, 100), RGB(255, 210, 120)};
        for (i = 0; i < 4; i++)
            cv_rect(0, i * (floor_y / 4), 240, floor_y / 4 + 1, SKY[i]);
        disc(170, 40, 18, RGB(255, 240, 160));
        {
            float l[12] = {0, 30, 34, 26, 52, 50, 50, (float)floor_y, 0, (float)floor_y};
            float r[12] = {240, 20, 200, 30, 186, 54, 196, 70, 190, (float)floor_y, 240, (float)floor_y};
            poly(l, 5, RGB(190, 86, 60));
            poly(r, 6, RGB(176, 76, 56));
            cv_rect(0, 60, 44, 3, RGB(150, 60, 46));
            cv_rect(198, 84, 42, 3, RGB(140, 56, 44));
        }
        break;
    }
    default:
        cv_rect(0, 0, 240, floor_y, PLACE_BG[3]);
        for (i = 0; i < 44; i++) {                /* stars, twinkling */
            uint32_t h = hash32((uint32_t)i * 13u + 5u);
            float tw = 0.5f + 0.5f * fm_sinf(t * (1.0f + (float)(h & 7u) * 0.4f) + (float)i);
            px_blend((int32_t)(h % 240u), (int32_t)((h >> 9) % (uint32_t)floor_y), CREAM, (uint32_t)(4.0f + tw * 12.0f));
        }
        disc(190, 34, 16, RGB(120, 90, 220));     /* a planet and its ring */
        thick(166, 40, 214, 28, 3.0f, RGB(250, 200, 120));
        disc(186, 30, 5, RGB(150, 120, 240));
        break;
    }
}

/* the floor's back: tiles that light as their pads play (ago: seconds since each pad) */
static void floor_draw(int pl, int y0, int y1, const float *ago)
{
    int i;
    uint16_t fl = PLACE_FLOOR[pl];
    cv_rect(0, y0, 240, y1 - y0, fl);
    cv_rect(0, y0, 240, 2, shade(fl, 200));
    for (i = 0; i < EN_NPAD; i++) {
        int x = 4 + i * 21;
        float g = ago[i] < 0.35f ? 1.0f - ago[i] / 0.35f : 0.0f;
        cv_round(x, y0 + 6, 19, y1 - y0 - 8, 4, mix565(shade(fl, 220), PAD_COL[i], (int)(g * 230.0f)));
    }
}

/* ----------------------------------------------------------------- icons --- */
static void pad_icon(int p, float x, float y, float s, uint16_t c)
{
    switch (p) {
    case PAD_KICK:
        disc(x, y, s, c);
        disc(x, y, s * 0.55f, INK);
        disc(x, y, s * 0.35f, c);
        break;
    case PAD_SNARE:
        ellipse(x, y, s, s * 0.7f, c);
        thick(x - s * 0.7f, y, x - s * 0.2f, y - s * 0.3f, s * 0.22f, INK);
        thick(x - s * 0.2f, y - s * 0.3f, x + s * 0.3f, y + s * 0.1f, s * 0.22f, INK);
        thick(x + s * 0.3f, y + s * 0.1f, x + s * 0.7f, y - s * 0.2f, s * 0.22f, INK);
        break;
    case PAD_CLAP:
        star(x, y, s, 0.0f, c);
        break;
    case PAD_HAT:
        ellipse(x, y - s * 0.2f, s, s * 0.32f, c);
        thick(x, y - s * 0.2f, x, y + s, s * 0.22f, c);
        break;
    case PAD_TOM:
        cv_round((int32_t)(x - s * 0.8f), (int32_t)(y - s * 0.6f), (int32_t)(s * 1.6f), (int32_t)(s * 1.3f), 3, c);
        ellipse(x, y - s * 0.6f, s * 0.8f, s * 0.25f, shade(c, 380));
        break;
    case PAD_COWBELL: {
        float xy[8] = {x - s * 0.45f, y - s, x + s * 0.45f, y - s, x + s * 0.85f, y + s * 0.8f, x - s * 0.85f, y + s * 0.8f};
        poly(xy, 4, c);
        break;
    }
    case PAD_BOING: {
        int k;
        for (k = 0; k < 4; k++)
            thick(x - s * 0.7f, y + s * 0.8f - (float)k * s * 0.5f, x + s * 0.7f, y + s * 0.55f - (float)k * s * 0.5f,
                  s * 0.25f, c);
        break;
    }
    case PAD_LASER: {
        float xy[12] = {x + s * 0.3f, y - s, x - s * 0.5f, y + s * 0.1f, x, y + s * 0.1f, x - s * 0.3f, y + s,
                        x + s * 0.5f, y - s * 0.1f, x, y - s * 0.1f};
        poly(xy, 6, c);
        break;
    }
    case PAD_POP:
        ring(x, y, s, s * 0.3f, c);
        disc(x - s * 0.3f, y - s * 0.3f, s * 0.2f, c);
        break;
    case PAD_WHOOSH:
        thick(x - s, y - s * 0.4f, x + s * 0.6f, y - s * 0.4f, s * 0.22f, c);
        thick(x - s * 0.6f, y + s * 0.1f, x + s, y + s * 0.1f, s * 0.22f, c);
        thick(x - s, y + s * 0.6f, x + s * 0.4f, y + s * 0.6f, s * 0.22f, c);
        break;
    default:
        star(x, y, s, 0.0f, c);
        star(x, y, s * 0.45f, 0.1f, INK);
        break;
    }
}

static void style_icon(int st, float x, float y, float s)
{
    switch (st) {
    case 0:                                       /* Bounce: a ball */
        disc(x, y, s, RGB(255, 90, 90));
        thick(x - s, y, x + s, y, s * 0.25f, CREAM);
        break;
    case 1:                                       /* March: a drum and its sticks */
        thick(x - s, y - s, x + s * 0.2f, y - s * 0.1f, s * 0.2f, CREAM);
        cv_round((int32_t)(x - s), (int32_t)(y - s * 0.4f), (int32_t)(2.0f * s), (int32_t)(s * 1.3f), 3, RGB(255, 80, 80));
        thick(x - s, y + s * 0.1f, x + s, y + s * 0.6f, s * 0.15f, CREAM);
        break;
    case 2:                                       /* Robot: a head */
        cv_round((int32_t)(x - s), (int32_t)(y - s * 0.8f), (int32_t)(2.0f * s), (int32_t)(1.7f * s), 3, RGB(170, 180, 200));
        disc(x - s * 0.4f, y - s * 0.1f, s * 0.22f, INK);
        disc(x + s * 0.4f, y - s * 0.1f, s * 0.22f, INK);
        thick(x, y - s * 0.8f, x, y - s * 1.2f, s * 0.15f, RGB(170, 180, 200));
        break;
    case 3: {                                     /* Space: a rocket */
        float xy[10] = {x, y - s * 1.2f, x + s * 0.5f, y - s * 0.3f, x + s * 0.5f, y + s * 0.8f, x - s * 0.5f,
                        y + s * 0.8f, x - s * 0.5f, y - s * 0.3f};
        poly(xy, 5, CREAM);
        disc(x, y - s * 0.2f, s * 0.22f, RGB(80, 150, 255));
        disc(x, y + s * 1.0f, s * 0.3f, RGB(255, 150, 60));
        break;
    }
    case 4:                                       /* Lullaby: the moon */
        disc(x, y, s, RGB(255, 236, 150));
        disc(x + s * 0.45f, y - s * 0.3f, s * 0.85f, NAVY);
        break;
    case 5:                                       /* Dino Stomp: a footprint */
        ellipse(x, y + s * 0.3f, s * 0.6f, s * 0.55f, RGB(120, 200, 90));
        disc(x - s * 0.6f, y - s * 0.5f, s * 0.25f, RGB(120, 200, 90));
        disc(x, y - s * 0.75f, s * 0.25f, RGB(120, 200, 90));
        disc(x + s * 0.6f, y - s * 0.5f, s * 0.25f, RGB(120, 200, 90));
        break;
    case 6: {                                     /* Jungle Party: a leaf */
        float xy[8] = {x, y - s * 1.1f, x + s * 0.8f, y, x, y + s * 1.1f, x - s * 0.8f, y};
        poly(xy, 4, RGB(60, 200, 110));
        thick(x, y - s * 0.9f, x, y + s * 1.1f, s * 0.12f, RGB(30, 120, 60));
        break;
    }
    default:                                      /* Disco: a mirror ball */
        disc(x, y, s, RGB(200, 210, 230));
        thick(x - s, y, x + s, y, 1.5f, RGB(120, 130, 160));
        thick(x, y - s, x, y + s, 1.5f, RGB(120, 130, 160));
        disc(x - s * 0.35f, y - s * 0.35f, s * 0.2f, CREAM);
        break;
    }
}

/* the knobs' pictures: shape, bright, wobble, the room (v 0..1) */
static void knob_icon(int k, float x, float y, float s, float v, uint16_t c)
{
    switch (k) {
    case 0: {                                     /* the friend's shape itself, small */
        fr_t f;
        static float xy[2 * NPT];
        memset(&f, 0, sizeof f);
        f.r = s;
        f.wave = v * 4.0f;
        body_pts(&f, x, y, 0.0f, xy);
        poly(xy, NPT, c);
        break;
    }
    case 1: {                                     /* a sun: more rays, brighter */
        int i, n = 3 + (int)(v * 7.0f);
        for (i = 0; i < n; i++) {
            float a = (float)i / (float)n;
            thick(x + fm_sin_turns(a) * s * 0.7f, y - fm_sin_turns(a + 0.25f) * s * 0.7f, x + fm_sin_turns(a) * s * 1.1f,
                  y - fm_sin_turns(a + 0.25f) * s * 1.1f, 2.5f, c);
        }
        disc(x, y, s * (0.35f + 0.2f * v), c);
        break;
    }
    case 2: {                                     /* a wave, taller with more wobble */
        int i;
        float px = x - s, py = y;
        for (i = 1; i <= 12; i++) {
            float nx = x - s + (float)i * s / 6.0f, ny = y - fm_sin_turns((float)i / 6.0f) * s * (0.15f + 0.75f * v);
            thick(px, py, nx, ny, 2.5f, c);
            px = nx;
            py = ny;
        }
        break;
    }
    default: {                                    /* the room: a house, a cave, a mountain, a planet */
        int pl = place_of((int)(v * 100.0f));
        if (pl == PL_BEDROOM) {
            float xy[6] = {x - s, y - s * 0.1f, x, y - s, x + s, y - s * 0.1f};
            poly(xy, 3, c);
            cv_rect((int32_t)(x - s * 0.7f), (int32_t)(y - s * 0.1f), (int32_t)(s * 1.4f), (int32_t)(s * 0.9f), c);
        } else if (pl == PL_CAVE) {
            ellipse(x, y, s, s * 0.8f, c);
            ellipse(x, y + s * 0.3f, s * 0.5f, s * 0.5f, INK);
        } else if (pl == PL_CANYON) {
            float xy[8] = {x - s, y + s * 0.7f, x - s * 0.3f, y - s * 0.7f, x + s * 0.3f, y - s * 0.7f, x + s, y + s * 0.7f};
            poly(xy, 4, c);
        } else {
            disc(x, y, s * 0.6f, c);
            thick(x - s, y + s * 0.3f, x + s, y - s * 0.3f, 2.5f, c);
        }
        break;
    }
    }
}
