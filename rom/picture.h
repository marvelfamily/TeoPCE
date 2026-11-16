#ifndef TEOPCE_PICTURE_H
#define TEOPCE_PICTURE_H

#define PICTURE_BAD_INPUT 0xe0
#define PICTURE_BAD_SIZE  0xe1
#define PICTURE_BAD_FORMAT 0xe2

/*
 * Read a 96x72 image from teo-menu2/art/NNNNNN-{c,s}.bin.
 * Legacy3488 or TPA2/3550 art. which: 0 cover, 1 screenshot.
 * slot: 0..5 selects independent VRAM and palette pair4+2*slot.
 * Position is measured in 8x8 tiles within the 40x28 visible screen.
 *
 * Returns zero on success or a validation/cartridge error. On a read error,
 * palette and tile data can be partial; the caller must clear the image area.
 * Tilemap cells are published only after all reads and file close succeed.
 */
unsigned char picture_draw(unsigned short index, unsigned char which,
                           unsigned char slot, unsigned char x,
                           unsigned char y);

#endif
