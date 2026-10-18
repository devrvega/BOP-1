/* SPDX-License-Identifier: GPL-3.0-only */
/* BOP-1's pads: six drums and five funny sounds, all synthesized (no samples), one voice each. The drums are
 * LCD-4's 808-style parts (a swept sine kick with a click, two heads and the snares, a clap of noise
 * bursts, the six-square metal of the hat, a swept tom, the cowbell's two squares); the funny sounds are
 * BOP-1's own: a boing (a sine that falls and wobbles), a laser (a pulse diving), a pop (a tiny drop with a
 * click), a whoosh (noise through a band that sweeps up and back) and a ta-da (a short chord, then a long
 * one, in the song's key). An accented hat rings open. Included once, by bop.c. */

const char *const EN_PAD_NAME[EN_NPAD] = {"Kick", "Snare", "Clap", "Hat", "Tom", "Cowbell", "Boing", "Laser",
                                          "Pop", "Whoosh", "Ta-da"};
static const float PAD_PAN[EN_NPAD] = {0.0f, 0.0f, 0.05f, 0.25f, -0.3f, -0.2f, 0.2f, -0.25f, 0.3f, 0.0f, 0.0f};
static const float PAD_GAIN[EN_NPAD] = {1.0f, 0.75f, 0.7f, 0.7f, 0.8f, 0.5f, 0.6f, 0.4f, 0.7f, 0.55f, 0.42f};
/* Play: the child's hits, more again for the quiet ones (short, or high), so that each is heard over the song */
static const float PAD_LIVE[EN_NPAD] = {1.0f, 1.3f, 1.3f, 2.4f, 1.0f, 1.2f, 1.0f, 1.9f, 1.3f, 1.4f, 1.5f};
static const float PAD_DEC[EN_NPAD] = {0.6f, 0.2f, 0.3f, 0.06f, 0.4f, 0.35f, 0.7f, 0.35f, 0.08f, 0.7f, 1.1f};

typedef struct {
    uint8_t on, live;                  /* live: the child's (its own gain, not the song's Beats layer) */
    float g, gl, gr;
    float amp, ad, amp2, ad2;
    float ph, inc, inc_end, pd, ph2, inc2;
    float nmp, nd;
    float s1[2], c1[4];
    float lp, hp, tone;
    float ph3[3], inc3[3];             /* the ta-da's chord */
    int32_t t, gap, len;
    int bursts;
} pv_t;
static pv_t pv[EN_NPAD];
static float metal_ph[6], metal_buf[256];
static const float METAL_F[6] = {205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f};
static float pad_key = 0.0f;           /* the song's key (the ta-da plays in it) */

static float pd_tune(int v) { return fm_exp2f((float)(v - 50) * (1.0f / 50.0f)); }    /* +- an octave */
static float pd_decay(int v) { return fm_exp2f((float)(v - 50) * (1.0f / 25.0f)); }   /* x0.25 .. x4 */

