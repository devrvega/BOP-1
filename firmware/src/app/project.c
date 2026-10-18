/* SPDX-License-Identifier: GPL-3.0-only */
/* BOP-1 project: the settings in one flash object (OBJ_GLOB), the song being played and made in another
 * (OBJ_WORK: power-on comes back to it), eight kept songs (OBJ_SLOT), My Friends and the sticker book. A song
 * never kept is no song: power-on then makes one (the style's first). Part of the unity build, after app.h. */
glob_t glob;
song_t work;
friends_t myf;
book_t book;
static song_t slot_buf;
static uint32_t dirty;

void project_dirty(int what) { dirty |= (uint32_t)what; }
int project_is_dirty(void) { return dirty != 0; }

static void glob_defaults(void)
{
    memset(&glob, 0, sizeof glob);
    glob.magic = GLOB_MAGIC;
    glob.format = PROJ_FORMAT;
    glob.doors = DOOR_BIT(D_PLAY) | DOOR_BIT(D_MAKE) | DOOR_BIT(D_LAB) | DOOR_BIT(D_GAMES);
    glob.lights = 0;
    glob.bright = 4;
    glob.sleep = 0;
    glob.idle = 1;
    glob.startup = 1;
    glob.door = D_PLAY;
    glob.style = 0;
    glob.seed = 1;
    glob.slot = 0xFF;
    en_glob_defaults();
    glob.glob = en_glob;
}
static int glob_valid(const glob_t *g)
{
    int i;
    if (g->magic != GLOB_MAGIC || g->format != PROJ_FORMAT || !(g->doors & DOOR_BIT(D_PLAY)) || g->doors > 15u ||
        g->lights > 2u || g->bright < 1u || g->bright > 4u || g->sleep > 4u || g->idle > 1u || g->startup > 1u ||
        g->door >= ND || g->style >= EN_NSTYLE || g->piano > 1u || (g->slot >= NSLOT && g->slot != 0xFF))
        return 0;
    for (i = 0; i < G_N; i++)
        if (!en_glob_ok(i, g->glob.v[i]))
            return 0;
    return 1;
}
int song_valid(const song_t *s)
{
    return s->magic == SONG_MAGIC && s->format == PROJ_FORMAT && en_song_valid(&s->s) && s->who < 2 * EN_NFRIEND &&
           s->colour < 16u;
}

void project_defaults(void)              /* the song a first power-on plays: the style's, from its seed */
{
    memset(&work, 0, sizeof work);
    work.magic = SONG_MAGIC;
    work.format = PROJ_FORMAT;
    en_generate(&work.s, glob.style, glob.seed);
    work.who = work.s.lay[L_TUNE].who;
    en_song = work.s;
}

static void book_defaults(void)
{
    memset(&book, 0, sizeof book);
    book.magic = BOOK_MAGIC;
    book.format = PROJ_FORMAT;
}
static void friends_defaults(void)
{
    memset(&myf, 0, sizeof myf);
    myf.magic = FRND_MAGIC;
    myf.format = PROJ_FORMAT;
}

int project_load(void)
{
    int n, rc = 0, i, k;
    dirty = 0;
    n = plat_store_load(OBJ_GLOB, &glob, sizeof glob);
    if (n != (int)sizeof glob || !glob_valid(&glob)) {
        glob_defaults();
        dirty |= PD_GLOB;
        rc = -1;
    }
    en_glob = glob.glob;
    n = plat_store_load(OBJ_WORK, &work, sizeof work);
    if (n != (int)sizeof work || !song_valid(&work)) {
        project_defaults();
        rc = -1;
    } else {
        en_song = work.s;
    }
    n = plat_store_load(OBJ_FRIENDS, &myf, sizeof myf);
    if (n != (int)sizeof myf || myf.magic != FRND_MAGIC || myf.format != PROJ_FORMAT) {
        friends_defaults();
    } else {
        for (i = 0; i < NMYF; i++)
            for (k = 0; k < PK_N; k++)
                if (myf.f[i].used > 1u || myf.f[i].p[k] > en_pk_info(k)->hi || myf.f[i].colour > 15u)
                    myf.f[i].used = 0;
    }
    n = plat_store_load(OBJ_BOOK, &book, sizeof book);
    if (n != (int)sizeof book || book.magic != BOOK_MAGIC || book.format != PROJ_FORMAT)
        book_defaults();
    return rc;
}

int project_save(void)
{
    int rc = 0;
    if (dirty & PD_WORK) {
        work.s = en_song;
        if (plat_store_save(OBJ_WORK, &work, sizeof work))
            rc = -1;
        else
            dirty &= ~PD_WORK;
    }
    if (dirty & PD_FRIENDS) {
        if (plat_store_save(OBJ_FRIENDS, &myf, sizeof myf))
            rc = -1;
        else
            dirty &= ~PD_FRIENDS;
    }
    if (dirty & PD_BOOK) {
        if (plat_store_save(OBJ_BOOK, &book, sizeof book))
            rc = -1;
        else
            dirty &= ~PD_BOOK;
    }
    if (dirty & PD_GLOB) {
        glob.glob = en_glob;
        if (plat_store_save(OBJ_GLOB, &glob, sizeof glob))
            rc = -1;
        else
            dirty &= ~PD_GLOB;
    }
    return rc;
}

int slot_look(int i, uint8_t *who, uint8_t *colour)
{
    if (i < 0 || i >= NSLOT)
        return 0;
    if (plat_store_load(OBJ_SLOT(i), &slot_buf, sizeof slot_buf) != (int)sizeof slot_buf || !song_valid(&slot_buf))
        return 0;
    if (who)
        *who = slot_buf.who;
    if (colour)
        *colour = slot_buf.colour;
    return 1;
}
int slot_used(int i) { return slot_look(i, 0, 0); }

int slot_keep(int i)
{
    if (i < 0 || i >= NSLOT)
        return -1;
    work.s = en_song;
    work.who = en_song.lay[L_TUNE].who;
    work.colour = en_song.lay[L_TUNE].colour;
    if (plat_store_save(OBJ_SLOT(i), &work, sizeof work))
        return -1;
    glob.slot = (uint8_t)i;
    dirty |= PD_GLOB | PD_WORK;
    return 0;
}

int slot_open(int i)
{
    if (!slot_look(i, 0, 0))
        return -1;
    work = slot_buf;
    en_song = work.s;
    glob.slot = (uint8_t)i;
    dirty |= PD_GLOB | PD_WORK;
    return 0;
}

void slot_clear_all(void)
{
    int i;
    static const uint32_t none[2] = {0, 0};
    for (i = 0; i < NSLOT; i++)
        if (slot_used(i))
            plat_store_save(OBJ_SLOT(i), none, sizeof none);
    glob.slot = 0xFF;
    dirty |= PD_GLOB;
}
