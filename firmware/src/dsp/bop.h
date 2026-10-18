/* SPDX-License-Identifier: GPL-3.0-only */
/* BOP-1's instrument: a toy band for children that is also a real subtractive synth.
 *
 * Four layers play a song: BEATS (eleven drum and funny-sound pads), BASS (one voice), TUNE (the friend:
 * the sound the keys play, six voices) and SPARKLE (arpeggios and chords, three voices). The three tuned
 * layers are one voice design: two polyBLEP oscillators whose shape morphs sine > triangle > saw > square >
 * noise, a sub-octave, a state-variable filter (low, band or high pass) with its own envelope, an amp
 * envelope, an LFO to pitch, filter or volume, glide, an arpeggiator, and sends to an echo and a reverb.
 * Every one of those is a number in a patch (PK_*), the same numbers the Lab door shows by their real names.
 *
 * A song is four one-bar parts (A B C D) in a chain, over a four-chord progression that moves a chord a bar.
 * BASS and SPARKLE follow the chords (written as if over C, moved to the chord playing); TUNE does not (the
 * generator writes it for the chord under each bar). The generator (bop_gen.c) makes whole songs from a
 * style and a seed, always the same song for the same seed. "Loop me" (Play) records two bars over it.
 *
 * Safety: an always-on limiter, a volume cap under the MASTER pot (a grown-up setting), the resonance and
 * the highest notes capped where children play (en_safe), fades in at start and on changes.
 *
 * Threads: en_render() runs in the audio interrupt. The song (en_song) is shared: the main loop writes it,
 * the render reads it a byte at a time (and writes it while recording). Everything else is a command,
 * drained at the start of a block; the state marked (UI) is written by the render and only read elsewhere.
 * Float only, no libm: fastmath.h. */
#pragma once
#include <stdint.h>

#define EN_SR 44100.0f
#define EN_NSTEP 16                    /* a part: one bar of sixteenths */
#define EN_NLAYER 4
#define EN_NSYN 3                      /* the tuned layers */
#define EN_NPART 4                     /* parts A B C D */
#define EN_CHAIN_MAX 16
#define EN_NPAD 11                     /* the black keys */
#define EN_NFRIEND 8                   /* the built-in friends (then eight My Friends) */
#define EN_NSTYLE 8
#define EN_NPROG 4
#define EN_NSCALE 8
#define EN_NTRICK 8
#define EN_LOOP 32                     /* Loop me: two bars */

enum { L_BEATS, L_BASS, L_TUNE, L_SPARK };
extern const char *const EN_LAYER_NAME[EN_NLAYER];

/* the pads (black keys, left to right) */
enum { PAD_KICK, PAD_SNARE, PAD_CLAP, PAD_HAT, PAD_TOM, PAD_COWBELL, PAD_BOING, PAD_LASER, PAD_POP, PAD_WHOOSH,
       PAD_TADA };
extern const char *const EN_PAD_NAME[EN_NPAD];

/* --------------------------------------------------------------- a patch --- */
/* every value 0..100 unless said */
enum {
    PK_WAVE1,      /* OSC 1 shape: 0 sine, 25 triangle, 50 saw, 75 square, 100 noise; between: a blend */
    PK_WAVE2,      /* OSC 2 shape */
    PK_TUNE2,      /* OSC 2 in semitones: 0..48 is -24..+24 */
    PK_DETUNE,     /* OSC 2 detuned: 0..50 cents */
    PK_MIX,        /* OSC 1 .. OSC 2 */
    PK_SUB,        /* the sub-octave under OSC 1 */
    PK_CUT,        /* filter cutoff: 40 Hz .. 16 kHz */
    PK_RES,        /* resonance */
    PK_FTYPE,      /* 0 low pass, 1 band pass, 2 high pass */
    PK_FENV,       /* the envelope's push on the cutoff */
    PK_KTRACK,     /* the cutoff follows the note */
    PK_A, PK_D, PK_S, PK_R,
    PK_LWAVE,      /* 0 sine, 1 triangle, 2 square, 3 random */
    PK_LRATE,      /* 0.1 .. 12 Hz */
    PK_LDEPTH,
    PK_LTARGET,    /* 0 pitch (vibrato), 1 filter (wah), 2 volume (tremolo) */
    PK_ARP,        /* 0 off, 1 up, 2 down, 3 up and down, 4 random */
    PK_ARATE,      /* 0 quarters, 1 eighths, 2 sixteenths, 3 thirty-seconds */
    PK_AOCT,       /* 1..3 octaves */
    PK_GLIDE,
    PK_ECHO,       /* sends */
    PK_REVERB,
    PK_CRUSH,      /* robot: fewer bits, a lower rate */
    PK_LEVEL,
    PK_N
};
typedef struct {
    const char *name;                  /* the real name: "Cutoff" */
    const char *kid;                   /* the toy's: "bright" */
    uint8_t lo, hi, def;
} en_pk_t;
const en_pk_t *en_pk_info(int k);
void en_pk_text(int k, int v, char *buf);   /* a value as the screen shows it (12 bytes): "1.2 kHz", "Saw" */
const char *en_wave_name(int v);            /* a shape's name, of the nearest */

