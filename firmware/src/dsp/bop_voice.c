/* SPDX-License-Identifier: GPL-3.0-only */
/* BOP-1's voice: the one design the three tuned layers play (bop.h's patch). Per voice, a sub-block (32
 * samples) at a time and in passes, so that each loop keeps few values alive (the FM-1's floats share its
 * sixteen registers; LCD-4's finding): the oscillators into a buffer, the filter over it, the amp onto the
 * layer. The pitch, the cutoff and the LFO move once a sub-block. One envelope moves the volume and, by
 * Env amount, the filter (what children see: the friend hops as it starts, settles as it ends).
 * Included once, by bop.c. */

#define NV_MAX 6
static const uint8_t LAYER_NV[EN_NSYN] = {1, 6, 3};   /* BASS, TUNE, SPARKLE */
#define SUB 32

typedef struct {
    uint8_t on, gate, note, st;        /* st: 1 attack, 2 decay / sustain, 3 release */
    float pitch, pitch_t;              /* semitones (glides to pitch_t) */
    float ph1, ph2, phs;
    float s[2];                        /* the filter */
    float env, vel;
    int32_t left;                      /* samples of gate left (-1: held until let go) */
    uint32_t age;
} vo_t;

typedef struct {
    vo_t v[NV_MAX];
    float lph, lval, lrnd;             /* the LFO */
    float crush_h;                     /* robot: the sample held, and for how long */
    int crush_n;
    uint32_t ages;
    /* the arpeggiator: the keys held, where it is */
    uint8_t held[8], held_n, arp_i, arp_dir, arp_note;
    float arp_t;
} ly_t;
static ly_t ly[EN_NSYN];
static float vbuf[SUB], fbuf[SUB];
static float bend_semi, bend_t;        /* the tricks' pitch (chipmunk, giant, slow-mo) */
static volatile int scope_lay = -1;
static int scope_pos, scope_stride = 1;          /* samples taken so far; one kept in stride */
#define SCOPE_FULL (scope_pos >= EN_SCOPE_N * scope_stride)
static void scope_put(int k, const float *b, uint32_t n)
{
    uint32_t j;
    for (j = 0; j < n; j++) {
        int i = (scope_pos + (int)j) / scope_stride;
        if (i < EN_SCOPE_N)
            en_scope[k][i] = b[j];
    }
}
volatile uint8_t en_poly_cap = NV_MAX;

static inline float mtof_(float n) { return 440.0f * fm_exp2f((n - 69.0f) * (1.0f / 12.0f)); }
float en_mtof(float n) { return mtof_(n); }

static const en_layer_t *lay_of(int l) { return &en_song.lay[l + 1]; }
static inline float pk(const en_layer_t *L, int k) { return (float)L->p[k] * 0.01f; }

/* --------------------------------------------------------- the oscillators --- */
/* one pure shape (0 sine, 1 triangle, 2 saw, 3 square, 4 noise) over the sub-block, added to x at gain g */
static float wave_add(float ph, float inc, int i, float *x, float g)
{
    int j;
    switch (i) {
    case 0:
        for (j = 0; j < SUB; j++) {
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            x[j] += g * sine(ph);
        }
        break;
    case 1:
        for (j = 0; j < SUB; j++) {
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            x[j] += g * (fm_fabsf(ph * 4.0f - 2.0f) - 1.0f);
        }
        break;
    case 2:
        for (j = 0; j < SUB; j++) {
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            x[j] += g * blep_saw(ph, inc);
        }
        break;
    case 3:
        for (j = 0; j < SUB; j++) {
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            x[j] += g * 0.5f * blep_pulse(ph, inc, 0.5f);
        }
        break;
    default:
        for (j = 0; j < SUB; j++)
            x[j] += g * white();
        ph = wrap1(ph + inc * (float)SUB);
        break;
    }
    return ph;
}

/* one oscillator over the sub-block, its shape w (0..100) between two neighbours, added to x at gain g;
 * silent, it only keeps its phase; on a shape (or nearly), one wave only */
