#include "card_io.h"
#include "main.h"
#include "preview.h"
#include "picture.h"
#include <string.h>
#include "preview_asset.h"

u16 preview_count(void) { return PREVIEW_COUNT; }
u8 preview_record(u16 index, CatalogRecord *record) {
    if (card_io_fault()) return CARD_IO_BLOCKED;
    if (index >= PREVIEW_COUNT) return 0xe0;
    memcpy(record, preview_records[index], 256);
    return 0;
}
u8 preview_index(u16 index, u8 *row) {
    const u8 *p;
    if (card_io_fault()) return CARD_IO_BLOCKED;
    if (index >= PREVIEW_COUNT) return 0xe0;
    p = preview_records[index];
    row[0] = p[248]; row[1] = p[253]; row[2] = p[255]; row[3] = p[254];
    memcpy(row + 4, p + 249, 4);
    return 0;
}
u8 preview_cheat_coverage(u16 index,u32 key,u16 *available,u16 *held){
 const u8 *p;
 if(!available||!held)return 0xe0;*available=*held=0;
 if(card_io_fault())return CARD_IO_BLOCKED;
 if(index>=PREVIEW_COUNT)return 0xe0;p=preview_records[index];
 if(((u32)p[249]|((u32)p[250]<<8)|((u32)p[251]<<16)|((u32)p[252]<<24))!=key)return 0xe2;
#ifdef PREVIEW_COVERAGE
 *available=preview_cheat_counts[index][0];*held=preview_cheat_counts[index][1];return 0;
#else
 return 0xe4;
#endif
}
u8 preview_year(u16 index, u32 key, u16 *year) {
    const u8 *record;
    u32 actual_key;
    u16 value;
    if (year) *year = 0;
    if (!year || index >= PREVIEW_COUNT) return PREVIEW_YEAR_ERR_INPUT;
    if (card_io_fault()) return CARD_IO_BLOCKED;
    record = preview_records[index];
    actual_key = (u32)record[249] | ((u32)record[250] << 8) |
                 ((u32)record[251] << 16) | ((u32)record[252] << 24);
    if (actual_key != key) return PREVIEW_YEAR_ERR_IDENTITY;
    value = preview_release_years[index];
    if (value && (value < 1970 || value > 2099)) return PREVIEW_YEAR_ERR_FORMAT;
    *year = value;
    return 0;
}
/* Asset tables are defined once above. Only this adapter selects their bank. */
static const u8 *art_data;
static u16 art_position, art_size;
static u8 art_bank, art_active;
static const u8 *description_data;
static u16 description_position,description_size;
static u8 description_bank,description_active;
#if PREVIEW_CHEATS
static const u8 *cheat_data;
static u16 cheat_position, cheat_size;
static u8 cheat_bank, cheat_active;
#endif

