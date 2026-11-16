#include "main.h"

u8 probe_check_device(const u8 *status);
u8 probe_begin(u8 mode);
u8 probe_resume(void);

static void show_error(u8 code) {
    gCleanScreen();
    gSetPal(PAL_ER);
    gConsPrintCX((u8 *)"PROBE ERROR");
    gConsPrint((u8 *)"ERROR CODE:");
    gAppendHex8(code);
    gSetPal(PAL_01);
    gConsPrint((u8 *)"CHECK TEST SD FILES; RESTART");
    while (1) gVsync();
}

void main(void) {
    u8 status[4], joy, error;
    sysInit();
    gSetPal(PAL_OK);
    gConsPrintCX((u8 *)"TEO LAUNCH PROBE");
    gSetPal(PAL_01);
    gConsPrint((u8 *)"EMPTY TEST SD ONLY");
    gConsPrint((u8 *)"EXPERIMENT - NO SAVES");
    ed_init();
    ed_cmd_status(status);
    error = probe_check_device(status);
    if (error) show_error(error);
    error = probe_resume();
    if (error) show_error(error);
    gConsPrint((u8 *)"");
    gConsPrint((u8 *)"I  = 240P HUCARD");
    gConsPrint((u8 *)"II = 240P SUPER CD");
    gConsPrint((u8 *)"BIOS REQUIRED FOR CD");
    do { joy = sysJoyWait(); } while (!(joy & (JOY_A | JOY_B)));
    if (joy & JOY_B) error = probe_begin(1);
    else error = probe_begin(2);
    if (error) show_error(error);
    show_error(0xee);
}
