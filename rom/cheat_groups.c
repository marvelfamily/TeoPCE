#include "ui_hint.h"
#include "card_io.h"
/* Streamed exact-metadata TCG1 reader. TCG closes before native store I/O. */
#include "main.h"
#include "cheat_groups.h"
#include "cheat_notes.h"
#include "cd_guard.h"
#include "app_paths.h"
#include "cheat_selection.h"
#include "cheat_store.h"
#include "cheat_cache.h"
#include "launch.h"
#include "pack.h"
#include <string.h>
#ifdef PREVIEW
#include "preview.h"
#define rd(b,n) preview_cheat_read(b,n)
#define seek(n) preview_cheat_seek(n)
#define close_art() card_io_close(preview_cheat_close)
#else
#define rd(b,n) ed_cmd_file_read(b,n)
#define seek(n) ed_cmd_file_set_ptr(n)
#define close_art() card_io_close(ed_cmd_file_close)
#endif
#define BAD 0xe5
static CheatSelection draft;
static u8 meta[40],*work;
static const char *game_title;
static u32 file_bytes,codes_at,names_at;
static u16 groups,selected,visible_count,visible_selected;
static u8 show_held,options,option,list_drawn;
static u8 handle,ready,last_error,close_fault,dirty,read_only,native_state,note_pending;
static u16 game_index;
static const CatalogRecord *game_record;
static NativeBridgeTarget game_target;
static u32 package_crc;
static const char *message;
#ifdef PREVIEW
#define GROUP_BANK 20
#else
#define GROUP_BANK 5
#endif
#ifdef __CC65__
#ifdef PREVIEW
#pragma code-name(push,"BNK16")
#pragma rodata-name("BNK16")
#else
#pragma code-name(push,"BNK01")
#pragma rodata-name("BNK01")
#endif
#endif
static const char conflict_prompt[]="CONFLICT / I OFF / II CANCEL";
#ifdef __CC65__
#pragma code-name(push,"CODE")
#endif
static u16 word(const u8 *b){return (u16)b[0]|((u16)b[1]<<8);}

