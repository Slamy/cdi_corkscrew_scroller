/* Minimal CD-i display experiment. The map is generated until a level is
 * supplied. */
#include "scroll.h"
#include <cdfm.h>
#include <csd.h>
#include <memory.h>
#include <stdio.h>
#include <sysio.h>
#include <ucm.h>

#define LINES 280
#define LCT_LINES (LINES * 2 + 2)
#define FCT_SIZE 160
#define SIG_BLANK 0x100
#define SIG_MUSIC 0x200
/* Set to 1 for a serial report of spare sprite clear/draw pairs per frame. */
#ifndef PROFILE_SPRITES
#define PROFILE_SPRITES 0
#endif
#ifndef AUDIO_DEBUG
#define AUDIO_DEBUG 0
#endif

static int path, fct_a, fct_b, lct_a, lct_b[2];
/* lctAddress() points to the hardware table starting at its line 1.
 * Like Nobelia's direct access, each visible line is 16 longwords apart. */
static unsigned int *lct_a_address, *lct_b_address[2];
int frame_ready;
int music_finished;
static int music_file = -1;
static int audio_path = -1;
static PCB music_pcb;
static unsigned int late_updates;
static unsigned char *screen, *blank;
static unsigned char *sprite_screens[2], *sprite_blank;
static unsigned char map[MAP_WIDTH * MAP_HEIGHT];
static unsigned char tiles[16 * TILE_SIZE * TILE_SIZE];
static Scroll scroll;
#if PROFILE_SPRITES == 1
static unsigned char *profile_scratch;
static unsigned int profile_count, profile_sum, profile_min, profile_max,
    profile_late;
#elif PROFILE_SPRITES == 2
static unsigned int profile_count, profile_late;
#endif

#define SPRITES 32
#define SPRITE_SIZE 16
static int sprite_y[2][SPRITES];
static int sprite_initialized[2];
static unsigned int sprite_frame;

/* The assembly routines take a 16-pixel-aligned foreground address. */
extern void sprite_clear(unsigned char *dst);
extern void sprite_draw(unsigned char *dst, unsigned int packed_color);
extern void sprite_move(unsigned char *old, unsigned char *next, int delta,
                        unsigned int packed_color);

static void animate_sprites(int buffer) {
    static unsigned char jump[32] = {0,  3,  6,  9,  12, 15, 18, 20, 22, 24, 25,
                                     26, 27, 28, 28, 28, 28, 28, 27, 26, 25, 24,
                                     22, 20, 18, 15, 12, 9,  6,  3,  0,  0};
    int i, next_y, delta;
    unsigned int x, color;
    static unsigned int colors[3] = {0x01010101L, 0x02020202L, 0x03030303L};
    unsigned int color_index = 0;
    unsigned char *surface = sprite_screens[buffer];
    for (i = 0; i < SPRITES; ++i) {
        x = 16 + (i & 7) * 44;
        color = colors[color_index];
        if (++color_index == 3)
            color_index = 0;
        next_y = 76 + (i >> 3) * 44 - jump[(sprite_frame + i * 3) & 31];
        delta = next_y - sprite_y[buffer][i];
        if (!sprite_initialized[buffer])
            sprite_draw(surface + next_y * VIEW_WIDTH + x, color);
        else if (delta > -10 && delta < 10) {
            if (delta)
                sprite_move(surface + sprite_y[buffer][i] * VIEW_WIDTH + x,
                            surface + next_y * VIEW_WIDTH + x, delta, color);
        } else {
            sprite_clear(surface + sprite_y[buffer][i] * VIEW_WIDTH + x);
            sprite_draw(surface + next_y * VIEW_WIDTH + x, color);
        }
        sprite_y[buffer][i] = next_y;
    }
    sprite_initialized[buffer] = 1;
    ++sprite_frame;
}

static unsigned int *lct_address(int plane, int lctid) {
    int dummy, instruction;
    dummy = dc_crlct(path, plane, 2, 0);
    if (dummy < 0)
        return 0;
    if (dc_llnk(path, dummy, 1, lctid, 1) < 0) {
        dc_dllct(path, dummy);
        return 0;
    }
    instruction = dc_rdli(path, dummy, 1, 7);
    dc_dllct(path, dummy);
    if (instruction == -1)
        return 0;
    return (unsigned int *)(instruction & 0x00ffffff);
}

