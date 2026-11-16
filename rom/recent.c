#include "recent.h"
#include "app_paths.h"
#include "card_io.h"
#include "preferences.h"
#include "pack.h"
#include <string.h>
#ifdef __CC65__
#ifdef PREVIEW
#define RECENT_BANK 19
#pragma code-name(push,"BNK15")
#pragma rodata-name("BNK15")
#else
#define RECENT_BANK 12
#pragma code-name(push,"BNK08")
#pragma rodata-name("BNK08")
#endif
#endif
#ifndef PREVIEW
static void put32(u8 *p,u32 value){u8 i;for(i=0;i<4;++i){p[i]=(u8)value;value>>=8;}}
static u32 checksum(const u8 *work){u8 i;u32 value=2166136261UL;for(i=0;i<44;++i)value=((value^work[i])*16777619UL)&0xffffffffUL;return value;}
static u8 *path(u8 slot){return (u8*)(slot?TEO_USER_ROOT "/recent1.bin":TEO_USER_ROOT "/recent0.bin");}
static u8 valid(const u8 *work,u8 slot){
 u8 i,j;if(memcmp(work,"TRCT",4)||work[4]!=1||work[5]>RECENT_MAX||work[6]||work[7]||(pack_key(work+8)&1)!=slot||pack_key(work+44)!=checksum(work))return 0;
 for(i=0;i<RECENT_MAX;++i){if(i>=work[5]){if(pack_key(work+12+i*4))return 0;}else for(j=0;j<i;++j)if(pack_key(work+12+i*4)==pack_key(work+12+j*4))return 0;}
 return 1;
}
static u8 finish(u8 error){u8 closed=card_io_close(ed_cmd_file_close);return closed?closed:error;}
static u8 read_slot(u8 slot,u8 *work){u8 error;if(card_io_fault())return CARD_IO_BLOCKED;error=ed_cmd_file_open(path(slot),FA_READ);if(error)return error;error=ed_cmd_file_available()!=RECENT_BYTES?RECENT_BAD_DATA:ed_cmd_file_read(work,RECENT_BYTES);error=finish(error);if(!error&&!valid(work,slot))error=RECENT_BAD_DATA;return error;}
static u8 save(Recent *state,u8 *work){
 u32 sequence=(state->sequence+1)&0xffffffffUL;u8 slot=(u8)(sequence&1),i,error;
 memset(work,0,RECENT_BYTES);memcpy(work,"TRCT",4);work[4]=1;work[5]=state->count;put32(work+8,sequence);
 for(i=0;i<state->count;++i)put32(work+12+i*4,state->keys[i]);put32(work+44,checksum(work));
 error=ed_cmd_dir_make((u8*)TEO_USER_ROOT);if(error&&error!=8)return error;
 error=ed_cmd_file_open(path(slot),FA_WRITE|FA_CREATE_ALWAYS);if(error)return error;
 error=ed_cmd_file_write(work,RECENT_BYTES);error=finish(error);if(error)return error;
 error=read_slot(slot,work);if(error)return error;
 if(pack_key(work+8)!=sequence||work[5]!=state->count)return RECENT_VERIFY_FAILED;
 for(i=0;i<state->count;++i)if(pack_key(work+12+i*4)!=state->keys[i])return RECENT_VERIFY_FAILED;
 state->sequence=sequence;return 0;
}
#endif
static u8 load_impl(Recent *state,u8 *work){
#ifndef PREVIEW
 u8 slot,error,have=0,bad=0,first=0,i;
#endif
 if(card_io_fault())return CARD_IO_BLOCKED;
 memset(state,0,sizeof(*state));
#ifdef PREVIEW
 (void)work;return 0;
#else
 for(slot=0;slot<2;++slot){
  error=read_slot(slot,work);if(card_io_fault()){state->read_error=error;return error;}
  if(!error){if(!have||prefs_newer(pack_key(work+8),state->sequence)){state->sequence=pack_key(work+8);state->count=work[5];for(i=0;i<RECENT_MAX;++i)state->keys[i]=pack_key(work+12+i*4);}have=1;}
  else if(error==RECENT_BAD_DATA)bad=1;else if(error!=4&&error!=5&&!first)first=error;
 }
 state->read_error=first?first:!have&&bad?RECENT_BAD_DATA:0;return state->read_error;
#endif
}
static u8 opened_impl(Recent *state,u32 key,u8 *work){
 u8 rank,i;
 if(card_io_fault())return CARD_IO_BLOCKED;
 if(state->count>RECENT_MAX)return RECENT_BAD_DATA;
 rank=recent_rank(state,key);if(!rank)return state->write_error;
 if(rank==0xff){if(state->count<RECENT_MAX)++state->count;rank=state->count-1;}
 for(i=rank;i;--i)state->keys[i]=state->keys[i-1];state->keys[0]=key;
 if(state->read_error){state->write_error=state->read_error;return state->write_error;}
#ifdef PREVIEW
 (void)work;state->write_error=0;
#else
 state->write_error=save(state,work);
#endif
 return state->write_error;
}
#ifdef __CC65__
#pragma code-name(pop)
#endif
u8 recent_rank(const Recent *state,u32 key){u8 i;if(!state||state->count>RECENT_MAX)return 0xff;for(i=0;i<state->count;++i)if(state->keys[i]==key)return i;return 0xff;}
u8 recent_load(Recent *state,u8 *work){
#ifdef __CC65__
 u8 bank=sysGetBank(),result;sysSetBank(RECENT_BANK);result=load_impl(state,work);sysSetBank(bank);return result;
#else
 return load_impl(state,work);
#endif
}
u8 recent_opened(Recent *state,u32 key,u8 *work){
#ifdef __CC65__
 u8 bank=sysGetBank(),result;sysSetBank(RECENT_BANK);result=opened_impl(state,key,work);sysSetBank(bank);return result;
#else
 return opened_impl(state,key,work);
#endif
}
