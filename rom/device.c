#include "device.h"

u8 device_check(const u8 *status) {
    if (!status || status[0] != STATUS_KEY) return DEVICE_ERR_KEY;
    if (status[1] != PROTOCOL_ID) return DEVICE_ERR_PROTOCOL;
    if (status[2] != DEVID_TE_PRO) return DEVICE_ERR_MODEL;
    return 0;
}
