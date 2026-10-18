/* SPDX-License-Identifier: GPL-3.0-only */
/* BOP-1 on the host: the whole app (UI, project, engine) against a simulated FM-1 driven by a script.
 * Audio goes to a WAV, the screen to PNGs, the lights to text, MIDI out to a log. The device-only
 * parts (HAL, USB, OTA, flash driver) are replaced by plat.h implemented here. (From CRT-4's crt4_host, LCD-4's
 * lcd4_host, IND-1's ind_host, AMB-1's amb_host, X0X's x0x_host.)
 *
 *   bop_host SCRIPT [OUTDIR]
 *
 * Script, one command per line (# comments):
 *   wait MS                      run the device for MS milliseconds
 *   press BTN | release BTN | tap BTN     (BTN: FX SEL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC OCT- OCT+)
 *   key K down|up | tapkey K     (K: 0..26 = F3..G5, or w0..w15 white keys, b0..b10 black keys)
 *   turn ENC N [MS]              (ENC: SELECT ALGO PRESET K1 K2 K3 K4; N detents, MS apart, default 80)
 *   spin ENC N                   N detents at once, exactly N (no acceleration)
 *   master N                     MASTER pot 0..4096
 *   midi B0 B1 B2                incoming USB MIDI message (hex bytes)
 *   wav FILE | wavstop           start / stop recording the output
 *   peakreset                    restart the output peak (expect peak_db_min / peak_db_max)
 *   shot FILE.png                save the screen
 *   leds                         print the lit buttons and keys
 *   expect WHAT VALUE            check state: exit 1 on mismatch; VALUE "<N" / ">N" is a bound
 *                                (WHAT: playing door home picker help gu rec loop_on style seed tempo friend oct
 *                                mute0..3 recorded steps bar part dirty store_writes clocks_out midi_out ext
 *                                pkK (the Tune layer's patch value K) hitP (part 0's pad P steps) noteL.S
 *                                (layer L = 1..3, part shown, step S) slot (the slot open) used (slots kept, a mask)
 *                                safe trick bright asleep dimmed flashes (the most screen flashes in any second)
 *                                ledflashes (the most whole-panel LED flashes in any second) peak_db_max peak_db_min)
 *   until WHAT VALUE [MS]        run until the expectation holds (at most MS, default 20000), then check
 *   reboot                       re-run boot from the simulated flash (persistence test)
 *   storeother                   write settings as another firmware left them (another magic): BOP-1 ignores them
 *   newday                       forget the songs (a first power-on)
 */
#define EN_HOST 1
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L          /* clock_gettime, with -std=c99 on Linux */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#define __attribute__(x)

/* ------------------------------------------------------------ screen --- */
static uint16_t fb[240 * 240];          /* RGB565, byte-swapped as the panel takes it (gfx.c) */
static void lcd_sync(void) {}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    uint16_t sc = (uint16_t)(((c >> 8) & 0xFFu) | ((c & 0xFFu) << 8));
    for (j = y; j < y + h && j < 240u; j++)
        for (i = x; i < x + w && i < 240u; i++)
            fb[j * 240u + i] = sc;
}
static uint32_t blits;
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t i, j;
    blits++;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u)
                fb[(y + j) * 240u + x + i] = px[j * w + i];
}
#include "../firmware/src/gfx.c"


/* ---------------------------------------------------------- platform --- */
#include "../firmware/src/app/plat.h"

static uint32_t now_ms, held_btn, held_keys, master = 2048;
static int32_t enc_acc[NE];
static uint32_t lit_btn, lit_keys;

uint32_t plat_ms(void) { return now_ms; }
uint32_t plat_buttons(void) { return held_btn; }
uint32_t plat_keys(void) { return held_keys; }
int32_t plat_enc(int role)
{
    int32_t v = enc_acc[role];
    enc_acc[role] = 0;
    return v;
}
uint32_t plat_master(void) { return master; }
void plat_leds(uint32_t b, uint32_t k)
{
    lit_btn = b;
    lit_keys = k;
}
static int glow_on = 1;                 /* the device's button glow (plat_glow); the lights printed are the bright ones */
void plat_glow(int on) { glow_on = on; }
void plat_play_red(int red) { (void)red; }      /* the device's PLAY colour: green, red while recording */
static uint32_t dim_b, dim_k;
void plat_leds_dim(uint32_t b, uint32_t k)
{
    dim_b = b;
    dim_k = k;
}
static int bl_level = 4;
void plat_backlight(int level) { bl_level = level; }
uint32_t plat_battery(void) { return 0; }

