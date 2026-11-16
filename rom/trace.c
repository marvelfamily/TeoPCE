#include "trace.h"

#ifdef USB_TRACE
void trace_boot(u8 stage, u8 code, u16 value, const u8 *status) {
    u8 packet[16], i, checksum = 0;
    packet[0] = 'T'; packet[1] = 'E'; packet[2] = 'O'; packet[3] = '1';
    packet[4] = stage; packet[5] = code;
    packet[6] = (u8)value; packet[7] = (u8)(value >> 8);
    for (i = 0; i < 4; ++i) packet[8 + i] = status ? status[i] : 0;
    packet[12] = packet[13] = packet[14] = 0;
    for (i = 0; i < 15; ++i) checksum ^= packet[i];
    packet[15] = checksum;
    ed_cmd_usb_wr(packet, sizeof(packet));
}
#endif
