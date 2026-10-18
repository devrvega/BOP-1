/* SPDX-License-Identifier: GPL-3.0-only */
/* BOP-1's effects: LCD-4's plate and ping-pong echo on sends (AMB-1's and IND-1's before it); the tricks
 * (FX held and a white key) on the whole mix; and the safety at the very end: the volume cap and a limiter
 * that the output never passes. Included once, by bop.c. */

/* ------------------------------------------------- reverb: Dattorro's plate --- */
/* J. Dattorro, "Effect Design, Part 1", JAES 1997 (as in FoMni, as in AMB-1): a predelay, four input
 * diffusers, then a figure-eight tank of two halves, each a modulated allpass, a delay, a damping lowpass,
 * an allpass and a delay, feeding the other half. Stereo out from seven taps a side. BOP-1 (as LCD-4) runs it at half
 * rate (22.05 kHz) with every line in one ring of 16384 (a power of two: a tap is an add and a mask): the
 * write point moves one place a step, each line keeps its own stretch of the ring ahead of it. The paper's
 * lengths are for 29761 Hz; DS() scales them to 22.05 kHz, and 6% smaller, to fit the ring. */
#define DS(n) (((n) * 6964 + 5000) / 10000)            /* 22050 / 29761 x 0.94 */
#define PRE_N 400                                    /* 19 ms */
#define EXC DS(16)
#define RING 16384u
#define RMASK (RING - 1u)
enum { RV_PRE, RV_IN1, RV_IN2, RV_IN3, RV_IN4, RV_APL, RV_D1L, RV_AP2L, RV_D2L, RV_APR, RV_D1R, RV_AP2R, RV_D2R, RV_N };
static const uint16_t RV_LEN[RV_N] = {PRE_N, DS(142), DS(107), DS(379), DS(277), DS(672) + EXC + 2, DS(4453), DS(1800),
                                      DS(3720), DS(908) + EXC + 2, DS(4217), DS(2656), DS(3163)};
static uint16_t rv_base[RV_N];                       /* each line's stretch of the ring */
static float ring[RING] EN_POOL;
static uint32_t rp;                                  /* the write point: one place back a step */
static float pl_bw, pl_damp_l, pl_damp_r, pl_decay = 0.8f, pl_dd2 = 0.5f, lfo_s, lfo_c = 1.0f;

#define RD(line, k) ring[(rp + rv_base[line] + (uint32_t)(k)) & RMASK]
#define WR(line, x) (ring[(rp + rv_base[line]) & RMASK] = (x))
static inline float ap(int line, float x, float g)
{
    float z = RD(line, RV_LEN[line]), v = x - g * z;
    WR(line, fm_flush(v));
    return z + g * v;
}
static inline float ap_mod(int line, float d, float x, float g)
{
    uint32_t k = (uint32_t)d;
    float f = d - (float)k, z0 = RD(line, k), z = z0 + (RD(line, k + 1u) - z0) * f, v = x - g * z;
    WR(line, fm_flush(v));
    return z + g * v;
}

static inline void plate(float in, float *ol, float *or_)
{
    float x, a, b, fb_l = RD(RV_D2R, DS(3163)), fb_r = RD(RV_D2L, DS(3720));
    WR(RV_PRE, in);
    x = RD(RV_PRE, PRE_N);
    pl_bw += 0.8f * (x - pl_bw);                      /* (the paper's at 44.1 kHz, set again for 22 kHz) */
    x = ap(RV_IN1, pl_bw, 0.75f);
    x = ap(RV_IN2, x, 0.75f);
    x = ap(RV_IN3, x, 0.625f);
    x = ap(RV_IN4, x, 0.625f);
    {
        float s = lfo_s + 0.0002f * lfo_c, c = lfo_c - 0.0002f * s;
        lfo_s = s;
        lfo_c = c;
    }
    a = ap_mod(RV_APL, (float)DS(672) + (float)EXC * lfo_s, x + pl_decay * fb_l, -0.7f);
    WR(RV_D1L, a);
    b = RD(RV_D1L, DS(4453));
    pl_damp_l += 0.64f * (b - pl_damp_l);
    b = ap(RV_AP2L, pl_damp_l * pl_decay, pl_dd2);
    WR(RV_D2L, b);
    a = ap_mod(RV_APR, (float)DS(908) + (float)EXC * lfo_c, x + pl_decay * fb_r, -0.7f);
    WR(RV_D1R, a);
    b = RD(RV_D1R, DS(4217));
    pl_damp_r += 0.64f * (b - pl_damp_r);
    b = ap(RV_AP2R, pl_damp_r * pl_decay, pl_dd2);
    WR(RV_D2R, b);
    *ol = 0.6f * (RD(RV_D1R, DS(266)) + RD(RV_D1R, DS(2974)) - RD(RV_AP2R, DS(1913)) + RD(RV_D2R, DS(1996)) -
                  RD(RV_D1L, DS(1990)) - RD(RV_AP2L, DS(187)) - RD(RV_D2L, DS(1066)));
    *or_ = 0.6f * (RD(RV_D1L, DS(353)) + RD(RV_D1L, DS(3627)) - RD(RV_AP2L, DS(1228)) + RD(RV_D2L, DS(2673)) -
                   RD(RV_D1R, DS(2111)) - RD(RV_AP2R, DS(335)) - RD(RV_D2R, DS(121)));
    rp = (rp - 1u) & RMASK;
}