static int signal_handler(int code) {
    if (code == SIG_BLANK)
        frame_ready = 1;
    if (code == SIG_MUSIC)
        music_finished = 1;
    return 0;
}

static int start_music(void) {
    if (music_file >= 0)
        close(music_file);
    music_file = open("MUSIC.RTF", READ_);
    if (music_file < 0) {
        printf("ERROR: cannot open MUSIC.RTF\n");
        return -1;
    }
    memset(&music_pcb, 0, sizeof(music_pcb));
    music_pcb.PCB_Sig = SIG_MUSIC;
    music_pcb.PCB_Rec = 1;
    music_pcb.PCB_Chan = 1;
    music_pcb.PCB_AChan = 1;
    music_finished = 0;
    if (ss_play(music_file, &music_pcb) == -1) {
        printf("ERROR: could not start music\n");
        close(music_file);
        music_file = -1;
        return -1;
    }
    return 0;
}

static int init_music(void) {
    char *device = csd_devname(DT_AUDIO, 1);
    if (!device)
        return -1;
    audio_path = open(device, UPDAT_);
    free(device);
    if (audio_path < 0)
        return -1;
    sc_atten(audio_path, 0x00800080);
    return start_music();
}

static void make_level(void) {
    unsigned int t, x, y;
    for (t = 0; t < 16; ++t)
        for (y = 0; y < 16; ++y)
            for (x = 0; x < 16; ++x)
                tiles[t * 256 + y * 16 + x] = (x == 0 || y == 0) ? 1 : 2 + t;
    for (y = 0; y < MAP_HEIGHT; ++y)
        for (x = 0; x < MAP_WIDTH; ++x)
            map[y * MAP_WIDTH + x] = (x + y * 3) & 15;
}

static int display_init(void) {
    char *device = csd_devname(DT_VIDEO, 1);
    unsigned int settings[12];
    unsigned int i;
    if (!device)
        return -1;
    path = open(device, UPDAT_);
    free(device);
    if (path < 0)
        return -1;
    dc_setcmp(path, 0);
    dc_intl(path, 0);
    gc_hide(path);
    fct_a = dc_crfct(path, PA, FCT_SIZE, 0);
    fct_b = dc_crfct(path, PB, FCT_SIZE, 0);
    lct_a = dc_crlct(path, PA, LCT_LINES, 0);
    lct_b[0] = dc_crlct(path, PB, LCT_LINES, 0);
    lct_b[1] = dc_crlct(path, PB, LCT_LINES, 0);
    if (fct_a < 0 || fct_b < 0 || lct_a < 0 || lct_b[0] < 0 || lct_b[1] < 0)
        return -1;
    lct_a_address = lct_address(PA, lct_a);
    lct_b_address[0] = lct_address(PB, lct_b[0]);
    lct_b_address[1] = lct_address(PB, lct_b[1]);
    if (!lct_a_address || !lct_b_address[0] || !lct_b_address[1])
        return -1;
    dc_nop(path, lct_a, 0, 0, LCT_LINES, 8);
    dc_nop(path, lct_b[0], 0, 0, LCT_LINES, 8);
    dc_nop(path, lct_b[1], 0, 0, LCT_LINES, 8);
    dc_flnk(path, fct_a, lct_a, 0);
    dc_flnk(path, fct_b, lct_b[0], 0);
    settings[0] = cp_icm(ICM_CLUT7, ICM_CLUT7, NM_1, EV_OFF, CS_A);
    settings[1] = cp_tci(MIX_OFF, TR_CKEY_T, TR_OFF);
    settings[2] = cp_po(PR_AB);
    settings[3] = cp_bkcol(BK_BLACK, BK_LOW);
    settings[4] = cp_tcol(PA, 0, 0, 0);
    settings[5] = cp_icf(PA, ICF_MAX);
    settings[6] = cp_dprm(RMS_NORMAL, PRF_X2, BP_NORMAL);
    settings[7] = cp_cbnk(0);
    settings[8] = cp_clut(0, 0, 0, 0);
    settings[9] = cp_clut(1, 252, 240, 32);
    settings[10] = cp_clut(2, 252, 80, 80);
    settings[11] = cp_clut(3, 80, 240, 252);
    dc_wrfct(path, fct_a, 0, 12, settings);
    settings[0] = cp_icf(PB, ICF_MAX);
    settings[1] = cp_dprm(RMS_NORMAL, PRF_X2, BP_NORMAL);
    settings[2] = cp_cbnk(2);
    dc_wrfct(path, fct_b, 0, 3, settings);
    for (i = 0; i < 18; ++i)
        dc_wrfi(path, fct_b, 3 + i,
                cp_clut(i, (i * 53) & 252, (i * 91) & 252, (i * 137) & 252));
    dc_wrli(path, lct_a, 0, 0, cp_sig());
    dc_exec(path, fct_a, fct_b);
    return 0;
}

