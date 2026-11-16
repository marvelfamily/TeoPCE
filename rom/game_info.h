#ifndef TEOPCE_GAME_INFO_H
#define TEOPCE_GAME_INFO_H
#include "catalog.h"
void game_cheat_summary(u16 index,u32 key,u8 x,u8 y);
void game_info_draw(const CatalogRecord *record);
void game_start_draw(const CatalogRecord *record,u8 loading);
#endif
