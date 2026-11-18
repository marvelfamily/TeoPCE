#include "cheat_cd.h"
#include "cheat_notes.h"
#include "cheat_store_codec.h"
#include "cheat_store_io.h"
#include "app_paths.h"
#include "card_io.h"
#include "launch.h"
#include "pack.h"
#include <string.h>
#ifndef PREVIEW
static u8 *work,*status;
#define A ((char*)work+256)
#define B ((char*)work+336)
#define NATIVE ((char*)work+64)
#define CD_ROOT TEO_USER_ROOT "/cd"
static const char pending[]=TEO_USER_ROOT "/cdtxn.bin";
static const u8 blank[]={32,13,10};
/* IO helpers remap their code bank: pass resident strings or RAM only. */
static const char user_root[]=TEO_USER_ROOT;
static const char cd_root[]=CD_ROOT;
typedef char cd_blob_path_fits[(sizeof(CD_ROOT "/B")+20<=80)?1:-1];
#ifdef __CC65__
#pragma code-name(push,"BNK09")
#pragma rodata-name("BNK09")
#endif
static u16 word(const u8*p){return (u16)p[0]|((u16)p[1]<<8);}
static void put16(u8*p,u16 n){p[0]=(u8)n;p[1]=(u8)(n>>8);}
static void put32(u8*p,u32 n){u8 i;for(i=0;i<4;++i){p[i]=(u8)n;n>>=8;}}
static u8 usable(void){if(card_io_fault()){*status|=CHEAT_STORE_UNCERTAIN;return CARD_IO_BLOCKED;}return *status&CHEAT_STORE_UNCERTAIN?CHEAT_STORE_ERR_CLOSE:0;}
static u8 zero(const u8*p,u8 n){while(n--)if(*p++)return 0;return 1;}
static void hex(char*p,u32 n){static const char digits[]="0123456789ABCDEF";u8 i;for(i=0;i<8;++i){p[7-i]=digits[n&15];n>>=4;}}
static void blob_path(u32 n,u32 crc){u8 at=sizeof(CD_ROOT "/B")-1;strcpy(B,CD_ROOT "/B");hex(B+at,crc);hex(B+at+8,n);strcpy(B+at+16,".txt");}
static u8 dir(const char*p){u8 found,e;u32 n;e=cs_io_meta(p,&n,&found,1);if(e||found)return e;e=ed_cmd_dir_make((u8*)p);if(e&&e!=8)return e;e=cs_io_meta(p,&n,&found,1);return e?e:found?0:CHEAT_STORE_ERR_VERIFY;}
static u8 prefix_equal(const char*full,u32 size,const char*partial,u32 prefix){u32 at=0;u16 count;u8 e;
 while(at<prefix){count=prefix-at>64?64:(u16)(prefix-at);e=cs_io_read(full,size,at,work+416,count);if(e)return e;e=cs_io_read(partial,prefix,at,work+480,count);if(e)return e;if(memcmp(work+416,work+480,count))return CHEAT_STORE_ERR_VERIFY;at+=count;}return 0;
}
static u8 blob(const char*source,u32 n,u32 crc){char damaged[80];u8 found,e;u32 size,actual,qn;
 blob_path(n,crc);e=cs_io_meta(B,&size,&found,0);if(e)return e;
 if(found){
  if(size==n){e=source?cs_io_equal(source,B,n):cs_io_mem(B,blank,3);if(!e)return 0;if(e!=CHEAT_STORE_ERR_VERIFY)return e;}
  if(size>0x1000000UL)return CHEAT_STORE_ERR_FOREIGN;e=cs_io_crc(B,size,&actual);if(e)return e;
  /* A genuine same-size/CRC collision is never overwritten. An interrupted
   * private snapshot is retained verbatim under its actual content identity. */
  if(size==n&&actual==crc)return CHEAT_STORE_ERR_VERIFY;strcpy(damaged,B);blob_path(size,actual);B[sizeof(CD_ROOT "/")-1]='X';
  e=cs_io_meta(B,&qn,&found,0);if(e)return e;
  if(found&&qn<size){
   /* A short quarantine may be an interrupted copy. It is redundant only
    * after proving every byte is a prefix of the still-retained full source. */
   e=prefix_equal(damaged,size,B,qn);if(e)return e;e=ed_cmd_file_del((u8*)B);if(e)return e;e=cs_io_meta(B,&qn,&found,0);if(e||found)return e?e:CHEAT_STORE_ERR_VERIFY;
  }
  if(found){if(qn!=size)return CHEAT_STORE_ERR_VERIFY;e=cs_io_equal(damaged,B,size);}else e=cs_io_copy(damaged,B,size,FA_WRITE|FA_CREATE_NEW);if(e)return e;
  e=cs_io_equal(damaged,B,size);if(e)return e;e=ed_cmd_file_del((u8*)damaged);if(e)return e;e=cs_io_meta(damaged,&qn,&found,0);if(e||found)return e?e:CHEAT_STORE_ERR_VERIFY;blob_path(n,crc);
 }
 return source?cs_io_copy(source,B,n,FA_WRITE|FA_CREATE_NEW):cs_io_write(B,blank,3);
}
static u8 blob_guard(u32 n,u32 expected){u8 found,e;u32 size,crc;blob_path(n,expected);e=cs_io_meta(B,&size,&found,0);if(e)return e;if(!found||size!=n)return CHEAT_STORE_ERR_DATA;e=cs_io_crc(B,n,&crc);return e?e:crc==expected?0:CHEAT_STORE_ERR_DATA;}
static u8 clear(void){u8 e,found;u32 n;e=cs_io_mem(pending,work,256);if(e)return e;e=ed_cmd_file_del((u8*)pending);if(e)return e;e=cs_io_meta(pending,&n,&found,0);if(e)return e;if(found)return CHEAT_STORE_ERR_VERIFY;*status&=(u8)~CHEAT_STORE_PENDING;return 0;}
static u8 cue_guard(const CatalogRecord*r,const NativeBridgeTarget*t){u32 crc;u8 e=cs_io_crc(r->path,t->size,&crc);return e?e:crc==t->crc32?0:CHEAT_STORE_ERR_VERIFY;}
/* Private committed CD carriers must contain only native base-RAM writes.
 * CD RAM/overlay addresses are not ROM offsets in the System Card image. */