#ifdef __CC65__
#pragma code-name(pop)
#endif
static u8 finish(u8 error){
 u8 result=0;
 if(card_io_fault()){close_fault=card_io_fault();return error?error:CARD_IO_BLOCKED;}
 if(close_fault)return error?error:close_fault;
 if(handle){result=close_art();if(result)close_fault=result;else handle=0;}
 return error?error:result;
}
static u8 fail(u8 error){
 last_error=finish(error);ready=0;memset(&draft,0,sizeof(draft));return last_error==4||last_error==5?BAD:last_error;
}
static u8 record_at_impl(u16 i){
 u8 error=seek(40UL+(u32)i*20);
 return error?error:rd(meta,20);
}
#ifdef __CC65__
#pragma code-name(push,"CODE")
#endif
#if defined(PREVIEW) && !defined(__CC65__)
static u8 record_at(u16 i){return record_at_impl(i);}
#else
static u8 record_at(u16 i){u8 bank=sysGetBank(),e;sysSetBank(GROUP_BANK);e=record_at_impl(i);sysSetBank(bank);return e;}
#endif
#ifdef __CC65__
#pragma code-name(pop)
#endif
static u8 unique_ids_impl(void){
 u16 base,end,i,j,k;
 u32 id;
 u8 error;
 /*96 IDs use the borrowed384B buffer. No whole-file/ID-array allocation. */
 for(base=0;base<groups;base=end){
  end=base+96;if(end>groups)end=groups;
  for(i=base;i<groups;++i){
   error=record_at(i);if(error)return error;
   id=pack_key(meta);
   for(j=base;j<i&&j<end;++j)
    if(pack_key(work+(j-base)*4)==id)return BAD;
   if(i<end)for(k=0;k<4;++k)work[(i-base)*4+k]=meta[k];
  }
 }
 return 0;
}
#ifdef __CC65__
#pragma code-name(push,"CODE")
#endif
#if defined(PREVIEW) && !defined(__CC65__)
static u8 unique_ids(void){return unique_ids_impl();}
#else
static u8 unique_ids(void){u8 bank=sysGetBank(),e;sysSetBank(GROUP_BANK);e=unique_ids_impl();sysSetBank(bank);return e;}
#endif
static u8 validate(void);
#ifdef __CC65__
#pragma code-name(pop)
#ifdef PREVIEW
#pragma code-name(push,"BNK17")
#pragma rodata-name(push,"BNK17")
#else
/* Full TCG validation is separate from interaction; RAM operands only. */
#pragma code-name(push,"BNK05")
#pragma rodata-name(push,"BNK05")
#endif
#endif
/* Named data stays with validation; cc65 pools anonymous literals at EOF. */
static const char tcg_magic[]="TCG1";
static u8 kind(u32 a){
 if(a && a<0x100000UL)return 1;
 if((a>=0xf82000UL&&a<=0xf83fffUL)||(a>=0x1f0000UL&&a<=0x1f1fffUL))return 2;
 return 0;
}
static u8 validate_impl(void){
 u16 i,j,n,first,next_code=0,length;
 u32 checksum,crc=0xffffffffUL,left,next_name,address;
 u8 error,bit,reason,k;
 if(memcmp(meta,tcg_magic,4)||meta[4]!=1||meta[5]!=20||word(meta+6)!=40||word(meta+26))return BAD;
 groups=word(meta+24);codes_at=pack_key(meta+28);names_at=pack_key(meta+32);
 if(!groups||groups>1024||pack_key(meta+36)!=file_bytes||codes_at!=40UL+(u32)groups*20||codes_at>names_at||names_at>file_bytes||((names_at-codes_at)&7))return BAD;
 checksum=pack_key(meta+20);error=seek(40);if(error)return error;
 for(left=file_bytes-40;left;left-=n){
  n=left>384?384:(u16)left;error=rd(work,n);if(error)return error;
  for(i=0;i<n;++i){crc^=work[i];for(bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320UL:0);}
 }
 if((crc^0xffffffffUL)!=checksum)return BAD;
 error=unique_ids();if(error)return error;
 next_name=names_at;
 for(i=0;i<groups;++i){
  error=record_at(i);if(error)return error;
  first=word(meta+4);n=word(meta+6);length=word(meta+12);reason=meta[14];
  if(first!=next_code||n>16||(!n&&!reason)||(reason&~31)||meta[15]||pack_key(meta+16)||((reason&1)&&n)||pack_key(meta+8)!=next_name||!length||length>120||next_name+length>file_bytes||codes_at+(u32)(first+n)*8>names_at)return BAD;
  error=seek(codes_at+(u32)first*8);if(error)return error;
  for(j=0;j<n;++j){
   error=rd(meta,8);if(error)return error;
   address=pack_key(meta);k=kind(address);
   if(address>0xffffffUL||meta[5]!=k||meta[6]||meta[7]||((!k||(game_record->kind==2&&k!=2))&&!(reason&8)))return BAD;
  }
  error=seek(next_name);if(error)return error;
  error=rd(work,length);if(error)return error;
  for(j=0;j<length;++j)if(work[j]<32||work[j]>=127)return BAD;
  next_code+=n;next_name+=length;
 }
 return codes_at+(u32)next_code*8==names_at&&next_name==file_bytes?0:BAD;
}
#ifdef __CC65__
#pragma code-name(pop)
#pragma rodata-name(pop)
#pragma code-name(push,"CODE")
#endif
#if defined(PREVIEW) && !defined(__CC65__)
static u8 validate(void){return validate_impl();}
#else
static u8 validate(void){u8 bank=sysGetBank(),e;
#ifdef PREVIEW
 sysSetBank(21);
#else
 sysSetBank(9);
#endif
 e=validate_impl();sysSetBank(bank);return e;}
