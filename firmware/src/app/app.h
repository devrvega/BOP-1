/* SPDX-License-Identifier: GPL-3.0-only */
/* BOP-1 app: what is saved (the settings, the song being played and made, eight kept songs, My Friends and
 * the sticker book) and the app's entry points. Included by the unity build after gfx.c and by the host
 * simulator. */
#pragma once
#include <stdint.h>
#include "plat.h"
#include "../dsp/bop.h"

#ifndef EN_VERSION                       /* a release build passes its own (tools/build.py --release) */
#define EN_VERSION "DEV"
#endif
#define GLOB_MAGIC 0x47314F42u           /* "BO1G" (the other firmwares', at the same place in flash, differ) */
#define SONG_MAGIC 0x53314F42u           /* "BO1S" */
#define FRND_MAGIC 0x46314F42u           /* "BO1F" */
#define BOOK_MAGIC 0x4B314F42u           /* "BO1K" */
#define PROJ_FORMAT 1u

/* the doors */
enum { D_PLAY, D_MAKE, D_LAB, D_GAMES, ND };
#define DOOR_BIT(d) (1u << (d))

/* the settings (OBJ_GLOB): the grown-ups' page, and where things were */
typedef struct {
    uint32_t magic, format;
    uint8_t doors;                       /* the doors shown (Play always) */
    uint8_t lights;                      /* 0 full, 1 calm (no flashes, no chases), 2 off */
    uint8_t bright;                      /* the screen: 1..4 */
    uint8_t sleep;                       /* the sleep timer: 0 off, 1..4 = 5, 10, 20, 30 minutes */
    uint8_t idle;                        /* dim after 2 min untouched, stop the music after 10 (1) */
    uint8_t startup;                     /* music at power-on (1) or quiet */
    uint8_t door;                        /* the door open last */
    uint8_t style;                       /* Play's style */
    uint32_t seed;                       /* ... and its song */
    uint8_t piano;                       /* Lab: the keys as they are (chromatic) */
    uint8_t slot;                        /* the song slot last kept or opened, 0xFF none */
    uint8_t rsv0[2];
    en_glob_t glob;                      /* the volume cap, MIDI, the clock, tune */
    uint8_t rsv[16];
} glob_t;

/* a song (OBJ_WORK, OBJ_SLOT): the engine's, and its picture */
typedef struct {
    uint32_t magic, format;
    en_song_t s;
    uint8_t who, colour;                 /* its picture: a friend, a colour */
    uint8_t rsv[6];
} song_t;

/* My Friends (OBJ_FRIENDS): sounds made in the Lab, each with its look */
#define NMYF 8
typedef struct {
    uint8_t used, colour, eyes, rsv;
    uint8_t p[PK_N];
} myfriend_t;
typedef struct {
    uint32_t magic, format;
    myfriend_t f[NMYF];
} friends_t;

/* the sticker book (OBJ_BOOK): stars from the games, the lessons done, what they unlocked */
#define NGAME 6
#define NLESSON 10
typedef struct {
    uint32_t magic, format;
    uint16_t stars[NGAME];               /* each game's stars */
    uint8_t level[NGAME];                /* how far each game has gone */
    uint16_t lessons;                    /* bit i: lesson i done */
    uint32_t stickers;                   /* bit i: sticker i unlocked */
    uint8_t rsv[16];
} book_t;
_Static_assert(sizeof(song_t) <= PLAT_STORE_MAX && sizeof(friends_t) <= PLAT_STORE_MAX, "a flash object holds them");

extern glob_t glob;
extern song_t work;                      /* the song: work.s is copied to and from en_song */
extern friends_t myf;
extern book_t book;

void project_defaults(void);
int project_load(void);                  /* everything: 0 = it was all there */
int project_save(void);                  /* what changed: 0 = saved */
void project_dirty(int what);            /* PD_* */
int project_is_dirty(void);
int slot_used(int i);                    /* a kept song is in slot i (its picture in *who, *colour) */
int slot_look(int i, uint8_t *who, uint8_t *colour);
int slot_keep(int i);                    /* the song into slot i: 0 = kept */
int slot_open(int i);                    /* slot i becomes the song: 0 = opened */
void slot_clear_all(void);
int song_valid(const song_t *s);
#define PD_GLOB 1u
#define PD_WORK 2u
#define PD_FRIENDS 4u
#define PD_BOOK 8u

void ui_init(void);
void ui_frame(void);
void ui_input_only(void);
int ui_dirty(void);
void ui_say(const char *a, const char *b);
