#ifndef TEOPCE_UI_HINT_H
#define TEOPCE_UI_HINT_H
#include "main.h"
/* Common bounded ASCII text, callable while a UI data bank is mapped. */
void ui_text(u8 x,u8 y,const char *text,u8 width);
/* Fixed code: callers may pass literals from the currently mapped bank. */
void ui_hint_line(u8 y,const char *text);
void ui_hint_footer(const char *first,const char *second);
#endif