typedef struct {
    const char *name;                  /* "Bloop" */
    uint8_t p[PK_N];
    uint16_t colour;                   /* RGB565 */
    int8_t oct;                        /* where it sings (the layers it is put on add their own) */
} en_friend_t;
extern const en_friend_t EN_FRIEND[EN_NFRIEND];

/* ---------------------------------------------------------------- a song --- */
enum { EN_N_ACC = 1, EN_N_CHORD = 2 };  /* a note's flags: accented; a chord (its third and fifth too) */
typedef struct {
    uint8_t note;                      /* MIDI (BASS and SPARKLE: as if over C), 0 = none */
    uint8_t len;                       /* in sixteenths, 1..16 */
    uint8_t fl;
} en_note_t;
typedef struct {
    uint16_t hit[EN_NPAD];             /* the beats: bit s = step s */
    uint16_t acc;                      /* their accents */
    en_note_t syn[EN_NSYN][EN_NSTEP];  /* BASS, TUNE, SPARKLE */
} en_part_t;
typedef struct {
    uint8_t who;                       /* who it is: 0..7 built in, 8..15 My Friends (the picture) */
    uint8_t colour;                    /* My Friends' colour (an index), else 0 */
    int8_t oct;                        /* -2..2 */
    uint8_t mute;
    uint8_t p[PK_N];                   /* the sound */
} en_layer_t;
typedef struct {
    uint8_t note[2], len, pads_lo;     /* Loop me, a step: up to two notes (TUNE), their length */
    uint8_t pads_hi;                   /* ... and the pads hit (bits 0..10 across lo, hi) */
} en_loop_t;
typedef struct {
    uint32_t seed;                     /* what the generator made it from */
    uint8_t style, prog, key, scale;   /* key 0..11 (C..B); scale EN_SCALE_* */
    uint8_t tempo, swing;              /* 50..200; 0..100 */
    uint8_t echo_time, echo_fb;        /* 0..100 */
    uint8_t chain[EN_CHAIN_MAX], chain_n;
    uint8_t loop_on;                   /* Loop me has notes */
    uint8_t dk[EN_NPAD][2];            /* each pad's tune and decay (50: as built) */
    en_layer_t lay[EN_NLAYER];
    en_part_t part[EN_NPART];
    en_loop_t loop[EN_LOOP];
    uint8_t rsv[8];
} en_song_t;
extern en_song_t en_song;
int en_song_valid(const en_song_t *s);

/* scales (white keys play these in Play and Make; Lab may play the keys as they are) */
enum { SC_PENTA, SC_MAJOR, SC_MINPENTA, SC_MINOR, SC_BLUES, SC_DORIAN, SC_WHOLE, SC_CHROMA };
extern const char *const EN_SCALE_NAME[EN_NSCALE];
int en_scale_note(int scale, int key, int i);   /* the i-th note of the scale up from the key's C3 (MIDI) */
int en_scale_len(int scale);
/* styles and progressions */
extern const char *const EN_STYLE_NAME[EN_NSTYLE];   /* Bounce, March, Robot, Space, Lullaby, Dino Stomp, ... */
extern const char *const EN_PROG_NAME[EN_NPROG];     /* happy, sad, brave, sleepy */
void en_prog_chord(int prog, int bar, int *root, int *minor);   /* the chord of bar (0..3): root over C, minor? */
extern const char *const EN_TRICK_NAME[EN_NTRICK];   /* chipmunk, giant, robot, stutter, slow-mo, wah, rise, drop */
enum { TR_NONE, TR_CHIPMUNK, TR_GIANT, TR_ROBOT, TR_STUTTER, TR_SLOWMO, TR_WAH, TR_RISE, TR_DROP };

/* the generator: a whole song from a style and a seed (the same song for the same seed); one layer only
 * ("help me"): the rest of the song as it is */
void en_generate(en_song_t *s, int style, uint32_t seed);
void en_generate_layer(en_song_t *s, int layer, uint32_t seed);
void en_song_empty(en_song_t *s, int style);   /* Make's blank page: the style's sounds, no notes */
void en_layer_friend(en_layer_t *l, int who, const uint8_t *p);   /* put a friend (its sound) on a layer */
uint32_t en_song_hash(const en_song_t *s);      /* (the tests) */

