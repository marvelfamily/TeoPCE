#ifndef TEOPCE_CHEAT_STORE_CODEC_H
#define TEOPCE_CHEAT_STORE_CODEC_H
#include "cheat_store.h"
typedef struct {u32 key,size,rom_crc,package_crc,generation;u16 index;} CheatStoreIdentity;
u32 cs_crc32(const u8 *data,u16 size);
u8 cs_profile_valid(const u8 *data,const CheatStoreIdentity *identity);
u8 cs_profile_make(u8 *data,const u8 *prior,const CheatSelection *desired,
                   const CheatStoreIdentity *identity,u16 native_size,u32 native_crc,u8 alloff);
u8 cs_profile_observe(const u8 *data,u16 bits,CheatSelection *observed);
u8 cs_journal_valid(const u8 *data);
u8 cs_journal_make(u8 *data,const CheatStoreIdentity *identity,const u8 *old_profile,u16 old_size,u32 old_crc,u8 present,const u8 *new_profile,const char *native);
void cs_journal_rebuild(u8 *data,const u8 *prefix,const char *native);
u8 cs_native_path(const char *rom,char *native_path);
/* Actual stock basename profile; CD committed carriers use the private path above. */
u8 cs_stock_path(const char *rom,char *native_path);
u8 cs_row_line(const u8 *row,u8 *line);
u8 cs_parse_line(const u8 *line,u8 size,u8 *row);
#endif
