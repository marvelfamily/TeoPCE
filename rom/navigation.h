#ifndef TEOPCE_NAVIGATION_H
#define TEOPCE_NAVIGATION_H
typedef struct {
    unsigned short count, selected, top;
} Cursor;
unsigned char cursor_move(Cursor *cursor, short delta, unsigned char page_size);
#endif
