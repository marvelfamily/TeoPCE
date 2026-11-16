#ifndef TEOPCE_GAME_META_H
#define TEOPCE_GAME_META_H
#include "main.h"

#define GAME_META_ERR_INPUT 0xe0
#define GAME_META_ERR_FORMAT 0xe1
#define GAME_META_ERR_IDENTITY 0xe2

/* Missing optional years.bin returns success with year 0. */
u8 game_meta_year(u16 index, u32 key, u16 *year);
#endif

