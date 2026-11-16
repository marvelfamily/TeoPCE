#include "game_meta.h"
#include "card_io.h"
#include "catalog.h"
#include "resource_root.h"
#ifdef PREVIEW
#include "preview.h"
#else
#include <string.h>
#endif

#ifndef PREVIEW
#ifdef __CC65__
#pragma code-name(push,"BNK06")
#pragma rodata-name(push,"BNK06")
#endif
static u16 read16(const u8 *p) {
    return (u16)p[0] | ((u16)p[1] << 8);
}
static u32 read32(const u8 *p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}
static u8 valid_year(u16 year) {
    return year == 0 || (year >= 1970 && year <= 2099);
}
static u8 finish(u8 result) {
    u8 close_error = card_io_close(ed_cmd_file_close);
    return close_error ? close_error : result;
}
static u8 game_meta_year_impl(u16 index, u32 key, u16 *year) {
    char path[] = TEO_METADATA_ROOT "/years.bin";
    u8 header[16], row[8], result;
    u16 count, row_year;
    u32 generation, size;
    if (year) *year = 0;
    if (!year) return GAME_META_ERR_INPUT;
    if (card_io_fault()) return CARD_IO_BLOCKED;
    result = catalog_identity(&count, &generation);
    if (result) return result;
    if (index >= count) return GAME_META_ERR_IDENTITY;
    result = ed_cmd_file_open((u8 *)path, FA_READ);
    if (result == 4) return 0;
    if (result) return result;
    size = ed_cmd_file_available();
    if (size != 16UL + (u32)count * 8UL) {
        result = GAME_META_ERR_FORMAT;
    } else {
        result = ed_cmd_file_read(header, sizeof(header));
        if (!result &&
            (memcmp(header, "TPY1", 4) || header[4] != 1 || header[5] != 8 ||
             read16(header + 6) != count || read32(header + 8) != generation ||
             read32(header + 12) != 0))
            result = GAME_META_ERR_FORMAT;
        if (!result)
            result = ed_cmd_file_set_ptr(16UL + (u32)index * 8UL);
        if (!result)
            result = ed_cmd_file_read(row, sizeof(row));
        if (!result) {
            row_year = read16(row + 4);
            if (read16(row + 6) != 0 || !valid_year(row_year))
                result = GAME_META_ERR_FORMAT;
            else if (read32(row) != key)
                result = GAME_META_ERR_IDENTITY;
            else
                *year = row_year;
        }
    }
    result = finish(result);
    if (result) *year = 0;
    return result;
}
#ifdef __CC65__
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
#endif

u8 game_meta_year(u16 index, u32 key, u16 *year) {
#ifdef PREVIEW
    return preview_year(index, key, year);
#else
#ifdef __CC65__
    u8 bank = sysGetBank();
    u8 result;
    sysSetBank(10);
    result = game_meta_year_impl(index, key, year);
    sysSetBank(bank);
    return result;
#else
    return game_meta_year_impl(index, key, year);
#endif
#endif
}

