#ifndef TEOPCE_PACK_H
#define TEOPCE_PACK_H
#define CATALOG_MAX 1200
#define GENRE_COUNT 26
unsigned char pack_header(const unsigned char *data, unsigned long size,
                         unsigned char index, unsigned short *count, unsigned long *signature);
unsigned char pack_match(const unsigned char *row, unsigned char medium,
                        unsigned char genre, unsigned char letter, unsigned char region);
unsigned long pack_key(const unsigned char *p);
#endif
