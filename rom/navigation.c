#include "navigation.h"
unsigned char cursor_move(Cursor *cursor, short delta, unsigned char page_size) {
    unsigned short before = cursor->selected;
    long next;
    if (!cursor->count) { cursor->selected = cursor->top = 0; return 0; }
    if (!page_size) return 0;
    next = (long)cursor->selected + delta;
    if (next < 0) next = 0;
    if ((unsigned short)next >= cursor->count) next = cursor->count - 1;
    cursor->selected = (unsigned short)next;
    cursor->top = (cursor->selected / page_size) * page_size;
    return before != cursor->selected;
}
