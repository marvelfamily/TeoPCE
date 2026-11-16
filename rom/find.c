#include "find.h"
#include "cheat_index.h"
#include "card_io.h"
#include <string.h>
#ifdef PREVIEW
#include "preview.h"
#endif
#ifdef __CC65__
#ifdef PREVIEW
#define FIND_BANK 19
#pragma code-name(push,"BNK15")
#pragma rodata-name("BNK15")
#else
#define FIND_BANK 12
#pragma code-name(push,"BNK08")
#pragma rodata-name("BNK08")
#endif
#endif
static u8 upper(u8 c){return c>='a'&&c<='z'?c-32:c;}
static u8 valid_prefix(const CatalogFilter *f){u8 i;for(i=0;i<=FIND_PREFIX_MAX;++i){if(!f->prefix[i])return 1;if(i==FIND_PREFIX_MAX||(u8)f->prefix[i]<32||(u8)f->prefix[i]>126)return 0;}return 0;}
static u8 prefix_match(const char *prefix,const u8 *title){u8 i;for(i=0;prefix[i];++i)if(upper((u8)prefix[i])!=title[i])return 0;return 1;}
#ifdef PREVIEW
/* Preview reads the same curated roots from its embedded catalog. */
static u8 preview_category(const char *path){
 static const char roots[8][17]={"01 HuCard","02 CD","03 SuperGrafx","04 Translations","05 Homebrew","07 Museum","06 Tools","08 Unlicensed"};
 u8 i,n;for(i=0;i<8;++i){n=(u8)strlen(roots[i]);if(!strncmp(path,roots[i],n)&&path[n]=='/')return i<3?1:i-1;}return 0;
}
#endif
static u8 find_impl(const CatalogFilter *f,const Preferences *prefs,const Recent *recent,u16 *matches,u16 *count,u8 *work){
 u16 total,index=0,found=0,amount,i,j;u32 generation;u8 error=0,match,rank,ended,c,version=2,category;u8 *row;
#ifndef PREVIEW
 u8 closed;u32 size;
#endif
 if(!count)return CATALOG_ERR_INPUT;*count=0;
 if(card_io_fault())return CARD_IO_BLOCKED;
 if(!f||!prefs||!matches||!work||f->medium>2||f->genre>=GENRE_COUNT||f->region>4||f->favorites>1||f->recent>1||f->cheats>1||f->category>=CATEGORY_COUNT||(f->favorites&&f->recent)||!valid_prefix(f)||(f->letter&&f->letter!='#'&&(f->letter<'A'||f->letter>'Z'))||prefs->count>FAVORITES_MAX||(f->recent&&(!recent||recent->count>RECENT_MAX)))return CATALOG_ERR_INPUT;
 if(!f->prefix[0]&&!f->recent&&!f->category)return catalog_filter(f,prefs,matches,count);
 error=catalog_identity(&total,&generation);if(error)return error;
 if(f->recent)for(i=0;i<RECENT_MAX;++i)matches[i]=0xffff;
#ifndef PREVIEW
 error=ed_cmd_file_open((u8*)TEO_METADATA_ROOT "/find.bin",FA_READ);if(error)return error;
 size=ed_cmd_file_available();error=size!=16UL+(u32)total*20?CATALOG_ERR_FORMAT:ed_cmd_file_read(work,16);
 if(!error&&(memcmp(work,"TPF1",4)||(work[4]!=1&&work[4]!=2)||work[5]!=20||((u16)work[6]|((u16)work[7]<<8))!=total||pack_key(work+8)!=generation||pack_key(work+12)))error=CATALOG_ERR_FORMAT;
 if(!error){version=work[4];if(f->category&&version==1)error=CATALOG_ERR_CATEGORY;}
#endif
 while(index<total&&!error){
#ifdef PREVIEW
  amount=1;error=preview_record(index,(CatalogRecord*)work);if(error)break;
  category=preview_category((const char*)work+88);
  /* Keep title bytes before replacing this record with its exact index row. */
  ended=0;for(j=0;j<FIND_PREFIX_MAX;++j){if(!work[j])ended=1;work[256+j]=ended?0:upper(work[j]);}
  error=preview_index(index,work);if(error)break;memcpy(work+8,work+256,FIND_PREFIX_MAX);work[0]|=category<<2;
#else
  amount=total-index;if(amount>26)amount=26;error=ed_cmd_file_read(work,amount*20);if(error)break;
#endif
  for(i=0;i<amount;++i){
   row=work+i*20;ended=0;
   for(j=0;j<FIND_PREFIX_MAX;++j){c=row[8+j];if(!c)ended=1;else if(ended||c<32||c>126||(c>='a'&&c<='z')){error=CATALOG_ERR_FORMAT;break;}}
   if(error)break;
   category=row[0]>>2;
   if(category>=CATEGORY_COUNT||(version==1&&category)){error=CATALOG_ERR_FORMAT;break;}
#ifndef PREVIEW
   if(version==2&&!category){error=CATALOG_ERR_FORMAT;break;}
#endif
   row[0]&=3;
   match=pack_match(row,f->medium,f->genre,f->letter,f->region);if(match==2){error=CATALOG_ERR_FORMAT;break;}
   if(!match||row[0]==3||(f->category&&category!=f->category)||!prefix_match(f->prefix,row+8)||(f->favorites&&!prefs_has(prefs,pack_key(row+4))))continue;
   if(f->recent){rank=recent_rank(recent,pack_key(row+4));if(rank!=0xff)matches[rank]=index+i;}
   else matches[found++]=index+i;
  }
  index+=amount;
 }
#ifndef PREVIEW
 closed=card_io_close(ed_cmd_file_close);if(closed)error=closed;
#endif
 if(!error){if(f->recent)for(i=0;i<RECENT_MAX;++i)if(matches[i]!=0xffff)matches[found++]=matches[i];*count=found;}
 return error;
}
#ifdef __CC65__
#pragma code-name(pop)
#endif
u8 find_filter(const CatalogFilter *f,const Preferences *prefs,const Recent *recent,u16 *matches,u16 *count,u8 *work){
#ifdef __CC65__
 u8 bank=sysGetBank(),result;sysSetBank(FIND_BANK);result=find_impl(f,prefs,recent,matches,count,work);sysSetBank(bank);
 if(!result&&f->cheats)result=cheat_index_filter(matches,count,work);return result;
#else
 u8 result=find_impl(f,prefs,recent,matches,count,work);
 if(!result&&f->cheats)result=cheat_index_filter(matches,count,work);return result;
#endif
}
