#ifndef SCROLL_H
#define SCROLL_H

/* Each row contains a 416-pixel ring and a mirrored 384-pixel prefix.
 * The 32 pixels outside the view protect the displayed pixels while drawing. */
#define TILE_SIZE 16
#define VIEW_WIDTH 384
#define VIEW_HEIGHT 272
#define RING_WIDTH (VIEW_WIDTH + 32)
#define ROW_STRIDE (RING_WIDTH + VIEW_WIDTH)
#define BUFFER_SIZE (ROW_STRIDE * VIEW_HEIGHT)
#define MAP_WIDTH 128
#define MAP_HEIGHT (VIEW_HEIGHT / TILE_SIZE)

typedef struct {
    unsigned char *pixels;
    unsigned char *map;
    unsigned char *tiles;
    unsigned int camera;
} Scroll;

void scroll_init(Scroll *s, unsigned char *pixels, unsigned char *map,
                 unsigned char *tiles);
void scroll_move(Scroll *s, unsigned int camera);
void scroll_move_slice(Scroll *s, unsigned int camera, unsigned int first,
                       unsigned int end, int finish);
unsigned char *scroll_line(Scroll *s, unsigned int y);

#endif
