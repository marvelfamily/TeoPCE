#ifndef TEOPCE_CHEAT_GROUPS_H
#define TEOPCE_CHEAT_GROUPS_H
#include "catalog.h"
/* Borrows544 bytes. Closes TCG before native/profile store operations. */
u8 cheat_draft_open(u16 index,const CatalogRecord *record,u8 *workspace);
void cheat_draft_draw(void);
/* 1 cancels/unchanged, 2 verified saved, 3 verified all-off saved. */
u8 cheat_draft_event(u8 event);
#endif