static u8 ram_carrier(const char*path,u16 size,u8 rows,u16 bits){u16 at=0,count;u8 i,n=0,seen=0,e;u32 address;
 while(at<size){count=size-at>64?64:size-at;e=cs_io_read(path,size,at,work+416,count);if(e)return e;
  for(i=0;i<count;++i){if(n==63)return CHEAT_STORE_ERR_DATA;work[336+n++]=work[416+i];if(work[416+i]=='\n'){
   if(!cs_parse_line(work+336,n,work+480))return CHEAT_STORE_ERR_DATA;address=pack_key(work+480);
   if(!((address>=0xf82000UL&&address<=0xf83fffUL)||(address>=0x1f0000UL&&address<=0x1f1fffUL)))return CHEAT_STORE_ERR_DATA;
   if(seen>=16||work[485]!=((bits>>seen)&1))return CHEAT_STORE_ERR_DATA;++seen;n=0;
  }}at+=count;
 }return !n&&seen==rows?0:CHEAT_STORE_ERR_DATA;
}
static u8 next(const CatalogRecord*r,const NativeBridgeTarget*t,u16 index,u32*size,u32*crc){u8 p,pp,e,i,shift=0;u16 bits;u32 n,pn,actual;CheatStoreIdentity id;
 if(!cs_native_path(r->path,A))return CHEAT_STORE_ERR_INPUT;strcpy(B,A);strcpy(strrchr(B,'/')+1,"profile.bin");
 /* Both files are optional until this game's first saved selection. FatFs
  * reports NO_PATH (5), not NO_FILE (4), when its private folder is absent.
  * Keep that exception local: required files and transaction IO stay strict. */
 e=cs_io_meta(A,&n,&p,0);if(e&&e!=5)return e;e=cs_io_meta(B,&pn,&pp,0);if(e&&e!=5)return e;if(p!=pp)return CHEAT_STORE_ERR_FOREIGN;
 if(!p){*size=3;*crc=cs_crc32(blank,3);A[0]=0;return 0;}if(pn!=128||!n||n>1024)return CHEAT_STORE_ERR_DATA;
 e=cs_io_read(B,128,0,work,128);if(e)return e;id.key=pack_key(r->reserved);id.size=t->size;id.rom_crc=t->crc32;id.package_crc=pack_key(work+24);id.index=index;id.generation=pack_key(work+8);
 if(!cs_profile_valid(work,&id)||word(work+32)!=n)return CHEAT_STORE_ERR_DATA;*crc=pack_key(work+36);*size=n;
 bits=word(work+34);
 /* Validate active IDs before any transaction/export. All-off historical
  * profiles remain usable so users can remove codes held by newer releases.
  * Validated v2 consent is scoped to this exact profile identity and group. */
 for(i=0;i<work[5];++i){
  if(((bits>>shift)&((1UL<<work[40+i])-1))&&!(word(work+120)&(1U<<i))&&cheat_held(id.key,pack_key(work+56+(u16)i*4),id.size,id.rom_crc))return CHEAT_STORE_ERR_HELD;
  shift+=work[40+i];
 }
 e=cs_io_package(&id);if(e)return e;e=cs_io_crc(A,n,&actual);if(e)return e;if(actual!=*crc)return CHEAT_STORE_ERR_VERIFY;
 return ram_carrier(A,(u16)n,work[6],bits);
}
static u8 identity_valid(void){CatalogRecord*r=(CatalogRecord*)work;NativeBridgeTarget t,expected;NativeBridgeReport report;char native[160];u8 e;u16 n=word(work+40),index;u32 key,checksum;
 if(memcmp(work,"TCD1",4)||work[4]!=1||work[5]>1||!zero(work+6,2)||word(work+8)>=1200||!zero(work+10,2)||!pack_key(work+16)||pack_key(work+16)>0x800000UL||!pack_key(work+32)||pack_key(work+32)>1024||pack_key(work+24)>0x1000000UL||(!work[5]&&(pack_key(work+24)||pack_key(work+28)))||!zero(work+42,22)||!n||n>=160||!zero(work+64+n,(u8)(160-n))||!zero(work+224,28)||pack_key(work+252)!=cs_crc32(work,252))return CHEAT_STORE_ERR_DATA;
 index=word(work+8);key=pack_key(work+12);expected.size=pack_key(work+16);expected.crc32=pack_key(work+20);checksum=pack_key(work+252);strcpy(native,NATIVE);
 /* Reuse the shared workspace: cc65 local frames are limited to 255 bytes. */
 e=catalog_record(index,r);if(e)return e;if(r->kind!=2||pack_key(r->reserved)!=key||!cs_stock_path(r->path,A)||strcmp(A,native))return CHEAT_STORE_ERR_DATA;
 e=launch_metadata(index,r,&t,&report);if(e){if(report.stage==LAUNCH_STAGE_CLOSE)*status|=CHEAT_STORE_UNCERTAIN;return e;}if(t.size!=expected.size||t.crc32!=expected.crc32)return CHEAT_STORE_ERR_DATA;e=cue_guard(r,&t);if(e)return e;
 e=cs_io_read(pending,256,0,work,256);if(e)return e;return pack_key(work+252)==checksum&&cs_crc32(work,252)==checksum?0:CHEAT_STORE_ERR_DATA;
}
static u8 recover(void){u8 e,p,state;u32 n,crc;
 e=usable();if(e)return e;e=cs_io_meta(user_root,&n,&p,1);if(e)return e;if(!p)return 0;e=cs_io_meta(pending,&n,&p,0);if(e)return e;if(!p){*status&=(u8)~CHEAT_STORE_PENDING;return 0;}
 *status|=CHEAT_STORE_PENDING;if(n!=256)return CHEAT_STORE_ERR_DATA;e=cs_io_read(pending,256,0,work,256);if(e)return e;e=identity_valid();if(e)return e;
 e=blob_guard(pack_key(work+32),pack_key(work+36));if(e)return e;if(work[5]){e=blob_guard(pack_key(work+24),pack_key(work+28));if(e)return e;}
 e=cs_io_stock_meta(NATIVE,&n,&p);if(e)return e;if(n>0x1000000UL)return CHEAT_STORE_ERR_FOREIGN;state=2;
 if(p&&n==pack_key(work+32)){blob_path(n,pack_key(work+36));e=cs_io_equal(NATIVE,B,n);if(!e)state=1;else if(e!=CHEAT_STORE_ERR_VERIFY)return e;}
 if(state==2){if(!p&&!work[5])state=0;else if(p&&work[5]&&n==pack_key(work+24)){blob_path(n,pack_key(work+28));e=cs_io_equal(NATIVE,B,n);if(!e)state=0;else if(e!=CHEAT_STORE_ERR_VERIFY)return e;}}
 if(state!=2)return clear();
 /* Preserve interrupted or external bytes before restoring the old snapshot. */
 if(p){e=cs_io_crc(NATIVE,n,&crc);if(e)return e;e=blob(NATIVE,n,crc);if(e)return e;}
 e=cs_io_mem(pending,work,256);if(e)return e;
 if(work[5]){blob_path(pack_key(work+24),pack_key(work+28));e=cs_io_copy(B,NATIVE,pack_key(work+24),FA_WRITE|(p?FA_CREATE_ALWAYS:FA_CREATE_NEW));if(e)return e;}
 else if(p){e=ed_cmd_file_del((u8*)NATIVE);if(e)return e;e=cs_io_meta(NATIVE,&n,&p,0);if(e||p)return e?e:CHEAT_STORE_ERR_VERIFY;}
 return clear();
}
static u8 prepare(u16 index,const CatalogRecord*r){NativeBridgeTarget t;NativeBridgeReport report;u8 e,p;u32 n,crc,nn,nc;u16 cut;
 e=usable();if(e)return e;if(*status&CHEAT_STORE_PENDING)return CHEAT_STORE_ERR_PENDING;if(!r||r->kind!=2||index>=1200)return CHEAT_STORE_ERR_INPUT;
 e=recover();if(e)return e;e=launch_metadata(index,r,&t,&report);if(e){if(report.stage==LAUNCH_STAGE_CLOSE)*status|=CHEAT_STORE_UNCERTAIN;return e;}e=cue_guard(r,&t);if(e)return e;e=next(r,&t,index,&nn,&nc);if(e)return e;
 memset(work,0,256);if(!cs_stock_path(r->path,NATIVE))return CHEAT_STORE_ERR_INPUT;e=cs_io_stock_meta(NATIVE,&n,&p);if(e)return e;
 if(!p&&!A[0])return 0; /* Already off: no journal and no carrier needed. */
 if(n>0x1000000UL)return CHEAT_STORE_ERR_FOREIGN;
 if(p&&n==nn){e=A[0]?cs_io_equal(NATIVE,A,n):cs_io_mem(NATIVE,blank,3);if(!e)return 0;if(e!=CHEAT_STORE_ERR_VERIFY)return e;}
 e=dir(user_root);if(e)return e;e=dir(cd_root);if(e)return e;e=blob(A[0]?A:0,nn,nc);if(e)return e;
 crc=0;if(p){e=cs_io_crc(NATIVE,n,&crc);if(e)return e;e=blob(NATIVE,n,crc);if(e)return e;}
 cut=(u16)strlen(NATIVE)-11;NATIVE[cut]=0;e=dir(NATIVE);NATIVE[cut]='/';if(e)return e;
 memcpy(work,"TCD1",4);work[4]=1;work[5]=p;put16(work+8,index);put32(work+12,pack_key(r->reserved));put32(work+16,t.size);put32(work+20,t.crc32);put32(work+24,n);put32(work+28,crc);put32(work+32,nn);put32(work+36,nc);put16(work+40,(u16)strlen(NATIVE));put32(work+252,cs_crc32(work,252));
 *status|=CHEAT_STORE_PENDING;e=cs_io_write(pending,work,256);if(e)return e;
 if(p){blob_path(n,crc);e=cs_io_equal(NATIVE,B,n);}else{u8 found;u32 size;e=cs_io_meta(NATIVE,&size,&found,0);if(!e&&found)e=CHEAT_STORE_ERR_VERIFY;}if(e)return e;
 blob_path(nn,nc);e=cs_io_copy(B,NATIVE,nn,FA_WRITE|(p?FA_CREATE_ALWAYS:FA_CREATE_NEW));if(e)return e;return clear();
}
#ifdef __CC65__
#pragma code-name(pop)
#endif
u8 cs_cd_recover(u8*w,u8*f){u8 e,bank=sysGetBank();work=w;status=f;cs_io_bind(w,f,0);sysSetBank(13);e=recover();if(e)*f|=CHEAT_STORE_PENDING;sysSetBank(bank);return e;}
u8 cs_cd_prepare(u16 i,const CatalogRecord*r,u8*w,u8*f){u8 e,bank=sysGetBank();work=w;status=f;cs_io_bind(w,f,0);sysSetBank(13);e=prepare(i,r);sysSetBank(bank);return e;}
#else
u8 cs_cd_recover(u8*w,u8*f){(void)w;(void)f;return 0;}
u8 cs_cd_prepare(u16 i,const CatalogRecord*r,u8*w,u8*f){(void)i;(void)r;(void)w;(void)f;return 0;}
#endif
