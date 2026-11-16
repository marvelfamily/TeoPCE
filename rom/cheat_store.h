#ifndef TEOPCE_CHEAT_STORE_H
#define TEOPCE_CHEAT_STORE_H
#include "catalog.h"
#include "native_bridge.h"
#include "cheat_selection.h"
#define CHEAT_STORE_WORK_BYTES 544
#define CHEAT_STORE_ROW_BYTES 26
#define CHEAT_STORE_READONLY 1
#define CHEAT_STORE_PENDING 2
#define CHEAT_STORE_UNCERTAIN 4
#define CHEAT_STORE_ERR_INPUT 0xe0
#define CHEAT_STORE_ERR_DATA 0xe1
#define CHEAT_STORE_ERR_FOREIGN 0xe2
#define CHEAT_STORE_ERR_PENDING 0xe3
#define CHEAT_STORE_ERR_CLOSE 0xe4
#define CHEAT_STORE_ERR_VERIFY 0xe5
#define CHEAT_STORE_ERR_GENERATION 0xe6
#define CHEAT_STORE_ERR_HELD 0xd4
/* Call init after catalog initialization. Native transitions must reject
 * PENDING|UNCERTAIN; UNCERTAIN requires restart and no more store filesystem IO.
 * READONLY is selected-game ownership state and never grants a write. */
u8 cheat_store_init(u8 *work);
u8 cheat_store_load(u16 index,const CatalogRecord *record,
                    const NativeBridgeTarget *target,u32 package_crc,
                    CheatSelection *observed,u8 *work);
/* Cache rows in desired group insertion/observed order, including disabled
 * members of retained MIXED groups. Row: address LE4,value,enabled,label20NUL.
 * desired.count0,row_count0 reuses the prior complete immutable carrier OFF.
 * TCG handle must already be closed. desired/work stay valid through return. */
u8 cheat_store_apply(u16 index,const CatalogRecord *record,
                     const NativeBridgeTarget *target,u32 package_crc,
                     const CheatSelection *desired,u8 row_count,u8 *work);
u8 cheat_store_recover(u8 explicit_restore,u8 *work);
u8 cheat_store_flags(void);
#define CHEAT_SAVED_UNKNOWN 0
#define CHEAT_SAVED_OFF 1
#define CHEAT_SAVED_ON 2
/* Read-only reminder of the exact game's persisted TeoPCE selection.
 * Not the global EverDrive switch or live IGM state. No library-wide scan. */
u8 cheat_store_saved_status(u16 index,const CatalogRecord *record,u8 *workspace);
/* Called only after explicit launch confirmation; HuCard is unchanged. */
u8 cheat_store_prepare_launch(u16 index,const CatalogRecord *record,u8 *work);
#endif
