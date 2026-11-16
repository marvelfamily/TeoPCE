#include "ui_hint.h"
#include "card_io.h"
/* Existing TPCH fallback viewer; native application belongs to cheat_store. */
#include "main.h"
#include "cheat_legacy.h"
#include "pack.h"
#include "app_paths.h"
#include <string.h>
#if defined(__CC65__) && !defined(PREVIEW)
#pragma code-name(push,"BNK04")
#pragma rodata-name(push,"BNK04")
#endif
static const char title[]="TEOPCE 0.9.13 / CHEAT CODES";
static const char view_only[]="REFERENCE CODES / READ ONLY";
static const char cd_view_only[]="CD CODES / READ ONLY";
static const char back[]="II Back";
static const char controls[]="LEFT/RIGHT Code pages  II Back";
static const char reference_note[]="REFERENCE ONLY / CODES ARE NOT APPLIED";
static const char cd_note[]="No selectable groups for this disc.";
#ifdef PREVIEW
static const char preview_note[]="Code files are read on the cartridge.";
#else
static const char unavailable[]="CHEATS NOT AVAILABLE";
static const char no_pack[]="NO VERIFIED CODES FOR THIS VERSION";
static const char no_cd_pack[]="NO REFERENCE CODES FOR THIS DISC";
static const char empty_pack[]="THIS CODE FILE IS EMPTY";
static const char missing_note[]="No prepared cheat pack is installed.";
static const char normal_launch[]="Game launch is still available.";
static const char read_error[]="CHEAT FILE COULD NOT BE READ";
static const char repair_note[]="Restore the TeoPCE files on the SD.";
static const char error_heading[]="CHEAT DATA ERROR";
static const char hex[]="0123456789ABCDEF";
static const char path_pattern[]=TEO_LEGACY_CHEAT_ROOT "/00000000.bin";
#endif
static void text(u8 x,u8 y,const char *s,u8 width){gSetXY(x,y);while(*s&&width--)gAppendChar(*s++);}
static void wrapped(const char *s){
 u8 row,n,space;
 for(row=2;*s&&row<5;++row){
  while(*s==' ')++s;n=space=0;
  while(n<38&&s[n]){if(s[n]==' ')space=n;++n;}
  if(n==38&&s[n]&&space)n=space;
  text(1,row,s,n);s+=n;while(*s==' ')++s;
 }
}
#ifndef PREVIEW
static void cheat_path(char *path,const CatalogRecord *record){
 u32 key=pack_key(record->reserved);u8 i,end;
 strcpy(path,path_pattern);end=(u8)(sizeof(TEO_LEGACY_CHEAT_ROOT "/")-1+7);
 for(i=0;i<8;++i){path[end-i]=hex[key&15];key>>=4;}
}
#endif
static void draw_impl(const CatalogRecord *record,u8 *work,u16 *page,u16 *lines){
#ifndef PREVIEW
 char path[sizeof(path_pattern)];u8 header[8],result,close_result,row,missing;u16 count=0;u32 size,offset;
#endif
 gCleanScreen();gSetPal(PAL_OK);text(2,1,title,36);
 gSetPal(PAL_01);wrapped(record->title);gSetPal(PAL_00);
#ifdef PREVIEW
 (void)work;(void)page;*lines=0;
 text(1,5,record->kind==2?cd_view_only:view_only,38);
 text(2,9,preview_note,36);ui_hint_line(25,back);
 text(1,26,record->kind==2?cd_note:reference_note,38);
#else
 cheat_path(path,record);*lines=0;memset(header,0,sizeof(header));
 result=card_io_fault()?CARD_IO_BLOCKED:ed_cmd_file_open((u8*)path,FA_READ);
 missing=result==4;
 if(!result){
  size=ed_cmd_file_available();result=ed_cmd_file_read(header,8);count=(u16)header[6]|((u16)header[7]<<8);
  if(!result&&(memcmp(header,"TPCH",4)||header[4]!=1||header[5]||count>1024||size!=8UL+(u32)count*40))result=0xf1;
  if(!result&&count){
   *lines=count;offset=(u32)*page*17;if(offset>=count){*page=0;offset=0;}
   result=ed_cmd_file_set_ptr(8UL+offset*40);
   for(row=0;row<17&&offset+row<count&&!result;++row){
    result=ed_cmd_file_read(work,40);if(!result&&!memchr(work,0,40))result=0xf1;
    if(!result){gSetPal(PAL_01);text(1,6+row,(char*)work,38);}
   }
  }
  close_result=card_io_close(ed_cmd_file_close);if(!result)result=close_result;
 }
 if(result||!count){
  *page=0;*lines=0;gSetPal(PAL_01);gFillRect(' ',1,6,38,17);
  if(result&&!missing){
   gSetPal(PAL_ER);text(1,5,error_heading,38);text(2,9,read_error,36);
   gSetPal(PAL_01);text(2,11,repair_note,36);text(2,13,"Error:",8);gSetXY(10,13);gAppendHex8(result);
  }else{
   gSetPal(PAL_00);text(1,5,record->kind==2?cd_view_only:unavailable,38);
   text(2,9,result?(record->kind==2?no_cd_pack:no_pack):empty_pack,36);
   gSetPal(PAL_01);text(2,11,record->kind==2?cd_note:missing_note,36);text(2,13,normal_launch,36);
  }
 }else{
  gSetPal(PAL_00);text(1,5,record->kind==2?cd_view_only:view_only,38);
  gSetXY(28,24);gAppendNum(*page+1);gAppendChar('/');gAppendNum((*lines+16)/17);
 }
 gSetPal(PAL_00);ui_hint_line(25,*lines>17?controls:back);
 if(*lines)text(1,26,record->kind==2?cd_note:reference_note,38);
#endif
}
#if defined(__CC65__) && !defined(PREVIEW)
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
void cheat_legacy_draw(const CatalogRecord *record,u8 *work,u16 *page,u16 *lines){
#ifdef PREVIEW
 draw_impl(record,work,page,lines);
#else
 u8 bank=sysGetBank();sysSetBank(8);draw_impl(record,work,page,lines);sysSetBank(bank);
#endif
}