#endif
#ifdef __CC65__
#pragma code-name(pop)
#endif
static u8 open_tcg(void){
#ifndef PREVIEW
 char path[sizeof(TEO_CHEAT_ROOT "/00000000.tcg")];u8 i;u32 key=pack_key(game_record->reserved);
 static const char hex[]="0123456789ABCDEF";
#else
 NativeBridgeTarget target;
#endif
 u8 error;
 if(card_io_fault())return CARD_IO_BLOCKED;
#ifdef PREVIEW
 error=preview_cheat_open(game_index,pack_key(game_record->reserved),&target,&file_bytes);
 if(!error){if(!game_target.size)game_target=target;else if(target.size!=game_target.size||target.crc32!=game_target.crc32){handle=1;return BAD;}}
#else
 strcpy(path,TEO_CHEAT_ROOT "/00000000.tcg");
 for(i=0;i<8;++i){path[sizeof(TEO_CHEAT_ROOT "/")-1+7-i]=hex[key&15];key>>=4;}
 error=ed_cmd_file_open((u8*)path,FA_READ);
#endif
 if(error)return error;
 handle=1;
#ifndef PREVIEW
 file_bytes=ed_cmd_file_available();
#endif
 if(file_bytes<40||file_bytes>262144UL)return BAD;
 error=rd(meta,40);if(error)return error;
 if(pack_key(meta+8)!=pack_key(game_record->reserved)||pack_key(meta+12)!=game_target.size||pack_key(meta+16)!=game_target.crc32)return BAD;
 return 0;
}
static u8 find_group(u32 id,u16 *position){
 u16 i;u8 error;
 for(i=0;i<groups;++i){error=record_at(i);if(error)return error;if(pack_key(meta)==id){if(position)*position=i;return 0;}}
 return BAD;
}
/* Policy is separate from immutable TCG metadata, so old ON/MIXED drafts
 * remain readable and can be cleared after a release introduces a hold. */
