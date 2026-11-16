#include "cheat_store_codec.h"
#include "app_paths.h"
#include <string.h>
#ifndef PREVIEW
#if defined(__CC65__)
#pragma code-name(push,"BNK03")
#endif
static u16 get16(const u8*p){return (u16)p[0]|((u16)p[1]<<8);}
static u32 get32(const u8*p){return (u32)p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24);}
static void put16(u8*p,u16 v){p[0]=(u8)v;p[1]=(u8)(v>>8);}
static void put32(u8*p,u32 v){u8 i;for(i=0;i<4;++i){p[i]=(u8)v;v>>=8;}}
static u32 crc_impl(const u8*p,u16 n){u32 c=0xffffffffUL;u8 bit;while(n--){c^=*p++;for(bit=0;bit<8;++bit)c=(c>>1)^((c&1)?0xedb88320UL:0);}return c^0xffffffffUL;}
static u8 zero(const u8*p,u16 n){while(n--)if(*p++)return 0;return 1;}
static u8 address(u32 a){return (a&&a<0x100000UL)||(a>=0xf82000UL&&a<=0xf83fffUL)||(a>=0x1f0000UL&&a<=0x1f1fffUL);}
static u8 label(u8 c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c==' '||c=='.'||c=='('||c==')'||c=='-'||c=='_'||c=='/'||c=='+';}
static u8 profile_impl(const u8*p,const CheatStoreIdentity*t){
 u8 i,j,total=0,n,cost;u16 bits,part;
 if(!p||!t)return 0;n=p[5];bits=get16(p+34);
 if(memcmp(p,"TCS1",4)||(p[4]!=1&&p[4]!=2)||!n||n>16||!p[6]||p[6]>16||(p[7]&~1)||!get32(p+8)||get32(p+8)!=get32(p+28)||get32(p+12)!=t->key||get32(p+16)!=t->size||get32(p+20)!=t->rom_crc||get32(p+24)!=t->package_crc||!t->size||t->size>0x800000UL||!get16(p+32)||get16(p+32)>1024||!zero(p+122,2)||get32(p+124)!=crc_impl(p,124))return 0;
 /* Version 2 carries explicit group consent; version 1 stays byte-compatible.
  * An old reader rejects v2 rather than silently losing the consent boundary. */
 if(p[4]==1?get16(p+120)!=0:(!get16(p+120)||(p[7]&1)||(get16(p+120)&~(u16)((1UL<<n)-1))))return 0;
 for(i=0;i<16;++i){
  cost=p[40+i];if(i>=n){if(cost||get32(p+56+(u16)i*4))return 0;continue;}
  if(!cost||cost>16||total+cost>16)return 0;
  for(j=0;j<i;++j)if(get32(p+56+(u16)i*4)==get32(p+56+(u16)j*4))return 0;
  part=(u16)((bits>>total)&((1UL<<cost)-1));if(!(p[7]&1)&&!part)return 0;total+=cost;
 }
 return total==p[6]&&!(bits&~(u16)((1UL<<total)-1))&&(!(p[7]&1)||!bits);
}
static u8 make_impl(u8*p,const u8*prior,const CheatSelection*s,const CheatStoreIdentity*t,u16 size,u32 crc,u8 off){
 u8 i,j,n,cost,total=0;u16 bits=0,part;
 if(!p||!s||!t||!t->generation||!size||size>1024||off>1)return CHEAT_STORE_ERR_INPUT;
 if(off){if(!prior||!profile_impl(prior,t))return CHEAT_STORE_ERR_DATA;n=prior[5];}
 else{n=s->count;if(!n||n>16)return CHEAT_STORE_ERR_INPUT;}
 memset(p,0,128);memcpy(p,"TCS1",4);p[4]=1;p[5]=n;p[7]=off;
 for(i=0;i<n;++i){
  cost=off?prior[40+i]:s->costs[i];part=off?0:s->bits[i];
  if(!cost||cost>16||total+cost>16||(!off&&!part)||(part&~(u16)((1UL<<cost)-1)))return CHEAT_STORE_ERR_DATA;
  for(j=0;j<i;++j)if((off?get32(prior+56+(u16)i*4):s->ids[i])==get32(p+56+(u16)j*4))return CHEAT_STORE_ERR_DATA;
  p[40+i]=cost;put32(p+56+(u16)i*4,off?get32(prior+56+(u16)i*4):s->ids[i]);bits|=(u16)(part<<total);total+=cost;
 }
 if(!off&&s->risk){p[4]=2;put16(p+120,s->risk);}
 p[6]=total;put32(p+8,t->generation);put32(p+12,t->key);put32(p+16,t->size);put32(p+20,t->rom_crc);put32(p+24,t->package_crc);put32(p+28,t->generation);put16(p+32,size);put16(p+34,bits);put32(p+36,crc);put32(p+124,crc_impl(p,124));return profile_impl(p,t)?0:CHEAT_STORE_ERR_DATA;
}
static u8 observe_impl(const u8*p,u16 bits,CheatSelection*s){
 u8 i,j=0,shift=0,cost;u16 part;
 if(!s||!p||p[5]>16||p[6]>16||(bits&~(u16)((1UL<<p[6])-1)))return CHEAT_STORE_ERR_DATA;
 memset(s,0,sizeof(*s));
 for(i=0;i<p[5];++i){cost=p[40+i];if(!cost||shift+cost>16)return CHEAT_STORE_ERR_DATA;part=(u16)((bits>>shift)&((1UL<<cost)-1));shift+=cost;if(part){s->ids[j]=get32(p+56+(u16)i*4);s->costs[j]=cost;s->bits[j]=part;if(p[4]==2&&(get16(p+120)&(1U<<i)))s->risk|=1U<<j;++j;}}
 s->count=j;return shift==p[6]?0:CHEAT_STORE_ERR_DATA;
}
#if defined(__CC65__)
/* Path construction shares the read-only status bank; codec stays in bank7. */
#pragma code-name(push,"BNK06")
#endif
static u8 path_impl(const char*rom,char*out,u8 stock){
 static const char hex[]="0123456789ABCDEF";u32 key;u16 n,start=0,segment=0;u8 c,i,cd;
 if(!rom||!out)return 0;
 for(n=0;n<=127;++n){c=(u8)rom[n];if(!c)break;if(c<32||c>126||c=='\\'||c==':'||c=='*'||c=='?'||c=='"'||c=='<'||c=='>'||c=='|')return 0;
  if(c=='/'){if(!segment||rom[start]==' '||rom[n-1]==' '||rom[n-1]=='.'||(segment==1&&rom[start]=='.')||(segment==2&&rom[start]=='.'&&rom[start+1]=='.'))return 0;start=n+1;segment=0;}else ++segment;}
 if(n>127||segment<=4||rom[start]==' '||rom[n-1]==' '||rom[n-1]=='.'||rom[n-4]!='.'||((u8)rom[n-1]|32)!='e')return 0;
 cd=((u8)rom[n-3]|32)=='c'&&((u8)rom[n-2]|32)=='u';
 if(!cd&&(((u8)rom[n-3]|32)!='p'||((u8)rom[n-2]|32)!='c'))return 0;
 if(cd&&!stock){strcpy(out,TEO_CHEAT_PROFILE_ROOT "/");key=cs_crc32((const u8*)rom,n);start=sizeof(TEO_CHEAT_PROFILE_ROOT "/")-1;for(i=0;i<8;++i){out[start+7-i]=hex[key&15];key>>=4;}strcpy(out+start+8,"/cheats.txt");return 1;}
 strcpy(out,"edturbo/gamedata/");strcat(out,rom+start);strcat(out,"/cheats.txt");return 1;
}
#if defined(__CC65__)
#pragma code-name(pop)
#endif
static u8 basename_valid(const u8*p,u8 n){
 u8 i,c;if(n<=4||p[0]==' '||p[n-1]==' '||p[n-1]=='.'||p[n-4]!='.'||(p[n-3]|32)!='p'||(p[n-2]|32)!='c'||(p[n-1]|32)!='e')return 0;
 for(i=0;i<n;++i){c=p[i];if(c<32||c>126||c=='/'||c=='\\'||c==':'||c=='*'||c=='?'||c=='"'||c=='<'||c=='>'||c=='|')return 0;}return 1;
}
static u8 journal_impl(const u8*p){
 static const char hex[]="0123456789ABCDEF";u32 key;u16 n;u8 i;
 if(!p||memcmp(p,"TCJ1",4)||p[4]!=1||(p[5]&~3)||!zero(p+6,2)||!get32(p+12)||get32(p+12)>0x800000UL||!get32(p+24)||!get16(p+30)||get16(p+30)>1024||get16(p+50)>=1200||!zero(p+52,12)||!zero(p+224,28)||get32(p+252)!=crc_impl(p,252))return 0;
 if((p[5]&1)?(!get16(p+28)||get16(p+28)>1024):(get16(p+28)||get32(p+32)))return 0;
 if(!(p[5]&2)&&get32(p+40))return 0;
 n=get16(p+48);if(!n||n>=160||!zero(p+64+n,160-n))return 0;
 if(n==sizeof(TEO_CHEAT_PROFILE_ROOT "/")-1+19&&!memcmp(p+64,TEO_CHEAT_PROFILE_ROOT "/",sizeof(TEO_CHEAT_PROFILE_ROOT "/")-1)){
  i=sizeof(TEO_CHEAT_PROFILE_ROOT "/")-1;key=get32(p+8);if(memcmp(p+64+i+8,"/cheats.txt",11))return 0;
  for(n=0;n<8;++n){if(p[64+i+7-n]!=hex[key&15])return 0;key>>=4;}return 1;
 }
 if(memcmp(p+64,"edturbo/gamedata/",17))return 0;
 for(i=17;i<n&&p[64+i]!='/';++i){}if(i==n||memcmp(p+64+i,"/cheats.txt",11)||i+11!=n)return 0;
 return basename_valid(p+81,(u8)(i-17));
}
static void rebuild_impl(u8*j,const u8*prefix,const char*native){memset(j,0,256);memcpy(j,prefix,64);strcpy((char*)j+64,native);put32(j+252,crc_impl(j,252));}
static u8 journal_make_impl(u8*j,const CheatStoreIdentity*id,const u8*old,u16 old_size,u32 old_crc,u8 present,const u8*next,const char*native){if(!j||!id||!next||!native||(present&2&&!old)||!profile_impl(next,id)||get32(next+28)!=id->generation)return CHEAT_STORE_ERR_DATA;memset(j,0,256);memcpy(j,"TCJ1",4);j[4]=1;j[5]=present;put32(j+8,id->key);put32(j+12,id->size);put32(j+16,id->rom_crc);put32(j+20,id->package_crc);put32(j+24,id->generation);put16(j+28,old_size);put16(j+30,get16(next+32));put32(j+32,old_crc);put32(j+36,get32(next+36));put32(j+40,present&2?get32(old+124):0);put32(j+44,get32(next+124));put16(j+48,(u16)strlen(native));put16(j+50,id->index);strcpy((char*)j+64,native);put32(j+252,crc_impl(j,252));return journal_impl(j)?0:CHEAT_STORE_ERR_DATA;}
static u8 line_impl(const u8*r,u8*out){
 static const char hex[]="0123456789ABCDEF";u8 i,n=0,len=0;u32 a;
 if(!r||!out)return 0;a=get32(r);if(!address(a)||r[5]>1)return 0;
 while(len<20&&r[6+len]){if(!label(r[6+len]))return 0;++len;}if(!len||len>19||!zero(r+6+len,20-len))return 0;
 out[n++]='{';out[n++]=r[5]?'+':'-';out[n++]=',';for(i=0;i<6;++i)out[n++]=hex[(u8)(a>>(20-i*4))&15];out[n++]=',';out[n++]=hex[r[4]>>4];out[n++]=hex[r[4]&15];out[n++]=',';out[n++]='"';for(i=0;i<len;++i)out[n++]=r[6+i];out[n++]='"';out[n++]='}';out[n++]='\r';out[n++]='\n';return n;
}
static u8 white(u8 c){return c==' '||c=='\t'||c=='\r'||c=='\n';}
static void skip(const u8*p,u8 n,u8*i){while(*i<n&&white(p[*i]))++*i;}
static u8 token(const u8*p,u8 n,u8*i,u8 c){skip(p,n,i);if(*i>=n||p[*i]!=c)return 0;++*i;return 1;}
static u8 number(const u8*p,u8 n,u8*i,u8 max,u32*v){u8 count=0,c,d;*v=0;skip(p,n,i);while(*i<n){c=p[*i];d=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:255;if(d==255)break;if(++count>max)return 0;*v=(*v<<4)|d;++*i;}return count!=0;}
static u8 parse_impl(const u8*p,u8 n,u8*r){
 u8 i=0,len=0;u32 a,v;if(!p||!r)return 0;memset(r,0,26);if(!token(p,n,&i,'{'))return 0;skip(p,n,&i);if(i>=n||(p[i]!='+'&&p[i]!='-'))return 0;r[5]=p[i++]== '+';
 if(!token(p,n,&i,',')||!number(p,n,&i,6,&a)||!token(p,n,&i,',')||!number(p,n,&i,2,&v)||!token(p,n,&i,',')||!token(p,n,&i,'"'))return 0;
 while(i<n&&p[i]!='"'){if(len>=19||!label(p[i]))return 0;r[6+len++]=p[i++];}
 if(!len||!token(p,n,&i,'"')||!token(p,n,&i,'}'))return 0;skip(p,n,&i);if(i!=n||!address(a))return 0;put32(r,a);r[4]=(u8)v;return 1;
}
#if defined(__CC65__)
#pragma code-name(pop)
#endif
#define ENTER u8 bank=sysGetBank();sysSetBank(7)
#define LEAVE sysSetBank(bank)
u32 cs_crc32(const u8*p,u16 n){u32 r;ENTER;r=crc_impl(p,n);LEAVE;return r;}
u8 cs_profile_valid(const u8*p,const CheatStoreIdentity*t){u8 r;ENTER;r=profile_impl(p,t);LEAVE;return r;}
u8 cs_profile_make(u8*p,const u8*old,const CheatSelection*s,const CheatStoreIdentity*t,u16 n,u32 crc,u8 off){u8 r;ENTER;r=make_impl(p,old,s,t,n,crc,off);LEAVE;return r;}
u8 cs_profile_observe(const u8*p,u16 bits,CheatSelection*s){u8 r;ENTER;r=observe_impl(p,bits,s);LEAVE;return r;}
u8 cs_journal_valid(const u8*p){u8 r;ENTER;r=journal_impl(p);LEAVE;return r;}
u8 cs_journal_make(u8*j,const CheatStoreIdentity*id,const u8*old,u16 n,u32 crc,u8 present,const u8*next,const char*native){u8 r;ENTER;r=journal_make_impl(j,id,old,n,crc,present,next,native);LEAVE;return r;}
void cs_journal_rebuild(u8*j,const u8*prefix,const char*native){ENTER;rebuild_impl(j,prefix,native);LEAVE;}
u8 cs_native_path(const char*p,char*out){u8 r,bank=sysGetBank();sysSetBank(10);r=path_impl(p,out,0);LEAVE;return r;}
u8 cs_stock_path(const char*p,char*out){u8 r,bank=sysGetBank();sysSetBank(10);r=path_impl(p,out,1);LEAVE;return r;}
u8 cs_row_line(const u8*r,u8*out){u8 n;ENTER;n=line_impl(r,out);LEAVE;return n;}
u8 cs_parse_line(const u8*p,u8 n,u8*r){u8 result;ENTER;result=parse_impl(p,n,r);LEAVE;return result;}
#endif
