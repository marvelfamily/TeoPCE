#ifndef TEOPCE_NATIVE_BRIDGE_H
#define TEOPCE_NATIVE_BRIDGE_H
#include "main.h"
#define NATIVE_BRIDGE_ERR_INPUT 0xe0
#define NATIVE_BRIDGE_ERR_DEVICE 0xe1
#define NATIVE_BRIDGE_ERR_REGISTRY 0xe2
#define NATIVE_BRIDGE_ERR_JOURNAL 0xe3
#define NATIVE_BRIDGE_ERR_ACK 0xe4
#define NATIVE_BRIDGE_ERR_TARGET 0xe5
#define NATIVE_BRIDGE_ERR_MENU 0xe6
#define NATIVE_BRIDGE_ERR_MEMORY 0xe7
#define NATIVE_BRIDGE_ERR_PATCH 0xe8
#define NATIVE_BRIDGE_ERR_START 0xe9
#define NATIVE_BRIDGE_MAX_PATH 127
#define NATIVE_BRIDGE_MAX_TARGET 0x800000UL
#define NATIVE_BRIDGE_ROLLBACK_ATTEMPTED 1
#define NATIVE_BRIDGE_ROLLBACK_HOOK_OK 2
#define NATIVE_BRIDGE_ROLLBACK_SLOT_OK 4
#define NATIVE_BRIDGE_ROLLBACK_GUARD_OK 8
#define NATIVE_BRIDGE_ROLLBACK_GUARDED 16
/* Trusted caller metadata; this module does not create catalog metadata. */
typedef struct { u32 size; u32 crc32; } NativeBridgeTarget;
typedef struct { u8 error; u8 stage; u8 rollback_flags; u32 detail; } NativeBridgeReport;
/* Single-threaded synchronous call. Caller keeps path/expected stable until return.
 * Success transfers control and does not return; every returned value is an error.
 * Non-NULL report is mandatory and receives rollback verification flags. */
u8 native_bridge_run(const char *path, const NativeBridgeTarget *expected,
                     NativeBridgeReport *report);

/* Caller verifies exact BHA Japan disc/BIOS and active rows before use.
 * Mode1=P1; 2=P1,Time; 3=Time,P1; 4=validated cd_guard_packet().
 * Exact final native packet required. */
u8 native_bridge_run_guarded(const char *path,const NativeBridgeTarget *expected,
                            NativeBridgeReport *report,u8 guard);

/* Native master status API: UNKNOWN on unverified read/close or in PREVIEW.
 * Reads once with no other SD handle open. Never changes native settings. */
#define NATIVE_CHEATS_UNKNOWN 0
#define NATIVE_CHEATS_OFF 1
#define NATIVE_CHEATS_ON 2
u8 native_bridge_cheat_state(void);
/* End native master status API. */
#endif
