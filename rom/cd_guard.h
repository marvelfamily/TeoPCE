#ifndef TEOPCE_CD_GUARD_H
#define TEOPCE_CD_GUARD_H
#include "launch.h"
#include "cd_guard_packet.h"
#define CD_GUARD_ERR_SELECTION 0xda
#define CD_GUARD_ERR_IMAGE 0xdb
#define CD_GUARD_ERR_BIOS 0xdc
#define CD_GUARD_ERR_STATE 0xdd
/* Called after committed CD rows have been exported. Read-only; mode zero on error. */
/* NULL until a successful generic preflight; invalidated on every prepare. */
const CdGuardPacket *cd_guard_packet(void);
u8 cd_guard_prepare(const CatalogRecord *record,const NativeBridgeTarget *target,u8 *mode);
/* Validate cached draft rows without file I/O or authorizing a launch packet. */
u8 cd_guard_check_rows(const CatalogRecord *record,const NativeBridgeTarget *target,const u8 *cache,u8 count);
#endif