static float osc_add(float ph, float inc, int w, float *x, float g)
{
    float t = (float)w * 0.04f, f;
    int i = (int)t, j;
    if (g < 1e-4f)
        return wrap1(ph + inc * (float)SUB);
    if (i > 3)
        return wave_add(ph, inc, 4, x, g);
    f = t - (float)i;
    if (f < 0.02f)
        return wave_add(ph, inc, i, x, g);
    if (f > 0.98f)
        return wave_add(ph, inc, i + 1, x, g);
    {
        float ga = g * (1.0f - f), gb = g * f;
        switch (i) {
        case 0:                                   /* sine .. triangle */
            for (j = 0; j < SUB; j++) {
                ph += inc;
                if (ph >= 1.0f)
                    ph -= 1.0f;
                x[j] += ga * sine(ph) + gb * (fm_fabsf(ph * 4.0f - 2.0f) - 1.0f);
            }
            break;
        case 1:                                   /* triangle .. saw */
            for (j = 0; j < SUB; j++) {
                ph += inc;
                if (ph >= 1.0f)
                    ph -= 1.0f;
                x[j] += ga * (fm_fabsf(ph * 4.0f - 2.0f) - 1.0f) + gb * blep_saw(ph, inc);
            }
            break;
        case 2:                                   /* saw .. square */
            for (j = 0; j < SUB; j++) {
                float s;
                ph += inc;
                if (ph >= 1.0f)
                    ph -= 1.0f;
                s = blep_saw(ph, inc);
                x[j] += ga * s + gb * 0.5f * (s - blep_saw(wrap1(ph + 0.5f), inc));
            }
            break;
        default:                                  /* square .. noise */
            for (j = 0; j < SUB; j++) {
                ph += inc;
                if (ph >= 1.0f)
                    ph -= 1.0f;
                x[j] += ga * 0.5f * blep_pulse(ph, inc, 0.5f) + gb * white();
            }
            break;
        }
    }
    return ph;
}

/* --------------------------------------------------------------- a voice --- */
static inline float powi(float k, uint32_t n)     /* k to the n (n <= 32: by squaring) */
{
    float r = 1.0f;
    while (n) {
        if (n & 1u)
            r *= k;
        k *= k;
        n >>= 1;
    }
    return r;
}
static float att_k(const en_layer_t *L) { return 1.0f - fm_expf(-1.0f / (exp01(L->p[PK_A], 0.001f, 2.5f) * EN_SR * 0.35f)); }
static float dec_k(const en_layer_t *L) { return t60(exp01(L->p[PK_D], 0.02f, 4.0f)); }
static float rel_k(const en_layer_t *L) { return t60(exp01(L->p[PK_R], 0.015f, 5.0f)); }

/* the voice over m samples (any number: sub-blocks of SUB, the last shorter when a tick cuts the chunk),
 * onto out; its pieces for the scope */
