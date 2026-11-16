#ifndef TEOPCE_RECENT_H
#define TEOPCE_RECENT_H
#include "main.h"
#define RECENT_MAX 8
#define RECENT_BYTES 48
#define RECENT_BAD_DATA 0xe2
#define RECENT_VERIFY_FAILED 0xe3
/* Recent means entered game screens, never a claim of successful play. */
typedef struct {u32 sequence;u32 keys[RECENT_MAX];u8 count,read_error,write_error;} Recent;
u8 recent_rank(const Recent *state,u32 key);
u8 recent_load(Recent *state,u8 *workspace);
u8 recent_opened(Recent *state,u32 key,u8 *workspace);
#endif
