#include "scroll.h"

#ifdef CDI_ASM_COLUMNS
extern void column_copy(unsigned char *source, unsigned char *dest,
                        unsigned int count);
#else
static void column_copy(unsigned char *source, unsigned char *dest,
                        unsigned int count) {
    while (count--) {
        *dest = *source;
        source += TILE_SIZE;
        dest += ROW_STRIDE;
    }
}
#endif

/* World coordinates are limited to the level width, so at most four
 * subtractions are needed; avoid a runtime 32-bit division on the 68070. */
static unsigned int ring_position(unsigned int world_x) {
    while (world_x >= RING_WIDTH)
        world_x -= RING_WIDTH;
    return world_x;
}

static void draw_column_rows(Scroll *s, unsigned int world_x,
                             unsigned int first, unsigned int end) {
    unsigned int y = first;
    unsigned int ring_x = ring_position(world_x);
    unsigned int tile_x = world_x >> 4;
    unsigned int pixel_x = world_x & 15;
    while (y < end) {
        unsigned int tile = s->map[((y >> 4) << 7) + tile_x];
        unsigned char *src = s->tiles + (tile << 8) + ((y & 15) << 4) + pixel_x;
        unsigned int count = TILE_SIZE - (y & 15);
        if (count > end - y)
            count = end - y;
        column_copy(src, s->pixels + y * ROW_STRIDE + ring_x, count);
        if (ring_x < VIEW_WIDTH)
            column_copy(src, s->pixels + y * ROW_STRIDE + RING_WIDTH + ring_x,
                        count);
        y += count;
    }
}

static void redraw(Scroll *s, unsigned int camera) {
    unsigned int i;
    for (i = 0; i < VIEW_WIDTH; ++i)
        draw_column_rows(s, camera + i, 0, VIEW_HEIGHT);
}

void scroll_init(Scroll *s, unsigned char *pixels, unsigned char *map,
                 unsigned char *tiles) {
    s->pixels = pixels;
    s->map = map;
    s->tiles = tiles;
    s->camera = 0;
    redraw(s, 0);
}

void scroll_move(Scroll *s, unsigned int camera) {
    scroll_move_slice(s, camera, 0, VIEW_HEIGHT, 1);
}

void scroll_move_slice(Scroll *s, unsigned int camera, unsigned int first,
                       unsigned int end, int finish) {
    unsigned int old = s->camera;
    unsigned int i;
    unsigned int max_camera = MAP_WIDTH * TILE_SIZE - VIEW_WIDTH;
    if (camera > max_camera)
        camera = max_camera;
    if (camera > old && camera - old <= TILE_SIZE) {
        while (old < camera)
            draw_column_rows(s, old++ + VIEW_WIDTH, first, end);
    } else if (camera < old && old - camera <= TILE_SIZE) {
        while (old > camera)
            draw_column_rows(s, --old, first, end);
    } else if (camera != old) {
        for (i = 0; i < VIEW_WIDTH; ++i)
            draw_column_rows(s, camera + i, first, end);
    }
    if (finish)
        s->camera = camera;
}

unsigned char *scroll_line(Scroll *s, unsigned int y) {
    return s->pixels + y * ROW_STRIDE + ring_position(s->camera);
}
