#include "cheat_index.h"
#include "catalog.h"
#include "card_io.h"
#include <string.h>
#ifdef PREVIEW
#include "preview.h"
#endif
#ifdef __CC65__
#ifdef PREVIEW
#define COVERAGE_BANK 15
#pragma code-name(push,"BNK11")
#pragma rodata-name(push,"BNK11")
#else
#define COVERAGE_BANK 9
#pragma code-name(push,"BNK05")
#pragma rodata-name(push,"BNK05")
#endif
#endif
#ifndef PREVIEW
static u16 word(const u8 *p){return (u16)p[0]|((u16)p[1]<<8);}
/* Ordered CRC covers keys without retaining a second catalog-sized table. */
static u32 keys_crc(u32 crc,const u8 *p){
 u8 i,bit;for(i=0;i<4;++i){crc^=p[i];for(bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320UL:0);}return crc;
}
static u8 verify_keys(u16 total,u32 expected,u8 *work){
 u8 error,closed;u16 count,entries,index=0,amount,i;u32 generation,signature,size,crc=0xffffffffUL;
 error=catalog_identity(&count,&generation);if(error)return error;
 error=ed_cmd_file_open((u8*)TEO_METADATA_ROOT "/index.bin",FA_READ);if(error)return error;
 size=ed_cmd_file_available();error=size<16?CATALOG_ERR_FORMAT:ed_cmd_file_read(work,16);
 if(!error&&(pack_header(work,size,1,&entries,&signature)||entries!=total||count!=total||signature!=generation))error=CATALOG_ERR_FORMAT;
 while(index<total&&!error){
  amount=total-index;if(amount>49)amount=49;error=ed_cmd_file_read(work,amount*8);if(error)break;
  for(i=0;i<amount;++i)crc=keys_crc(crc,work+i*8+4);index+=amount;
 }
 if(!error&&crc!=expected)error=CATALOG_ERR_FORMAT;
 closed=card_io_close(ed_cmd_file_close);return closed?closed:error;
}
static u8 open_index(u8 *work,u16 *total){
 u32 generation,size;u8 error;
 if(card_io_fault())return CARD_IO_BLOCKED;
 error=catalog_identity(total,&generation);if(error)return error;
 if(*total>CATALOG_MAX)return CATALOG_ERR_RANGE;
 error=ed_cmd_file_open((u8*)TEO_METADATA_ROOT "/cheats.idx",FA_READ);
 if(error)return error==4||error==5?CHEAT_INDEX_MISSING:error;
 size=ed_cmd_file_available();error=size!=16UL+(u32)*total*8?CATALOG_ERR_FORMAT:ed_cmd_file_read(work,16);
 if(!error&&(memcmp(work,"TPQ1",4)||work[4]!=1||work[5]!=8||word(work+6)!=*total||pack_key(work+8)!=generation||pack_key(work+12)))error=CATALOG_ERR_FORMAT;
 if(error){u8 closed=card_io_close(ed_cmd_file_close);if(closed)error=closed;}
 return error;
}
#endif
static u8 read_impl(u16 index,u32 key,CheatCoverage *coverage){
 u8 error;
#ifndef PREVIEW
 u16 total;
 u8 work[16],closed;
#endif
 if(!coverage)return CATALOG_ERR_INPUT;
 coverage->available=coverage->held=0;
#ifdef PREVIEW
 error=preview_cheat_coverage(index,key,&coverage->available,&coverage->held);
#else
 error=open_index(work,&total);if(error)return error;
 if(index>=total)error=CATALOG_ERR_RANGE;
 if(!error)error=ed_cmd_file_set_ptr(16UL+(u32)index*8);
 if(!error)error=ed_cmd_file_read(work,8);
 if(!error&&(pack_key(work)!=key||word(work+4)>1024||word(work+6)>1024-word(work+4)))error=CATALOG_ERR_FORMAT;
 closed=card_io_close(ed_cmd_file_close);if(closed)error=closed;
 if(!error){coverage->available=word(work+4);coverage->held=word(work+6);}
#endif
 return error;
}
static u8 filter_impl(u16 *matches,u16 *count,u8 *work){
 u16 n,total,index=0,i,found=0;u8 error=0;u8 *rows;
#ifdef PREVIEW
 u32 generation;u16 available,held;
#else
 u8 closed;u16 amount;u32 key_crc=0xffffffffUL;
#endif
 if(!count)return CATALOG_ERR_INPUT;n=*count;*count=0;
 if(!matches||!work||n>CATALOG_MAX)return CATALOG_ERR_INPUT;
 if(card_io_fault())return CARD_IO_BLOCKED;
 memset(work,0,150);rows=work+150;
#ifdef PREVIEW
 error=catalog_identity(&total,&generation);if(error)return error;
 if(total>CATALOG_MAX)return CATALOG_ERR_RANGE;
 for(index=0;index<total&&!error;++index){error=preview_index(index,rows);if(!error)error=preview_cheat_coverage(index,pack_key(rows+4),&available,&held);if(!error&&available)work[index>>3]|=1<<(index&7);}
#else
 error=open_index(rows,&total);if(error)return error;
 while(index<total&&!error){
  amount=total-index;if(amount>49)amount=49;error=ed_cmd_file_read(rows,amount*8);if(error)break;
  for(i=0;i<amount;++i){u16 available=word(rows+i*8+4),held=word(rows+i*8+6);
   if(available>1024||held>1024-available){error=CATALOG_ERR_FORMAT;break;}
   key_crc=keys_crc(key_crc,rows+i*8);
   if(available)work[(index+i)>>3]|=1<<((index+i)&7);
  }index+=amount;
 }
 closed=card_io_close(ed_cmd_file_close);if(closed)error=closed;
 if(!error)error=verify_keys(total,key_crc,rows);
#endif
 if(error)return error;
 for(i=0;i<n;++i)if(matches[i]>=total)return CATALOG_ERR_RANGE;
 for(i=0;i<n;++i)if(work[matches[i]>>3]&(1<<(matches[i]&7)))matches[found++]=matches[i];
 *count=found;return 0;
}
#ifdef __CC65__
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
u8 cheat_index_read(u16 index,u32 key,CheatCoverage *coverage){
#ifdef __CC65__
 u8 bank=sysGetBank(),result;sysSetBank(COVERAGE_BANK);result=read_impl(index,key,coverage);sysSetBank(bank);return result;
#else
 return read_impl(index,key,coverage);
#endif
}
u8 cheat_index_filter(u16 *matches,u16 *count,u8 *work){
#ifdef __CC65__
 u8 bank=sysGetBank(),result;sysSetBank(COVERAGE_BANK);result=filter_impl(matches,count,work);sysSetBank(bank);return result;
#else
 return filter_impl(matches,count,work);
#endif
}
