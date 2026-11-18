#include "card_io.h"
/* Private one-handle streamed IO for native cheat transactions. */
#include "cheat_store_io.h"
#include "app_paths.h"
#include "cheat_store_codec.h"
#include "launch.h"
#include "pack.h"
#include <string.h>
#ifndef PREVIEW
static u8 *work,*flags,opened;
void cs_io_bind(u8*w,u8*f,u8 restart){work=w;flags=f;if(restart&&!card_io_fault())opened=0;}
#if defined(__CC65__)
#pragma code-name(push,"BNK05")
#endif
static u8 usable(void){if(card_io_fault())return CARD_IO_BLOCKED;return !work||!flags?CHEAT_STORE_ERR_INPUT:(*flags&CHEAT_STORE_UNCERTAIN)?CHEAT_STORE_ERR_CLOSE:0;}
static u8 finish(u8 error){u8 e;if(!opened)return error;e=card_io_close(ed_cmd_file_close);if(e){*flags|=CHEAT_STORE_UNCERTAIN;return error?error:CHEAT_STORE_ERR_CLOSE;}opened=0;return error;}
static u8 open_read(const char*p,u32 n){u8 e=usable();if(e)return e;if(opened)return CHEAT_STORE_ERR_CLOSE;e=ed_cmd_file_open((u8*)p,FA_READ);if(e)return e;opened=1;if(ed_cmd_file_available()!=n)return finish(CHEAT_STORE_ERR_VERIFY);return 0;}
static u8 meta_impl(const char*p,u32*n,u8*present,u8 dir){FileInfo info;u8 e=usable();if(e)return e;*present=0;*n=0;info.file_name=0;e=ed_cmd_file_info((u8*)p,&info);if(e==4)return 0;if(e)return e;if((info.is_dir!=0)!=(dir!=0))return CHEAT_STORE_ERR_FOREIGN;*present=1;*n=info.size;return 0;}
static u8 read_impl(const char*p,u32 size,u32 offset,u8*out,u16 n){u8 e=open_read(p,size);if(e)return e;if(offset>size||(u32)n>size-offset)return finish(CHEAT_STORE_ERR_DATA);if(offset)e=ed_cmd_file_set_ptr(offset);if(!e&&n)e=ed_cmd_file_read(out,n);return finish(e);}
static u8 crc_impl(const char*p,u32 n,u32 offset,u32*c){u8 e=open_read(p,n);if(e)return e;if(offset>n)return finish(CHEAT_STORE_ERR_DATA);if(offset)e=ed_cmd_file_set_ptr(offset);*c=0;if(!e)e=ed_cmd_file_crc(n-offset,c);return finish(e);}
static u8 equal_impl(const char*a,const char*b,u32 n){u32 at=0;u8 e;u16 count;do{count=n-at>64?64:(u16)(n-at);e=read_impl(a,n,at,work+416,count);if(e)return e;e=read_impl(b,n,at,work+480,count);if(e)return e;if(memcmp(work+416,work+480,count))return CHEAT_STORE_ERR_VERIFY;at+=count;}while(at<n);return 0;}
static u8 mem_impl(const char*p,const u8*data,u16 n){u16 at=0,count;u8 e;do{count=n-at>64?64:n-at;e=read_impl(p,n,at,work+480,count);if(e)return e;if(memcmp(data+at,work+480,count))return CHEAT_STORE_ERR_VERIFY;at+=count;}while(at<n);return 0;}
static u8 copy_impl(const char*a,const char*b,u32 n,u8 mode){u8 e=usable();if(e)return e;if(opened||n>0x1000000UL||(mode!=(FA_WRITE|FA_CREATE_NEW)&&mode!=(FA_WRITE|FA_CREATE_ALWAYS)))return CHEAT_STORE_ERR_INPUT;e=ed_cmd_file_copy((u8*)a,(u8*)b,mode);if(e)return e;return equal_impl(a,b,n);}
static u8 write_impl(const char*p,const u8*data,u16 n){u8 e=usable();u16 at=0,count;if(e)return e;if(opened||!n||n>256)return CHEAT_STORE_ERR_INPUT;e=ed_cmd_file_open((u8*)p,FA_WRITE|FA_CREATE_NEW);if(e)return e;opened=1;while(at<n){count=n-at>128?128:n-at;e=ed_cmd_file_write((void*)(data+at),count);if(e)break;at+=count;}e=finish(e);if(e)return e;return mem_impl(p,data,n);}
static u8 line(const char*p,u16 total,u16*at,u8*buffer,u8*length){u8 e,c,n=0;e=open_read(p,total);if(e)return e;if(*at)e=ed_cmd_file_set_ptr(*at);while(!e&&*at<total){e=ed_cmd_file_read(&c,1);if(e)break;++*at;if(n>=63){e=CHEAT_STORE_ERR_FOREIGN;break;}buffer[n++]=c;if(c=='\n')break;}*length=n;return finish(e);}
static u8 rows_impl(const char*p,u8 rows,u16*size,u32*crc){u8 i,n,e;u16 at=0;if(!rows||rows>16)return CHEAT_STORE_ERR_DATA;e=usable();if(e)return e;e=ed_cmd_file_open((u8*)p,FA_WRITE|FA_CREATE_NEW);if(e)return e;opened=1;for(i=0;i<rows;++i){n=cs_row_line(work+(u16)i*26,work+416);if(!n){e=CHEAT_STORE_ERR_DATA;break;}e=ed_cmd_file_write(work+416,n);if(e)break;at+=n;}e=finish(e);if(e)return e;*size=at;e=crc_impl(p,at,0,crc);if(e)return e;at=0;for(i=0;i<rows;++i){n=cs_row_line(work+(u16)i*26,work+416);e=read_impl(p,*size,at,work+480,n);if(e)return e;if(memcmp(work+416,work+480,n))return CHEAT_STORE_ERR_VERIFY;at+=n;}return 0;}
static u8 carrier_impl(const char*p,u16 size,u8 rows){u8 i,n,e;u16 at=0;if(!rows||rows>16)return CHEAT_STORE_ERR_DATA;for(i=0;i<rows;++i){e=line(p,size,&at,work+416,&n);if(e)return e;if(!cs_parse_line(work+416,n,work+(u16)i*26))return CHEAT_STORE_ERR_FOREIGN;work[(u16)i*26+5]=0;}return at==size?0:CHEAT_STORE_ERR_FOREIGN;}
static u8 tuples_impl(const char*a,const char*b,u16 na,u16 nb,u8 rows,u16*bits){u8 i,n,e,ra[26],rb[26];u16 aa=0,bb=0;*bits=0;if(!rows||rows>16||!na||na>1024||!nb||nb>1024)return CHEAT_STORE_ERR_FOREIGN;for(i=0;i<rows;++i){e=line(a,na,&aa,work+416,&n);if(e)return e;if(!cs_parse_line(work+416,n,ra))return CHEAT_STORE_ERR_FOREIGN;e=line(b,nb,&bb,work+480,&n);if(e)return e;if(!cs_parse_line(work+480,n,rb)||memcmp(ra,rb,5)||memcmp(ra+6,rb+6,20))return CHEAT_STORE_ERR_FOREIGN;if(rb[5])*bits|=(u16)(1U<<i);}return aa==na&&bb==nb?0:CHEAT_STORE_ERR_FOREIGN;}
static u16 word(const u8*p){return (u16)p[0]|((u16)p[1]<<8);}
static void key_hex(char*p,u32 n){static const char hex[]="0123456789ABCDEF";u8 i;for(i=0;i<8;++i){p[7-i]=hex[n&15];n>>=4;}}
static u8 binding_impl(u16 index,const CatalogRecord*r,const NativeBridgeTarget*t,u32 crc,CheatStoreIdentity*id,char*native){NativeBridgeTarget actual;NativeBridgeReport report;u8 e;if(card_io_fault())return CARD_IO_BLOCKED;if(!r||!t||(r->kind!=1&&r->kind!=2)||index>=1200||!cs_native_path(r->path,native))return CHEAT_STORE_ERR_INPUT;id->key=pack_key(r->reserved);id->size=t->size;id->rom_crc=t->crc32;id->package_crc=crc;id->index=index;if(!t->size||t->size>0x800000UL||cs_crc32((const u8*)r->path,(u16)strlen(r->path))!=id->key)return CHEAT_STORE_ERR_INPUT;e=launch_metadata(index,r,&actual,&report);if(e){if(report.stage==LAUNCH_STAGE_CLOSE)*flags|=CHEAT_STORE_UNCERTAIN;return e;}return actual.size==t->size&&actual.crc32==t->crc32?0:CHEAT_STORE_ERR_DATA;}
static u8 package_impl(const CheatStoreIdentity*id){char path[sizeof(TEO_CHEAT_ROOT "/00000000.tcg")];u8 p,e;u32 n,crc;u8*h=work+416;strcpy(path,TEO_CHEAT_ROOT "/00000000.tcg");key_hex(path+sizeof(TEO_CHEAT_ROOT "/")-1,id->key);e=meta_impl(path,&n,&p,0);if(e)return e;if(!p||n<40||n>262144UL)return CHEAT_STORE_ERR_DATA;e=read_impl(path,n,0,h,40);if(e)return e;if(memcmp(h,"TCG1",4)||h[4]!=1||h[5]!=20||word(h+6)!=40||pack_key(h+8)!=id->key||pack_key(h+12)!=id->size||pack_key(h+16)!=id->rom_crc||pack_key(h+20)!=id->package_crc||!word(h+24)||word(h+24)>1024||word(h+26)||pack_key(h+28)!=40UL+(u32)word(h+24)*20||pack_key(h+28)>pack_key(h+32)||pack_key(h+32)>n||((pack_key(h+32)-pack_key(h+28))&7)||pack_key(h+36)!=n)return CHEAT_STORE_ERR_DATA;e=crc_impl(path,n,40,&crc);return e?e:crc==id->package_crc?0:CHEAT_STORE_ERR_DATA;}
#if defined(__CC65__)
#pragma code-name(pop)
#endif
#define ENTER u8 bank=sysGetBank();sysSetBank(9)
#define LEAVE sysSetBank(bank)
u8 cs_io_binding(u16 i,const CatalogRecord*r,const NativeBridgeTarget*t,u32 c,CheatStoreIdentity*id,char*native){u8 e;ENTER;e=binding_impl(i,r,t,c,id,native);LEAVE;return e;}
u8 cs_io_package(const CheatStoreIdentity*id){u8 e;ENTER;e=package_impl(id);LEAVE;return e;}
u8 cs_io_meta(const char*p,u32*n,u8*present,u8 dir){u8 r;ENTER;r=meta_impl(p,n,present,dir);LEAVE;return r;}
/* A missing game directory or cheats.txt is normal on a fresh card.
 * Check each level so an inaccessible gamedata root still blocks launch. */
