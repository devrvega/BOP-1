/* SPDX-License-Identifier: GPL-3.0-only */
/* BOP-1's drawing kit, on gfx.c's canvas: colours mixed and shaded, rounded boxes, anti-aliased discs, rings,
 * ellipses and thick lines (X0X's and CRT-4's px_blend and disc, extended), filled polygons with soft edges
 * (the friends' bodies), stars, hearts and notes, the knobs' rings, and text placed by its middle. Part of
 * ui.c. */

static inline uint16_t mix565(uint16_t a, uint16_t b, int t)   /* t 0..256: a .. b */
{
    uint32_t ra = a >> 11, ga = (a >> 5) & 63u, ba = a & 31u, rb = b >> 11, gb = (b >> 5) & 63u, bb = b & 31u;
    uint32_t u = (uint32_t)(t < 0 ? 0 : t > 256 ? 256 : t), v = 256u - u;
    return (uint16_t)((((ra * v + rb * u) >> 8) << 11) | (((ga * v + gb * u) >> 8) << 5) | ((ba * v + bb * u) >> 8));
}
static inline uint16_t shade(uint16_t c, int k)                 /* k 0..256 darker, 256..512 lighter */
{
    return k <= 256 ? mix565(0, c, k) : mix565(c, 0xFFFFu, k - 256);
}

static void px_blend(int32_t x, int32_t y, uint16_t c, uint32_t a)   /* a 0..16 */
{
    uint16_t *q;
    uint32_t bg;
    y += cv_oy;
    if ((uint32_t)x >= cv_w || y < cv_y0 || y >= cv_y1 || !a)
        return;
    q = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
    if (a >= 16u) {
        *q = swap16(c);
        return;
    }
    bg = swap16(*q);
    *q = swap16((uint32_t)(((((c >> 11) * a + (bg >> 11) * (16u - a)) >> 4) << 11) |
                           (((((c >> 5) & 63u) * a + ((bg >> 5) & 63u) * (16u - a)) >> 4) << 5) |
                           (((c & 31u) * a + (bg & 31u) * (16u - a)) >> 4)));
}
/* a horizontal run from xa to xb (floats: the ends blended by how much of their pixel they cover) */
static void span(float xa, float xb, int32_t y, uint16_t c)
{
    int32_t i0, i1, i;
    float f;
    if (xb <= xa)
        return;
    if (xa < 0.0f)
        xa = 0.0f;
    if (xb > (float)cv_w)
        xb = (float)cv_w;
    if (xb <= xa)
        return;
    i0 = (int32_t)xa;
    i1 = (int32_t)xb;
    if (i0 == i1) {
        px_blend(i0, y, c, (uint32_t)((xb - xa) * 16.0f + 0.5f));
        return;
    }
    f = 1.0f - (xa - (float)i0);
    px_blend(i0, y, c, (uint32_t)(f * 16.0f + 0.5f));
    if (i1 > i0 + 1) {
        int32_t yy = y + cv_oy;
        if (yy >= cv_y0 && yy < cv_y1) {
            uint16_t sc = swap16(c), *row = &cv_px[(uint32_t)yy * cv_w];
            for (i = i0 + 1; i < i1; i++)
                row[i] = sc;
        }
    }
    if (i1 < (int32_t)cv_w)
        px_blend(i1, y, c, (uint32_t)((xb - (float)i1) * 16.0f + 0.5f));
}

static void cv_round(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    int32_t j;
    if (r > h / 2)
        r = h / 2;
    if (r > w / 2)
        r = w / 2;
    if (r <= 0) {
        cv_rect(x, y, w, h, c);
        return;
    }
    cv_rect(x, y + r, w, h - 2 * r, c);
    for (j = 0; j < r; j++) {
        float dy = (float)r - (float)j - 0.5f, d = (float)r - fm_sqrtf(fm_maxf((float)(r * r) - dy * dy, 0.0f));
        span((float)x + d, (float)(x + w) - d, y + j, c);
        span((float)x + d, (float)(x + w) - d, y + h - 1 - j, c);
    }
}