static u8 policy_held(void){return game_record->kind==2&&cheat_held(pack_key(game_record->reserved),pack_key(meta),game_target.size,game_target.crc32);}
/* Filter only the view: active groups always remain reachable for disabling. */
static u8 visible(void){return show_held||(!meta[14]&&!policy_held())||cheat_group_state(&draft,pack_key(meta))!=CHEAT_OFF;}
static u8 risk_allowed(void){u8 i;u32 id=pack_key(meta);for(i=0;i<draft.count;++i)if(draft.ids[i]==id)return (draft.risk&(1U<<i))!=0;return 0;}
static u8 refresh_visible(void){
 u16 i,last=0,choice=0;u8 error,found=0;
 visible_count=visible_selected=0;
 for(i=0;i<groups;++i){error=record_at(i);if(error)return error;if(!visible())continue;
  if(!found&&i>=selected){choice=i;visible_selected=visible_count;found=1;}last=i;++visible_count;
 }
 if(visible_count){if(found)selected=choice;else{selected=last;visible_selected=visible_count-1;}}else selected=0;
 return 0;
}
static u8 move_visible(u8 forward,u8 steps){
 u16 next=selected;u8 error;
 while(steps&&visible_count){
  if(forward){if(next+1>=groups)break;++next;}else{if(!next)break;--next;}
  error=record_at(next);if(error)return error;if(visible()){selected=next;--steps;}
 }
 return 0;
}
static u8 observed_valid(void){
 u8 i,error;
 if(cheat_slots(&draft)==CHEAT_INVALID)return BAD;
 for(i=0;i<draft.count;++i){
  error=find_group(draft.ids[i],0);if(error)return error;
  if(meta[14]||word(meta+6)!=draft.costs[i])return BAD;
 }
 return 0;
}
static u8 open_impl(u16 index,const CatalogRecord *record,u8 *workspace){
#ifndef PREVIEW
 NativeBridgeReport report;
#endif
 u8 error;
 if(card_io_fault()){close_fault=card_io_fault();last_error=CARD_IO_BLOCKED;ready=0;return CARD_IO_BLOCKED;}
 if(handle||close_fault)return 0xe3;
 ready=last_error=dirty=read_only=note_pending=show_held=options=option=list_drawn=0;native_state=NATIVE_CHEATS_UNKNOWN;message=0;selected=groups=0;memset(&draft,0,sizeof(draft));
 if(!record||!workspace||(record->kind!=1&&record->kind!=2))return fail(LAUNCH_ERR_UNSUPPORTED);
 game_record=record;game_index=index;game_title=record->title;work=workspace;
#ifdef PREVIEW
 game_target.size=0;error=0;
#else
 error=launch_metadata(index,record,&game_target,&report);
 if(error&&report.stage==LAUNCH_STAGE_CLOSE){handle=1;close_fault=error;}
#endif
 if(error)return fail(error);
 error=open_tcg();if(error){last_error=error;return handle?fail(error):error;}
 package_crc=pack_key(meta+20);
 error=validate();if(error)return fail(error);
 error=finish(0);if(error)return fail(error);
 error=cheat_store_load(index,record,&game_target,package_crc,&draft,work);
 read_only=(cheat_store_flags()&CHEAT_STORE_READONLY)!=0;
 if(cheat_store_flags()&CHEAT_STORE_UNCERTAIN){close_fault=error?error:CHEAT_STORE_ERR_CLOSE;return fail(close_fault);}
 if(error&&!(read_only&&error==CHEAT_STORE_ERR_FOREIGN))return fail(error);
#ifndef PREVIEW
 native_state=native_bridge_cheat_state();
 if(card_io_fault()){close_fault=card_io_fault();return fail(CARD_IO_BLOCKED);}
#endif
 error=open_tcg();if(error)return fail(error);
 if(pack_key(meta+20)!=package_crc)return fail(BAD);
 error=validate();if(!error)error=observed_valid();if(error)return fail(error);
 ready=1;return 0;
}
#ifdef __CC65__
#pragma code-name(push,"CODE")
#endif
static void text(u8 x,u8 y,const char *s,u8 n){
 gSetXY(x,y);while(n--&&*s)gAppendChar((u8)*s++);
}
static void wrapped(u8 y,const char *s,u8 rows){
 u8 n,space;
 while(*s&&rows--){
  while(*s==' ')++s;n=space=0;
  while(n<38&&s[n]){if(s[n]==' ')space=n;++n;}
  if(n==38&&s[n]&&space)n=space;
  text(1,y++,s,n);s+=n;while(*s==' ')++s;
 }
}
#ifdef __CC65__
#pragma code-name(pop)
#endif
/* All group UI strings belong to the mapped group code bank. */
static const char ui_unknown[]="UNKNOWN";
static const char native_unknown[]="EVERDRIVE STATUS: UNKNOWN";
#ifndef PREVIEW
static const char native_off[]="EVERDRIVE CHEATS: OFF";
#endif
#if defined(__CC65__) && !defined(PREVIEW)
#pragma rodata-name(push,"BNK01")
#endif
#ifndef PREVIEW
/* Used only inside this mapped group bank; spare room here saves resident PRG. */
static const char native_on[]="EVERDRIVE CHEATS: ON";
static const char native_enable[]="Enable Cheats in EverDrive Options";
#endif
static const char why_policy[]="HELD / I DETAILS OR OFF";
static const char why_malformed[]="INCOMPLETE OR INVALID SOURCE";
static const char why_count[]="SOURCE GROUP EXCEEDS 16 CODES";
static const char why_flags[]="UNSUPPORTED SOURCE FLAGS";
static const char why_active[]="SOURCE WAS ALREADY ACTIVE";
static const char why_address[]="ADDRESS OUTSIDE NATIVE RANGE";
static const char error_title[]="GROUP FILE ERROR / DRAFT DISCARDED";
static const char error_close[]="FILE CLOSE NOT CONFIRMED";
static const char error_restart[]="RESTART REQUIRED";
static const char error_footer[]="Restart console to continue";
static const char error_back[]="II Back";
static const char ui_heading[]="TEOPCE / CHEAT GROUPS";
static const char ui_note_heading[]="CODE DETAILS";
static const char ui_note_confirm[]="I Enable  II Cancel";
static const char ui_draft[]="CHANGES APPLY ON RUN";
#ifdef PREVIEW
static const char ui_readonly[]="PREVIEW / NATIVE APPLY DISABLED";
#else
static const char ui_readonly[]="READ ONLY / FOREIGN NATIVE FILE";
#endif
static const char ui_readonly_controls[]="I Info  SEL Options  II Back";
static const char ui_readonly_paging[]="L/R Page / no native changes";
static const char ui_notsaved[]="NOT SAVED / VERIFY ERROR";
static const char ui_held[]="HELD";
static const char ui_on[]="ON";
static const char ui_mixed[]="MIXED";
static const char ui_off[]="OFF";

