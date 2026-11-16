#include "ui_hint.h"
#include "cheat_index.h"
#include "library_ui.h"
#include <string.h>
#ifdef __CC65__
#ifdef PREVIEW
#define LIBRARY_BANK 18
#define CONTEXT_BANK 19
#pragma code-name(push,"BNK14")
#pragma rodata-name("BNK14")
#else
#define LIBRARY_BANK 11
#define CONTEXT_BANK 12
#pragma code-name(push,"BNK07")
#pragma rodata-name("BNK07")
#endif
#endif
static const char * const genres[GENRE_COUNT]={"ALL GENRES","Action","Adventure","Beat'em Up","Board","Fighting","Gambling","Lightgun Shooter","Pinball","Platform","Puzzle","Quiz","Racing","Role-playing (RPG)","Shoot'em Up","Shooter","Simulation","Sports","Sports with Animals","Strategy","Unknown","Utility","Music","Compilation","Multimedia","Education"};
/* Summary legend: full option names and these fixed tags share the same IDs. */
static const char * const media[]={"ALL MEDIA","HUCARD","CD-ROM"};
static const char * const regions[]={"ALL REGIONS/LANG","JAPAN","USA","ENGLISH PATCH","OTHER"};
static const char * const themes[THEME_COUNT]={"NEON","EMBER","ICE","CLASSIC"};
static const char * const labels[]={"TITLE PREFIX","LIBRARY","CATEGORY","MEDIA","GENRE","LETTER","REGION/LANG","CHEATS","VIEW","THEME"};
static const char * const categories[CATEGORY_COUNT]={"ALL CATEGORIES","ORIGINAL RELEASES","TRANSLATIONS","HOMEBREW","MUSEUM","TOOLS / TESTS","UNLICENSED"};
static const char * const option_help[]={
 "Find by title prefix.",
 "All games, favorites or recent.",
 "Choose a library category.",
 "HuCard or CD-ROM games.",
 "Choose a genre.",
 "Choose a first title letter.",
 "Region or English patch.",
 "Games with selectable cheats.",
 "List or six-cover grid.",
 "Choose menu colors."
};
static const char * const commands[]={"SPACE","'","-","DEL","CLEAR","FIND"};
#define text ui_text
static void footer(const char *a,const char *b){ui_hint_footer(a,b);}
static void heading(const char *s){gSetPal(PAL_OK);gFillRow(' ',1,1,38);text(2,1,s,36);}
static u8 cycle(u8 n,u8 count,u8 back){return back?(n?n-1:count-1):(n+1==count?0:n+1);}
static void open_impl(LibraryUI *ui,const CatalogFilter *filter){memset(ui,0,sizeof(*ui));strcpy(ui->draft,filter->prefix);}
static u8 event_impl(LibraryUI *ui,CatalogFilter *f,Preferences *prefs,u8 event){
 u8 value,length,back;
 if(event&JOY_A){if(ui->mode==LIBRARY_OPTIONS||ui->mode==LIBRARY_ERROR)return LIBRARY_CANCEL;ui->mode=LIBRARY_OPTIONS;ui->error=0;return LIBRARY_WAIT;}
 if(ui->mode==LIBRARY_ERROR)return event&JOY_B?LIBRARY_APPLY:LIBRARY_WAIT;
 if(ui->mode==LIBRARY_GENRE){
  if(ui->focus>=GENRE_COUNT)ui->focus=0;
  if(event&JOY_B){f->genre=ui->focus;return LIBRARY_APPLY;}
  if(event&JOY_U){if(ui->focus)--ui->focus;}
  else if(event&JOY_D){if(ui->focus+1<GENRE_COUNT)++ui->focus;}
  else if(event&JOY_L)ui->focus=ui->focus>=12?ui->focus-12:0;
  else if(event&JOY_R)ui->focus=ui->focus+12<GENRE_COUNT?ui->focus+12:GENRE_COUNT-1;
  return LIBRARY_WAIT;
 }
 if(ui->mode==LIBRARY_TITLE||ui->mode==LIBRARY_LETTER){
  if(ui->mode==LIBRARY_TITLE&&(event&JOY_STA)){strcpy(f->prefix,ui->draft);return LIBRARY_APPLY;}
  if(event&JOY_B){
   if(ui->mode==LIBRARY_LETTER){f->letter=ui->focus==0?0:ui->focus==1?'#':'A'+ui->focus-2;return LIBRARY_APPLY;}
   length=(u8)strlen(ui->draft);
   if(ui->focus==41){strcpy(f->prefix,ui->draft);return LIBRARY_APPLY;}
   if(ui->focus==40)ui->draft[0]=0;
   else if(ui->focus==39){if(length)ui->draft[length-1]=0;}
   else if(length<FIND_PREFIX_MAX){value=ui->focus<26?'A'+ui->focus:ui->focus<36?'0'+ui->focus-26:ui->focus==36?' ':ui->focus==37?'\'':'-';ui->draft[length]=value;ui->draft[length+1]=0;}
   else ui->error=1;
  }else{
   value=ui->mode==LIBRARY_TITLE?42:28;back=ui->focus;
   if(event&JOY_U)ui->focus=ui->focus>=6?ui->focus-6:ui->focus;
   else if(event&JOY_D)ui->focus=ui->focus+6<value?ui->focus+6:value-1;
   else if(event&JOY_L){if(ui->focus)--ui->focus;}
   else if(event&JOY_R){if(ui->focus+1<value)++ui->focus;}
   if(back!=ui->focus)ui->error=0;
  }
  return LIBRARY_WAIT;
 }
 if(event&JOY_B){
  if(ui->row==0){strcpy(ui->draft,f->prefix);ui->mode=LIBRARY_TITLE;ui->focus=ui->error=0;return LIBRARY_WAIT;}
  if(ui->row==4){ui->mode=LIBRARY_GENRE;ui->focus=f->genre<GENRE_COUNT?f->genre:0;return LIBRARY_WAIT;}
  if(ui->row==5){ui->mode=LIBRARY_LETTER;ui->focus=!f->letter?0:f->letter=='#'?1:f->letter-'A'+2;return LIBRARY_WAIT;}
  return LIBRARY_APPLY;
 }
 if(event&JOY_STA){memset(f,0,sizeof(*f));return LIBRARY_WAIT;}
 if(event&JOY_U){ui->row=cycle(ui->row,10,1);return LIBRARY_WAIT;}
 if(event&JOY_D){ui->row=cycle(ui->row,10,0);return LIBRARY_WAIT;}
 if(!(event&(JOY_L|JOY_R)))return LIBRARY_WAIT;back=(event&JOY_L)!=0;
 switch(ui->row){
  case 1:value=f->favorites?1:f->recent?2:0;value=cycle(value,3,back);f->favorites=value==1;f->recent=value==2;break;
  case 3:f->medium=cycle(f->medium,3,back);break;
  case 4:f->genre=cycle(f->genre,GENRE_COUNT,back);break;
  case 5:value=!f->letter?0:f->letter=='#'?1:f->letter-'A'+2;value=cycle(value,28,back);f->letter=!value?0:value==1?'#':'A'+value-2;break;
  case 6:f->region=cycle(f->region,5,back);break;
  case 8:prefs->view^=1;break;
  case 9:prefs->theme=cycle(prefs->theme,THEME_COUNT,back);break;
  case 7:f->cheats^=1;break;
  case 2:f->category=cycle(f->category,CATEGORY_COUNT,back);break;
 }
 return LIBRARY_WAIT;
}
static void draw_impl(const LibraryUI *ui,const CatalogFilter *f,const Preferences *prefs,const Recent *recent){
 u8 i,x,y,top;const char *value;
 gCleanScreen();
 if(ui->mode==LIBRARY_ERROR){heading("TEOPCE / FIND DATA ERROR");gSetPal(PAL_ER);text(1,8,f->category?"Category data needs an update.":f->cheats?"Cheat availability is unavailable.":ui->error==4||ui->error==5?"Title search index is missing.":ui->error==CATALOG_ERR_FORMAT?"Search index does not match library.":"Search data could not be read.",38);gSetPal(PAL_01);text(1,11,"The requested query was not applied.",38);text(1,13,f->cheats?TEO_METADATA_ROOT "/cheats.idx":TEO_METADATA_ROOT "/find.bin",38);text(1,15,"Error:",10);gSetXY(12,15);gAppendHex8(ui->error);footer("I Retry  II Cancel","Restore matching menu data on the SD.");return;}
 if(ui->mode==LIBRARY_GENRE){
  heading("TEOPCE / CHOOSE GENRE");gSetPal(PAL_00);text(2,3,"Choose a genre, then I to apply.",36);
  top=(ui->focus/12)*12;
  for(i=0;i<12&&top+i<GENRE_COUNT;++i){
   y=5+i;gSetPal(top+i==ui->focus?PAL_OK:PAL_01);
   if(top+i==ui->focus){gFillRow(' ',1,y,38);text(1,y,">",1);}
   text(2,y,genres[top+i],36);
  }
  gSetPal(PAL_00);text(2,20,"Page ",5);gAppendNum(top/12+1);gAppendChar('/');gAppendNum((GENRE_COUNT+11)/12);
  footer("I Choose  II Back","UP/DOWN Genre  L/R Page");return;
 }
 if(ui->mode==LIBRARY_TITLE||ui->mode==LIBRARY_LETTER){
  heading(ui->mode==LIBRARY_TITLE?"TEOPCE / FIND TITLE":"TEOPCE / FIRST LETTER");
  gSetPal(PAL_01);text(2,3,ui->mode==LIBRARY_TITLE?"PREFIX:":"Choose a first letter.",36);
  if(ui->mode==LIBRARY_TITLE){text(10,3,ui->draft,12);text(2,5,"Type the start of the game title.",36);}
  for(i=0;i<(ui->mode==LIBRARY_TITLE?42:28);++i){
   x=2+(i%6)*6;y=7+(i/6)*2;gSetPal(i==ui->focus?PAL_OK:PAL_01);if(i==ui->focus)gFillRow(' ',x,y,6);text(x,y,i==ui->focus?">":" ",1);
   if(ui->mode==LIBRARY_LETTER){if(!i)text(x+1,y,"ALL",4);else{gSetXY(x+1,y);gAppendChar(i==1?'#':'A'+i-2);}}
   else if(i<36){gSetXY(x+1,y);gAppendChar(i<26?'A'+i:'0'+i-26);}else text(x+1,y,commands[i-36],5);
  }
  if(ui->error){gSetPal(PAL_ER);text(2,22,"12 characters maximum.",36);}
  footer(ui->mode==LIBRARY_TITLE?"I Type / choose FIND  II Cancel":"I Choose letter  II Cancel",ui->mode==LIBRARY_TITLE?"D-pad Key  DEL Delete  RUN Find":"D-pad Letter");return;
 }
 heading("TEOPCE / FIND AND FILTERS");gSetPal(PAL_00);text(2,3,"Combine options, then I to apply.",36);
 top=ui->row==9?1:0;text(32,1,top?"2-10/10":"1-9/10",7);
 for(i=top;i<top+9;++i){
  y=5+(i-top)*2;gSetPal(i==ui->row?PAL_OK:PAL_01);if(i==ui->row){gFillRow(' ',1,y,38);text(1,y,">",1);}text(2,y,labels[i],13);
  switch(i){case 0:value=f->prefix[0]?f->prefix:"ANY TITLE";break;case 1:value=f->favorites?"FAVORITES":f->recent?"RECENTLY OPENED":"ALL GAMES";break;case 3:value=media[f->medium];break;case 4:value=genres[f->genre];break;case 5:value=f->letter?"":"ALL LETTERS";break;case 6:value=regions[f->region];break;case 8:value=prefs->view?"IMAGE GRID":"LIST";break;case 9:value=themes[prefs->theme];break;case 7:value=f->cheats?"AVAILABLE":"ANY";break;default:value=categories[f->category];break;}
  text(17,y,value,21);if(i==5&&f->letter){gSetXY(17,y);gAppendChar(f->letter);}
 }
 gSetPal(PAL_00);text(2,22,ui->row==1&&(recent->read_error||recent->write_error)?
 "Recent history is session-only.":option_help[ui->row],36);
 footer(ui->row==0?"I Edit title  II Cancel":ui->row==4?"I Pick genre  II Cancel":ui->row==5?"I Pick letter  II Cancel":"I Apply  II Cancel","D-pad Option/change  RUN Clear filters");
}
#ifdef __CC65__
#pragma code-name(pop)
#ifdef PREVIEW
#pragma code-name(push,"BNK15")
#pragma rodata-name(push,"BNK15")
#else
#pragma code-name(push,"BNK08")
#pragma rodata-name(push,"BNK08")
#endif
#endif
/* cc65 pools anonymous string literals at the final rodata segment. These
 * inline arrays own their bytes in CONTEXT_BANK even with two banks in this TU. */