u8 cs_io_stock_meta(char *path,u32 *size,u8 *present){
    char *slash=strrchr(path,'/');
    u8 error;
    if(!slash)return CHEAT_STORE_ERR_INPUT;
    error=cs_io_meta("edturbo/gamedata",size,present,1);
    if(error)return error;
    if(!*present)return CHEAT_STORE_ERR_DATA;
    *slash=0;
    error=cs_io_meta(path,size,present,1);
    *slash='/';
    if(error||!*present)return error;
    return cs_io_meta(path,size,present,0);
}
u8 cs_io_read(const char*p,u32 size,u32 offset,u8*out,u16 n){u8 r;ENTER;r=read_impl(p,size,offset,out,n);LEAVE;return r;}
u8 cs_io_crc(const char*p,u32 n,u32*c){u8 r;ENTER;r=crc_impl(p,n,0,c);LEAVE;return r;}
u8 cs_io_equal(const char*a,const char*b,u32 n){u8 r;ENTER;r=equal_impl(a,b,n);LEAVE;return r;}
u8 cs_io_mem(const char*p,const u8*data,u16 n){u8 r;ENTER;r=mem_impl(p,data,n);LEAVE;return r;}
u8 cs_io_copy(const char*a,const char*b,u32 n,u8 mode){u8 r;ENTER;r=copy_impl(a,b,n,mode);LEAVE;return r;}
u8 cs_io_write(const char*p,const u8*data,u16 n){u8 r;ENTER;r=write_impl(p,data,n);LEAVE;return r;}
u8 cs_io_rows(const char*p,u8 n,u16*size,u32*crc){u8 r;ENTER;r=rows_impl(p,n,size,crc);LEAVE;return r;}
u8 cs_io_carrier(const char*p,u16 n,u8 rows){u8 r;ENTER;r=carrier_impl(p,n,rows);LEAVE;return r;}
u8 cs_io_tuples(const char*a,const char*b,u16 na,u16 nb,u8 rows,u16*bits){u8 r;ENTER;r=tuples_impl(a,b,na,nb,rows,bits);LEAVE;return r;}
#endif
