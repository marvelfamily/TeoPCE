#ifndef CARD_IO_H
#define CARD_IO_H
/* The SDK has one handle. An unconfirmed close blocks every card user. */
#define CARD_IO_BLOCKED 0xf6
void card_io_boot(void);
unsigned char card_io_fault(void);
unsigned char card_io_close(unsigned char (*close_fn)(void));
#endif
