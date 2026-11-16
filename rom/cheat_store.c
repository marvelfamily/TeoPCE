#include "card_io.h"
/* Native file preparation only; stock Krikzz executes all cheat records. */
#include "cheat_store.h"
#include "cheat_cd.h"
#include "app_paths.h"
#include "cheat_store_codec.h"
#include "cheat_store_io.h"
#include "launch.h"
#include "pack.h"
#include <string.h>
#ifdef PREVIEW
u8 cheat_store_saved_status(u16 i,const CatalogRecord*r,u8*w){(void)i;(void)r;(void)w;return CHEAT_SAVED_UNKNOWN;}
u8 cheat_store_prepare_launch(u16 i,const CatalogRecord*r,u8*w){(void)i;(void)r;(void)w;return card_io_fault()?CARD_IO_BLOCKED:0;}
u8 cheat_store_init(u8*w){(void)w;return card_io_fault()?CARD_IO_BLOCKED:0;}
u8 cheat_store_load(u16 i,const CatalogRecord*r,const NativeBridgeTarget*t,u32 c,CheatSelection*s,u8*w){(void)i;(void)r;(void)t;(void)c;(void)w;if(s)memset(s,0,sizeof(*s));return card_io_fault()?CARD_IO_BLOCKED:0;}
u8 cheat_store_apply(u16 i,const CatalogRecord*r,const NativeBridgeTarget*t,u32 c,const CheatSelection*s,u8 n,u8*w){(void)i;(void)r;(void)t;(void)c;(void)s;(void)n;(void)w;return card_io_fault()?CARD_IO_BLOCKED:CHEAT_STORE_ERR_FOREIGN;}
u8 cheat_store_recover(u8 restore,u8*w){(void)restore;(void)w;return card_io_fault()?CARD_IO_BLOCKED:0;}
u8 cheat_store_flags(void){return CHEAT_STORE_READONLY|(card_io_fault()?CHEAT_STORE_UNCERTAIN:0);}
#else
static u8 *work,flags,prior[128],have_profile,have_native;
#define PRIVATE_GAME_PREFIX TEO_CHEAT_PROFILE_ROOT "/"
#define PRIVATE_GAME_PREFIX_BYTES (sizeof(PRIVATE_GAME_PREFIX) - 1)
#define PRIVATE_GAME_PATH_BYTES sizeof(PRIVATE_GAME_PREFIX "00000000/n00000000.bin")
static char native[160],path_a[PRIVATE_GAME_PATH_BYTES],path_b[PRIVATE_GAME_PATH_BYTES];
static const char *rom_path;
static CheatStoreIdentity identity;
static u16 old_size;
static u32 old_crc;
static const char pending_path[]=TEO_CHEAT_PROFILE_ROOT "/txn.bin";
#if defined(__CC65__)
#pragma code-name(push,"BNK02")
#endif
static u16 get16(const u8*p){return (u16)p[0]|((u16)p[1]<<8);}
static void hex8(char*p,u32 n){static const char hex[]="0123456789ABCDEF";u8 i;for(i=0;i<8;++i){p[7-i]=hex[n&15];n>>=4;}}
static void game_dir(char*p){strcpy(p,PRIVATE_GAME_PREFIX);hex8(p+PRIVATE_GAME_PREFIX_BYTES,identity.key);p[PRIVATE_GAME_PREFIX_BYTES+8]=0;}
static void private_path(char*p,char kind,u32 generation,u8 binary){u8 game_end=(u8)(PRIVATE_GAME_PREFIX_BYTES+8);game_dir(p);p[game_end]='/';if(!kind)strcpy(p+game_end+1,"profile.bin");else{p[game_end+1]=kind;hex8(p+game_end+2,generation);strcpy(p+game_end+10,binary?".bin":".txt");}}
static u8 directory(const char*p,u8 create,u8*present){u32 n;u8 e=cs_io_meta(p,&n,present,1);if(e||*present||!create)return e;e=ed_cmd_dir_make((u8*)p);if(e&&e!=8)return e;e=cs_io_meta(p,&n,present,1);return e?e:*present?0:CHEAT_STORE_ERR_VERIFY;}
static u8 private_root(u8 create,u8*present){u8 e;e=directory(TEO_USER_PARENT,0,present);if(e||!*present)return e?e:CHEAT_STORE_ERR_DATA;e=directory(TEO_USER_ROOT,create,present);if(e||!*present)return e;e=directory(TEO_CHEAT_PROFILE_ROOT,create,present);return e;}
static u8 private_game(u8 create,u8*present){u8 e=private_root(create,present);if(e||!*present)return e;game_dir(path_a);return directory(path_a,create,present);}
static u8 native_parent(u8 create,u8*present){u8 e;u16 cut;u32 n;e=cs_io_meta("edturbo/gamedata",&n,present,1);if(e||!*present)return e?e:CHEAT_STORE_ERR_DATA;cut=(u16)strlen(native)-11;native[cut]=0;e=directory(native,create,present);native[cut]='/';return e;}
static u8 pending_probe(u8*present,u32*size){u8 e=private_root(0,present);if(e||!*present){*size=0;return e;}return cs_io_meta(pending_path,size,present,0);}
static u8 no_pending(void){u8 p,e;u32 n;if(card_io_fault())return CARD_IO_BLOCKED;if(flags&CHEAT_STORE_UNCERTAIN)return CHEAT_STORE_ERR_CLOSE;if(flags&CHEAT_STORE_PENDING)return CHEAT_STORE_ERR_PENDING;e=pending_probe(&p,&n);if(e){flags|=CHEAT_STORE_PENDING;return e;}if(p){flags|=CHEAT_STORE_PENDING;return CHEAT_STORE_ERR_PENDING;}return 0;}
static u8 bind(u16 index,const CatalogRecord*r,const NativeBridgeTarget*t,u32 crc){u8 e=cs_io_binding(index,r,t,crc,&identity,native);if(!e)rom_path=r->path;return e;}
static u8 rom_guard(void){u32 crc;u8 e=cs_io_crc(rom_path,identity.size,&crc);return e?e:crc==identity.rom_crc?0:CHEAT_STORE_ERR_VERIFY;}
static u8 package_guard(void){return cs_io_package(&identity);}
static u8 foreign(u8 e){flags|=CHEAT_STORE_READONLY;return e?e:CHEAT_STORE_ERR_FOREIGN;}
static u8 owner(CheatSelection*out){u8 p,e;u32 n,crc;u16 bits=0;have_profile=have_native=0;if(out)memset(out,0,sizeof(*out));memset(prior,0,128);e=private_game(0,&p);if(e)return foreign(e);if(p){private_path(path_a,0,0,1);e=cs_io_meta(path_a,&n,&p,0);if(e)return foreign(e);if(p){if(n!=128)return foreign(0);e=cs_io_read(path_a,128,0,prior,128);if(e)return foreign(e);if(!cs_profile_valid(prior,&identity))return foreign(0);have_profile=1;private_path(path_b,'n',pack_key(prior+28),1);e=cs_io_equal(path_a,path_b,128);if(e)return foreign(e);private_path(path_b,'n',pack_key(prior+28),0);e=cs_io_crc(path_b,get16(prior+32),&crc);if(e)return foreign(e);if(crc!=pack_key(prior+36))return foreign(0);}}
 e=native_parent(0,&p);if(e)return foreign(e);if(p){e=cs_io_meta(native,&n,&p,0);if(e)return foreign(e);}if(p){if(!have_profile||!n||n>1024)return foreign(0);have_native=1;old_size=(u16)n;private_path(path_b,'n',pack_key(prior+28),0);e=cs_io_tuples(path_b,native,get16(prior+32),old_size,prior[6],&bits);if(e)return foreign(e);e=cs_io_crc(native,n,&old_crc);if(e)return foreign(e);}else{old_size=0;old_crc=0;}
 if(out&&have_profile){e=cs_profile_observe(prior,bits,out);if(e)return foreign(e);}flags&=(u8)~CHEAT_STORE_READONLY;return 0;
}
static u8 cache_valid(const CheatSelection*s,u8 rows){u8 i,j,k,total=0,n;u16 bits;if(!s||s->count>16||!s->count)return CHEAT_STORE_ERR_INPUT;for(i=0;i<s->count;++i){n=s->costs[i];bits=s->bits[i];if(!n||n>16||!bits||(bits&~(u16)((1UL<<n)-1))||total+n>16)return CHEAT_STORE_ERR_DATA;for(j=0;j<i;++j)if(s->ids[i]==s->ids[j])return CHEAT_STORE_ERR_DATA;for(k=0;k<n;++k){if(work[(u16)(total+k)*26+5]!=((bits>>k)&1)||!cs_row_line(work+(u16)(total+k)*26,work+416))return CHEAT_STORE_ERR_DATA;}total+=n;}return total==rows?0:CHEAT_STORE_ERR_DATA;}
static u8 generation(void){u8 tries,i,p,e;u32 n;identity.generation=have_profile?pack_key(prior+8)+1:1;for(tries=0;tries<32;++tries){if(!identity.generation)return CHEAT_STORE_ERR_GENERATION;for(i=0;i<4;++i){private_path(path_a,i==0?'b':i==1?'p':'n',identity.generation,i==1||i==3);e=cs_io_meta(path_a,&n,&p,0);if(e)return e;if(p)break;}if(i==4)return 0;++identity.generation;}return CHEAT_STORE_ERR_GENERATION;}
static u8 absent(const char*p){u32 n;u8 found,e=cs_io_meta(p,&n,&found,0);return e?e:found?CHEAT_STORE_ERR_VERIFY:0;}
static u8 delete_absent(const char*p){u8 e=ed_cmd_file_del((u8*)p);if(e)return e;return absent(p);}
static u8 journal_clear(const u8*j){u8 e=cs_io_mem(pending_path,j,256);if(e)return e;e=delete_absent(pending_path);if(!e)flags&=(u8)~CHEAT_STORE_PENDING;return e;}
static u8 load_impl(u16 index,const CatalogRecord*r,const NativeBridgeTarget*t,u32 crc,CheatSelection*out){u8 e;if(!work||!out)return CHEAT_STORE_ERR_INPUT;memset(out,0,sizeof(*out));e=no_pending();if(e)return e;e=bind(index,r,t,crc);if(e)return e;e=package_guard();return e?e:owner(out);}
static u8 apply_impl(u16 index,const CatalogRecord*r,const NativeBridgeTarget*t,u32 crc,const CheatSelection*desired,u8 rows){u8 e,p,off;u16 size;u32 native_crc;if(!work||!desired)return CHEAT_STORE_ERR_INPUT;e=no_pending();if(e)return e;e=bind(index,r,t,crc);if(e)return e;e=package_guard();if(e)return e;e=rom_guard();if(e)return e;e=owner(0);if(e)return e;off=desired->count==0;if(off){if(rows)return CHEAT_STORE_ERR_INPUT;if(!have_profile)return 0;private_path(path_a,'n',pack_key(prior+28),0);e=cs_io_carrier(path_a,get16(prior+32),prior[6]);if(e)return e;rows=prior[6];}else{e=cache_valid(desired,rows);if(e)return e;}
 e=private_game(1,&p);if(e)return e;e=native_parent(1,&p);if(e)return e;e=generation();if(e)return e;
 if(have_native){private_path(path_a,'b',identity.generation,0);e=cs_io_copy(native,path_a,old_size,FA_WRITE|FA_CREATE_NEW);if(e)return e;}
 if(have_profile){private_path(path_a,0,0,1);private_path(path_b,'p',identity.generation,1);e=cs_io_copy(path_a,path_b,128,FA_WRITE|FA_CREATE_NEW);if(e)return e;}
 private_path(path_a,'n',identity.generation,0);e=cs_io_rows(path_a,rows,&size,&native_crc);if(e)return e;e=cs_profile_make(work,have_profile?prior:0,desired,&identity,size,native_crc,off);if(e)return e;private_path(path_a,'n',identity.generation,1);e=cs_io_write(path_a,work,128);if(e)return e;e=cs_journal_make(work+128,&identity,have_profile?prior:0,old_size,old_crc,(u8)(have_native|(have_profile<<1)),work,native);if(e)return e;
 flags|=CHEAT_STORE_PENDING;e=cs_io_write(pending_path,work+128,256);if(e)return e;
 if(have_native){private_path(path_a,'b',identity.generation,0);e=cs_io_equal(native,path_a,old_size);}else e=absent(native);if(e)return e;private_path(path_a,0,0,1);e=have_profile?cs_io_mem(path_a,prior,128):absent(path_a);if(e)return e;
 private_path(path_a,'n',identity.generation,0);e=cs_io_copy(path_a,native,size,FA_WRITE|(have_native?FA_CREATE_ALWAYS:FA_CREATE_NEW));if(e)return e;private_path(path_a,'n',identity.generation,1);private_path(path_b,0,0,1);e=cs_io_copy(path_a,path_b,128,FA_WRITE|(have_profile?FA_CREATE_ALWAYS:FA_CREATE_NEW));if(e)return e;e=journal_clear(work+128);if(e)return e;memcpy(prior,work,128);have_native=have_profile=1;old_size=size;old_crc=native_crc;return 0;
}
/* Prefix64 of a valid pending record lives in prior while workspace is reused. */
static u8 preserve(const char*source,u32 size,char kind,u8 binary,u32*used){u8 i,p,e;u32 n,g=identity.generation;for(i=0;i<32&&g;++i,++g){private_path(path_b,kind,g,binary);e=cs_io_meta(path_b,&n,&p,0);if(e)return e;if(!p){e=cs_io_copy(source,path_b,size,FA_WRITE|FA_CREATE_NEW);if(!e)*used=g;return e;}if(n==size){e=cs_io_equal(source,path_b,size);if(!e){*used=g;return 0;}if(e!=CHEAT_STORE_ERR_VERIFY)return e;}}return CHEAT_STORE_ERR_GENERATION;}
static u8 recovery_identity(void){CatalogRecord*r=(CatalogRecord*)work;NativeBridgeTarget t;NativeBridgeReport report;u8 e;e=catalog_record(identity.index,r);if(e){flags|=CHEAT_STORE_UNCERTAIN;return e;}if((r->kind!=1&&r->kind!=2)||pack_key(r->reserved)!=identity.key||!cs_native_path(r->path,(char*)work+256)||strcmp((char*)work+256,native)||cs_crc32((const u8*)r->path,(u16)strlen(r->path))!=identity.key)return CHEAT_STORE_ERR_DATA;e=launch_metadata(identity.index,r,&t,&report);if(e){if(report.stage==LAUNCH_STAGE_CLOSE)flags|=CHEAT_STORE_UNCERTAIN;return e;}if(t.size!=identity.size||t.crc32!=identity.rom_crc)return CHEAT_STORE_ERR_DATA;memcpy(work+256,r->path,128);rom_path=(char*)work+256;e=rom_guard();return e?e:package_guard();}
static u8 recover_impl(u8 explicit_restore){u8 e,p,np,pp,state;u16 bits;u32 n,crc,ns,ps,ng=0,pg=0;u8*new_profile=work;if(card_io_fault())return CARD_IO_BLOCKED;if(!work||explicit_restore>1)return CHEAT_STORE_ERR_INPUT;if(flags&CHEAT_STORE_UNCERTAIN)return CHEAT_STORE_ERR_CLOSE;e=pending_probe(&p,&n);if(e)return e;if(!p){flags&=(u8)~CHEAT_STORE_PENDING;return 0;}flags|=CHEAT_STORE_PENDING;if(n!=256)return CHEAT_STORE_ERR_DATA;e=cs_io_read(pending_path,256,0,work,256);if(e)return e;if(!cs_journal_valid(work)||(work[5]==1))return CHEAT_STORE_ERR_DATA;memcpy(prior,work,64);memcpy(native,work+64,160);identity.key=pack_key(prior+8);identity.size=pack_key(prior+12);identity.rom_crc=pack_key(prior+16);identity.package_crc=pack_key(prior+20);identity.generation=pack_key(prior+24);identity.index=get16(prior+50);e=recovery_identity();if(e)return e;
 private_path(path_a,'n',identity.generation,1);e=cs_io_read(path_a,128,0,new_profile,128);if(e)return e;if(!cs_profile_valid(new_profile,&identity)||pack_key(new_profile+8)!=identity.generation||pack_key(new_profile+124)!=pack_key(prior+44)||get16(new_profile+32)!=get16(prior+30)||pack_key(new_profile+36)!=pack_key(prior+36))return CHEAT_STORE_ERR_DATA;private_path(path_a,'n',identity.generation,0);e=cs_io_crc(path_a,get16(prior+30),&crc);if(e)return e;if(crc!=pack_key(prior+36))return CHEAT_STORE_ERR_DATA;e=cs_io_tuples(path_a,path_a,get16(prior+30),get16(prior+30),new_profile[6],&bits);if(e)return e;if(bits!=get16(new_profile+34))return CHEAT_STORE_ERR_DATA;
 if(prior[5]&2){private_path(path_a,'p',identity.generation,1);e=cs_io_read(path_a,128,0,work+128,128);if(e)return e;if(!cs_profile_valid(work+128,&identity)||pack_key(work+252)!=pack_key(prior+40))return CHEAT_STORE_ERR_DATA;private_path(path_b,'n',pack_key(work+156),1);e=cs_io_equal(path_a,path_b,128);if(e)return e;private_path(path_b,'n',pack_key(work+156),0);e=cs_io_crc(path_b,get16(work+160),&crc);if(e)return e;if(crc!=pack_key(work+164))return CHEAT_STORE_ERR_DATA;if(prior[5]&1){private_path(path_a,'b',identity.generation,0);e=cs_io_crc(path_a,get16(prior+28),&crc);if(e)return e;if(crc!=pack_key(prior+32))return CHEAT_STORE_ERR_DATA;e=cs_io_tuples(path_b,path_a,get16(work+160),get16(prior+28),work[134],&bits);if(e)return e;}}
 e=cs_io_meta(native,&ns,&np,0);if(e)return e;if(ns>0x1000000UL)return CHEAT_STORE_ERR_FOREIGN;state=2;if(np&&ns==get16(prior+30)){private_path(path_a,'n',identity.generation,0);e=cs_io_equal(path_a,native,ns);if(!e)state=1;else if(e!=CHEAT_STORE_ERR_VERIFY)return e;}if(state==2){if(!(prior[5]&1)&&!np)state=0;else if((prior[5]&1)&&np&&ns==get16(prior+28)){private_path(path_a,'b',identity.generation,0);e=cs_io_equal(path_a,native,ns);if(!e)state=0;else if(e!=CHEAT_STORE_ERR_VERIFY)return e;}}
 if(state==2&&np){e=preserve(native,ns,'r',0,&ng);if(e)return e;}if(state==2&&!explicit_restore)return CHEAT_STORE_ERR_PENDING;
 private_path(path_a,0,0,1);e=cs_io_meta(path_a,&ps,&pp,0);if(e)return e;if(ps>0x1000000UL)return CHEAT_STORE_ERR_FOREIGN;if(pp){e=preserve(path_a,ps,'s',1,&pg);if(e)return e;}
 e=rom_guard();if(e)return e;cs_journal_rebuild(work+128,prior,native);e=cs_io_mem(pending_path,work+128,256);if(e)return e;
 if(state==2){if(np){private_path(path_a,'r',ng,0);e=cs_io_equal(path_a,native,ns);}else e=absent(native);if(e)return e;if(prior[5]&1){private_path(path_a,'b',identity.generation,0);e=cs_io_copy(path_a,native,get16(prior+28),FA_WRITE|(np?FA_CREATE_ALWAYS:FA_CREATE_NEW));}else e=np?delete_absent(native):0;if(e)return e;}
 private_path(path_a,0,0,1);if(pp){private_path(path_b,'s',pg,1);e=cs_io_equal(path_a,path_b,ps);}else e=absent(path_a);if(e)return e;if(state==1){private_path(path_b,'n',identity.generation,1);e=cs_io_copy(path_b,path_a,128,FA_WRITE|(pp?FA_CREATE_ALWAYS:FA_CREATE_NEW));}else if(prior[5]&2){private_path(path_b,'p',identity.generation,1);e=cs_io_copy(path_b,path_a,128,FA_WRITE|(pp?FA_CREATE_ALWAYS:FA_CREATE_NEW));}else e=pp?delete_absent(path_a):0;if(e)return e;e=journal_clear(work+128);if(e)return e;have_native=have_profile=0;memset(prior,0,128);return 0;
}
#if defined(__CC65__)
#pragma code-name(pop)
#endif
#define ENTER(w) u8 bank=sysGetBank();work=(w);cs_io_bind(work,&flags,0);sysSetBank(6)
#define LEAVE sysSetBank(bank)
u8 cheat_store_flags(void){return flags|(card_io_fault()?CHEAT_STORE_UNCERTAIN:0);}
u8 cheat_store_init(u8*w){u8 r;u8 bank=sysGetBank();if(card_io_fault()){flags|=CHEAT_STORE_UNCERTAIN;return CARD_IO_BLOCKED;}flags=have_native=have_profile=0;work=w;cs_io_bind(work,&flags,1);sysSetBank(6);r=recover_impl(0);if(!r)r=cs_cd_recover(work,&flags);if(r)flags|=CHEAT_STORE_PENDING;sysSetBank(bank);return r;}
u8 cheat_store_load(u16 i,const CatalogRecord*r,const NativeBridgeTarget*t,u32 c,CheatSelection*s,u8*w){u8 e;ENTER(w);e=load_impl(i,r,t,c,s);LEAVE;return e;}
u8 cheat_store_apply(u16 i,const CatalogRecord*r,const NativeBridgeTarget*t,u32 c,const CheatSelection*s,u8 n,u8*w){u8 e;ENTER(w);e=apply_impl(i,r,t,c,s,n);LEAVE;return e;}
u8 cheat_store_recover(u8 restore,u8*w){u8 e;ENTER(w);e=recover_impl(restore);if(!e)e=cs_cd_recover(work,&flags);if(e)flags|=CHEAT_STORE_PENDING;LEAVE;return e;}
u8 cheat_store_prepare_launch(u16 i,const CatalogRecord*r,u8*w){u8 e;ENTER(w);e=no_pending();if(!e&&r&&r->kind==2)e=cs_cd_prepare(i,r,w,&flags);LEAVE;return e;}
#if defined(__CC65__)
#pragma code-name(push,"BNK06")
#endif
static u8 saved_status_impl(u16 index,const CatalogRecord*r,u8*w){
 static const char digits[]="0123456789ABCDEF";
 NativeBridgeTarget target;NativeBridgeReport report;CheatStoreIdentity id;
 char *path=(char*)w+128;u32 key;u8 e,closed,i;
 if(!r||!w||(r->kind!=1&&r->kind!=2)||card_io_fault()||(flags&(CHEAT_STORE_PENDING|CHEAT_STORE_UNCERTAIN)))return CHEAT_SAVED_UNKNOWN;
 key=pack_key(r->reserved);
 if(cs_crc32((const u8*)r->path,(u16)strlen(r->path))!=key)return CHEAT_SAVED_UNKNOWN;
 strcpy(path,TEO_CHEAT_PROFILE_ROOT "/00000000/profile.bin");
 for(i=0;i<8;++i){path[sizeof(TEO_CHEAT_PROFILE_ROOT "/")-1+7-i]=digits[key&15];key>>=4;}
 e=ed_cmd_file_open((u8*)path,FA_READ);
 if(e)return e==4||e==5?CHEAT_SAVED_OFF:CHEAT_SAVED_UNKNOWN;
 e=ed_cmd_file_available()!=128?CHEAT_STORE_ERR_DATA:ed_cmd_file_read(w,128);
 closed=card_io_close(ed_cmd_file_close);if(e||closed)return CHEAT_SAVED_UNKNOWN;
 e=launch_metadata(index,r,&target,&report);if(e)return CHEAT_SAVED_UNKNOWN;
 id.key=pack_key(r->reserved);id.size=target.size;id.rom_crc=target.crc32;id.package_crc=pack_key(w+24);id.generation=0;id.index=index;
 if(!cs_profile_valid(w,&id))return CHEAT_SAVED_UNKNOWN;
 return w[34]||w[35]?CHEAT_SAVED_ON:CHEAT_SAVED_OFF;
}
#if defined(__CC65__)
#pragma code-name(pop)
#endif
u8 cheat_store_saved_status(u16 i,const CatalogRecord*r,u8*w){u8 bank=sysGetBank(),s;sysSetBank(10);s=saved_status_impl(i,r,w);sysSetBank(bank);return s;}

#endif