static void plate_init(void)
{
    uint32_t i, b = 0;
    int k;
    for (k = 0; k < RV_N; k++) {
        rv_base[k] = (uint16_t)b;
        b += RV_LEN[k] + 2u;
    }
    for (i = 0; i < RING; i++)
        ring[i] = 0.0f;
    rp = 0;
    pl_bw = pl_damp_l = pl_damp_r = 0.0f;
    lfo_s = 0.0f;
    lfo_c = 1.0f;
}

/* ------------------------------------------------------ the ping-pong delay --- */
/* left then right then left, each repeat darker, its time in sixteenths (Time); at half rate (22 kHz,
 * 16-bit), which the repeats don't miss */
#define DLY_N 16560                             /* 0.75 s at 22.05 kHz: a dotted eighth at 60 */
#define DLY_SR (EN_SR * 0.5f)
static int16_t dly_l[DLY_N] EN_POOL, dly_r[DLY_N] EN_POOL;
static int dly_w;
static float dly_len = 8000.0f, dly_len_t = 8000.0f, dly_lpl, dly_lpr, dly_fb = 0.5f;
static const uint8_t DLY_SIXTEENTHS[5] = {1, 2, 3, 4, 6};

static inline float dread(const int16_t *b, float d)
{
    int k = (int)d, i0 = dly_w - k, i1;
    float f = d - (float)k, a, c;
    if (i0 < 0)
        i0 += DLY_N;
    i1 = i0 - 1;
    if (i1 < 0)
        i1 += DLY_N;
    a = (float)b[i0];
    c = (float)b[i1];
    return (a + (c - a) * f) * (1.0f / 12000.0f);
}
static inline int16_t s16(float x)
{
    x *= 12000.0f;
    return (int16_t)(x > 32767.0f ? 32767.0f : x < -32767.0f ? -32767.0f : x);
}

static void delay_time(float step_secs)
{
    float t = (float)DLY_SIXTEENTHS[clampi(en_song.echo_time * 5 / 101, 0, 4)] * step_secs;
    dly_len_t = fm_minf(t * DLY_SR, (float)(DLY_N - 8));
    if (dly_len <= 0.0f)
        dly_len = dly_len_t;
}

/* the sends, a chunk at a time: at half rate, the delay's step and then the plate's (which hears the delay's
 * repeats too), their outputs slid between steps */