static const char ui_slots_word[]="slots";
static const char ui_slots[]="Slots ";
static const char ui_active[]="Active ";
#if defined(PREVIEW) && defined(__CC65__)
/* The cartridge preview store always returns READONLY; mutable controls are dead. */
#define ui_controls ui_readonly_controls
#define ui_paging ui_readonly_paging
#else
static const char ui_controls[]="I Toggle  SEL Options  II Cancel";
static const char ui_paging[]="L/R Page  RUN Save and back";
#endif
static const char *held_reason(u8 reason){
 if(reason&1)return why_malformed;
 if(reason&16)return why_count;
 if(reason&2)return why_flags;
 if(reason&4)return why_active;
 if(reason&8)return why_address;
 return why_policy;
}
static void draw_error(void){
 list_drawn=0;gCleanScreen();gSetPal(PAL_OK);text(2,1,ui_heading,36);
 gSetPal(PAL_ER);text(1,6,error_title,38);
 gSetXY(1,8);gAppendHex8(last_error);
 if(close_fault){text(1,11,error_close,38);text(1,13,error_restart,38);}
 gSetPal(PAL_00);ui_hint_line(25,close_fault?error_footer:error_back);
}
#if defined(__CC65__) && !defined(PREVIEW)
#pragma rodata-name(pop)
#endif
static void draw_impl(void){
 u16 first,i,length,ordinal;u8 row,error,state,cost,reason,selected_reason=0,selected_note=0;
 if(card_io_fault()){close_fault=card_io_fault();last_error=CARD_IO_BLOCKED;ready=0;}
 if(!ready){draw_error();return;}
 if(options){
  list_drawn=0;gCleanScreen();gSetPal(PAL_OK);text(1,2,"CHEAT OPTIONS",38);
  gSetPal(option==0?PAL_OK:PAL_01);if(!option)gFillRow(' ',1,6,38);
  text(2,6,read_only?"All cheats off (read only)":"All cheats off",36);
  gSetPal(option==1?PAL_OK:PAL_01);if(option)gFillRow(' ',1,9,38);text(2,9,show_held?"Hide held codes":"Show held codes",36);
  gSetPal(PAL_01);wrapped(13,option?"HELD: codes with known issues or unsupported data. Active codes stay visible.":"Turn off every code for this game and save. No extra RUN needed.",4);
  gSetPal(PAL_00);ui_hint_line(25,"I Choose  II Back");return;
 }
 if(note_pending){
  error=record_at(selected);if(error){fail(error);draw_error();return;}
  list_drawn=0;gCleanScreen();gSetPal(PAL_OK);text(1,2,meta[14]?"UNSUPPORTED CODE":policy_held()?"HELD CODE":ui_note_heading,38);gSetPal(PAL_01);
  if(meta[14]){wrapped(5,held_reason(meta[14]),3);wrapped(10,"Cannot be enabled on this cartridge.",3);}
  else{
   if(!cheat_note(pack_key(game_record->reserved),pack_key(meta),game_target.size,game_target.crc32,(char*)work,240)){fail(BAD);draw_error();return;}
   wrapped(5,(char*)work,8);
   if(policy_held())wrapped(15,"May stop loading or disrupt play. Enable at your own risk. Incompatible combinations stay blocked.",5);
  }
  gSetPal(PAL_00);ui_hint_line(25,(read_only||meta[14])?error_back:policy_held()?"I Accept risk  II Cancel":ui_note_confirm);return;
 }
 /* Fetch the complete visible page before touching VRAM. The previous frame
  * stays visible through SD reads; no new resident page/name buffer is needed. */
 error=refresh_visible();if(error){fail(error);draw_error();return;}
 first=(visible_selected/3)*3;ordinal=0;
 if(visible_count){
  error=record_at(selected);if(error){fail(error);draw_error();return;}
  selected_reason=meta[14]|(policy_held()?32:0);
  if(!selected_reason)selected_note=cheat_note(pack_key(game_record->reserved),pack_key(meta),game_target.size,game_target.crc32,(char*)work,240);
 }
 row=0;
 for(i=0;i<groups&&row<3;++i){
  error=record_at(i);if(error){fail(error);draw_error();return;}
  if(!visible())continue;if(ordinal++<first)continue;
  length=(u16)row*128;cost=(u8)word(meta+6);reason=meta[14]|(policy_held()?32:0);
  state=cheat_group_state(&draft,pack_key(meta));
  work[length+121]=cost;work[length+122]=reason;work[length+123]=state;work[length+124]=risk_allowed();
  error=seek(pack_key(meta+8));if(!error)error=rd(work+length,word(meta+12));
  if(error){fail(error);draw_error();return;}work[length+word(meta+12)]=0;++row;
 }
 if(!list_drawn){
  gCleanScreen();gSetPal(PAL_OK);text(2,1,ui_heading,36);
  gSetPal(PAL_01);wrapped(2,game_title,3);
  gSetPal(PAL_00);text(1,5,read_only?ui_readonly:ui_draft,38);
 }
 gSetPal(PAL_01);gFillRect(' ',1,6,38,15);
 gSetPal(PAL_00);gFillRect(' ',1,22,38,3);
 if(!visible_count){text(1,8,"NO VISIBLE CODES",38);text(1,10,"SEL Options: Show held codes",38);}
 for(i=0;i<row;++i){
  length=i*128;cost=work[length+121];reason=work[length+122];state=work[length+123];
  gSetPal(i==visible_selected-first?PAL_OK:PAL_01);
  if(i==visible_selected-first)gFillRow(' ',1,6+(u8)i*5,38);
  text(1,6+(u8)i*5,i==visible_selected-first?">":" ",1);
  text(3,6+(u8)i*5,read_only?ui_unknown:reason?(state==CHEAT_ON||state==CHEAT_MIXED?(work[length+124]?"RISK ON":"HELD ON"):ui_held):state==CHEAT_ON?ui_on:state==CHEAT_MIXED?ui_mixed:ui_off,7);
  gSetXY(10,6+(u8)i*5);gAppendNum(cost);text(13,6+(u8)i*5,ui_slots_word,5);
  gSetPal(PAL_01);wrapped(7+(u8)i*5,(char*)work+length,4);
 }
 list_drawn=1;
 gSetPal(PAL_00);text(1,22,ui_slots,6);
 if(read_only)text(7,22,ui_unknown,7);else{gAppendNum(cheat_slots(&draft));gAppendChar('/');gAppendNum(16);}
 text(15,22,ui_active,7);if(read_only)text(22,22,ui_unknown,7);else gAppendNum(cheat_active(&draft));
 if(message||selected_reason){gSetPal(PAL_ER);text(1,23,message?message:held_reason(selected_reason),38);}
#ifndef PREVIEW
 else if(native_state==NATIVE_CHEATS_OFF){gSetPal(PAL_ER);text(1,23,native_enable,38);}
#endif
 if(!message&&!selected_reason&&selected_note==CHEAT_NOTE_INFO&&native_state!=NATIVE_CHEATS_OFF){gSetPal(PAL_00);text(1,23,"PROTECTED START",38);}
 gSetPal(PAL_00);
#ifdef PREVIEW
 text(1,24,native_unknown,28);
#else
 text(1,24,native_state==NATIVE_CHEATS_ON?native_on:native_state==NATIVE_CHEATS_OFF?native_off:native_unknown,28);
#endif
 gSetXY(30,24);gAppendNum(visible_count?visible_selected+1:0);gAppendChar('/');gAppendNum(visible_count);
 ui_hint_line(25,read_only?ui_readonly_controls:ui_controls);
 ui_hint_line(26,read_only?ui_readonly_paging:ui_paging);
}
static u8 event_impl(u8 event){
 u8 error,rows,disable,i;u32 conflict;
 if(!event)return 0;
 if(card_io_fault()){close_fault=card_io_fault();last_error=CARD_IO_BLOCKED;ready=0;}
 if(close_fault){draw_error();return 0;}
 if(options){
  if(event&JOY_A)options=0;
  else if(event&(JOY_U|JOY_D))option^=1;
  else if(event&JOY_B){
   if(option){show_held^=1;if(show_held)selected=0;options=0;}
   else if(!read_only){memset(&draft,0,sizeof(draft));dirty=1;options=0;return event_impl(JOY_STA);}
  }
  draw_impl();return 0;
 }
 if(note_pending){
  if(event&JOY_A){note_pending=0;draw_impl();return 0;}
  if(read_only||!(event&JOY_B))return 0;
  error=record_at(selected);if(error){fail(error);draw_error();return 0;}
  if(meta[14])return 0;
  note_pending=0;
  error=cheat_toggle(&draft,pack_key(meta),(u8)word(meta+6),0);
  if(!error&&policy_held())draft.risk|=1U<<(draft.count-1);
  if(error)message=error==CHEAT_LIMIT?"16 SLOTS MAX / WHOLE GROUP KEPT":error==CHEAT_HELD?0:"INVALID GROUP / DRAFT UNCHANGED";else dirty=1;
  draw_impl();return 0;
 }
 if(event&JOY_A){last_error=finish(0);ready=0;memset(&draft,0,sizeof(draft));if(last_error){draw_error();return 0;}return 1;}
 if(event&JOY_STA){
  if(read_only){message="READ ONLY / NO FILES CHANGED";draw_impl();return 0;}
  if(!ready||!dirty){last_error=finish(0);ready=0;if(last_error){draw_error();return 0;}return 1;}
  for(i=0;i<draft.count;++i){
   error=find_group(draft.ids[i],0);if(error){fail(error);draw_error();return 0;}
   if(policy_held()&&!(draft.risk&(1U<<i))){error=find_group(draft.ids[i],&selected);if(error){fail(error);draw_error();return 0;}message="REMOVE HELD CODE / I OFF / RUN SAVE";draw_impl();return 0;}
  }
  error=cheat_cache_rows(&draft,groups,codes_at,work,meta,&rows,&conflict);
  if(error==CHEAT_CACHE_CONFLICT){
   error=find_group(conflict,&selected);if(error){fail(error);draw_error();return 0;}
   message=conflict_prompt;draw_impl();return 0;
  }
  if(error){fail(error);draw_error();return 0;}
#ifndef PREVIEW
  if(game_record->kind==2){
   error=cd_guard_check_rows(game_record,&game_target,work,rows);
   if(error){message=error==CD_GUARD_ERR_SELECTION?"CODES CANNOT BE COMBINED / CHANGE SET":"PROTECTION NEEDS MATCHING DISC";draw_impl();return 0;}
  }
#endif
  error=finish(0);if(error){fail(error);draw_error();return 0;}
  error=cheat_store_apply(game_index,game_record,&game_target,package_crc,&draft,rows,work);
  if(cheat_store_flags()&CHEAT_STORE_UNCERTAIN)close_fault=error?error:CHEAT_STORE_ERR_CLOSE;
  if(error||cheat_store_flags()&(CHEAT_STORE_PENDING|CHEAT_STORE_UNCERTAIN)){
   last_error=error?error:CHEAT_STORE_ERR_PENDING;ready=0;draw_error();if(!close_fault){gSetPal(PAL_ER);text(1,10,ui_notsaved,38);}return 0;
  }
  ready=dirty=0;return draft.count?2:3;
 }
 if(!ready)return 0;
 disable=message==conflict_prompt;message=0;
 if(event&JOY_SEL){
  options=1;option=0;
 }else if((event&JOY_B)&&visible_count){
  error=record_at(selected);if(error){fail(error);draw_error();return 0;}
  if(policy_held()&&cheat_group_state(&draft,pack_key(meta))!=CHEAT_OFF)disable=1;
  if(meta[14]){note_pending=1;draw_impl();return 0;}
  if(read_only||(!disable&&!meta[14]&&cheat_group_state(&draft,pack_key(meta))!=CHEAT_ON)){
   error=cheat_note(pack_key(game_record->reserved),pack_key(meta),game_target.size,game_target.crc32,(char*)work,240);
   if(error&&(read_only||error==CHEAT_NOTE_ADVICE)){note_pending=1;draw_impl();return 0;}
  }
  if(read_only){message="NO ADVISORY";draw_impl();return 0;}
  error=disable?cheat_disable(&draft,pack_key(meta)):cheat_toggle(&draft,pack_key(meta),(u8)word(meta+6),meta[14]|(policy_held()?32:0));
  if(error)message=error==CHEAT_LIMIT?"16 SLOTS MAX / WHOLE GROUP KEPT":error==CHEAT_HELD?0:"INVALID GROUP / DRAFT UNCHANGED";
  else dirty=1;
 }else if(event&(JOY_U|JOY_D|JOY_L|JOY_R)){
  error=move_visible((event&(JOY_D|JOY_R))!=0,(event&(JOY_L|JOY_R))?3:1);
  if(error){fail(error);draw_error();return 0;}
 }
 draw_impl();return 0;
}
#if defined(__CC65__)
#pragma code-name(pop)
#endif
u8 cheat_draft_open(u16 index,const CatalogRecord *record,u8 *workspace){
#if defined(PREVIEW) && !defined(__CC65__)
 return open_impl(index,record,workspace);
#else
 u8 bank=sysGetBank(),result;sysSetBank(GROUP_BANK);result=open_impl(index,record,workspace);sysSetBank(bank);return result;
#endif
}
void cheat_draft_draw(void){
#if defined(PREVIEW) && !defined(__CC65__)
 draw_impl();
#else
 u8 bank=sysGetBank();sysSetBank(GROUP_BANK);draw_impl();sysSetBank(bank);
#endif
}
u8 cheat_draft_event(u8 event){
#if defined(PREVIEW) && !defined(__CC65__)
 return event_impl(event);
#else
 u8 bank=sysGetBank(),result;sysSetBank(GROUP_BANK);result=event_impl(event);sysSetBank(bank);return result;
#endif
}
