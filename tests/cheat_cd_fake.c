#include "card_io.h"
#include "app_paths.h"
#include "cheat_cd.h"
#include "cd_guard.h"
#include "cheat_store_codec.h"
#include "cheat_store_io.h"
#include "launch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAXF 128
#define CHECK(x) do{if(!(x)){fprintf(stderr,"CD line%d %s op%d %s\n",__LINE__,#x,ops,last);return 1;}}while(0)
#define NEED(x) do{if(!(x)){fprintf(stderr,"CD SDK line%d %s op%d %s\n",__LINE__,#x,ops,last);abort();}}while(0)
typedef struct{char path[160];u8 data[2048];u32 size;u8 dir,present;} F;
static F fs[MAXF],base[MAXF],phases[3][MAXF];static u8 have_phase[3];static u8 work[544],flags,bank=4;static int opened=-1,ops,inject,writes,close_fail;static u32 at;static const char*last="none";
static u8 held_test;
u8 native_bridge_cheat_state(void){return NATIVE_CHEATS_ON;}
unsigned char cheat_held(unsigned long key,unsigned long id,unsigned long size,unsigned long crc){return held_test&&id==11;}
static CatalogRecord records[3];static NativeBridgeTarget targets[3];
static const char*stock="edturbo/gamedata/disc.cue/cheats.txt",*txn=TEO_USER_ROOT "/cdtxn.bin";
static const u8 old[]="{+,F83010,09,\"Stock user code\"}\r\n",a[]="{+,F82098,70,\"Infinite health\"}\r\n",b[]="{+,F83000,09,\"Infinite lives\"}\r\n";
static u32 get32(const u8*p){return (u32)p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24);}
static void put32(u8*p,u32 n){u8 i;for(i=0;i<4;++i){p[i]=(u8)n;n>>=8;}}
static u32 crc(const u8*p,u32 n){u32 v=0xffffffffUL;u8 j;while(n--){v^=*p++;for(j=0;j<8;++j)v=(v>>1)^((v&1)?0xedb88320UL:0);}return v^0xffffffffUL;}
static int find(const char*p){int i;for(i=0;i<MAXF;++i)if(fs[i].present&&!strcmp(fs[i].path,p))return i;return -1;}
static int add(const char*p,const u8*d,u32 n,u8 dir){int i=find(p);if(i<0)for(i=0;i<MAXF&&fs[i].present;++i){}NEED(i<MAXF&&n<=2048);memset(fs+i,0,sizeof(F));strcpy(fs[i].path,p);fs[i].present=1;fs[i].dir=dir;fs[i].size=n;if(d)memcpy(fs[i].data,d,(size_t)n);return i;}
static int step(const char*p){NEED(!close_fail);last=p;return ++ops==inject;}
static void write_guard(const char*p){++writes;NEED(!strcmp(p,stock)||!strncmp(p,TEO_USER_ROOT "/",sizeof(TEO_USER_ROOT "/")-1));if(!strcmp(p,stock)){int i=find(txn);NEED(i>=0&&fs[i].size==256&&crc(fs[i].data,252)==get32(fs[i].data+252));}}
u8 sysGetBank(void){return bank;}void sysSetBank(u8 n){bank=n;}
u8 ed_cmd_file_info(u8*p,FileInfo*i){int k;NEED(opened<0);if(step((char*)p))return 0x70;k=find((char*)p);if(k<0){char parent[160],*slash;strcpy(parent,(char*)p);slash=strrchr(parent,'/');if(slash){*slash=0;if(find(parent)<0)return 5;}return 4;}i->size=fs[k].size;i->is_dir=fs[k].dir?16:0;return 0;}
u8 ed_cmd_file_open(u8*p,u8 mode){int k;NEED(opened<0);if(step((char*)p))return 0x70;k=find((char*)p);if(mode==1){if(k<0){FileInfo info;return ed_cmd_file_info(p,&info);}NEED(!fs[k].dir);}else{NEED(mode==6);write_guard((char*)p);if(k>=0)return 8;k=add((char*)p,0,0,0);}opened=k;at=0;return 0;}
u32 ed_cmd_file_available(void){NEED(opened>=0);return fs[opened].size-at;}
u8 ed_cmd_file_set_ptr(u32 n){NEED(opened>=0&&n<=fs[opened].size);if(step(fs[opened].path))return 0x70;at=n;return 0;}
u8 ed_cmd_file_read(void*p,u32 n){NEED(opened>=0&&at+n<=fs[opened].size);if(step(fs[opened].path))return 0x70;memcpy(p,fs[opened].data+at,(size_t)n);at+=n;return 0;}
u8 ed_cmd_file_crc(u32 n,u32*c){NEED(opened>=0&&at+n<=fs[opened].size);if(step(fs[opened].path))return 0x70;*c=crc(fs[opened].data+at,n);at+=n;return 0;}
u8 ed_cmd_file_write(void*p,u32 n){u32 length=n;NEED(opened>=0&&at+n<=2048);write_guard(fs[opened].path);if(step(fs[opened].path)){length=n/2;memcpy(fs[opened].data+at,p,(size_t)length);at+=length;fs[opened].size=at;return 0x70;}memcpy(fs[opened].data+at,p,(size_t)n);at+=n;fs[opened].size=at;return 0;}
u8 ed_cmd_file_close(void){NEED(opened>=0);if(step(fs[opened].path)){close_fail=1;return 0x71;}opened=-1;return 0;}
u8 ed_cmd_file_copy(u8*src,u8*dst,u8 mode){int s,d,fail;NEED(opened<0);s=find((char*)src);d=find((char*)dst);NEED(s>=0&&!fs[s].dir&&((mode==6&&d<0)||(mode==10&&d>=0)));write_guard((char*)dst);fail=step((char*)dst);if(d<0)d=add((char*)dst,0,0,0);fs[d].size=fail?fs[s].size/2:fs[s].size;memcpy(fs[d].data,fs[s].data,(size_t)fs[d].size);return fail?0x70:0;}
u8 ed_cmd_file_del(u8*p){int i;NEED(opened<0);i=find((char*)p);if(i<0)return 4;write_guard((char*)p);fs[i].present=0;return step((char*)p)?0x70:0;}
u8 ed_cmd_dir_make(u8*p){if(step((char*)p))return 0x70;if(find((char*)p)>=0)return 8;add((char*)p,0,0,1);return 0;}
u8 catalog_record(u16 i,CatalogRecord*r){NEED(opened<0);if(i>=3)return 0x72;*r=records[i];return 0;}
u8 launch_metadata(u16 i,const CatalogRecord*r,NativeBridgeTarget*t,NativeBridgeReport*report){NEED(opened<0);memset(report,0,sizeof(*report));if(i>=3||r->kind!=2||strcmp(r->path,records[i].path)||memcmp(r->reserved,records[i].reserved,4))return 0x72;*t=targets[i];return 0;}
static int equal(const char*p,const u8*d,u32 n){int i=find(p);return i>=0&&fs[i].size==n&&!memcmp(fs[i].data,d,(size_t)n);}
static int retained(const u8*d,u32 n){int i;for(i=0;i<MAXF;++i)if(fs[i].present&&!fs[i].dir&&!strncmp(fs[i].path,TEO_USER_ROOT "/cd/",sizeof(TEO_USER_ROOT "/cd/")-1)&&fs[i].size==n&&!memcmp(fs[i].data,d,(size_t)n))return 1;return 0;}
static void seed(u8 i,const u8*text,u16 n){char p[160],q[160];u8 profile[128],pack[80];CheatSelection sel;CheatStoreIdentity id;u32 key=get32(records[i].reserved);memset(&sel,0,sizeof(sel));sel.count=1;sel.ids[0]=11;sel.costs[0]=sel.bits[0]=1;memset(pack,0,sizeof(pack));memcpy(pack,"TCG1",4);pack[4]=1;pack[5]=20;pack[6]=40;put32(pack+8,key);put32(pack+12,targets[i].size);put32(pack+16,targets[i].crc32);put32(pack+20,crc(pack+40,40));pack[24]=1;put32(pack+28,60);put32(pack+32,68);put32(pack+36,80);id.key=key;id.size=targets[i].size;id.rom_crc=targets[i].crc32;id.package_crc=get32(pack+20);id.generation=1;id.index=i;NEED(!cs_profile_make(profile,0,&sel,&id,n,crc(text,n),0));NEED(cs_native_path(records[i].path,p));strcpy(q,p);*strrchr(q,'/')=0;add(q,0,0,1);add(p,text,n,0);strcpy(q,p);strcpy(strrchr(q,'/')+1,"profile.bin");add(q,profile,128,0);sprintf(q,TEO_CHEAT_ROOT "/%08lX.tcg",key);add(q,pack,80,0);}
static void setup(void){u8 i;const char*paths[]={"CD/One/disc.cue","CD/Two/disc.cue","CD/None/disc.cue"};const char*cues[]={"FILE One.bin BINARY\n","FILE Two.bin BINARY\n","FILE None.bin BINARY\n"};memset(fs,0,sizeof(fs));flags=ops=inject=writes=close_fail=0;opened=-1;card_io_boot();add(TEO_USER_PARENT,0,0,1);add(TEO_USER_ROOT,0,0,1);add(TEO_CHEAT_PROFILE_ROOT,0,0,1);add("edturbo/gamedata",0,0,1);add("edturbo/gamedata/disc.cue",0,0,1);add(stock,old,sizeof(old)-1,0);add("edturbo/gamedata/disc.cue/bram_exp.brm",(const u8*)"KEEP SAVE",9,0);add("edturbo/sysdata/registry.dat",(const u8*)"KEEP SETTINGS",13,0);for(i=0;i<3;++i){memset(records+i,0,sizeof(*records));strcpy(records[i].path,paths[i]);records[i].kind=2;put32(records[i].reserved,crc((const u8*)paths[i],strlen(paths[i])));targets[i].size=strlen(cues[i]);targets[i].crc32=crc((const u8*)cues[i],targets[i].size);add(paths[i],(const u8*)cues[i],targets[i].size,0);}seed(0,a,sizeof(a)-1);seed(1,b,sizeof(b)-1);memcpy(base,fs,sizeof(fs));}
static void reboot(void){opened=-1;close_fail=inject=0;flags=0;card_io_boot();cs_io_bind(work,&flags,1);}
static u8 prepare(u8 i){return cs_cd_prepare(i,records+i,work,&flags);}
static int protected_ok(void){return equal("edturbo/gamedata/disc.cue/bram_exp.brm",(const u8*)"KEEP SAVE",9)&&equal("edturbo/sysdata/registry.dat",(const u8*)"KEEP SETTINGS",13);}
int main(int argc,char**argv){int i,total,result,before,n,k,type,recovery_total;u8 snapshot[2048];u32 length;NEED(argc==2);setup();
 if(!strcmp(argv[1],"switch")){CHECK(!prepare(0)&&equal(stock,a,sizeof(a)-1)&&retained(old,sizeof(old)-1));CHECK(!prepare(1)&&equal(stock,b,sizeof(b)-1)&&retained(a,sizeof(a)-1));CHECK(!prepare(2)&&equal(stock,(const u8*)" \r\n",3)&&retained(b,sizeof(b)-1));CHECK(!prepare(0)&&equal(stock,a,sizeof(a)-1));before=writes;CHECK(!prepare(0)&&writes==before);}
 else if(!strcmp(argv[1],"fresh-bha-launch")){
  const char *parent="edturbo/gamedata/disc.cue";
  const char *path="02 CD/Japan/0-F/Black Hole Assault (Japan)/disc.cue";
  u8 mode=99;
  fs[find(stock)].present=0;
  fs[find("edturbo/gamedata/disc.cue/bram_exp.brm")].present=0;
  fs[find(parent)].present=0;
  strcpy(records[2].path,path);
  put32(records[2].reserved,crc((const u8*)path,strlen(path)));
  add(path,(const u8*)"FILE None.bin BINARY\n",targets[2].size,0);
  before=writes;
  CHECK(!prepare(2));
  result=cd_guard_prepare(records+2,targets+2,&mode);
  if(result)fprintf(stderr,"Stage 24 error %02X\n",result);
  CHECK(result==0&&mode==0&&!cd_guard_packet());
  CHECK(before==writes&&find(txn)<0&&find(stock)<0&&find(parent)<0);
  fs[find("edturbo/gamedata")].present=0;
  CHECK(cd_guard_prepare(records+2,targets+2,&mode)!=0);
  return 0;
 }
 else if(!strcmp(argv[1],"fresh-stock")||!strcmp(argv[1],"fresh-active")||!strcmp(argv[1],"legacy-fresh-txn")){
  const char*parent="edturbo/gamedata/disc.cue";u8 j[256]={0};char blob[160];const u8 off[]={32,13,10};
  fs[find(stock)].present=0;fs[find("edturbo/gamedata/disc.cue/bram_exp.brm")].present=0;fs[find(parent)].present=0;
  if(!strcmp(argv[1],"fresh-active")){CHECK(!prepare(0)&&equal(stock,a,sizeof(a)-1)&&find(txn)<0);}
  else if(!strcmp(argv[1],"fresh-stock")){before=writes;CHECK(!prepare(2)&&writes==before&&find(txn)<0&&find(stock)<0);}
  else{
   memcpy(j,"TCD1",4);j[4]=1;j[8]=2;put32(j+12,get32(records[2].reserved));put32(j+16,targets[2].size);put32(j+20,targets[2].crc32);
   put32(j+32,3);put32(j+36,crc(off,3));j[40]=(u8)strlen(stock);strcpy((char*)j+64,stock);put32(j+252,crc(j,252));add(txn,j,256,0);
   sprintf(blob,TEO_USER_ROOT "/cd/B%08lX00000003.txt",(u32)crc(off,3));add(TEO_USER_ROOT "/cd",0,0,1);add(blob,off,3,0);
   reboot();CHECK(!cs_cd_recover(work,&flags)&&find(txn)<0&&find(stock)<0&&find(parent)<0);
  }
  CHECK(find("edturbo/gamedata/disc.cue/bram_exp.brm")<0&&equal("edturbo/sysdata/registry.dat",(const u8*)"KEEP SETTINGS",13)&&bank==4);return 0;
 }
 else if(!strcmp(argv[1],"no-profile")){char path[160];u32 size;u8 present;
 CHECK(cs_native_path(records[2].path,path));cs_io_bind(work,&flags,1);
 CHECK(ed_cmd_file_info((u8*)path,(FileInfo*)work)==5);
 CHECK(!prepare(2)&&equal(stock,(const u8*)" \r\n",3)&&retained(old,sizeof(old)-1));
 CHECK(find(txn)<0&&!(flags&CHEAT_STORE_PENDING));before=writes;CHECK(!prepare(2)&&writes==before);
 /* An existing empty directory is the equivalent no-selection state. */
 *strrchr(path,'/')=0;add(path,0,0,1);CHECK(!prepare(2)&&writes==before);
 /* Missing parents must stay errors for mandatory metadata probes. */
 CHECK(cs_io_meta("missing/required.bin",&size,&present,0)==5);
 }
 else if(!strcmp(argv[1],"faults")){ops=0;CHECK(!prepare(0));total=ops;for(i=1;i<=total;++i){memcpy(fs,base,sizeof(fs));reboot();ops=0;inject=i;result=prepare(0);inject=0;CHECK(result&&protected_ok());{int j=find(txn);if(j>=0&&fs[j].size==256&&crc(fs[j].data,252)==get32(fs[j].data+252)){type=equal(stock,a,sizeof(a)-1)?1:equal(stock,old,sizeof(old)-1)?0:2;if(!have_phase[type]){have_phase[type]=1;memcpy(phases[type],fs,sizeof(fs));}}}n=find(stock);length=n<0?0:fs[n].size;if(n>=0)memcpy(snapshot,fs[n].data,(size_t)length);reboot();result=cs_cd_recover(work,&flags);if(result){CHECK(flags&CHEAT_STORE_PENDING);CHECK(n<0?find(stock)<0:equal(stock,snapshot,length));}else{CHECK(!(flags&6));CHECK(equal(stock,a,sizeof(a)-1)||equal(stock,old,sizeof(old)-1));CHECK(find(txn)<0);CHECK(!prepare(0)&&equal(stock,a,sizeof(a)-1));}CHECK(protected_ok());}
 for(type=0;type<3;++type){CHECK(have_phase[type]);memcpy(fs,phases[type],sizeof(fs));reboot();ops=0;CHECK(!cs_cd_recover(work,&flags));recovery_total=ops;for(k=1;k<=recovery_total;++k){memcpy(fs,phases[type],sizeof(fs));reboot();ops=0;inject=k;CHECK(cs_cd_recover(work,&flags));inject=0;CHECK(flags&6);reboot();CHECK(!cs_cd_recover(work,&flags)&&!(flags&6));CHECK(!prepare(0)&&equal(stock,a,sizeof(a)-1)&&protected_ok());}}
 printf("CD prepare I/O faults: %d; recovery old/new/partial campaigns passed\n",total);}
 else if(!strcmp(argv[1],"double")){char bad[160],quarantine[160];int found=0;sprintf(bad,TEO_USER_ROOT "/cd/B%08lX%08lX.txt",crc(a,sizeof(a)-1),(u32)sizeof(a)-1);sprintf(quarantine,TEO_USER_ROOT "/cd/X%08lX%08lX.txt",crc(a,10),10UL);add(TEO_USER_ROOT "/cd",0,0,1);add(bad,a,10,0);memcpy(base,fs,sizeof(fs));ops=0;CHECK(!prepare(0));total=ops;for(i=1;i<=total;++i){memcpy(fs,base,sizeof(fs));reboot();ops=0;inject=i;result=prepare(0);inject=0;n=find(quarantine);if(result&&n>=0&&fs[n].size<10&&find(bad)>=0&&find(txn)<0){++found;reboot();CHECK(!prepare(0)&&equal(stock,a,sizeof(a)-1)&&protected_ok());}}CHECK(found);}
 else if(!strcmp(argv[1],"risk")){char path[160];held_test=1;CHECK(cs_native_path(records[0].path,path));strcpy(strrchr(path,'/')+1,"profile.bin");n=find(path);fs[n].data[4]=2;fs[n].data[120]=1;put32(fs[n].data+124,crc(fs[n].data,124));CHECK(!prepare(0)&&equal(stock,a,sizeof(a)-1));before=writes;CHECK(prepare(1)==CHEAT_STORE_ERR_HELD&&writes==before);}
 else if(!strcmp(argv[1],"held")){held_test=0;
 {char path[160];CHECK(cs_native_path(records[0].path,path));n=find(path);fs[n].data[1]='-';strcpy(strrchr(path,'/')+1,"profile.bin");k=find(path);put32(fs[k].data+36,crc(fs[n].data,fs[n].size));put32(fs[k].data+124,crc(fs[k].data,124));before=writes;CHECK(prepare(0)==CHEAT_STORE_ERR_DATA&&writes==before);}
 setup();held_test=1;before=writes;CHECK(prepare(0)==CHEAT_STORE_ERR_HELD&&writes==before&&equal(stock,old,sizeof(old)-1));
 {char path[160];int nf,pf;CHECK(cs_native_path(records[0].path,path));nf=find(path);strcpy(strrchr(path,'/')+1,"profile.bin");pf=find(path);NEED(nf>=0&&pf>=0);fs[nf].data[1]='-';fs[pf].data[7]=1;fs[pf].data[34]=fs[pf].data[35]=0;put32(fs[pf].data+36,crc(fs[nf].data,fs[nf].size));put32(fs[pf].data+124,crc(fs[pf].data,124));CHECK(!prepare(0)&&equal(stock,fs[nf].data,fs[nf].size));}}
 else if(!strcmp(argv[1],"invalid")){char path[160];CHECK(cs_native_path(records[0].path,path));n=find(path);NEED(n>=0);fs[n].data[5]^=1;before=writes;CHECK(prepare(0)&&writes==before&&equal(stock,old,sizeof(old)-1));setup();records[0].path[0]='X';before=writes;CHECK(prepare(0)&&writes==before);setup();seed(0,(const u8*)"{+,012345,01,\"Disc ROM offset\"}\r\n",31);before=writes;CHECK(prepare(0)&&writes==before&&equal(stock,old,sizeof(old)-1));}
 else{ops=0;CHECK(!prepare(0));total=ops;for(i=1;i<=total;++i){memcpy(fs,base,sizeof(fs));reboot();ops=0;inject=i;result=prepare(0);inject=0;if(card_io_fault()){before=ops;CHECK(result&&(flags&CHEAT_STORE_UNCERTAIN));CHECK(cs_cd_recover(work,&flags)&&prepare(1)&&ops==before);break;}}CHECK(i<=total);}
 CHECK(protected_ok()&&bank==4);return 0;}