static int snd_ph;
static float snd_dacc, snd_racc, snd_prev[4], snd_cur[4];
static void sends_run(const float *ds, const float *rs, float *l, float *r, uint32_t n)
{
    uint32_t j;
    for (j = 0; j < n; j++) {
        float f;
        snd_dacc += ds[j];
        snd_racc += rs[j];
        if (++snd_ph >= 2) {
            float yl, yr, wl, wr;
            int k;
            snd_ph = 0;
            dly_len += (dly_len_t - dly_len) * 0.0005f;
            yl = dread(dly_l, dly_len);
            yr = dread(dly_r, dly_len);
            dly_lpl += 0.5f * (yl - dly_lpl);
            dly_lpr += 0.5f * (yr - dly_lpr);
            if (++dly_w >= DLY_N)
                dly_w = 0;
            dly_l[dly_w] = s16(snd_dacc * 0.5f + dly_fb * dly_lpr);   /* the left hears the send and the right's repeats */
            dly_r[dly_w] = s16(dly_fb * dly_lpl);
            plate((snd_racc * 0.5f + (yl + yr) * 0.2f) * 0.6f, &wl, &wr);
            for (k = 0; k < 4; k++)
                snd_prev[k] = snd_cur[k];
            snd_cur[0] = yl;
            snd_cur[1] = yr;
            snd_cur[2] = wl;
            snd_cur[3] = wr;
            snd_dacc = snd_racc = 0.0f;
        }
        f = (float)snd_ph * 0.5f;
        l[j] += 0.75f * (snd_prev[0] + (snd_cur[0] - snd_prev[0]) * f) + snd_prev[2] + (snd_cur[2] - snd_prev[2]) * f;
        r[j] += 0.75f * (snd_prev[1] + (snd_cur[1] - snd_prev[1]) * f) + snd_prev[3] + (snd_cur[3] - snd_prev[3]) * f;
    }
}

/* ------------------------------------------------------------- the tricks --- */
/* chipmunk and giant (an octave up, down), slow-mo (a tape stopping: the tempo halves, the pitch falls) are
 * in the voices (bend_t) and the clock (tempo_fac); robot, wah, rise and drop are here, on the mix */
static float tr_rise, tr_drop, tr_robot, tr_wah, wah_ph, robot_ph;
static float trs_l[2], trs_r[2], trh_l[2], trh_r[2], trw_l[2], trw_r[2];
static void tricks_reset(void)
{
    tr_rise = tr_drop = tr_robot = tr_wah = wah_ph = robot_ph = 0.0f;
    trs_l[0] = trs_l[1] = trs_r[0] = trs_r[1] = trh_l[0] = trh_l[1] = trh_r[0] = trh_r[1] = 0.0f;
    trw_l[0] = trw_l[1] = trw_r[0] = trw_r[1] = 0.0f;
}
static void tricks_run(float *l, float *r, uint32_t n, int t, float bar_secs)
{
    float rate = (float)n / (2.0f * bar_secs * EN_SR), back = (float)n / (0.12f * EN_SR), k = fm_minf((float)n / (0.01f * EN_SR), 1.0f);
    uint32_t j;
    tr_drop = t == TR_DROP ? fm_minf(tr_drop + rate, 1.0f) : fm_maxf(tr_drop - back, 0.0f);
    tr_rise = t == TR_RISE ? fm_minf(tr_rise + rate, 1.0f) : fm_maxf(tr_rise - back, 0.0f);
    tr_robot += ((t == TR_ROBOT ? 1.0f : 0.0f) - tr_robot) * k;
    tr_wah += ((t == TR_WAH ? 1.0f : 0.0f) - tr_wah) * k;
    if (tr_drop > 0.001f) {                       /* the low-pass comes down over two bars */
        float c[4], bp;
        svf_coefs(c, 18000.0f * fm_exp2f(-7.0f * tr_drop), 0.7f);
        for (j = 0; j < n; j++) {
            l[j] = svf(trs_l, c, l[j], &bp);
            r[j] = svf(trs_r, c, r[j], &bp);
        }
    } else {
        trs_l[0] = trs_l[1] = trs_r[0] = trs_r[1] = 0.0f;
    }
    if (tr_rise > 0.001f) {                       /* the high-pass goes up */
        float c[4], bp;
        svf_coefs(c, 30.0f * fm_exp2f(6.5f * tr_rise), 0.7f);
        for (j = 0; j < n; j++) {
            l[j] = svf_hp(trh_l, c, l[j], &bp);
            r[j] = svf_hp(trh_r, c, r[j], &bp);
        }
    } else {
        trh_l[0] = trh_l[1] = trh_r[0] = trh_r[1] = 0.0f;
    }
    if (tr_wah > 0.001f) {                        /* a band swept up and down, three times a second */
        float c[4], bp, f;
        wah_ph = wrap1(wah_ph + 2.5f * (float)n / EN_SR);
        f = 350.0f * fm_exp2f(2.6f * (0.5f + 0.5f * sine(wah_ph)));
        svf_coefs(c, f, 0.35f);
        for (j = 0; j < n; j++) {
            svf(trw_l, c, l[j], &bp);
            l[j] += (bp * 1.6f - l[j]) * tr_wah;
            svf(trw_r, c, r[j], &bp);
            r[j] += (bp * 1.6f - r[j]) * tr_wah;
        }
    }
    if (tr_robot > 0.001f) {                      /* a ring at 220 Hz, and the bits cut */
        float inc = 220.0f / EN_SR, ph = robot_ph, a = tr_robot;
        for (j = 0; j < n; j++) {
            float m, x, y;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            m = sine(ph);
            x = (float)(int)(l[j] * m * 20.0f) * (1.0f / 20.0f);
            y = (float)(int)(r[j] * m * 20.0f) * (1.0f / 20.0f);
            l[j] += (x * 1.5f - l[j]) * a;
            r[j] += (y * 1.5f - r[j]) * a;
        }
        robot_ph = ph;
    }
}

