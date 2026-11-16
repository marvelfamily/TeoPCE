#ifndef TEO_TRACE_H
#define TEO_TRACE_H
#include "main.h"

#ifdef USB_TRACE
/* Developer build only: host must already be connected and reading USB. */
void trace_boot(u8 stage, u8 code, u16 value, const u8 *status);
#else
/* No argument evaluation, storage, code or USB command in production. */
#define trace_boot(stage, code, value, status) ((void)0)
#endif
#endif