/* ------------------------------------------------------------- settings --- */
/* what every song shares (the grown-ups' page and the safety) */
enum { G_VOLCAP, G_SAFE, G_MIDIOUT, G_MIDICH, G_CLOCK, G_TUNE, G_N };
typedef struct {
    int16_t v[G_N];
} en_glob_t;
extern en_glob_t en_glob;
void en_glob_defaults(void);
int en_glob_ok(int i, int v);         /* a setting's value in its range */

/* ---------------------------------------------------------------- commands --- */
/* (main loop -> render) */
void en_init(void);                    /* the song is the caller's */
void en_play(int on);                  /* the song from its top / stop */
void en_note(int layer, int note, int on);   /* a key (TUNE, BASS, SPARKLE: as heard, unless it follows) */
void en_pad(int pad, int acc);         /* a black key */
enum { REC_OFF, REC_LOOP, REC_LAYER };
void en_rec(int mode, int layer);      /* Loop me (REC_LOOP), or into a layer of the part playing */
void en_trick(int t);                  /* the trick held (TR_*; 0 none) */
void en_part_now(int p);               /* play part p (Make: the part being edited) until the chain is asked for */
void en_chain_play(void);              /* the chain again, from its top at the next bar */
void en_panic(void);
void en_fade_in(void);                 /* the output from silence (a change of door, of song) */
void en_preview(int layer, int note);  /* a short note on a layer (a friend saying hello, a game's) */
enum { EN_CLK_TICK, EN_CLK_START, EN_CLK_CONTINUE, EN_CLK_STOP };
void en_clock(int msg);
/* the safe range: in Play and Make, resonance and the highest notes held back (1), Lab all of it (0) */
extern volatile uint8_t en_safe;
extern volatile uint8_t en_arp_live;   /* Play's sparkle: the keys held run as an arpeggio */
extern volatile uint8_t en_duck;       /* Play: the song's tune makes room while the child plays */

/* the render (audio ISR): n stereo frames, 24-bit in int32; gain Q12 (the MASTER pot) */
void en_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2);   /* from the render: the app sends it */

/* ------------------------------------------------------ state for the screen --- */
extern volatile uint8_t en_playing, en_ext, en_recording;
extern volatile uint8_t en_step;                   /* 0..15 */
extern volatile uint8_t en_tick;                   /* 0..23 in the step */
extern volatile uint8_t en_part_playing;           /* 0..3 */
extern volatile uint8_t en_chain_pos;
extern volatile uint8_t en_bar;                    /* bars since start, mod 256 */
extern volatile uint8_t en_chord_root, en_chord_minor;   /* the chord playing (root as a pitch class, key in) */
extern volatile uint32_t en_steps;                 /* steps since start */
extern volatile uint8_t en_lay_on[EN_NLAYER];      /* each layer's notes, counted (a change: a note began) */
extern volatile uint8_t en_lay_note[EN_NLAYER];    /* its last note (BEATS: the pad) */
extern volatile uint8_t en_pad_on[EN_NPAD];        /* each pad's hits, counted */
extern volatile uint8_t en_tune_next;              /* the song's tune: the note it plays now (play along), 0 none */
extern volatile uint8_t en_live_on;                /* the child's notes, counted */
extern volatile float en_level[EN_NLAYER];         /* loudness now, 0..1 */
extern volatile float en_env[EN_NLAYER];           /* the loudest voice's amp envelope, 0..1 (the friend hops) */
extern volatile float en_lfo[EN_NLAYER];           /* the LFO now, -1..1 */
extern volatile float en_out_peak;                 /* the output after the limiter, 0..1 (tests, meters) */
extern volatile uint8_t en_trick_now;
extern volatile uint32_t en_rec_count;             /* notes written live (tests) */
/* the scope (Lab): the newest voice of a layer, after its oscillators, its filter, its amp; asked for by
 * setting en_scope_req (layer + 1); the render fills en_scope and clears it */
#define EN_SCOPE_N 256
extern volatile uint8_t en_scope_req;
extern float en_scope[3][EN_SCOPE_N];
extern volatile float en_scope_hz;                 /* that voice's pitch */
/* where the render's time goes (the device's SysEx reports it) */
enum { PR_SEQ, PR_DRUMS, PR_BASS, PR_TUNE, PR_SPARK, PR_SENDS, PR_MASTER, PR_OTHER, PR_N };
extern uint32_t (*en_time)(void);
extern volatile uint32_t en_prof[PR_N];
extern volatile uint8_t en_poly_cap;               /* the governor's voices a layer (the TUNE's) */
#ifdef EN_HOST
void en_heavy_song(void);                          /* (the host's bench) */
#endif
#ifdef BOP_BENCH
extern volatile uint32_t en_prof_worst[PR_N + 1];
void bop_bench(int k);                             /* a heavy song (0) or the first style (1), playing */
#endif

/* the pieces the tests and the screen use */
int en_follow(int layer, int note);    /* a written note as it sounds now (chord, key, the caps) */
float en_mtof(float note);