/* ---------------------------------------------------------- the safety --- */
/* the limiter: it looks ahead (the mix is delayed 64 samples, 1.45 ms) so that it can bring the gain down
 * before a peak instead of on it: a gain that steps on a peak flattens the wave's top, which is heard as
 * clipping. A block (32 samples) at a time: the gain glides in a straight line to what the block coming out
 * and the one after it need, so it is under the ceiling all through the block; it comes back slowly (a
 * second). The ceiling is the volume cap: whatever the MASTER pot says, the output's peak is never above
 * the cap (the clamp after it is only a last guard). */
#define LIM_B 32u                                    /* a block */
#define LIM_D 64u                                    /* the delay: two blocks */
static float lim_l[LIM_D], lim_r[LIM_D];
static float lim_g = 1.0f, lim_dg, lim_to = 1.0f, lim_pk0, lim_pk1;   /* the gain, its glide, the blocks' peaks */
static uint32_t lim_w, lim_c;
volatile float en_out_peak;
static float out_fade = 0.0f, out_fade_k;
static void limiter_reset(void)
{
    uint32_t j;
    for (j = 0; j < LIM_D; j++)
        lim_l[j] = lim_r[j] = 0.0f;
    lim_g = lim_to = 1.0f;
    lim_dg = lim_pk0 = lim_pk1 = 0.0f;
    lim_w = lim_c = 0;
}
static void limiter(const float *l, const float *r, int32_t *out, uint32_t n, float gain, float ceil)
{
    uint32_t j, w = lim_w, c = lim_c;
    float g = lim_g, dg = lim_dg, pk1 = lim_pk1, pk = 0.0f, fd = out_fade, fk = out_fade_k, thr = ceil * 0.94f;
    const float rel = 1.0f - (float)LIM_B / (0.8f * EN_SR);
    for (j = 0; j < n; j++) {
        float a = l[j] * gain * fd, b = r[j] * gain * fd, y0 = lim_l[w] * g, y1 = lim_r[w] * g;
        fd = fm_minf(fd + fk, 1.0f);
        lim_l[w] = a;
        lim_r[w] = b;
        w = (w + 1u) & (LIM_D - 1u);
        pk1 = fm_maxf(pk1, fm_maxf(fm_fabsf(a), fm_fabsf(b)));
        g += dg;
        y0 = fm_clampf(y0, -ceil, ceil);
        y1 = fm_clampf(y1, -ceil, ceil);
        pk = fm_maxf(pk, fm_maxf(fm_fabsf(y0), fm_fabsf(y1)));
        out[2 * j] = (int32_t)(y0 * 8300000.0f);
        out[2 * j + 1] = (int32_t)(y1 * 8300000.0f);
        if (++c == LIM_B) {                         /* a block in: the next one out is the one before it */
            float to = 1.0f - (1.0f - lim_to) * rel, m = fm_maxf(lim_pk0, pk1);
            c = 0;
            g = lim_to;
            if (m * to > thr)
                to = thr / m;
            lim_to = to;
            dg = (to - g) * (1.0f / (float)LIM_B);
            lim_pk0 = pk1;
            pk1 = 0.0f;
        }
    }
    lim_g = g;
    lim_dg = dg;
    lim_pk1 = pk1;
    lim_w = w;
    lim_c = c;
    out_fade = fd;
    en_out_peak = fm_maxf(pk, en_out_peak * 0.97f);
}
