#include "ui_hint.h"
#include <string.h>
void ui_text(u8 x,u8 y,const char *s,u8 width){
 u8 c;gSetXY(x,y);
 while(*s&&width--){c=(u8)*s++;gAppendChar(c>=32&&c<=126?c:'?');}
}
/* Palettes4..15 belong to artwork. Reuse the existing text key-cap palette. */
void ui_hint_line(u8 y,const char *s){
 u8 left=38,key,n,c;
 gSetPal(PAL_00);gFillRow(' ',1,y,38);gSetXY(1,y);
 while(*s&&left){
  key=!strncmp(s,"I ",2)?1:!strncmp(s,"II ",3)?2:
      !strncmp(s,"SEL ",4)?3:!strncmp(s,"SELECT ",7)?6:
      !strncmp(s,"RUN ",4)?3:0;
  gSetPal(key?PAL_OK:PAL_00);
  for(n=0;*s&&left;++n){
   if(*s==' '&&s[1]==' ')break;
   if(key&&n==key)gSetPal(PAL_01);
   c=(u8)*s++;gAppendChar(c>=32&&c<=126?c:'?');--left;
  }
  gSetPal(PAL_00);
  while(*s==' '&&left){gAppendChar(*s++);--left;}
 }
 gSetPal(PAL_00);
}
void ui_hint_footer(const char *first,const char *second){
 ui_hint_line(25,first);ui_hint_line(26,second);
}
