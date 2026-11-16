#ifndef TEOPCE_CHEAT_CACHE_H
#define TEOPCE_CHEAT_CACHE_H
#include "main.h"
#include "cheat_selection.h"
#define CHEAT_CACHE_CONFLICT 0xe6
/* On CONFLICT, conflicting_group identifies the selected group to review.
 * Caller owns the open validated TCG. No handle or banked pointer escapes. */
u8 cheat_cache_rows(const CheatSelection *draft,u16 groups,u32 codes_at,
                    u8 *work544,u8 *record40,u8 *row_count,u32 *conflicting_group);
#endif