static void display_init_lines(void) {
    unsigned int y;
    for (y = 0; y < LINES; ++y)
        if (y < (LINES - VIEW_HEIGHT) / 2 || y >= (LINES + VIEW_HEIGHT) / 2) {
            dc_wrli(path, lct_b[0], y * 2, 0, cp_dadr((unsigned int)blank));
            dc_wrli(path, lct_b[1], y * 2, 0, cp_dadr((unsigned int)blank));
        }
    for (y = 0; y < LINES; ++y)
        if (y < (LINES - VIEW_HEIGHT) / 2 || y >= (LINES + VIEW_HEIGHT) / 2)
            dc_wrli(path, lct_a, y * 2, 1, cp_dadr((unsigned int)sprite_blank));
    dc_wrli(path, lct_a, LINES - VIEW_HEIGHT, 1,
            cp_dadr((unsigned int)sprite_screens[0]));
}

static void display_scroll(int table, Scroll *source) {
    unsigned int y;
    unsigned int *lct = lct_b_address[table] + ((LINES - VIEW_HEIGHT) / 2) * 16;
    unsigned char *line = scroll_line(source, 0);
    for (y = 0; y < VIEW_HEIGHT; ++y) {
        *lct = cp_dadr((unsigned int)line);
        lct += 16;
        line += ROW_STRIDE;
    }
}