#define MQ 256
static uint32_t min_q[MQ], mi_w, mi_r;
static FILE *midi_log;
static uint32_t midi_out_count, clock_out_count;
int plat_midi_in(uint32_t *pkt)
{
    if (mi_r == mi_w)
        return 0;
    *pkt = min_q[mi_r++ % MQ];
    return 1;
}
void plat_midi_out(uint32_t pkt)
{
    midi_out_count++;
    if (((pkt >> 8) & 0xFF) == 0xF8) {
        clock_out_count++;
        return;
    }
    if (midi_log)
        fprintf(midi_log, "%u ms: %02X %02X %02X\n", now_ms, (pkt >> 8) & 0xFF, (pkt >> 16) & 0xFF, (pkt >> 24) & 0xFF);
}
/* the engine's MIDI out, as the device's glue sends it (USB-MIDI packet, cable 0) */
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { plat_midi_out(st >> 4 | st << 8 | d1 << 16 | d2 << 24); }

/* flash: one buffer per object, kept across a simulated reboot */
static uint8_t store[OBJ_NOBJ][PLAT_STORE_MAX];
static int store_len[OBJ_NOBJ];
static int store_ready;
static uint32_t store_writes;
static void store_init(void)
{
    int i;
    if (store_ready)
        return;
    for (i = 0; i < OBJ_NOBJ; i++)
        store_len[i] = -1;
    store_ready = 1;
}
int plat_store_load(uint32_t obj, void *dst, uint32_t max)
{
    int n;
    store_init();
    if (obj >= OBJ_NOBJ || (n = store_len[obj]) < 0)
        return -1;
    if ((uint32_t)n > max)
        n = (int)max;
    memcpy(dst, store[obj], (size_t)n);
    return n;
}
int plat_store_save(uint32_t obj, const void *src, uint32_t len)
{
    store_init();
    if (obj >= OBJ_NOBJ || len > PLAT_STORE_MAX)
        return -1;
    memcpy(store[obj], src, len);
    store_len[obj] = (int)len;
    store_writes++;
    return 0;
}

static uint32_t cpu_pct;
uint32_t plat_cpu_pct(void) { return cpu_pct; }
uint32_t plat_xruns(void) { return 0; }

/* -------------------------------------------------------------- app --- */
#include "../firmware/src/app/app.h"
#include "../firmware/src/app/project.c"
#include "../firmware/src/app/ui.c"

