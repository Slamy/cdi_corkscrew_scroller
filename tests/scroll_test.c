#include "scroll.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char pixels[BUFFER_SIZE];
static unsigned char back_pixels[BUFFER_SIZE];
static unsigned char snapshot[BUFFER_SIZE];
static unsigned char visible_snapshot[VIEW_WIDTH * VIEW_HEIGHT];
static unsigned char map[MAP_WIDTH * MAP_HEIGHT];
static unsigned char tiles[16 * TILE_SIZE * TILE_SIZE];

static void verify(Scroll *s) {
    unsigned int x, y;
    for (y = 0; y < VIEW_HEIGHT; ++y) {
        const unsigned char *line = scroll_line(s, y);
        for (x = 0; x < VIEW_WIDTH; ++x) {
            unsigned int wx = s->camera + x;
            unsigned int tile =
                map[(y / TILE_SIZE) * MAP_WIDTH + wx / TILE_SIZE];
            unsigned int expected =
                tiles[tile * TILE_SIZE * TILE_SIZE +
                      (y % TILE_SIZE) * TILE_SIZE + wx % TILE_SIZE];
            if (line[x] != expected) {
                fprintf(stderr, "camera=%u x=%u y=%u got=%u expected=%u\n",
                        s->camera, x, y, line[x], expected);
                exit(1);
            }
        }
    }
}

int main(void) {
    Scroll s;
    Scroll back;
    unsigned int x, y, i;
    for (i = 0; i < 16; ++i)
        for (y = 0; y < TILE_SIZE; ++y)
            for (x = 0; x < TILE_SIZE; ++x)
                tiles[i * 256 + y * 16 + x] = (i * 7 + x * 3 + y) & 127;
    for (y = 0; y < MAP_HEIGHT; ++y)
        for (x = 0; x < MAP_WIDTH; ++x)
            map[y * MAP_WIDTH + x] = (x + y * 3) % 16;
    scroll_init(&s, pixels, map, tiles);
    for (i = 0; i <= MAP_WIDTH * TILE_SIZE - VIEW_WIDTH; ++i) {
        scroll_move(&s, i);
        verify(&s);
    }
    for (i = MAP_WIDTH * TILE_SIZE - VIEW_WIDTH; i > 0; --i) {
        scroll_move(&s, i - 1);
        verify(&s);
    }
    scroll_move(&s, 1000);
    verify(&s);
    scroll_move(&s, 17);
    verify(&s);
    scroll_move(&s, 65535);
    verify(&s);
    scroll_init(&s, pixels, map, tiles);
    scroll_init(&back, back_pixels, map, tiles);
    for (i = 1; i < 900; ++i) {
        Scroll tmp;
        memcpy(snapshot, s.pixels, BUFFER_SIZE);
        scroll_move_slice(&back, i, 0, VIEW_HEIGHT / 2, 0);
        assert(back.camera != i);
        scroll_move_slice(&back, i, VIEW_HEIGHT / 2, VIEW_HEIGHT, 1);
        assert(memcmp(snapshot, s.pixels, BUFFER_SIZE) == 0);
        verify(&back);
        tmp = s;
        s = back;
        back = tmp;
    }
    for (i = 899; i > 0; --i) {
        Scroll tmp;
        memcpy(snapshot, s.pixels, BUFFER_SIZE);
        scroll_move_slice(&back, i - 1, 0, VIEW_HEIGHT / 2, 0);
        scroll_move_slice(&back, i - 1, VIEW_HEIGHT / 2, VIEW_HEIGHT, 1);
        assert(memcmp(snapshot, s.pixels, BUFFER_SIZE) == 0);
        verify(&back);
        tmp = s;
        s = back;
        back = tmp;
    }
    scroll_init(&s, pixels, map, tiles);
    for (i = 1; i < 900; ++i) {
        for (y = 0; y < VIEW_HEIGHT; ++y)
            memcpy(visible_snapshot + y * VIEW_WIDTH, scroll_line(&s, y),
                   VIEW_WIDTH);
        scroll_move_slice(&s, i, 0, VIEW_HEIGHT / 2, 0);
        scroll_move_slice(&s, i, VIEW_HEIGHT / 2, VIEW_HEIGHT, 0);
        for (y = 0; y < VIEW_HEIGHT; ++y)
            assert(memcmp(visible_snapshot + y * VIEW_WIDTH, scroll_line(&s, y),
                          VIEW_WIDTH) == 0);
        scroll_move_slice(&s, i, 0, 0, 1);
        verify(&s);
    }
    for (i = 899; i > 0; --i) {
        for (y = 0; y < VIEW_HEIGHT; ++y)
            memcpy(visible_snapshot + y * VIEW_WIDTH, scroll_line(&s, y),
                   VIEW_WIDTH);
        scroll_move_slice(&s, i - 1, 0, VIEW_HEIGHT, 0);
        for (y = 0; y < VIEW_HEIGHT; ++y)
            assert(memcmp(visible_snapshot + y * VIEW_WIDTH, scroll_line(&s, y),
                          VIEW_WIDTH) == 0);
        scroll_move_slice(&s, i - 1, 0, 0, 1);
        verify(&s);
    }
    puts("scroll ring: OK");
    return 0;
}
