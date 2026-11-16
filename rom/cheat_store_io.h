#ifndef TEOPCE_CHEAT_STORE_IO_H
#define TEOPCE_CHEAT_STORE_IO_H
#include "cheat_store.h"
#include "cheat_store_codec.h"
u8 cs_io_binding(u16 index,const CatalogRecord *record,const NativeBridgeTarget *target,u32 package_crc,CheatStoreIdentity *identity,char *native);
u8 cs_io_package(const CheatStoreIdentity *identity);
void cs_io_bind(u8 *work,u8 *flags,u8 restart);
u8 cs_io_meta(const char *path,u32 *size,u8 *present,u8 directory);
u8 cs_io_read(const char *path,u32 size,u32 offset,u8 *out,u16 count);
u8 cs_io_crc(const char *path,u32 size,u32 *crc);
u8 cs_io_equal(const char *a,const char *b,u32 size);
u8 cs_io_mem(const char *path,const u8 *data,u16 size);
u8 cs_io_copy(const char *from,const char *to,u32 size,u8 mode);
u8 cs_io_write(const char *path,const u8 *data,u16 size);
u8 cs_io_rows(const char *path,u8 rows,u16 *size,u32 *crc);
u8 cs_io_carrier(const char *path,u16 size,u8 rows);
u8 cs_io_tuples(const char *reference,const char *actual,u16 reference_size,u16 actual_size,u8 rows,u16 *bits);
#endif