u8 preview_art_open(u16 index, u8 which, u16 *size) {
    if (size) *size = 0;
    if (card_io_fault()) return CARD_IO_BLOCKED;
    if (!size || index >= PREVIEW_COUNT || which > 1) return PICTURE_BAD_INPUT;
    if (art_active || description_active) return PREVIEW_ART_BAD_STATE;
#if PREVIEW_CHEATS
    if (cheat_active) return PREVIEW_ART_BAD_STATE;
#endif
    if (!(preview_art_available[index] & (which ? 2 : 1))) return 4;
    art_bank = sysGetBank();
    sysSetBank((u8)(5 + index));
    art_data = which ? preview_screens[index] : preview_covers[index];
    art_size = preview_art_sizes[index][which];
    art_position = 0;
    art_active = 1;
    *size = art_size;
    return 0;
}
u8 preview_art_read(void *buffer, u16 size) {
    if (card_io_fault()) return CARD_IO_BLOCKED;
    if (!art_active || size > art_size - art_position || (size && !buffer))
        return PREVIEW_ART_BAD_STATE;
    if (size) memcpy(buffer, art_data + art_position, size);
    art_position += size;
    return 0;
}
u8 preview_art_close(void) {
    if (!art_active) return PREVIEW_ART_BAD_STATE;
    sysSetBank(art_bank);
    art_active = 0;
    art_data = 0;
    art_position = art_size = 0;
    return 0;
}
u8 preview_cheat_open(u16 index, u32 key, NativeBridgeTarget *target, u32 *size) {
#if PREVIEW_CHEATS
    const u8 *record;
    u32 actual_key;
#endif
    if (target) { target->size = 0; target->crc32 = 0; }
    if (size) *size = 0;
    if (card_io_fault()) return CARD_IO_BLOCKED;
    if (!target || !size || index >= PREVIEW_COUNT) return 0xe0;
#if PREVIEW_CHEATS
    if (cheat_active || art_active || description_active) return PREVIEW_ART_BAD_STATE;
    record = preview_records[index];
    actual_key = (u32)record[249] | ((u32)record[250] << 8) |
                 ((u32)record[251] << 16) | ((u32)record[252] << 24);
    if ((record[248] != 1 && record[248] != 2) || key != actual_key) return 0xe5;
    if (!preview_cheat_sizes[index]) return 4;
    cheat_bank = preview_cheat_banks[index];
    cheat_data = preview_cheat_data[index];
    cheat_position = 0;
    cheat_size = preview_cheat_sizes[index];
    cheat_active = 1;
    *target = preview_cheat_targets[index];
    *size = cheat_size;
    return 0;
#else
    (void)key;
    return 4;
#endif
}
u8 preview_cheat_close(void) {
#if PREVIEW_CHEATS
    if (!cheat_active) return PREVIEW_ART_BAD_STATE;
    cheat_active = 0;
    cheat_data = 0;
    cheat_position = cheat_size = 0;
    return 0;
#else
    return PREVIEW_ART_BAD_STATE;
#endif
}
u8 preview_cheat_read(void *buffer, u16 size) {
    if (card_io_fault()) return CARD_IO_BLOCKED;
#if PREVIEW_CHEATS
    if (!cheat_active) return PREVIEW_ART_BAD_STATE;
    if (size > cheat_size - cheat_position || (size && !buffer)) {
        card_io_close(preview_cheat_close);
        return PREVIEW_ART_BAD_STATE;
    }
    if (size) { u8 caller_bank=sysGetBank();sysSetBank(cheat_bank);memcpy(buffer,cheat_data+cheat_position,size);sysSetBank(caller_bank); }
    cheat_position += size;
    return 0;
#else
    (void)buffer; (void)size;
    return PREVIEW_ART_BAD_STATE;
#endif
}
u8 preview_cheat_seek(u32 offset) {
    if (card_io_fault()) return CARD_IO_BLOCKED;
#if PREVIEW_CHEATS
    if (!cheat_active) return PREVIEW_ART_BAD_STATE;
    if (offset > cheat_size) {
        card_io_close(preview_cheat_close);
        return PREVIEW_ART_BAD_STATE;
    }
    cheat_position = (u16)offset;
    return 0;
#else
    (void)offset;
    return PREVIEW_ART_BAD_STATE;
#endif
}
u8 preview_picture(u16 index, u8 which, u8 slot, u8 x, u8 y) {
    return picture_draw(index, which, slot, x, y);
}
u8 preview_description_open(u16 index,u32 *size){
    if(size)*size=0;
    if(card_io_fault())return CARD_IO_BLOCKED;
    if(!size||index>=PREVIEW_COUNT)return 0xe0;
    if(description_active||art_active)return PREVIEW_ART_BAD_STATE;
#if PREVIEW_CHEATS
    if(cheat_active)return PREVIEW_ART_BAD_STATE;
#endif
    if(!preview_description_available[index])return 4;
    description_bank=(u8)(5+index);
    description_data=preview_descriptions[index];description_size=preview_description_sizes[index];
    description_position=0;description_active=1;*size=description_size;return 0;
}
u8 preview_description_read(void *buffer,u16 size){
    u8 bank;
    if(card_io_fault())return CARD_IO_BLOCKED;
    if(!description_active||size>description_size-description_position||(size&&!buffer))return PREVIEW_ART_BAD_STATE;
    if(size){bank=sysGetBank();sysSetBank(description_bank);memcpy(buffer,description_data+description_position,size);sysSetBank(bank);}
    description_position+=size;return 0;
}
u8 preview_description_seek(u32 offset){
    if(card_io_fault())return CARD_IO_BLOCKED;
    if(!description_active||offset>description_size)return PREVIEW_ART_BAD_STATE;
    description_position=(u16)offset;return 0;
}
u8 preview_description_close(void){
    if(!description_active)return PREVIEW_ART_BAD_STATE;
    description_active=0;description_data=0;description_position=description_size=0;return 0;
}
