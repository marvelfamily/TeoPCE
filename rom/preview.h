#ifndef TEOPCE_PREVIEW_H
#define TEOPCE_PREVIEW_H
#include "catalog.h"
#include "native_bridge.h"
#define PREVIEW_ART_BAD_STATE 0xe3
#define PREVIEW_YEAR_ERR_INPUT 0xe0
#define PREVIEW_YEAR_ERR_FORMAT 0xe1
#define PREVIEW_YEAR_ERR_IDENTITY 0xe2
/* Internal bounded bank adapter for the shared picture parser. */
u8 preview_art_open(u16 index, u8 which, u16 *size);
u8 preview_art_read(void *buffer, u16 size);
u8 preview_art_close(void);
/* Exact-ROM PREVIEW package stream; no native filesystem or export. */
u8 preview_cheat_open(u16 index, u32 key, NativeBridgeTarget *target, u32 *size);
u8 preview_cheat_read(void *buffer, u16 size);
u8 preview_cheat_seek(u32 offset);
u8 preview_cheat_close(void);
u16 preview_count(void);
u8 preview_record(u16 index, CatalogRecord *record);
u8 preview_index(u16 index, u8 *row);
u8 preview_cheat_coverage(u16 index,u32 key,u16 *available,u16 *held);
u8 preview_year(u16 index, u32 key, u16 *year);
u8 preview_picture(u16 index, u8 which, u8 slot, u8 x, u8 y);
u8 preview_description_open(u16 index,u32 *size);
u8 preview_description_read(void *buffer,u16 size);
u8 preview_description_seek(u32 offset);
u8 preview_description_close(void);
#endif
