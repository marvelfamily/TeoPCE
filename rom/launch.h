#ifndef TEOPCE_LAUNCH_H
#define TEOPCE_LAUNCH_H
#include "catalog.h"
#include "native_bridge.h"
#define LAUNCH_ERR_UNSUPPORTED 0xd0
#define LAUNCH_ERR_METADATA 0xd1
#define LAUNCH_ERR_IDENTITY 0xd2
#define LAUNCH_ERR_PREVIEW 0xd3
#define LAUNCH_STAGE_IDENTITY 0x20
#define LAUNCH_STAGE_HEADER 0x21
#define LAUNCH_STAGE_RECORD 0x22
#define LAUNCH_STAGE_CLOSE 0x23
/* Success transfers control. Every return is an error, with zero rollback flags
 * for preflight rejection and native bridge phase/rollback evidence otherwise. */
/* Read-only shared metadata guard; target is zero on every failure. */
u8 launch_metadata(u16 catalog_index, const CatalogRecord *record,
                   NativeBridgeTarget *target, NativeBridgeReport *report);
u8 launch_selected(u16 catalog_index, const CatalogRecord *record,
                   NativeBridgeReport *report);
#endif
