/* SPDX-License-Identifier: GPL-3.0-only */
/* The bench songs (a development build's SysEx 02, the host's bench): the heaviest song the instrument can be
 * asked to play, and the first style's. Included once, by bop.c. */

/* every pad on every step, the tune's six voices held in chords, the sparkle in chords on every step, the
 * bass on sixteenths, the echo and the reverb all the way, each layer's filter wide and resonant */
static void heavy_song(en_song_t *s)
{
    int p, i, k;
    en_generate(s, 0, 1u);
    s->tempo = 160;
    for (p = 0; p < EN_NPART; p++) {
        en_part_t *P = &s->part[p];
        for (k = 0; k < EN_NPAD; k++)
            P->hit[k] = 0xFFFFu;
        P->acc = 0xFFFFu;
        for (i = 0; i < EN_NSTEP; i++) {
            P->syn[0][i].note = 48;
            P->syn[0][i].len = 1;
            P->syn[1][i].note = (uint8_t)(60 + (i % 5) * 2);
            P->syn[1][i].len = 8;
            P->syn[1][i].fl = EN_N_CHORD;
            P->syn[2][i].note = 72;
            P->syn[2][i].len = 2;
            P->syn[2][i].fl = EN_N_CHORD;
        }
    }
    for (k = L_BASS; k <= L_SPARK; k++) {
        en_layer_t *L = &s->lay[k];
        L->p[PK_WAVE1] = 60;                      /* the costliest blends: saw into square */
        L->p[PK_WAVE2] = 60;
        L->p[PK_MIX] = 50;
        L->p[PK_SUB] = 50;
        L->p[PK_RES] = 60;
        L->p[PK_ECHO] = 100;
        L->p[PK_REVERB] = 100;
        L->p[PK_CRUSH] = 30;
        L->p[PK_LDEPTH] = 50;
        L->p[PK_R] = 80;
    }
    s->lay[L_BEATS].p[PK_REVERB] = 100;
    s->lay[L_BEATS].p[PK_ECHO] = 100;
    s->echo_fb = 90;
    s->loop_on = 0;
}

#ifdef BOP_BENCH
void bop_bench(int k)
{
    en_play(0);
    if (k == 0)
        heavy_song(&en_song);
    else
        en_generate(&en_song, 0, 1u);
    en_play(1);
}
#endif

#ifdef EN_HOST
void en_heavy_song(void) { heavy_song(&en_song); }
#endif