static void vo_run(int l, vo_t *v, float *out, uint32_t m, int scope)
{
    const en_layer_t *L = lay_of(l);
    ly_t *Y = &ly[l];
    float ka = att_k(L), kd = dec_k(L), kr = rel_k(L), sus = pk(L, PK_S);
    float mix = pk(L, PK_MIX), sub = pk(L, PK_SUB) * 0.8f, det = (float)L->p[PK_DETUNE] * 0.01f;
    float lv = pk(L, PK_LEVEL), ldep = pk(L, PK_LDEPTH), fenv = pk(L, PK_FENV), kt = pk(L, PK_KTRACK);
    float res = pk(L, PK_RES), glide = (float)L->p[PK_GLIDE];
    int tgt = L->p[PK_LTARGET], ft = L->p[PK_FTYPE], w1 = L->p[PK_WAVE1], w2 = L->p[PK_WAVE2];
    float tune2 = (float)((int)L->p[PK_TUNE2] - 24) + det;
    float gk = glide > 0.0f ? 1.0f - fm_expf(-(float)SUB / (exp01((int)glide, 0.01f, 1.2f) * EN_SR * 0.3f)) : 1.0f;
    uint32_t o;
    if (en_safe)
        res = fm_minf(res, 0.7f);                 /* never a squeal where children play */
    for (o = 0; o < m; o += SUB) {
        uint32_t n = m - o < SUB ? m - o : SUB, j;
        float lfo = Y->lval * ldep, p, inc, inc2, ph1, ph2, c[4], bp, env = v->env, cut, amp, trem;
        v->pitch += (v->pitch_t - v->pitch) * gk;
        p = v->pitch + bend_semi + (tgt == 0 ? lfo * 1.2f : 0.0f);
        inc = mtof_(p) * tunefac * (1.0f / EN_SR);
        /* the oscillators make a whole sub-block (loops of a fixed length are the fast ones); when a tick cuts
         * it short, only n are heard, and the phases go on from the n-th, or the wave would jump ahead */
        for (j = 0; j < SUB; j++)
            vbuf[j] = 0.0f;
        inc2 = fm_minf(inc * fm_exp2f(tune2 * (1.0f / 12.0f)), 0.45f);
        ph1 = v->ph1;
        ph2 = v->ph2;
        v->ph1 = osc_add(ph1, inc, w1, vbuf, (1.0f - mix) * 0.6f);
        v->ph2 = osc_add(ph2, inc2, w2, vbuf, mix * 0.6f);
        if (sub > 0.0f) {                         /* the sub-octave: a square an octave down */
            float ps = v->phs, is = inc * 0.5f;
            for (j = 0; j < SUB; j++) {
                ps += is;
                if (ps >= 1.0f)
                    ps -= 1.0f;
                vbuf[j] += sub * 0.4f * blep_pulse(ps, is, 0.5f);
            }
            v->phs = n < SUB ? wrap1(v->phs + is * (float)n) : ps;
        }
        if (n < SUB) {
            v->ph1 = wrap1(ph1 + inc * (float)n);
            v->ph2 = wrap1(ph2 + inc2 * (float)n);
        }
        if (scope && !SCOPE_FULL)
            scope_put(0, vbuf, n);
        /* the filter: the knob, the envelope, the note, the LFO (in octaves) */
        cut = 40.0f * fm_exp2f(8.64f * pk(L, PK_CUT) + fenv * env * 5.0f + kt * (p - 60.0f) * (1.0f / 12.0f) +
                               (tgt == 1 ? lfo * 2.0f : 0.0f));
        svf_coefs(c, fm_minf(cut, 16000.0f), 2.0f - 1.9f * res);
        if (ft == 0) {
            for (j = 0; j < n; j++)
                fbuf[j] = svf(v->s, c, vbuf[j], &bp);
        } else if (ft == 1) {
            for (j = 0; j < n; j++) {
                svf(v->s, c, vbuf[j], &bp);
                fbuf[j] = bp * 1.4f;
            }
        } else {
            for (j = 0; j < n; j++)
                fbuf[j] = svf_hp(v->s, c, vbuf[j], &bp);
        }
        v->s[0] = fm_flush(v->s[0]);
        v->s[1] = fm_flush(v->s[1]);
        if (scope && !SCOPE_FULL)
            scope_put(1, fbuf, n);
        /* the amp: the envelope where the sub-block ends (its curve, exactly), a straight line to it, the tremolo */
        trem = tgt == 2 ? 1.0f - 0.5f * ldep * (1.0f + Y->lval) * 0.9f : 1.0f;
        amp = v->vel * lv * lv * trem;
        {
            float e1, a0, da;
            if (v->st == 1) {
                e1 = 1.25f - (1.25f - env) * powi(1.0f - ka, n);
                if (e1 >= 1.0f) {
                    e1 = 1.0f;
                    v->st = 2;
                }
            } else if (v->st == 2) {
                e1 = sus + (env - sus) * powi(kd, n);
            } else {
                e1 = env * powi(kr, n);
            }
            a0 = env * amp;
            da = (e1 - env) * amp / (float)n;
            for (j = 0; j < n; j++) {
                fbuf[j] *= a0;
                out[o + j] += fbuf[j];
                a0 += da;
            }
            env = e1;
        }
        if (scope && !SCOPE_FULL) {
            scope_put(2, fbuf, n);
            scope_pos += (int)n;
        }
        v->env = env;
        if (v->left > 0) {
            v->left -= (int32_t)n;
            if (v->left <= 0) {
                v->left = 0;
                v->gate = 0;
                v->st = 3;
            }
        }
        if (v->st == 3 && env < 0.0005f) {
            v->on = 0;
            return;
        }
    }
}

/* ------------------------------------------------------------ the layer --- */
static int cap_note(int n)                        /* the highest notes held back where children play */
{
    while (n < 24)
        n += 12;
    while (n > (en_safe ? 96 : 115))
        n -= 12;
    return n;
}