static void pad_fire(int p, float vel)
{
    pv_t *v = &pv[p];
    float tu = pd_tune(en_song.dk[p][0] <= 100 ? en_song.dk[p][0] : 50);
    float dm = pd_decay(en_song.dk[p][1] <= 100 ? en_song.dk[p][1] : 50), dec = PAD_DEC[p] * dm, pan = PAD_PAN[p];
    int k;
    v->on = 1;
    v->live = 0;
    v->g = vel * PAD_GAIN[p];
    v->gl = pan > 0.0f ? 1.0f - pan * 0.7f : 1.0f;
    v->gr = pan < 0.0f ? 1.0f + pan * 0.7f : 1.0f;
    v->t = 0;
    v->amp = 1.0f;
    v->ad = t60(dec);
    v->amp2 = 0.0f;
    v->nmp = 0.0f;
    v->lp = v->hp = 0.0f;
    v->s1[0] = v->s1[1] = 0.0f;
    v->ph = v->ph2 = 0.0f;
    switch (p) {
    case PAD_KICK:                                 /* a sine swept down, its click */
        v->inc_end = 52.0f * tu * (1.0f / EN_SR);
        v->inc = v->inc_end * 2.6f;
        v->pd = fm_expf(-1.0f / (0.012f * EN_SR));
        v->nmp = 0.6f;
        v->nd = t60(0.006f);
        break;
    case PAD_SNARE:
        v->inc_end = 185.0f * tu * (1.0f / EN_SR);
        v->inc = v->inc_end * 1.4f;
        v->pd = fm_expf(-1.0f / (0.012f * EN_SR));
        v->inc2 = 330.0f * tu * (1.0f / EN_SR);
        v->ad = t60(0.16f * dm);
        v->amp2 = 0.7f;
        v->ad2 = t60(0.09f * dm);
        v->nmp = 0.9f;
        v->nd = t60(dec);
        v->c1[0] = onepole(1500.0f);
        v->c1[1] = onepole(9000.0f);
        break;
    case PAD_CLAP:
        v->bursts = 3;
        v->gap = (int32_t)(0.0105f * EN_SR);
        v->nd = t60(0.012f);
        svf_coefs(v->c1, 1150.0f * tu, 0.9f);
        break;
    case PAD_HAT:
        if (vel > 0.9f)                            /* an accent: open */
            v->ad = t60(0.42f * dm);
        svf_coefs(v->c1, 7600.0f * tu, 0.85f);
        v->c1[3] = onepole(5600.0f);               /* (its high-pass, kept in the spare coefficient) */
        break;
    case PAD_TOM:
        v->inc_end = 120.0f * tu * (1.0f / EN_SR);
        v->inc = v->inc_end * 1.3f;
        v->pd = fm_expf(-1.0f / (0.05f * EN_SR));
        break;
    case PAD_COWBELL:
        v->inc = 540.0f * tu * (1.0f / EN_SR);
        v->inc2 = 800.0f * tu * (1.0f / EN_SR);
        v->amp2 = 1.0f;
        v->ad2 = t60(0.035f);
        svf_coefs(v->c1, 1480.0f * tu, 0.7f);
        break;
    case PAD_BOING:                                /* falls a fifth, wobbling ever faster */
        v->inc = 330.0f * tu * (1.0f / EN_SR);
        v->inc_end = 150.0f * tu * (1.0f / EN_SR);
        v->pd = fm_expf(-1.0f / (0.25f * EN_SR));
        v->inc2 = 9.0f * (1.0f / EN_SR);           /* the wobble */
        break;
    case PAD_LASER:
        v->inc = 3200.0f * tu * (1.0f / EN_SR);
        v->inc_end = 90.0f * (1.0f / EN_SR);
        v->pd = fm_expf(-1.0f / (0.06f * EN_SR));
        break;
    case PAD_POP:
        v->inc = 900.0f * tu * (1.0f / EN_SR);
        v->inc_end = 260.0f * tu * (1.0f / EN_SR);
        v->pd = fm_expf(-1.0f / (0.008f * EN_SR));
        v->nmp = 0.5f;
        v->nd = t60(0.002f);
        break;
    case PAD_WHOOSH:                               /* the band goes up and comes back */
        v->len = (int32_t)(0.7f * dm * EN_SR);
        v->tone = tu;
        v->amp = 0.0f;
        break;
    case PAD_TADA: {                               /* "ta" (short), "da" (long): a major chord in the key */
        static const float IV[3] = {0.0f, 4.0f, 7.0f};
        for (k = 0; k < 3; k++) {
            v->inc3[k] = mtof_(60.0f + pad_key + IV[k]) * tu * (1.0f / EN_SR);
            v->ph3[k] = 0.0f;
        }
        v->gap = (int32_t)(0.16f * EN_SR);
        v->ad = t60(0.12f);
        v->ad2 = t60(1.1f * dm);
        svf_coefs(v->c1, 2600.0f, 1.2f);
        break;
    }
    default: break;
    }
}

static void metal_run(uint32_t n)
{
    float inc[6];
    uint32_t j;
    int i;
    for (i = 0; i < 6; i++)
        inc[i] = METAL_F[i] * (1.0f / EN_SR);
    for (j = 0; j < n; j++) {
        float s = 0.0f;
        for (i = 0; i < 6; i++) {
            float p = metal_ph[i] + inc[i];
            if (p >= 1.0f)
                p -= 1.0f;
            metal_ph[i] = p;
            s += p < 0.5f ? 1.0f : -1.0f;
        }
        metal_buf[j] = s * (1.0f / 6.0f);
    }
}

