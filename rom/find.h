#ifndef TEOPCE_FIND_H
#define TEOPCE_FIND_H
#include "catalog.h"
#include "recent.h"
#define FIND_PREFIX_MAX 12
u8 find_filter(const CatalogFilter *filter,const Preferences *prefs,const Recent *recent,u16 *matches,u16 *count,u8 *workspace);
#endif