static vo_t *vo_alloc(int l)
{
    ly_t *Y = &ly[l];
    int nv = LAYER_NV[l], i, best = 0;
    uint32_t oldest = 0xFFFFFFFFu;
    if (l == 1 && nv > en_poly_cap)               /* the governor: the tune first, then the sparkle */
        nv = en_poly_cap;
    if (l == 2 && en_poly_cap < 4)
        nv = en_poly_cap < 3 ? 1 : 2;
    for (i = 0; i < nv; i++)
        if (!Y->v[i].on)
            return &Y->v[i];
    for (i = 0; i < nv; i++) {                    /* the oldest let go, else the oldest */
        uint32_t a = Y->v[i].age + (Y->v[i].gate ? 0x80000000u : 0u);
        if (a < oldest) {
            oldest = a;
            best = i;
        }
    }
    return &Y->v[best];
}

/* a note on layer l (0 BASS, 1 TUNE, 2 SPARKLE), heard as note; gate in samples (-1 until syn_off) */
static void syn_on(int l, int note, float vel, int32_t gate)
{
    ly_t *Y = &ly[l];
    vo_t *v;
    note = cap_note(note);
    if (LAYER_NV[l] == 1 && Y->v[0].on && Y->v[0].gate && lay_of(l)->p[PK_GLIDE] > 0) {
        v = &Y->v[0];                             /* legato: glide there */
        v->pitch_t = (float)note;
    } else {
        int retrig;
        v = vo_alloc(l);
        retrig = v->on;
        v->pitch_t = (float)note;
        if (LAYER_NV[l] == 1 && retrig && lay_of(l)->p[PK_GLIDE] > 0)
            ;                                     /* (the mono layer glides from where it was) */
        else
            v->pitch = (float)note;
        if (!retrig) {
            v->ph1 = v->ph2 = v->phs = 0.0f;
            v->s[0] = v->s[1] = 0.0f;
            v->env = 0.0f;
        }
    }
    v->on = 1;
    v->gate = 1;
    v->st = 1;
    v->note = (uint8_t)note;
    v->vel = vel;
    v->left = gate;
    v->age = ++Y->ages;
}

static void syn_off(int l, int note)
{
    ly_t *Y = &ly[l];
    int i;
    note = cap_note(note);
    for (i = 0; i < NV_MAX; i++)
        if (Y->v[i].on && Y->v[i].gate && Y->v[i].note == note && Y->v[i].left < 0) {
            Y->v[i].gate = 0;
            Y->v[i].st = 3;
        }
}

static void syn_quiet(int l)                      /* let everything go (it fades as it would) */
{
    int i;
    for (i = 0; i < NV_MAX; i++)
        if (ly[l].v[i].on) {
            ly[l].v[i].gate = 0;
            ly[l].v[i].st = 3;
            ly[l].v[i].left = 0;
        }
    ly[l].held_n = 0;
}

static void syn_reset(int l)                      /* all of it from the start: the voices, the LFO, the arpeggiator */
{
    static const ly_t zero;
    ly[l] = zero;
}

/* the arpeggiator on the keys held (the Lab's ARP page, or Play's sparkle on the keys) */
static int arp_on(int l)
{
    const en_layer_t *L = lay_of(l);
    return L->p[PK_ARP] > 0 || (l == 1 && en_arp_live);
}
static void arp_key(int l, int note, int on)
{
    ly_t *Y = &ly[l];
    int i;
    if (on) {
        if (Y->held_n == 0) {
            Y->arp_t = 0.0f;                      /* the first key: at once */
            Y->arp_i = 0;
            Y->arp_dir = 0;
        }
        for (i = 0; i < Y->held_n; i++)
            if (Y->held[i] == note)
                return;
        if (Y->held_n < 8) {
            int j = Y->held_n++;
            while (j > 0 && Y->held[j - 1] > note) {   /* kept in order, low to high */
                Y->held[j] = Y->held[j - 1];
                j--;
            }
            Y->held[j] = (uint8_t)note;
        }
    } else {
        for (i = 0; i < Y->held_n; i++)
            if (Y->held[i] == note) {
                for (; i + 1 < Y->held_n; i++)
                    Y->held[i] = Y->held[i + 1];
                Y->held_n--;
                break;
            }
    }
}
static void arp_run(int l, uint32_t n, float step_smp)
{
    ly_t *Y = &ly[l];
    const en_layer_t *L = lay_of(l);
    static const float DIV[4] = {4.0f, 2.0f, 1.0f, 0.5f};
    int mode = L->p[PK_ARP] ? L->p[PK_ARP] : 1, octs = L->p[PK_AOCT] < 1 ? 1 : L->p[PK_AOCT] > 3 ? 3 : L->p[PK_AOCT];
    float per = step_smp * DIV[L->p[PK_ARATE] & 3u];
    int total, k, note;
    if (!Y->held_n)
        return;
    if (L->p[PK_ARP] == 0)
        per = step_smp, octs = 2;                 /* Play's sparkle: sixteenths over two octaves */
    Y->arp_t -= (float)n;
    if (Y->arp_t > 0.0f)
        return;
    Y->arp_t += per;
    total = Y->held_n * octs;
    switch (mode) {
    case 2: k = total - 1 - Y->arp_i % total; break;
    case 3: {
        int span = total > 1 ? 2 * total - 2 : 1, x = Y->arp_i % span;
        k = x < total ? x : span - x;
        break;
    }
    case 4: k = (int)(white() * 0.5f * (float)total + 0.5f * (float)total) % total; break;
    default: k = Y->arp_i % total; break;
    }
    Y->arp_i++;
    note = Y->held[k % Y->held_n] + 12 * (k / Y->held_n);
    syn_on(l, note, 0.8f, (int32_t)(per * 0.6f));
    Y->arp_note = (uint8_t)note;
}