static const char genre_tags[GENRE_COUNT][5]={"","ACT","ADV","BEAT","BRD","FITE","GAMB","LGUN","PIN","PLAT","PUZ","QUIZ","RACE","RPG","STG","SHOT","SIM","SPRT","ANIM","STR","UNK","UTIL","MUS","COLL","VID","EDU"};
static const char region_tags[][5]={"","R:JP","R:US","L:EN","R:OT"};
static const char context_media[][4]={"","HUC","CD","SGX"};
static const char context_region_names[][8]={"Japan","USA","English"};
static const char context_regions[][5]={"JP","US","L:EN","OT"};
static const char context_library[][5]={"FAV","OPEN"};
static const char context_cheats[]="CH";
static const char context_all[]="ALL GAMES / A-Z";
static const char category_tags[CATEGORY_COUNT][9]={"","ORIGINAL","TRANSL.","HOMEBREW","MUSEUM","TOOLS","UNLIC."};
static const char context_list[]="TEO LIST",context_grid[]="TEO GRID",context_order[]="A-Z";
static void wrapped(const char *s){
 u8 rows=3,y=2,length,space,end;const char *rest;
 for(;*s&&rows;--rows,++y){while(*s==' ')++s;length=space=0;while(length<38&&s[length]){if(s[length]==' ')space=length;++length;}end=length;
  if(length==38&&s[length]&&s[length]!=' '&&space){rest=s+space;while(*rest==' ')++rest;if(strlen(rest)<=(u16)(rows-1)*38)end=space;}
  text(1,y,s,end);s+=end;while(*s==' ')++s;
 }
}
static void tag(const char *s,u8 *used){if(*used){gAppendChar(' ');++*used;}while(*s){gAppendChar(*s++);++*used;}}
static void context_impl(const CatalogRecord *record,const CatalogFilter *f,const Preferences *prefs){
 u8 i,used=0;
 gSetPal(PAL_OK);gFillRow(' ',2,1,8);text(2,1,f->category?category_tags[f->category]:prefs->view?context_grid:context_list,8);
 gSetPal(PAL_01);for(i=2;i<=4;++i)gFillRow(' ',1,i,38);gFillRow(' ',11,1,16);
 if(record){wrapped(record->title);gSetPal(PAL_00);text(11,1,context_media[record->kind==1?1:record->kind==2?2:3],3);
  text(15,1,context_regions[!strcmp(record->region,context_region_names[0])?0:!strcmp(record->region,context_region_names[1])?1:!strcmp(record->region,context_region_names[2])?2:3],5);
  if(prefs_has(prefs,pack_key(record->reserved))){gSetXY(21,1);gAppendChar('*');}
 }
 if(f->cheats){gSetPal(PAL_ER);text(22,1,context_cheats,2);}
 gSetPal(PAL_00);gFillRow(' ',1,25,38);gSetXY(1,25);
 if(f->medium)tag(context_media[f->medium==1?1:2],&used);if(f->genre)tag(genre_tags[f->genre],&used);if(f->region)tag(region_tags[f->region],&used);
 if(f->favorites||f->recent)tag(context_library[f->favorites?0:1],&used);
 if(f->letter){if(used)gAppendChar(' ');gAppendChar(f->letter);gAppendChar('*');used+=used?3:2;}
 if(f->prefix[0]){if(used)gAppendChar(' ');gAppendChar('"');for(i=0;i<FIND_PREFIX_MAX&&f->prefix[i];++i)gAppendChar(f->prefix[i]);gAppendChar('"');used=1;}
 if(!used)text(1,25,f->category?context_order:context_all,38);
}
#ifdef __CC65__
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
void library_open(LibraryUI *ui,const CatalogFilter *f){
#ifdef __CC65__
 u8 bank=sysGetBank();sysSetBank(LIBRARY_BANK);open_impl(ui,f);sysSetBank(bank);
#else
 open_impl(ui,f);
#endif
}
u8 library_event(LibraryUI *ui,CatalogFilter *f,Preferences *p,u8 event){
#ifdef __CC65__
 u8 bank=sysGetBank(),result;sysSetBank(LIBRARY_BANK);result=event_impl(ui,f,p,event);sysSetBank(bank);return result;
#else
 return event_impl(ui,f,p,event);
#endif
}
void library_draw(const LibraryUI *ui,const CatalogFilter *f,const Preferences *p,const Recent *recent){
#ifdef __CC65__
 u8 bank=sysGetBank();sysSetBank(LIBRARY_BANK);draw_impl(ui,f,p,recent);sysSetBank(bank);
#else
 draw_impl(ui,f,p,recent);
#endif
}
void library_error(LibraryUI *ui,u8 code){ui->mode=LIBRARY_ERROR;ui->error=code;}
void library_browse_context(const CatalogRecord *r,const CatalogFilter *f,const Preferences *p){
#ifdef __CC65__
 u8 bank=sysGetBank();sysSetBank(CONTEXT_BANK);context_impl(r,f,p);sysSetBank(bank);
#else
 context_impl(r,f,p);
#endif
}
