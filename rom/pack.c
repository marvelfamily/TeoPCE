#include "pack.h"
#include <string.h>
unsigned long pack_key(const unsigned char *p) {
    return (unsigned long)p[0] | ((unsigned long)p[1] << 8) |
        ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}
unsigned char pack_header(const unsigned char *data, unsigned long size,
                         unsigned char index, unsigned short *count, unsigned long *signature) {
    unsigned short entries;
    unsigned char version;
    if (!data || !count || !signature || index > 1) return 1;
    version = index ? 1 : 2;
    entries = (unsigned short)data[6] | ((unsigned short)data[7] << 8);
    if (memcmp(data, index ? "TPIX" : "TPCE", 4) || data[4] != version || data[5] ||
        entries > CATALOG_MAX || pack_key(data + 12) ||
        size != 16UL + (unsigned long)entries * (index ? 8 : 256)) return 1;
    *count = entries; *signature = pack_key(data + 8);
    return 0;
}
unsigned char pack_match(const unsigned char *row, unsigned char medium,
                        unsigned char genre, unsigned char letter, unsigned char region) {
    if (row[0] < 1 || row[0] > 3 || row[1] < 1 || row[1] >= GENRE_COUNT ||
        (row[2] != '#' && (row[2] < 'A' || row[2] > 'Z')) ||
        row[3] < 1 || row[3] > 4) return 2;
    if (medium && row[0] != medium) return 0;
    if (genre && row[1] != genre) return 0;
    if (letter && row[2] != letter) return 0;
    if (region && row[3] != region) return 0;
    return 1;
}