/* ---------------------------------------------------------- outputs --- */
static void png_write(const char *path)
{
    /* RGB PNG with stored (uncompressed) deflate blocks: no zlib needed */
    static uint8_t raw[240 * (1 + 240 * 3)];
    FILE *f = fopen(path, "wb");
    uint32_t crc_t[256], i, j, k, n = 0, a = 1, b = 0, pos, len = sizeof raw;
    uint8_t hdr[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (!f) {
        fprintf(stderr, "cannot write %s\n", path);
        return;
    }
    for (i = 0; i < 256; i++) {
        uint32_t c = i;
        for (k = 0; k < 8; k++)
            c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_t[i] = c;
    }
    for (j = 0; j < 240; j++) {
        raw[n++] = 0;
        for (i = 0; i < 240; i++) {
            uint16_t p = fb[j * 240 + i];
            p = (uint16_t)((p >> 8) | (p << 8));
            raw[n++] = (uint8_t)(((p >> 11) & 31) * 255 / 31);
            raw[n++] = (uint8_t)(((p >> 5) & 63) * 255 / 63);
            raw[n++] = (uint8_t)((p & 31) * 255 / 31);
        }
    }
    for (i = 0; i < len; i++) {
        a = (a + raw[i]) % 65521;
        b = (b + a) % 65521;
    }
#define PUT32(v) do { uint8_t q_[4] = {(uint8_t)((v) >> 24), (uint8_t)((v) >> 16), (uint8_t)((v) >> 8), (uint8_t)(v)}; fwrite(q_, 1, 4, f); } while (0)
    fwrite(hdr, 1, 8, f);
    {
        uint8_t ih[17] = {'I', 'H', 'D', 'R', 0, 0, 0, 240, 0, 0, 0, 240, 8, 2, 0, 0, 0};
        uint32_t c = 0xFFFFFFFFu;
        PUT32(13u);
        fwrite(ih, 1, 17, f);
        for (i = 0; i < 17; i++)
            c = crc_t[(c ^ ih[i]) & 255] ^ (c >> 8);
        PUT32(~c);
    }
    {
        uint32_t nblk = (len + 65534) / 65535, zlen = 2 + len + nblk * 5 + 4, c = 0xFFFFFFFFu;
        uint8_t *z = malloc(zlen + 4), *q = z;
        memcpy(q, "IDAT", 4);
        q += 4;
        *q++ = 0x78;
        *q++ = 0x01;
        for (pos = 0; pos < len;) {
            uint32_t m = len - pos > 65535 ? 65535 : len - pos;
            *q++ = pos + m >= len ? 1 : 0;
            *q++ = (uint8_t)m;
            *q++ = (uint8_t)(m >> 8);
            *q++ = (uint8_t)~m;
            *q++ = (uint8_t)(~m >> 8);
            memcpy(q, raw + pos, m);
            q += m;
            pos += m;
        }
        *q++ = (uint8_t)(b >> 8);
        *q++ = (uint8_t)b;
        *q++ = (uint8_t)(a >> 8);
        *q++ = (uint8_t)a;
        PUT32(zlen);
        fwrite(z, 1, zlen + 4, f);
        for (i = 0; i < zlen + 4; i++)
            c = crc_t[(c ^ z[i]) & 255] ^ (c >> 8);
        PUT32(~c);
        free(z);
    }
    {
        uint8_t ie[4] = {'I', 'E', 'N', 'D'};
        uint32_t c = 0xFFFFFFFFu;
        PUT32(0u);
        fwrite(ie, 1, 4, f);
        for (i = 0; i < 4; i++)
            c = crc_t[(c ^ ie[i]) & 255] ^ (c >> 8);
        PUT32(~c);
    }
    fclose(f);
}

static FILE *wav;
static uint32_t wav_frames;
static float peak_out;
static void wav_open(const char *path)
{
    static const uint8_t h[44] = {0};
    wav = fopen(path, "wb");
    if (wav)
        fwrite(h, 1, 44, wav);
    wav_frames = 0;
}
static void wav_close(void)
{
    uint32_t bytes = wav_frames * 4, v;
    if (!wav)
        return;
    fseek(wav, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, wav);
    v = 36 + bytes;
    fwrite(&v, 4, 1, wav);
    fwrite("WAVEfmt ", 1, 8, wav);
    v = 16;
    fwrite(&v, 4, 1, wav);
    {
        uint16_t fmt[2] = {1, 2};
        uint32_t r[2] = {44100, 44100 * 4};
        uint16_t al[2] = {4, 16};
        fwrite(fmt, 2, 2, wav);
        fwrite(r, 4, 2, wav);
        fwrite(al, 2, 2, wav);
    }
    fwrite("data", 1, 4, wav);
    fwrite(&bytes, 4, 1, wav);
    fclose(wav);
    wav = 0;
}

/* --------------------------------------------------------- the device --- */
static double audio_due_ms, render_ns_total, render_budget_ns_total;
static uint32_t ui_due;
static double now_ns(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e9 + (double)t.tv_nsec;
}

static void boot(void)
{
    project_load();
    en_init();
    lcd_fill(0, 0, 240, 240, 0);
    ui_init();
}

/* the photosensitivity meter (WCAG 2.3.1, simplified): each frame the screen's mean relative luminance; a flash
 * is a rise and a fall of 10% or more of full; counted in a sliding second, the most kept. The same for the
 * lights: a flash is more than half of all the LEDs changing at once, there and back. */
static float lum_last, lum_ext;
static int lum_dir;
static uint32_t flash_t[64], flash_n, flash_max, led_t[64], led_n, led_max, leds_last;
static float lum_of(uint16_t p)
{
    float r, g, b;
    p = (uint16_t)((p >> 8) | (p << 8));
    r = (float)((p >> 11) & 31) / 31.0f;
    g = (float)((p >> 5) & 63) / 63.0f;
    b = (float)(p & 31) / 31.0f;
    return 0.2126f * r * r + 0.7152f * g * g + 0.0722f * b * b;   /* (a square for the gamma, near enough) */
}
static void count_in(uint32_t *t, uint32_t *n, uint32_t *mx)
{
    uint32_t i, k = 0;
    t[*n % 64] = now_ms;
    (*n)++;
    for (i = 0; i < 64 && i < *n; i++)
        if (now_ms - t[i] < 1000u)
            k++;
    if (k > *mx)
        *mx = k;
}
static void flash_meter(void)
{
    uint32_t i;
    float l = 0.0f;
    for (i = 0; i < 240u * 240u; i += 7u)
        l += lum_of(fb[i]);
    l /= (float)(240u * 240u / 7u);
    if (lum_dir >= 0 && l > lum_ext)
        lum_ext = l;
    if (lum_dir <= 0 && l < lum_ext)
        lum_ext = l;
    if (lum_dir >= 0 && lum_ext - l >= 0.1f) {         /* it fell from a high: half a flash */
        if (lum_dir > 0)
            count_in(flash_t, &flash_n, &flash_max);
        lum_dir = -1;
        lum_ext = l;
    } else if (lum_dir <= 0 && l - lum_ext >= 0.1f) {
        lum_dir = 1;
        lum_ext = l;
    }
    lum_last = l;
    {
        uint32_t now = lit_btn | lit_keys << 14 | (glow_on ? 0u : 0u), d = now ^ leds_last;
        if ((uint32_t)__builtin_popcount(d) > (14u + 27u) / 2u)
            count_in(led_t, &led_n, &led_max);
        leds_last = now;
    }
}

static void run_ms(uint32_t ms)
{
    static int32_t blk[512];
    uint32_t i, k;
    for (i = 0; i < ms; i++) {
        now_ms++;
        while (audio_due_ms <= (double)now_ms) {      /* 256-frame blocks, as the device renders them */
            double t0 = now_ns(), dt;
            en_render(blk, 256, master);
#ifdef EN_WEB
            web_audio(blk, 256);
#endif
            dt = now_ns() - t0;
            render_ns_total += dt;
            render_budget_ns_total += 256.0 * 1e9 / 44100.0;
            cpu_pct = (uint32_t)(100.0 * dt / (256.0 * 1e9 / 44100.0));
            for (k = 0; k < 256; k++) {
                float l = (float)blk[2 * k] / 8388608.0f, r = (float)blk[2 * k + 1] / 8388608.0f;
                int16_t s[2];
                if (fabsf(l) > peak_out)
                    peak_out = fabsf(l);
                if (fabsf(r) > peak_out)
                    peak_out = fabsf(r);
                s[0] = (int16_t)(l * 32767.0f);
                s[1] = (int16_t)(r * 32767.0f);
                if (wav) {
                    fwrite(s, 2, 2, wav);
                    wav_frames++;
                }
            }
            audio_due_ms += 256.0 * 1000.0 / 44100.0;
        }
        if (now_ms >= ui_due) {
            ui_due = now_ms + 16;
            ui_frame();
            if (ui.frame % 2u == 0u)
                flash_meter();
        } else {
            ui_input_only();
        }
    }
}

/* ---------------------------------------------------------- script --- */
static const char *const BTN_N[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                      "PLAY", "REC", "OCT-", "OCT+"};
static const char *const ENC_N[NE] = {"SELECT", "ALGO", "PRESET", "K1", "K2", "K3", "K4"};

static int find_name(const char *const *t, int n, const char *s)
{
    int i;
    for (i = 0; i < n; i++)
        if (!strcmp(t[i], s))
            return i;
    return -1;
}

static const uint8_t WHITE_K[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t BLACK_K[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
static int key_of(const char *s)
{
    if (s[0] == 'w')
        return WHITE_K[atoi(s + 1) & 15];
    if (s[0] == 'b')
        return BLACK_K[atoi(s + 1) % 11];
    return atoi(s);
}

static int expect_quiet;                        /* until: test without printing */
static int ui_part_shown(void);
static int expect(const char *what, const char *val)
{
    int got, l = 1, st = 0;
    const char *dot = strchr(what, '.');
    if (dot) {                                  /* noteL.S: layer L (1..3), step S */
        l = dot[-1] - '0';
        st = atoi(dot + 1) & 15;
        if (l < 1 || l > 3)
            l = 1;
    }
    if (!strcmp(what, "playing"))
        got = en_playing;
    else if (!strcmp(what, "door"))
        got = ui.door;
    else if (!strcmp(what, "home"))
        got = ui.home;
    else if (!strcmp(what, "picker"))
        got = ui.picker;
    else if (!strcmp(what, "help"))
        got = ui.help;
    else if (!strcmp(what, "gu"))
        got = ui.gu;
    else if (!strcmp(what, "rec"))
        got = en_recording;
    else if (!strcmp(what, "loop_on"))
        got = en_song.loop_on;
    else if (!strcmp(what, "style"))
        got = glob.style;
    else if (!strcmp(what, "seed"))
        got = (int)(glob.seed & 0x7FFFFFFF);
    else if (!strcmp(what, "tempo"))
        got = en_song.tempo;
    else if (!strcmp(what, "friend"))
        got = en_song.lay[L_TUNE].who;
    else if (!strcmp(what, "oct"))
        got = en_song.lay[L_TUNE].oct;
    else if (!strncmp(what, "mute", 4))
        got = en_song.lay[atoi(what + 4) & 3].mute;
    else if (!strcmp(what, "recorded"))
        got = (int)en_rec_count;
    else if (!strcmp(what, "steps"))
        got = (int)en_steps;
    else if (!strcmp(what, "bar"))
        got = en_bar;
    else if (!strcmp(what, "part"))
        got = en_part_playing;
    else if (!strcmp(what, "dirty"))
        got = ui_dirty();
    else if (!strcmp(what, "store_writes"))
        got = (int)store_writes;
    else if (!strcmp(what, "clocks_out"))
        got = (int)clock_out_count;
    else if (!strcmp(what, "ext"))
        got = en_ext;
    else if (!strcmp(what, "midi_out"))
        got = (int)midi_out_count;
    else if (!strcmp(what, "slot"))
        got = glob.slot;
    else if (!strcmp(what, "used")) {
        int i;
        got = 0;
        for (i = 0; i < NSLOT; i++)
            got |= slot_used(i) << i;
    } else if (!strcmp(what, "safe"))
        got = en_safe;
    else if (!strcmp(what, "trick"))
        got = en_trick_now;
    else if (!strcmp(what, "bright"))
        got = bl_level;
    else if (!strcmp(what, "asleep"))
        got = ui.asleep;
    else if (!strcmp(what, "dimmed"))
        got = ui.dimmed;
    else if (!strcmp(what, "flashes"))
        got = (int)flash_max;
    else if (!strcmp(what, "ledflashes"))
        got = (int)led_max;
    else if (!strncmp(what, "pk", 2))
        got = en_song.lay[L_TUNE].p[atoi(what + 2) % PK_N];
    else if (!strncmp(what, "hit", 3))
        got = en_song.part[ui_part_shown()].hit[atoi(what + 3) % EN_NPAD];
    else if (!strncmp(what, "note", 4))
        got = en_song.part[ui_part_shown()].syn[l - 1][st].note;
    else if (!strcmp(what, "peak_db_max") || !strcmp(what, "peak_db_min")) {
        double db = 20.0 * log10(peak_out > 1e-9f ? peak_out : 1e-9f);
        int bad;
        if (val[0] == '<' || val[0] == '>') {     /* (the bound is in the name: a plain number) */
            printf("FAIL expect %s %s: a plain number (the %s it may be)\n", what, val, what + 8);
            return 1;
        }
        bad = what[8] == 'm' && what[9] == 'a' ? db > atof(val) : db < atof(val);
        printf("%s peak %.1f dBFS (%s %s)\n", bad ? "FAIL" : "  ok", db, what + 8, val);
        return bad;
    } else {
        printf("FAIL unknown expect %s\n", what);
        return 1;
    }
    if (val[0] == '<' || val[0] == '>') {
        int lim = (int)strtol(val + 1, 0, 0);
        if (val[0] == '<' ? got >= lim : got <= lim) {
            if (!expect_quiet)
                printf("FAIL expect %s %s, got %d\n", what, val, got);
            return 1;
        }
        if (!expect_quiet)
            printf("  ok %s == %d (%s)\n", what, got, val);
        return 0;
    }
    if (got != (int)strtol(val, 0, 0)) {
        if (!expect_quiet)
            printf("FAIL expect %s == %s, got %d\n", what, val, got);
        return 1;
    }
    if (!expect_quiet)
        printf("  ok %s == %d\n", what, got);
    return 0;
}
static int quiet_expect(const char *what, const char *val)
{
    int r;
    expect_quiet = 1;
    r = expect(what, val);
    expect_quiet = 0;
    return r;
}

#ifndef EN_WEB
int main(int argc, char **argv)
{
    FILE *sc;
    char line[512], out[400];
    const char *dir = argc > 2 ? argv[2] : ".";
    int lineno = 0, fails = 0;
    if (argc < 2) {
        fprintf(stderr, "usage: bop_host SCRIPT [OUTDIR]\n");
        return 2;
    }
    sc = fopen(argv[1], "r");
    if (!sc) {
        perror(argv[1]);
        return 2;
    }
    snprintf(out, sizeof out, "%s/midi_out.txt", dir);
    midi_log = fopen(out, "w");
    boot();
    while (fgets(line, sizeof line, sc)) {
        char cmd[32] = {0}, a[256] = {0}, b[64] = {0}, c[64] = {0};
        lineno++;
        if (line[0] == '#' || sscanf(line, "%31s %255s %63s %63s", cmd, a, b, c) < 1)
            continue;
        if (!strcmp(cmd, "wait"))
            run_ms((uint32_t)atoi(a));
        else if (!strcmp(cmd, "press") || !strcmp(cmd, "release") || !strcmp(cmd, "tap")) {
            int i = find_name(BTN_N, NB, a);
            if (i < 0) {
                printf("line %d: no button %s\n", lineno, a);
                return 2;
            }
            if (cmd[0] != 'r')
                held_btn |= 1u << i;
            if (cmd[0] == 't')
                run_ms(40);
            if (cmd[0] != 'p')
                held_btn &= ~(1u << i);
            run_ms(cmd[0] == 't' ? 40 : 2);
        } else if (!strcmp(cmd, "key") || !strcmp(cmd, "tapkey")) {
            int k = key_of(a);
            if (cmd[0] == 't') {
                held_keys |= 1u << k;
                run_ms(60);
                held_keys &= ~(1u << k);
                run_ms(30);
            } else {
                if (!strcmp(b, "down"))
                    held_keys |= 1u << k;
                else
                    held_keys &= ~(1u << k);
                run_ms(2);
            }
        } else if (!strcmp(cmd, "strum")) {          /* strum A B [MS]: a finger across the white keys */
            int x = atoi(a), y = atoi(b), gap = c[0] ? atoi(c) : 25, s = y >= x ? 1 : -1;
            for (;; x += s) {
                held_keys |= 1u << WHITE_K[x & 15];
                run_ms((uint32_t)gap);
                held_keys &= ~(1u << WHITE_K[x & 15]);
                if (x == y)
                    break;
            }
            run_ms(2);
        } else if (!strcmp(cmd, "turn")) {
            int e = find_name(ENC_N, NE, a), n = atoi(b), s = n > 0 ? 1 : -1;
            uint32_t gap = c[0] ? (uint32_t)atoi(c) : 80u;
            if (e < 0) {
                printf("line %d: no encoder %s\n", lineno, a);
                return 2;
            }
            for (; n; n -= s) {
                enc_acc[e] += s;
                run_ms(gap);
            }
        } else if (!strcmp(cmd, "spin")) {
            int e = find_name(ENC_N, NE, a);
            if (e < 0) {
                printf("line %d: no encoder %s\n", lineno, a);
                return 2;
            }
            enc_acc[e] += atoi(b);
            accel_off = 1;
            run_ms(1);
            accel_off = 0;
        } else if (!strcmp(cmd, "master"))
            master = (uint32_t)atoi(a);
        else if (!strcmp(cmd, "midi")) {
            uint32_t s = (uint32_t)strtoul(a, 0, 16), d1 = (uint32_t)strtoul(b, 0, 16), d2 = (uint32_t)strtoul(c, 0, 16);
            uint32_t cin = s >= 0xF0 ? 0x0F : s >> 4;
            min_q[mi_w++ % MQ] = cin | s << 8 | d1 << 16 | d2 << 24;
        } else if (!strcmp(cmd, "wav")) {
            snprintf(out, sizeof out, "%s/%s", dir, a);
            wav_open(out);
        } else if (!strcmp(cmd, "wavstop"))
            wav_close();
        else if (!strcmp(cmd, "peakreset"))
            peak_out = 0.0f;
        else if (!strcmp(cmd, "shot")) {
            snprintf(out, sizeof out, "%s/%s", dir, a);
            png_write(out);
        } else if (!strcmp(cmd, "leds")) {
            int i;
            printf("  leds: buttons");
            for (i = 0; i < NB; i++)
                if (lit_btn >> i & 1u)
                    printf(" %s", BTN_N[i]);
            printf(" | white ");
            for (i = 0; i < 16; i++)
                printf("%c", (lit_keys >> WHITE_K[i] & 1u) ? '#' : '.');
            printf(" black ");
            for (i = 0; i < 11; i++)
                printf("%c", (lit_keys >> BLACK_K[i] & 1u) ? '#' : '.');
            printf("\n");
        } else if (!strcmp(cmd, "expect"))
            fails += expect(a, b);
        else if (!strcmp(cmd, "until")) {           /* until WHAT VALUE [MS]: run until it holds, then check */
            uint32_t lim = c[0] ? (uint32_t)atoi(c) : 20000u, t;
            for (t = 0; t < lim && quiet_expect(a, b); t += 10)
                run_ms(10);
            fails += expect(a, b);
        }
        else if (!strcmp(cmd, "storeother")) {       /* another firmware's settings, as they would be in flash */
            static uint8_t o[1200];
            uint32_t m = 0x34544347u, f = 1u;
            memset(o, 0x11, sizeof o);
            memcpy(o, &m, 4);
            memcpy(o + 4, &f, 4);
            plat_store_save(OBJ_GLOB, o, sizeof o);
        } else if (!strcmp(cmd, "newday")) {
            int i;
            for (i = 0; i < OBJ_NOBJ; i++)
                store_len[i] = -1;
        } else if (!strcmp(cmd, "reboot")) {
            memset(&work, 0, sizeof work);
            memset(&glob, 0, sizeof glob);
            boot();
        } else if (!strcmp(cmd, "state")) {
            printf("  state: door %d style %s seed %u tempo %d key %d prog %s friend %d, playing %d bar %d part %d\n", ui.door,
                   EN_STYLE_NAME[glob.style], glob.seed, en_song.tempo, en_song.key, EN_PROG_NAME[en_song.prog],
                   en_song.lay[L_TUNE].who, en_playing, en_bar, en_part_playing);
        } else if (!strcmp(cmd, "echo"))
            printf("%s", line + 5);
        else {
            printf("line %d: unknown command %s\n", lineno, cmd);
            return 2;
        }
    }
    wav_close();
    if (midi_log)
        fclose(midi_log);
    printf("host: %u ms simulated, %u blits, render %.1f%% of real time (host CPU)%s\n", now_ms, blits,
           render_budget_ns_total > 0 ? 100.0 * render_ns_total / render_budget_ns_total : 0.0,
           fails ? ", EXPECTATIONS FAILED" : "");
    return fails ? 1 : 0;
}
#endif /* EN_WEB */
