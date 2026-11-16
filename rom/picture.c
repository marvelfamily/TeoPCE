#include "card_io.h"
#include "main.h"
#include "picture.h"
#include "resource_root.h"
#include <string.h>
#ifdef PREVIEW
#include "preview.h"
#define art_read(buffer, size) preview_art_read(buffer, size)
#define art_close() card_io_close(preview_art_close)
#else
#define art_read(buffer, size) ed_cmd_file_read(buffer, size)
#define art_close() card_io_close(ed_cmd_file_close)
#endif

/* One tile row also holds the bounded header/palette prefix. */
static u8 picture_buffer[384];
static u8 picture_bitmap[14];
static const u8 two_header[16] = {
    'T', 'P', 'A', '2', 2, 96, 72, 2, 108, 0, 14, 0, 0, 0, 0, 0
};

/* BNK00 is fixed at $E000. The bounded decoder never switches banks. */
#ifdef __CC65__
#pragma code-name (push, "BNK00")
#endif
static u8 art_open(u16 index, u8 which, u32 *size) {
#ifdef PREVIEW
    u16 length;
    u8 result = preview_art_open(index, which, &length);
    if (!result) *size = length;
    return result;
#else
    char path[] = TEO_RESOURCE_ROOT "/art/000000-c.bin";
    u8 i, result;
    for (i = 0; i < 6; ++i) {
        path[sizeof(path) - 8 - i] = (char)('0' + index % 10);
        index /= 10;
    }
    path[sizeof(path) - 6] = which ? 's' : 'c';
    result = ed_cmd_file_open((u8 *)path, FA_READ);
    if (!result) *size = ed_cmd_file_available();
    return result;
#endif
}

static u8 finish_read(u8 result) {
    u8 close_result = art_close();
    return result ? result : close_result;
}

u8 picture_draw(u16 index, u8 which, u8 slot, u8 x, u8 y) {
    u8 result, i, row, col, palette, color_count;
    u16 tile, at;
    u32 size;

    if (card_io_fault()) return CARD_IO_BLOCKED;
    if (which > 1 || slot >= 6 || x > 28 || y > 19)
        return PICTURE_BAD_INPUT;
    result = art_open(index, which, &size);
    if (result) return result;
    if (size != 3488 && size != 3550)
        return finish_read(PICTURE_BAD_SIZE);

    memset(picture_bitmap, 0, sizeof(picture_bitmap));
    color_count = 16;
    if (size == 3550) {
        result = art_read(picture_buffer, 16);
        if (result) return finish_read(result);
        if (memcmp(picture_buffer, two_header, sizeof(two_header)))
            return finish_read(PICTURE_BAD_FORMAT);
        result = art_read(picture_buffer, 78);
        if (result) return finish_read(result);
        if (picture_buffer[77] & 0xf0)
            return finish_read(PICTURE_BAD_FORMAT);
        memcpy(picture_bitmap, picture_buffer + 64, sizeof(picture_bitmap));
        color_count = 32;
    } else {
        result = art_read(picture_buffer, 32);
        if (result) return finish_read(result);
    }
    /* Reject the complete palette before changing any visible hardware state. */
    for (i = 0; i < color_count; ++i)
        if (picture_buffer[i * 2 + 1] & 0xfe)
            return finish_read(PICTURE_BAD_FORMAT);
    /* The validated palette is uploaded as one VBlank-synchronized batch.
     * SD latency must stay outside that short hardware transfer. */
    palette = 4 + slot * 2;
    gLoadPalette((u16)palette * 16, picture_buffer, color_count);

    tile = 512 + (u16)slot * 108;
    for (row = 0; row < 9; ++row) {
        result = art_read(picture_buffer, sizeof(picture_buffer));
        if (result) return finish_read(result);
        gLoadTileData(tile + (u16)row * 12, picture_buffer,
                      sizeof(picture_buffer));
    }
    result = finish_read(0);
    if (result) return result;

    for (row = 0; row < 9; ++row)
        for (col = 0; col < 12; ++col) {
            at = (u16)row * 12 + col;
            gDrawTile(x + col, y + row, tile + at,
                      palette + ((picture_bitmap[at >> 3] >> (at & 7)) & 1));
        }
    return 0;
}

#ifdef __CC65__
#pragma code-name (pop)
#endif