static void pv_run(int p, uint32_t n, float *outl, float *outr)
{
    pv_t *v = &pv[p];
    uint32_t j;
    float a = v->amp, gl = v->gl * v->g, gr = v->gr * v->g, bp, y;
    switch (p) {
    case PAD_KICK: {
        float inc = v->inc, ie = v->inc_end, pd = v->pd, ph = v->ph, nm = v->nmp;
        for (j = 0; j < n; j++) {
            inc = ie + (inc - ie) * pd;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            y = sine(ph) * a;
            if (nm > 0.001f) {
                y += nm * (0.6f + 0.4f * white());
                nm *= v->nd;
            }
            y = sat(y * 1.3f) * 0.95f;
            a *= v->ad;
            outl[j] += y * gl;
            outr[j] += y * gr;
        }
        v->inc = inc;
        v->ph = ph;
        v->nmp = nm;
        break;
    }
    case PAD_SNARE: {
        float inc = v->inc, ie = v->inc_end, ph = v->ph, ph2 = v->ph2, a2 = v->amp2, nm = v->nmp, lp = v->lp, hl = v->hp;
        float ch = v->c1[0], cl = v->c1[1];
        for (j = 0; j < n; j++) {
            float nzv = white(), hp;
            inc = ie + (inc - ie) * v->pd;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            ph2 += v->inc2;
            if (ph2 >= 1.0f)
                ph2 -= 1.0f;
            y = sine(ph) * a + sine(ph2) * a2;
            lp += cl * (nzv - lp);
            hl += ch * (lp - hl);
            hp = (lp - hl) * 1.4f;
            y = y * 0.8f + hp * nm;
            a *= v->ad;
            a2 *= v->ad2;
            nm *= v->nd;
            outl[j] += y * gl;
            outr[j] += y * gr;
        }
        v->inc = inc;
        v->ph = ph;
        v->ph2 = ph2;
        v->amp2 = a2;
        v->nmp = nm;
        v->lp = lp;
        v->hp = hl;
        break;
    }
    case PAD_CLAP: {
        int32_t t = v->t;
        float e = v->nmp, tail = v->amp;
        for (j = 0; j < n; j++, t++) {
            int burst = (int)(t / v->gap);
            if (burst < v->bursts) {
                if (t % v->gap == 0)
                    e = 1.0f;
                e *= v->nd;
                y = e;
            } else {
                y = tail * 0.75f;
                tail *= v->ad;
            }
            svf(v->s1, v->c1, white(), &bp);
            y *= bp * 2.2f;
            outl[j] += y * gl;
            outr[j] += y * gr;
        }
        v->nmp = e;
        a = tail;
        break;
    }
    case PAD_HAT: {
        float hl = v->hp, k2 = v->c1[3];
        for (j = 0; j < n; j++) {
            float x = metal_buf[j] * 0.6f + white() * 0.4f;
            svf(v->s1, v->c1, x, &bp);
            hl += k2 * (bp - hl);
            y = (bp - hl) * a * 1.9f;
            a *= v->ad;
            outl[j] += y * gl;
            outr[j] += y * gr;
        }
        v->hp = hl;
        break;
    }
    case PAD_TOM: {
        float inc = v->inc, ie = v->inc_end, ph = v->ph;
        for (j = 0; j < n; j++) {
            inc = ie + (inc - ie) * v->pd;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            y = sine(ph) * a;
            a *= v->ad;
            outl[j] += y * gl;
            outr[j] += y * gr;
        }
        v->inc = inc;
        v->ph = ph;
        break;
    }
    case PAD_COWBELL: {
        float ph = v->ph, ph2 = v->ph2, a2 = v->amp2;
        for (j = 0; j < n; j++) {
            float x;
            ph += v->inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            ph2 += v->inc2;
            if (ph2 >= 1.0f)
                ph2 -= 1.0f;
            x = ((ph < 0.5f ? 1.0f : -1.0f) + (ph2 < 0.5f ? 1.0f : -1.0f)) * 0.5f;
            svf(v->s1, v->c1, x, &bp);
            y = bp * (0.35f * a + 0.65f * a2) * 1.5f;
            a *= v->ad;
            a2 *= v->ad2;
            outl[j] += y * gl;
            outr[j] += y * gr;
        }
        v->ph = ph;
        v->ph2 = ph2;
        v->amp2 = a2;
        break;
    }
    case PAD_BOING: {
        float inc = v->inc, ie = v->inc_end, ph = v->ph, wph = v->ph2, wi = v->inc2;
        for (j = 0; j < n; j++) {
            inc = ie + (inc - ie) * v->pd;
            wph += wi;
            if (wph >= 1.0f)
                wph -= 1.0f;
            ph += inc * (1.0f + 0.12f * a * sine(wph));
            if (ph >= 1.0f)
                ph -= 1.0f;
            y = (sine(ph) + 0.3f * sine(wrap1(ph * 2.0f))) * a;
            a *= v->ad;
            outl[j] += y * gl;
            outr[j] += y * gr;
        }
        v->inc = inc;
        v->ph = ph;
        v->ph2 = wph;
        v->inc2 = wi * 1.00002f;                   /* the wobble speeds up as it settles */
        break;
    }
    case PAD_LASER: {
        float inc = v->inc, ie = v->inc_end, ph = v->ph;
        for (j = 0; j < n; j++) {
            inc = ie + (inc - ie) * v->pd;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            y = blep_pulse(ph, inc, 0.3f) * 0.5f * a;
            a *= v->ad;
            outl[j] += y * gl;
            outr[j] += y * gr;
        }
        v->inc = inc;
        v->ph = ph;
        break;
    }
    case PAD_POP: {
        float inc = v->inc, ie = v->inc_end, ph = v->ph, nm = v->nmp;
        for (j = 0; j < n; j++) {
            inc = ie + (inc - ie) * v->pd;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            y = sine(ph) * a + nm;
            nm *= v->nd;
            a *= v->ad;
            outl[j] += y * gl;
            outr[j] += y * gr;
        }
        v->inc = inc;
        v->ph = ph;
        v->nmp = nm;
        break;
    }
    case PAD_WHOOSH: {
        float c[4], u = (float)v->t / (float)v->len, sh;
        if (u > 1.0f)
            u = 1.0f;
        sh = u < 0.5f ? u * 2.0f : 2.0f - u * 2.0f;    /* up, then back */
        svf_coefs(c, (300.0f + 2800.0f * sh * sh) * v->tone, 1.0f);
        a = sh;
        for (j = 0; j < n; j++) {
            svf(v->s1, c, white(), &bp);
            y = bp * a * 1.8f;
            outl[j] += y * (gl * (1.0f - 0.6f * u) + 0.2f);   /* it passes, left to right */
            outr[j] += y * (gr * (0.4f + 0.6f * u) + 0.2f);
        }
        if (v->t >= v->len)
            a = 0.0f;
        break;
    }
    case PAD_TADA: {
        float p0 = v->ph3[0], p1 = v->ph3[1], p2 = v->ph3[2], i0 = v->inc3[0], i1 = v->inc3[1], i2 = v->inc3[2];
        int32_t t = v->t;
        for (j = 0; j < n; j++, t++) {
            float x;
            if (t == v->gap) {                     /* "da" */
                a = 1.0f;
                v->ad = v->ad2;
                i0 *= 2.0f;
                i1 *= 2.0f;
                i2 *= 2.0f;
            }
            p0 = wrap1(p0 + i0);
            p1 = wrap1(p1 + i1);
            p2 = wrap1(p2 + i2);
            x = blep_saw(p0, i0) + blep_saw(p1, i1) + blep_saw(p2, i2);
            y = svf(v->s1, v->c1, x, &bp) * 0.3f * a;
            a *= v->ad;
            outl[j] += y * gl;
            outr[j] += y * gr;
        }
        v->ph3[0] = p0;
        v->ph3[1] = p1;
        v->ph3[2] = p2;
        v->inc3[0] = i0;
        v->inc3[1] = i1;
        v->inc3[2] = i2;
        break;
    }
    default: a = 0.0f; break;
    }
    v->amp = a;
    v->t += (int32_t)n;
    if (p == PAD_WHOOSH ? v->t >= v->len
                        : a < 0.001f && v->amp2 < 0.001f && (p != PAD_SNARE || v->nmp < 0.001f) &&
                              (p != PAD_CLAP || v->t > v->gap * (v->bursts + 1)) && (p != PAD_TADA || v->t > v->gap))
        v->on = 0;
}

/* the pads over n samples: the song's into outl/outr, the child's into livel/liver; 1 if the song's
 * sounded, 2 if the child's (each pair is cleared here only then) */
static int pads_run(uint32_t n, float *outl, float *outr, float *livel, float *liver)
{
    int p, any = 0;
    uint32_t j;
    for (p = 0; p < EN_NPAD; p++)
        if (pv[p].on)
            any |= pv[p].live ? 2 : 1;
    if (!any)
        return 0;
    if (any & 1)
        for (j = 0; j < n; j++)
            outl[j] = outr[j] = 0.0f;
    if (any & 2)
        for (j = 0; j < n; j++)
            livel[j] = liver[j] = 0.0f;
    if (pv[PAD_HAT].on)
        metal_run(n);
    for (p = 0; p < EN_NPAD; p++)
        if (pv[p].on) {
            if (pv[p].live)
                pv_run(p, n, livel, liver);
            else
                pv_run(p, n, outl, outr);
        }
    return any;
}

static void pads_quiet(void)
{
    int p;
    for (p = 0; p < EN_NPAD; p++)
        pv[p].on = 0;
}
