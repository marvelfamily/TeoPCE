#ifndef TEOPCE_DESCRIPTION_H
#define TEOPCE_DESCRIPTION_H
#include "main.h"
#define DESCRIPTION_READY 0
#define DESCRIPTION_MISSING 1
#define DESCRIPTION_EMPTY 2
#define DESCRIPTION_ERROR 3
#define DESCRIPTION_ERR_FORMAT 0xf4
#define DESCRIPTION_MAX_BYTES 8192
#define DESCRIPTION_WORK_BYTES 544
#define DESCRIPTION_ROWS 6
#define DESCRIPTION_COLUMNS 38
#define DESCRIPTION_WINDOW 234
/* All positions refer to validated, LF-separated, at-most-38-column ASCII. */
typedef struct {
    u16 index,size,offset,line,lines;
    u8 status,error;
} Description;
/* Workspace is at least 544 bytes. Every API closes its file before returning.
 * READY publishes a NUL-terminated visible window of at most 234 bytes.
 * Reload preserves the exact byte/line position after another screen reuses it. */
u8 description_open(Description *state,u16 index,u8 *workspace);
u8 description_read(Description *state,u8 *workspace);
u8 description_move(Description *state,u8 backward,u8 *workspace);
#endif
