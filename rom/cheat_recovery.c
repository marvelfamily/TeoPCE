#include "ui_hint.h"
#include "card_io.h"
#include "cheat_recovery.h"
#if defined(PREVIEW) && defined(__CC65__)
void cheat_recovery_reset(void){}
void cheat_recovery_draw(void){}
u8 cheat_recovery_event(u8 event,u8 *work){(void)event;(void)work;return 0;}
#else
#include "cheat_recovery.h"
#include "cheat_store.h"
static u8 failed;
#if defined(__CC65__) && !defined(PREVIEW)
#pragma code-name(push,"BNK04")
#pragma rodata-name(push,"BNK04")
#endif
static const char restart_title[]="TEOPCE / RESTART REQUIRED";
static const char recovery_title[]="TEOPCE / CHEAT RECOVERY";
static const char sd_close_error[]="SD FILE CLOSE IS NOT CONFIRMED";
static const char state_error[]="CHEAT FILE STATE IS NOT VERIFIED";
static const char pending[]="UNFINISHED CHEAT FILE TRANSACTION";
static const char restart_note[]="Turn the console off, then on.";
static const char restore_note[]="I restores the saved previous files.";
static const char blocked[]="Starting games and Return are blocked.";
static const char restart_footer[]="Restart console to continue";
static const char restore_footer[]="I Restore previous cheats";
static void text(u8 x,u8 y,const char *s){u8 n=38;gSetXY(x,y);while(*s&&n--)gAppendChar(*s++);}
static void draw_impl(void){
 u8 restart=failed||(cheat_store_flags()&CHEAT_STORE_UNCERTAIN);
 gCleanScreen();gSetPal(PAL_OK);text(2,1,restart?restart_title:recovery_title);
 gSetPal(PAL_ER);text(1,8,card_io_fault()?sd_close_error:restart?state_error:pending);
 gSetPal(PAL_01);text(1,11,restart?restart_note:restore_note);text(1,14,blocked);
 gSetPal(PAL_00);ui_hint_line(25,restart?restart_footer:restore_footer);
}
static u8 event_impl(u8 event,u8 *work){
 u8 result;
 if(card_io_fault())return 0;
 if(!(event&JOY_B)||failed||(cheat_store_flags()&CHEAT_STORE_UNCERTAIN))return 0;
 result=cheat_store_recover(1,work);
 if(!result&&!(cheat_store_flags()&(CHEAT_STORE_PENDING|CHEAT_STORE_UNCERTAIN)))return 1;
 failed=1;draw_impl();return 0;
}
#if defined(__CC65__) && !defined(PREVIEW)
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
void cheat_recovery_reset(void){failed=0;}
void cheat_recovery_draw(void){
#ifdef PREVIEW
 draw_impl();
#else
 u8 bank=sysGetBank();sysSetBank(8);draw_impl();sysSetBank(bank);
#endif
}
u8 cheat_recovery_event(u8 event,u8 *work){
#ifdef PREVIEW
 return event_impl(event,work);
#else
 u8 bank=sysGetBank(),result;sysSetBank(8);result=event_impl(event,work);sysSetBank(bank);return result;
#endif
}

#endif