int main(void) {
    unsigned int camera = 0;
    int lct_prepared = 1;
    int sprite_visible = 0;
    int sprite_prepared = 1;
    int temp;
    int direction = 1;
    screen = (unsigned char *)srqcmem(BUFFER_SIZE, VIDEO2);
    blank = (unsigned char *)srqcmem(VIEW_WIDTH, VIDEO2);
    sprite_screens[0] =
        (unsigned char *)srqcmem(VIEW_WIDTH * VIEW_HEIGHT, VIDEO1);
    sprite_screens[1] =
        (unsigned char *)srqcmem(VIEW_WIDTH * VIEW_HEIGHT, VIDEO1);
    sprite_blank = (unsigned char *)srqcmem(VIEW_WIDTH, VIDEO1);
#if PROFILE_SPRITES == 1
    profile_scratch = (unsigned char *)srqcmem(VIEW_WIDTH * 16, VIDEO1);
    if (!profile_scratch)
        return 1;
    profile_min = 0xffffffff;
#endif
    if (!screen || !blank || !sprite_screens[0] || !sprite_screens[1] ||
        !sprite_blank)
        return 1;
    memset(blank, 0, VIEW_WIDTH);
    memset(sprite_blank, 0, VIEW_WIDTH);
    memset(sprite_screens[0], 0, VIEW_WIDTH * VIEW_HEIGHT);
    memset(sprite_screens[1], 0, VIEW_WIDTH * VIEW_HEIGHT);
    make_level();
    scroll_init(&scroll, screen, map, tiles);
    intercept(signal_handler);
    if (display_init() < 0)
        return 1;
    display_init_lines();
    display_scroll(0, &scroll);
    camera = 1;
    scroll_move(&scroll, camera);
    display_scroll(lct_prepared, &scroll);
    animate_sprites(sprite_prepared);
    if (init_music() < 0)
        printf("WARNING: music unavailable\n");
    for (;;) {
        /* The FCT link is fetched by the display at the next field boundary.
         * Do not write the old LCT until a new line signal confirms that the
         * newly linked table is in use.  Arm after linking so even a missed
         * boundary cannot make a stale signal release the active table. */
#if AUDIO_DEBUG != 2
        dc_flnk(path, fct_b, lct_b[lct_prepared], 0);
#endif
        frame_ready = 0;
        dc_ssig(path, SIG_BLANK, 0);
        while (!frame_ready)
            ;
        frame_ready = 0;
        /* Arm the next line signal while preparing this frame. */
        dc_ssig(path, SIG_BLANK, 0);
#if AUDIO_DEBUG != 2
        lct_prepared ^= 1;
        if (camera == 0)
            direction = 1;
        if (camera == MAP_WIDTH * TILE_SIZE - VIEW_WIDTH)
            direction = -1;
        camera += direction;
        lct_a_address[((LINES - VIEW_HEIGHT) / 2) * 16 + 1] =
            cp_dadr((unsigned int)sprite_screens[sprite_prepared]);
        temp = sprite_visible;
        sprite_visible = sprite_prepared;
        sprite_prepared = temp;
        scroll_move(&scroll, camera);
        display_scroll(lct_prepared, &scroll);
        animate_sprites(sprite_prepared);
#endif
        if (music_finished) {
            printf("Music playback ended, status=%04x\n", music_pcb.PCB_Stat);
            if (start_music() < 0)
                music_finished = 0;
        }
#if AUDIO_DEBUG
        {
            static unsigned int frames;
            if ((++frames & 63) == 0) {
                unsigned short *a = (unsigned short *)0x30280c;
                unsigned short *b = (unsigned short *)0x30320c;
                unsigned int a_sum = 0, b_sum = 0, n;
                for (n = 0; n < 32; ++n) {
                    a_sum = (a_sum << 1) + a[n];
                    b_sum = (b_sum << 1) + b[n];
                }
                printf(
                    "AUDIO frame=%u stat=%04x a=%08x b=%08x "
                    "data=%04x%04x/%04x%04x filters=%04x/%08x/%04x dbuf=%04x\n",
                    frames, music_pcb.PCB_Stat, a_sum, b_sum,
                    *(unsigned short *)0x300000, *(unsigned short *)0x300002,
                    *(unsigned short *)0x300a00, *(unsigned short *)0x300a02,
                    *(unsigned short *)0x303c06, *(unsigned int *)0x303c08,
                    *(unsigned short *)0x303c0c, *(unsigned short *)0x303ffe);
            }
        }
#endif
        if (frame_ready) {
            ++late_updates;
            /* UART output is slow; warn on the first late update and then every
             * 64 late updates rather than making an overload much worse. */
            if (late_updates == 1 || (late_updates & 63) == 0)
                printf(
                    "WARNING: frame update missed refresh (late updates: %u)\n",
                    late_updates);
        }
#if PROFILE_SPRITES == 1
        {
            unsigned int pairs = 0;
            if (frame_ready)
                ++profile_late;
            /* Signal is rearmed above. Count full clear/draw pairs until the
             * next refresh, using a hidden allocation in the same video memory
             * bank. */
            while (!frame_ready) {
                sprite_clear(profile_scratch);
                sprite_draw(profile_scratch, 0x01010101L);
                ++pairs;
            }
            if (pairs < profile_min)
                profile_min = pairs;
            if (pairs > profile_max)
                profile_max = pairs;
            profile_sum += pairs;
            if (++profile_count == 200) {
                /* UART output is slow: resynchronize to a later refresh. */
                printf("SPRITE_SPARE count=%u min=%u avg=%u max=%u late=%u\n",
                       profile_count, profile_min, profile_sum / profile_count,
                       profile_max, profile_late);
                profile_count = 0;
                profile_sum = 0;
                profile_min = 0xffffffff;
                profile_max = 0;
                profile_late = 0;
                while (!frame_ready)
                    ;
                frame_ready = 0;
                dc_ssig(path, SIG_BLANK, 0);
                while (!frame_ready)
                    ;
            }
        }
#elif PROFILE_SPRITES == 2
        if (frame_ready)
            ++profile_late;
        if (++profile_count == 200) {
            printf("FRAME_RATE updates=%u late=%u\n", profile_count,
                   profile_late);
            profile_count = 0;
            profile_late = 0;
        }
#endif
    }
    return 0;
}
