#ifndef TEOPCE_CHEAT_INDEX_H
#define TEOPCE_CHEAT_INDEX_H
#include "main.h"
#define CHEAT_INDEX_MISSING 0xe4
typedef struct {u16 available,held;} CheatCoverage;
u8 cheat_index_read(u16 index,u32 key,CheatCoverage *coverage);
/* workspace is the existing 544-byte browser scratch; output order is retained. */
u8 cheat_index_filter(u16 *matches,u16 *count,u8 *workspace);
#endif
