#ifndef TEO_DEVICE_H
#define TEO_DEVICE_H
#include "main.h"

#define DEVICE_ERR_RETURN  0xec
#define DEVICE_ERR_KEY      0xed
#define DEVICE_ERR_PROTOCOL 0xee
#define DEVICE_ERR_MODEL    0xef

/* STATUS bytes 0..2 identify the transport/device. Byte 3 is EDIO status
 * belonging to an operation, not a fourth identity field. */
u8 device_check(const u8 *status);

/* Native success never returns. Preview is a no-op (0); an unexpected native
 * return reports DEVICE_ERR_RETURN so the UI can remain usable. */
u8 device_return_to_menu(void);
#endif
