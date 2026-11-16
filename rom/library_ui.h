#ifndef TEOPCE_LIBRARY_UI_H
#define TEOPCE_LIBRARY_UI_H
#include "find.h"
#define LIBRARY_WAIT 0
#define LIBRARY_APPLY 1
#define LIBRARY_CANCEL 2
#define LIBRARY_OPTIONS 0
#define LIBRARY_TITLE 1
#define LIBRARY_LETTER 2
#define LIBRARY_ERROR 3
#define LIBRARY_GENRE 4
/* Six tags max:3+4+4+4+2+14 chars +five spaces=36, never truncate a field. */
#define LIBRARY_SUMMARY_MAX 36
typedef char library_summary_fits[(LIBRARY_SUMMARY_MAX<=38)?1:-1];
typedef struct {char draft[13];u8 row,mode,focus,error;} LibraryUI;
void library_open(LibraryUI *ui,const CatalogFilter *filter);
u8 library_event(LibraryUI *ui,CatalogFilter *filter,Preferences *prefs,u8 event);
void library_draw(const LibraryUI *ui,const CatalogFilter *filter,const Preferences *prefs,const Recent *recent);
void library_error(LibraryUI *ui,u8 code);
void library_browse_context(const CatalogRecord *record,const CatalogFilter *filter,const Preferences *prefs);
#endif
