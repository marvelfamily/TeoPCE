#include "card_io.h"
#include "device.h"

u8 device_return_to_menu(void) {
    if (card_io_fault()) return CARD_IO_BLOCKED;
#ifdef PREVIEW
    return 0;
#else
    ed_cmd_reboot();
    return DEVICE_ERR_RETURN;
#endif
}
