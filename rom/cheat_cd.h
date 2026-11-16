#ifndef TEOPCE_CHEAT_CD_H
#define TEOPCE_CHEAT_CD_H
#include "cheat_store.h"
/* Private native CD export transaction. Caller owns shared flags/workspace.
 * No game/BIOS/BRAM writes; pending/uncertain flags gate every native launch. */
u8 cs_cd_recover(u8 *work,u8 *flags);
u8 cs_cd_prepare(u16 index,const CatalogRecord *record,u8 *work,u8 *flags);
#endif