/* an ellipse (rx, ry), filled, its edge soft */
static void ellipse(float cx, float cy, float rx, float ry, uint16_t c)
{
    int32_t y0 = (int32_t)fm_floorf(cy - ry), y1 = (int32_t)(cy + ry) + 1, y;
    if (rx <= 0.2f || ry <= 0.2f)
        return;
    for (y = y0; y <= y1; y++) {
        float dy = ((float)y + 0.5f - cy) / ry, w;
        if (dy <= -1.0f || dy >= 1.0f)
            continue;
        w = rx * fm_sqrtf(1.0f - dy * dy);
        span(cx - w, cx + w, y, c);
    }
}
static void disc(float cx, float cy, float r, uint16_t c) { ellipse(cx, cy, r, r, c); }

/* a ring (an outline of thickness t) */
static void ring(float cx, float cy, float r, float t, uint16_t c)
{
    int32_t x0 = (int32_t)fm_floorf(cx - r - 1.0f), x1 = (int32_t)(cx + r + 1.0f);
    int32_t y0 = (int32_t)fm_floorf(cy - r - 1.0f), y1 = (int32_t)(cy + r + 1.0f), x, y;
    float ri = r - t;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, d = fm_sqrtf(dx * dx + dy * dy);
            float a = fm_clampf(r - d + 0.5f, 0.0f, 1.0f) * fm_clampf(d - ri + 0.5f, 0.0f, 1.0f);
            if (a > 0.0f)
                px_blend(x, y, c, (uint32_t)(a * 16.0f + 0.5f));
        }
}

/* a thick line with round ends */
static void thick(float x0, float y0, float x1, float y1, float w, uint16_t c)
{
    float dx = x1 - x0, dy = y1 - y0, l2 = dx * dx + dy * dy, r = w * 0.5f;
    int32_t xa = (int32_t)fm_floorf(fm_minf(x0, x1) - r - 1.0f), xb = (int32_t)(fm_maxf(x0, x1) + r + 1.0f);
    int32_t ya = (int32_t)fm_floorf(fm_minf(y0, y1) - r - 1.0f), yb = (int32_t)(fm_maxf(y0, y1) + r + 1.0f), x, y;
    float il = l2 > 1e-6f ? 1.0f / l2 : 0.0f;
    for (y = ya; y <= yb; y++)
        for (x = xa; x <= xb; x++) {
            float px = (float)x + 0.5f - x0, py = (float)y + 0.5f - y0, t = fm_clampf((px * dx + py * dy) * il, 0.0f, 1.0f);
            float ex = px - t * dx, ey = py - t * dy, d = fm_sqrtf(ex * ex + ey * ey), a = fm_clampf(r - d + 0.5f, 0.0f, 1.0f);
            if (a > 0.0f)
                px_blend(x, y, c, (uint32_t)(a * 16.0f + 0.5f));
        }
}

/* a polygon (any shape: even-odd), its edge soft across */
#define PMAX 112
static void poly(const float *xy, int n, uint16_t c)
{
    float ymin = 1e9f, ymax = -1e9f;
    int32_t y, y0, y1;
    int i;
    for (i = 0; i < n; i++) {
        ymin = fm_minf(ymin, xy[2 * i + 1]);
        ymax = fm_maxf(ymax, xy[2 * i + 1]);
    }
    y0 = (int32_t)fm_floorf(ymin);
    y1 = (int32_t)ymax + 1;
    if (y0 < cv_y0 - cv_oy)
        y0 = cv_y0 - cv_oy;
    if (y1 > cv_y1 - cv_oy)
        y1 = cv_y1 - cv_oy;
    for (y = y0; y < y1; y++) {
        float yc = (float)y + 0.5f, xs[24];
        int m = 0, a, b;
        for (i = 0; i < n && m < 24; i++) {
            float ax = xy[2 * i], ay = xy[2 * i + 1], bx = xy[2 * ((i + 1) % n)], by = xy[2 * ((i + 1) % n) + 1];
            if ((ay <= yc && by > yc) || (by <= yc && ay > yc))
                xs[m++] = ax + (yc - ay) * (bx - ax) / (by - ay);
        }
        for (a = 1; a < m; a++) {                 /* (few: an insertion sort) */
            float v = xs[a];
            b = a;
            while (b > 0 && xs[b - 1] > v) {
                xs[b] = xs[b - 1];
                b--;
            }
            xs[b] = v;
        }
        for (a = 0; a + 1 < m; a += 2)
            span(xs[a], xs[a + 1], y, c);
    }
}

