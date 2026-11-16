#include "card_io.h"
static unsigned char close_fault;
void card_io_boot(void) { close_fault = 0; }
unsigned char card_io_fault(void) { return close_fault; }
unsigned char card_io_close(unsigned char (*close_fn)(void)) {
    unsigned char result;
    if (close_fault) return CARD_IO_BLOCKED;
    result = close_fn();
    if (result) close_fault = result;
    return result;
}
