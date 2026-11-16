#include "card_io.h"
/* Readonly catalog-bound HuCard/CUE launch metadata. Native OS owns game setup. */
#include "launch.h"
#include "cd_guard.h"
#include <string.h>
#ifndef PREVIEW
static u8 bytes[16];
static u32 path_crc(const char *path,u8 length) {
    u32 crc=0xffffffffUL;u8 bit;
    while(length--) {crc^=(u8)*path++;for(bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320UL:0);}
    return crc^0xffffffffUL;
}
#endif
static u8 failed(NativeBridgeReport *report,u8 error) {
    if(report)report->error=error;
    return error;
}
u8 launch_metadata(u16 catalog_index,const CatalogRecord *record,NativeBridgeTarget *target,NativeBridgeReport *report) {
    u8 length;
#ifndef PREVIEW
    u8 error,close_error;
    u16 count;
    u32 signature,key;
#endif
    if(report) {memset(report,0,sizeof(*report));report->stage=LAUNCH_STAGE_IDENTITY;}
    if(target)memset(target,0,sizeof(*target));
    if(card_io_fault())return failed(report,CARD_IO_BLOCKED);
    if(!report || !record || !target)return failed(report,LAUNCH_ERR_IDENTITY);
    if(record->kind!=1 && record->kind!=2)return failed(report,LAUNCH_ERR_UNSUPPORTED);
    for(length=0;length<sizeof(record->path) && record->path[length];++length) {}
    if(!length || length==sizeof(record->path))return failed(report,LAUNCH_ERR_IDENTITY);
    if(length<5 || record->path[length-4]!='.' ||
       ((u8)record->path[length-3]|32)!=(record->kind==1?'p':'c') ||
       ((u8)record->path[length-2]|32)!=(record->kind==1?'c':'u') ||
       ((u8)record->path[length-1]|32)!='e')return failed(report,LAUNCH_ERR_UNSUPPORTED);
#ifdef PREVIEW
    (void)catalog_index;
    return failed(report,LAUNCH_ERR_PREVIEW);
#else
    error=catalog_identity(&count,&signature);
    if(error || catalog_index>=count)return failed(report,LAUNCH_ERR_IDENTITY);
    key=pack_key(record->reserved);
    if(path_crc(record->path,length)!=key)return failed(report,LAUNCH_ERR_IDENTITY);
    report->detail=catalog_index;report->stage=LAUNCH_STAGE_HEADER;
    error=ed_cmd_file_open((u8*)(TEO_METADATA_ROOT "/launch.bin"),FA_READ);
    if(error)return failed(report,error);
    if(ed_cmd_file_available()!=16UL+(u32)count*12)error=LAUNCH_ERR_METADATA;
    if(!error)error=ed_cmd_file_read(bytes,sizeof(bytes));
    if(!error && (memcmp(bytes,"TLA1",4) || bytes[4]!=1 || bytes[5]!=12 || pack_key(bytes+12)))error=LAUNCH_ERR_METADATA;
    if(!error && (((u16)bytes[6]|((u16)bytes[7]<<8))!=count || pack_key(bytes+8)!=signature))error=LAUNCH_ERR_IDENTITY;
    if(!error) {
        report->stage=LAUNCH_STAGE_RECORD;
        error=ed_cmd_file_set_ptr(16UL+(u32)catalog_index*12);
        if(!error)error=ed_cmd_file_read(bytes,12);
        if(!error && pack_key(bytes)!=key) {report->detail=pack_key(bytes);error=LAUNCH_ERR_IDENTITY;}
        if(!error) {
            target->size=pack_key(bytes+4);target->crc32=pack_key(bytes+8);
            if(!target->size || target->size>NATIVE_BRIDGE_MAX_TARGET) {report->detail=target->size;error=LAUNCH_ERR_METADATA;}
        }
    }
    close_error=card_io_close(ed_cmd_file_close);
    if(close_error) {report->stage=LAUNCH_STAGE_CLOSE;if(!error)error=close_error;}
    if(error) {memset(target,0,sizeof(*target));return failed(report,error);}
    return 0;
#endif
}
u8 launch_selected(u16 catalog_index,const CatalogRecord *record,NativeBridgeReport *report) {
    NativeBridgeTarget target;
#ifndef PREVIEW
    u8 guard=0;
#endif
    u8 error=launch_metadata(catalog_index,record,&target,report);
    if(error)return error;
#ifndef PREVIEW
    /* No caller metadata handle remains open when native bridge reads SD. */
    report->stage=0x24;
    error=cd_guard_prepare(record,&target,&guard);
    if(error){if(card_io_fault())report->stage=LAUNCH_STAGE_CLOSE;return failed(report,error);}
    error=guard?native_bridge_run_guarded(record->path,&target,report,guard):native_bridge_run(record->path,&target,report);
    if(!error)error=NATIVE_BRIDGE_ERR_START;
#endif
    return failed(report,error);
}