/* a five-point star, a heart, a quaver */
static void star(float cx, float cy, float r, float turn, uint16_t c)
{
    float xy[20];
    int i;
    for (i = 0; i < 10; i++) {
        float a = turn + (float)i * 0.1f, rr = (i & 1) ? r * 0.45f : r;
        xy[2 * i] = cx + rr * fm_sin_turns(a);
        xy[2 * i + 1] = cy - rr * fm_sin_turns(a + 0.25f);
    }
    poly(xy, 10, c);
}
static void heart(float cx, float cy, float r, uint16_t c)
{
    float xy[2 * 3];
    disc(cx - r * 0.5f, cy - r * 0.2f, r * 0.55f, c);
    disc(cx + r * 0.5f, cy - r * 0.2f, r * 0.55f, c);
    xy[0] = cx - r * 1.02f;
    xy[1] = cy;
    xy[2] = cx + r * 1.02f;
    xy[3] = cy;
    xy[4] = cx;
    xy[5] = cy + r * 1.05f;
    poly(xy, 3, c);
}
static void quaver(float x, float y, float s, uint16_t c)   /* a note: its head at x, y */
{
    ellipse(x, y, s * 0.55f, s * 0.42f, c);
    thick(x + s * 0.45f, y, x + s * 0.45f, y - s * 1.6f, s * 0.22f, c);
    thick(x + s * 0.45f, y - s * 1.6f, x + s * 1.05f, y - s * 1.05f, s * 0.22f, c);
}

/* a knob's ring: the track, the value; from 7:30 round to 4:30 (X0X's) */
static float turns_of(float y, float x)
{
    float ax = fm_fabsf(x), ay = fm_fabsf(y), a = ax < ay ? ax / (ay + 1e-9f) : ay / (ax + 1e-9f), s = a * a, r;
    r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax)
        r = 1.57079637f - r;
    if (x < 0.0f)
        r = FM_PI - r;
    if (y < 0.0f)
        r = FM_TWO_PI - r;
    return r * (1.0f / FM_TWO_PI);
}
static void arc(int32_t cx, int32_t cy, int32_t r, float th_, float v, uint16_t track, uint16_t fill)
{
    const float a0 = 0.375f, sweep = 0.75f;
    float ro = (float)r, ri = ro - th_;
    int32_t x, y;
    v = fm_clampf(v, 0.0f, 1.0f);
    for (y = -r - 1; y <= r + 1; y++)
        for (x = -r - 1; x <= r + 1; x++) {
            float dx = (float)x, dy = (float)y, d = fm_sqrtf(dx * dx + dy * dy), t, a;
            a = fm_clampf(ro + 0.5f - d, 0.0f, 1.0f) * fm_clampf(d - ri + 0.5f, 0.0f, 1.0f);
            if (a <= 0.0f)
                continue;
            t = turns_of(dy, dx) - a0;
            if (t < 0.0f)
                t += 1.0f;
            t /= sweep;
            if (t > 1.0f)
                continue;
            px_blend(cx + x, cy + y, t <= v ? fill : track, (uint32_t)(a * 16.0f + 0.5f));
        }
    {
        float e = (a0 + sweep * v) * FM_TWO_PI, rm = ro - th_ * 0.5f;
        disc((float)cx + 0.5f + fm_cosf(e) * rm, (float)cy + 0.5f + fm_sinf(e) * rm, th_ * 0.5f + 1.2f, fill);
    }
}

/* text by its middle, with a soft shadow if sh */
static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}
static void text_cs(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c, uint16_t sh)
{
    int32_t x = cx - text_w(f, s) / 2;
    cv_text(x + 1, y + 2, f, s, sh);
    cv_text(x, y, f, s, c);
}
