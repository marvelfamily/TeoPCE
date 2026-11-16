#include "card_io.h"
#include "cheat_cache.h"
#if defined(PREVIEW) && defined(__CC65__)
u8 cheat_cache_rows(const CheatSelection*d,u16 g,u32 c,u8*w,u8*m,u8*n,u32*conflict){(void)d;(void)g;(void)c;(void)w;(void)m;if(n)*n=0;if(conflict)*conflict=0;return 0xe2;}
#else
#include "main.h"
#include "cheat_cache.h"
#include "pack.h"
#include <string.h>
#ifdef PREVIEW
#include "preview.h"
#define rd(b,n) preview_cheat_read(b,n)
#define seek(n) preview_cheat_seek(n)
#else
#define rd(b,n) ed_cmd_file_read(b,n)
#define seek(n) ed_cmd_file_set_ptr(n)
#endif
#if defined(__CC65__) && !defined(PREVIEW)
#pragma code-name(push,"BNK04")
#pragma rodata-name(push,"BNK04")
#endif
static const char empty_label[]="Cheat";
static u16 word(const u8 *b){return (u16)b[0]|((u16)b[1]<<8);}
static u8 find_group(u16 groups,u32 id,u8 *meta){
 u16 i;u8 error;
 for(i=0;i<groups;++i){
  error=seek(40UL+(u32)i*20);if(!error)error=rd(meta,20);
  if(error)return error;if(pack_key(meta)==id)return 0;
 }
 return 0xe5;
}
#define BAD 0xe5
/* Normalize within the 128-byte tail; native labels never contain delimiters. */
static void clean_label(u8 *label,u16 length){
 u16 i;u8 c,n=0,space=0;
 for(i=0;i<length;++i){
  c=label[i];
  if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='.'||c=='('||c==')'||c=='-'||c=='_'||c=='/'||c=='+')){if(n)space=1;continue;}
  if(space){label[n++]=' ';space=0;}label[n++]=c;
 }
 label[n]=0;
}
static u8 decimal(u8 *out,u8 n){u8 i=0;if(n>=10)out[i++]='0'+n/10;out[i++]='0'+n%10;return i;}
static u32 canonical(const u8 *p){
 u32 address=pack_key(p);
 if((address>=0xf82000UL&&address<=0xf83fffUL)||(address>=0x1f0000UL&&address<=0x1f1fffUL))return 0x1f0000UL+(address&0x1fff);
 return address;
}
static u8 cache_impl(const CheatSelection *draft,u16 groups,u32 codes_at,u8 *work,u8 *meta,u8 *count,u32 *conflict){
 u8 group,row,cost,error,n,suffix,length,j;u16 first;u8 *out,*label=work+416;
 *count=0;*conflict=0;
 if(card_io_fault())return CARD_IO_BLOCKED;
 for(group=0;group<draft->count;++group){
  error=find_group(groups,draft->ids[group],meta);if(error)return error;
  cost=draft->costs[group];first=word(meta+4);
  if(meta[14]||word(meta+6)!=cost||*count+cost>16)return BAD;
  length=(u8)word(meta+12);error=seek(pack_key(meta+8));if(!error)error=rd(label,length);if(error)return error;clean_label(label,length);
  for(row=0;row<cost;++row){
   error=seek(codes_at+(u32)(first+row)*8);if(!error)error=rd(meta,8);if(error)return error;
   out=work+(u16)(*count)*26;memcpy(out,meta,5);out[5]=(u8)((draft->bits[group]>>row)&1);memset(out+6,0,20);
   if(out[5])for(j=0;j<*count;++j){
    u8 *prior=work+(u16)j*26;
    if(prior[5]&&prior[4]!=out[4]&&canonical(prior)==canonical(out)){*count=0;*conflict=draft->ids[group];return CHEAT_CACHE_CONFLICT;}
   }
   suffix=cost>1?4+(row>=9)+(cost>=10):0;
   for(n=0;n<19-suffix&&label[n];++n)out[6+n]=label[n];
   if(suffix){out[6+n++]=' ';n+=decimal(out+6+n,row+1);out[6+n++]='/';n+=decimal(out+6+n,cost);}
   if(!n)memcpy(out+6,empty_label,5);++*count;
  }
 }
 return 0;
}
#if defined(__CC65__) && !defined(PREVIEW)
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
u8 cheat_cache_rows(const CheatSelection *draft,u16 groups,u32 codes_at,u8 *work,u8 *meta,u8 *count,u32 *conflict){
#ifdef PREVIEW
 return cache_impl(draft,groups,codes_at,work,meta,count,conflict);
#else
 u8 bank=sysGetBank(),result;sysSetBank(8);result=cache_impl(draft,groups,codes_at,work,meta,count,conflict);sysSetBank(bank);return result;
#endif
}

#endif