/* the live keys of a layer: through the arpeggiator if it runs */
static void syn_key(int l, int note, int on)
{
    if (arp_on(l)) {
        arp_key(l, note, on);
        return;
    }
    if (on)
        syn_on(l, note, 0.85f, -1);
    else
        syn_off(l, note);
}

/* the LFO, a chunk at a time */
static void lfo_run(int l, uint32_t n)
{
    const en_layer_t *L = lay_of(l);
    ly_t *Y = &ly[l];
    float rate = exp01(L->p[PK_LRATE], 0.1f, 12.0f), ph = Y->lph + rate * (float)n * (1.0f / EN_SR);
    int w = L->p[PK_LWAVE];
    if (ph >= 1.0f) {
        ph -= (float)(int)ph;
        Y->lrnd = white();
    }
    Y->lph = ph;
    switch (w) {
    case 1: Y->lval = fm_fabsf(ph * 4.0f - 2.0f) - 1.0f; break;
    case 2: Y->lval = ph < 0.5f ? 1.0f : -1.0f; break;
    case 3: Y->lval += (Y->lrnd - Y->lval) * 0.2f; break;
    default: Y->lval = sine(ph); break;
    }
}

/* layer l over n samples into x (mono, cleared here); 0 if nothing sounds */
static int syn_run(int l, uint32_t n, float *x, float step_smp)
{
    ly_t *Y = &ly[l];
    int i, any = 0, sc = -1;
    float e = 0.0f;
    uint32_t j;
    lfo_run(l, n);
    en_lfo[l + 1] = Y->lval;
    if (arp_on(l))
        arp_run(l, n, step_smp);
    for (j = 0; j < n; j++)
        x[j] = 0.0f;
    {
        uint32_t newest = 0;                      /* the scope follows the newest voice */
        for (i = 0; i < NV_MAX; i++)
            if (Y->v[i].on && Y->v[i].age >= newest) {
                newest = Y->v[i].age;
                sc = i;
            }
        if (scope_lay != l)
            sc = -1;
    }
    for (i = 0; i < NV_MAX; i++)
        if (Y->v[i].on) {
            if (i == sc)
                en_scope_hz = mtof_(Y->v[i].pitch);
            vo_run(l, &Y->v[i], x, n, i == sc);
            any = 1;
            e = fm_maxf(e, Y->v[i].env);
        }
    en_env[l + 1] = e;
    if (!any)
        return 0;
    {   /* robot: fewer bits and a lower rate */
        const en_layer_t *L = lay_of(l);
        int cr = L->p[PK_CRUSH];
        if (cr > 0) {
            int hold = 1 + cr / 12;
            float q = 64.0f - 0.58f * (float)cr, h = Y->crush_h;
            int c = Y->crush_n;
            for (j = 0; j < n; j++) {
                if (++c >= hold) {
                    c = 0;
                    h = (float)(int)(x[j] * q) / q;
                }
                x[j] = h;
            }
            Y->crush_h = h;
            Y->crush_n = c;
        }
    }
    return 1;
}
