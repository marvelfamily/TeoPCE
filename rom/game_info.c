#include "ui_hint.h"
#include "card_io.h"
/* Existing file-information screen, banked to retain the native code budget. */
#include "game_info.h"
#include "cheat_index.h"
#ifdef PREVIEW
#define GAME_INFO_BANK 17
#else
#define GAME_INFO_BANK 8
#endif
#ifdef __CC65__
#ifdef PREVIEW
#pragma code-name(push,"BNK13")
#pragma rodata-name(push,"BNK13")
#else
#pragma code-name(push,"BNK04")
#pragma rodata-name(push,"BNK04")
#endif
#endif
static const char heading[]="TEOPCE 0.9.13 / FILE INFORMATION";
static const char path_heading[]="PATH IN STOCK MENU";
#ifdef PREVIEW
static const char preview[]="PREVIEW: SD FILES ARE NOT READ";
#else
static const char failed[]="FILE CHECK FAILED:";
static const char found[]="IMAGE FILE FOUND";
static const char empty[]="IMAGE FILE IS EMPTY";
static const char cue[]="CUE FILE BYTES:";
static const char rom[]="ROM BYTES:";
#endif
static const char hucard[]="HUCARD: START GAME IN GAME MENU";
static const char sgx[]="REQUIRES A SUPERGRAFX CONSOLE";
static const char cd[]="CD-ROM: START GAME IN GAME MENU";
static const char back_start[]="II Back: choose START GAME";
static const char back[]="II Back: choose RETURN TO EVERDRIVE";
static const char ownership[]="Native saves and IGM: EverDrive";
static const char controls[]="I View cheat codes  II Back";
static const char exit_note[]="Exit returns to the EverDrive menu.";
static const char cd_no_states[]="NO QUICK SAVE STATES FOR CD";
static const char cd_save[]="Use the game's own save option.";
static const char start_heading[]="TEOPCE 0.9.13 / START GAME";
static const char loading_heading[]="TEOPCE 0.9.13 / LOADING GAME";
static const char start_question[]="Start this game?";
static const char loading_note[]="Loading selected game...";
static const char everdrive_note[]="The EverDrive will load this game.";
static const char power_note[]="Keep the console on while loading.";
static const char start_controls[]="I Start game  II Cancel";
static const char wait_note[]="Please wait";
static const char cd_bios[]="Uses the BIOS set in EverDrive.";
static const char cd_run[]="On the BIOS screen, press RUN.";
static void text(u8 x,u8 y,const char *s,u8 width){gSetXY(x,y);while(*s&&width--)gAppendChar(*s++);}
static void append(const char*s){while(*s)gAppendChar(*s++);}
static void coverage_impl(u16 index,u32 key,u8 x,u8 y){
 CheatCoverage counts;u8 error=cheat_index_read(index,key,&counts);
 if(error){text(x,y,error==CHEAT_INDEX_MISSING?"CHEATS: UNKNOWN":"CHEATS: READ ERROR",38-x);return;}
 if(!counts.available&&!counts.held){text(x,y,"CHEATS: NONE",38-x);return;}
 gSetXY(x,y);append("CHEATS: ");gAppendNum(counts.available);
 if(counts.held){append(" / ");gAppendNum(counts.held);append(" HELD");}
}
static void wrapped(const char *s){
 u8 row,n,space;
 for(row=2;*s&&row<5;++row){
  while(*s==' ')++s;n=space=0;
  while(n<38&&s[n]){if(s[n]==' ')space=n;++n;}
  if(n==38&&s[n]&&space)n=space;
  text(1,row,s,n);s+=n;while(*s==' ')++s;
 }
}
static void draw_impl(const CatalogRecord *record){
#ifndef PREVIEW
 u8 result,close_result;u32 size=0;
#endif
 u8 row,col,c;u16 offset=0;
 gCleanScreen();gSetPal(PAL_OK);text(2,1,heading,36);gSetPal(PAL_01);wrapped(record->title);
 gSetPal(PAL_00);text(1,5,path_heading,38);
 for(row=0;row<4;++row){gSetXY(1,6+row);for(col=0;col<38&&record->path[offset];++col){c=(u8)record->path[offset++];gAppendChar(c>=32&&c<=126?c:'?');}}
 gSetPal(PAL_01);
#ifdef PREVIEW
 text(1,12,preview,38);
#else
 result=card_io_fault()?CARD_IO_BLOCKED:ed_cmd_file_open((u8*)record->path,FA_READ);
 if(!result){size=ed_cmd_file_available();close_result=card_io_close(ed_cmd_file_close);if(close_result)result=close_result;}
 if(result){gSetPal(PAL_ER);text(1,12,failed,25);gSetXY(26,12);gAppendHex8(result);}
 else{text(1,12,size?found:empty,38);text(1,14,record->kind==2?cue:rom,24);gSetXY(20,14);gAppendNum(size);}
#endif
 gSetPal(PAL_ER);text(1,17,record->kind==1?hucard:record->kind==3?sgx:cd,38);
 gSetPal(PAL_00);text(1,19,record->kind==1||record->kind==2?back_start:back,38);text(1,21,ownership,38);ui_hint_line(25,controls);text(1,26,exit_note,38);
}
static void start_impl(const CatalogRecord *record,u8 loading){
 gCleanScreen();gSetPal(PAL_OK);text(2,1,loading?loading_heading:start_heading,36);
 gSetPal(PAL_01);wrapped(record->title);text(2,8,loading?loading_note:start_question,36);
 gSetPal(PAL_00);text(2,11,loading?power_note:everdrive_note,36);
 if(!loading)text(2,14,power_note,36);
 if(record->kind==2){text(2,16,cd_bios,36);text(2,17,cd_run,36);
  if(!loading){text(2,19,cd_no_states,36);text(2,20,cd_save,36);}
 }
 if(!loading)text(2,22,exit_note,36);
 ui_hint_line(25,loading?wait_note:start_controls);if(loading)gVsync();
}
#ifdef __CC65__
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
void game_info_draw(const CatalogRecord *record){
#if defined(PREVIEW) && !defined(__CC65__)
 draw_impl(record);
#else
 u8 bank=sysGetBank();sysSetBank(GAME_INFO_BANK);draw_impl(record);sysSetBank(bank);
#endif
}

void game_start_draw(const CatalogRecord *record,u8 loading){
#if defined(PREVIEW) && !defined(__CC65__)
 start_impl(record,loading);
#else
 u8 bank=sysGetBank();sysSetBank(GAME_INFO_BANK);start_impl(record,loading);sysSetBank(bank);
#endif
}

void game_cheat_summary(u16 index,u32 key,u8 x,u8 y){
#ifdef __CC65__
 u8 bank=sysGetBank();sysSetBank(GAME_INFO_BANK);coverage_impl(index,key,x,y);sysSetBank(bank);
#else
 coverage_impl(index,key,x,y);
#endif
}
