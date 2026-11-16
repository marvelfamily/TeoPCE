#include "description.h"
#include "card_io.h"
#include "resource_root.h"
#include <string.h>
#ifdef PREVIEW
#include "preview.h"
#define file_read(buffer,size) preview_description_read(buffer,size)
#define file_seek(offset) preview_description_seek(offset)
#define file_close preview_description_close
#else
#define file_read(buffer,size) ed_cmd_file_read(buffer,size)
#define file_seek(offset) ed_cmd_file_set_ptr(offset)
#define file_close ed_cmd_file_close
#endif
#ifdef __CC65__
#ifdef PREVIEW
#define DESCRIPTION_BANK 16
#pragma code-name(push,"BNK12")
#pragma rodata-name(push,"BNK12")
#else
#define DESCRIPTION_BANK 10
#pragma code-name(push,"BNK06")
#pragma rodata-name(push,"BNK06")
#endif
#endif
static u8 failed(Description *state,u8 *work,u8 error){
    work[0]=0;state->error=error;state->status=DESCRIPTION_ERROR;return state->status;
}
static u8 open_file(u16 index,u32 *size){
#ifndef PREVIEW
    char path[]=TEO_RESOURCE_ROOT "/desc/000000.txt";u8 i,result;
#endif
    if(card_io_fault())return CARD_IO_BLOCKED;
#ifdef PREVIEW
    return preview_description_open(index,size);
#else
    for(i=0;i<6;++i){path[sizeof(path)-6-i]='0'+index%10;index/=10;}
    result=ed_cmd_file_open((u8*)path,FA_READ);
    if(!result)*size=ed_cmd_file_available();
    return result;
#endif
}
static u8 finish(u8 error){
    u8 close_error=card_io_close(file_close);
    return close_error?close_error:error;
}
/* Validate even the last partial row; no untrusted byte reaches the screen. */
static u8 valid_window(u8 *work,u16 size){
    u16 i;u8 columns=0,c;
    for(i=0;i<size;++i){
        c=work[i];
        if(c=='\n')columns=0;
        else if(c<32||c>126||++columns>DESCRIPTION_COLUMNS)return DESCRIPTION_ERR_FORMAT;
    }
    return 0;
}
static u8 window(u16 offset,u16 size,u8 *work){
    u16 length=size-offset;u8 error;
    if(length>DESCRIPTION_WINDOW)length=DESCRIPTION_WINDOW;
    error=file_seek(offset);
    if(!error)error=file_read(work,length);
    if(!error)error=valid_window(work,length);
    if(!error)work[length]=0;
    return error;
}
static u8 read_impl(Description *state,u8 *work){
    u32 size=0;u8 error;
    if(card_io_fault())return failed(state,work,CARD_IO_BLOCKED);
    if(state->status!=DESCRIPTION_READY){work[0]=0;return state->status;}
    error=open_file(state->index,&size);
    if(error)return failed(state,work,error);
    error=size!=state->size?DESCRIPTION_ERR_FORMAT:window(state->offset,state->size,work);
    error=finish(error);
    return error?failed(state,work,error):state->status;
}
static u8 open_impl(Description *state,u16 index,u8 *work){
    u32 size=0;u16 remaining,count,at,lines=1;u8 error,c,columns=0;
    memset(state,0,sizeof(*state));state->index=index;work[0]=0;
    error=open_file(index,&size);
    if(error){
        if(error==4||error==5){state->status=DESCRIPTION_MISSING;return state->status;}
        return failed(state,work,error);
    }
    error=size>DESCRIPTION_MAX_BYTES?DESCRIPTION_ERR_FORMAT:0;
    remaining=error?0:(u16)size;
    while(remaining&&!error){
        count=remaining>DESCRIPTION_WORK_BYTES?DESCRIPTION_WORK_BYTES:remaining;
        error=file_read(work,count);if(error)break;
        remaining-=count;
        for(at=0;at<count;++at){
            c=work[at];
            if(c=='\n'){
                columns=0;if(remaining||at+1<count)++lines;
            }else if(c<32||c>126||++columns>DESCRIPTION_COLUMNS){error=DESCRIPTION_ERR_FORMAT;break;}
        }
    }
    error=finish(error);
    if(error)return failed(state,work,error);
    state->size=(u16)size;state->lines=size?lines:0;
    state->status=size?DESCRIPTION_READY:DESCRIPTION_EMPTY;
    return read_impl(state,work);
}
static u8 move_impl(Description *state,u8 backward,u8 *work){
    u32 size=0;u16 start,length,target,i;u8 error;
    if(card_io_fault())return failed(state,work,CARD_IO_BLOCKED);
    if(state->status!=DESCRIPTION_READY)return state->status;
    if(backward?!state->line:state->line+DESCRIPTION_ROWS>=state->lines)return state->status;
    error=open_file(state->index,&size);
    if(error)return failed(state,work,error);
    target=state->offset;
    if(size!=state->size)error=DESCRIPTION_ERR_FORMAT;
    if(!error){
        start=backward?(target>39?target-39:0):target;
        length=backward?target-start:state->size-target;
        if(length>39)length=39;
        error=file_seek(start);if(!error)error=file_read(work,length);
        if(!error)error=valid_window(work,length);
        if(!error&&backward){
            if(!length||work[length-1]!='\n')error=DESCRIPTION_ERR_FORMAT;
            else{target=start;i=length-1;while(i){--i;if(work[i]=='\n'){target=start+i+1;break;}}}
        }else if(!error){
            for(i=0;i<length&&work[i]!='\n';++i){}
            if(i==length)error=DESCRIPTION_ERR_FORMAT;else target=start+i+1;
        }
        if(!error)error=window(target,state->size,work);
    }
    error=finish(error);
    if(error)return failed(state,work,error);
    state->offset=target;if(backward)--state->line;else ++state->line;
    return state->status;
}
#ifdef __CC65__
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
u8 description_open(Description *state,u16 index,u8 *work){
#ifdef __CC65__
    u8 bank=sysGetBank(),result;sysSetBank(DESCRIPTION_BANK);result=open_impl(state,index,work);sysSetBank(bank);return result;
#else
    return open_impl(state,index,work);
#endif
}
u8 description_read(Description *state,u8 *work){
#ifdef __CC65__
    u8 bank=sysGetBank(),result;sysSetBank(DESCRIPTION_BANK);result=read_impl(state,work);sysSetBank(bank);return result;
#else
    return read_impl(state,work);
#endif
}
u8 description_move(Description *state,u8 backward,u8 *work){
#ifdef __CC65__
    u8 bank=sysGetBank(),result;sysSetBank(DESCRIPTION_BANK);result=move_impl(state,backward,work);sysSetBank(bank);return result;
#else
    return move_impl(state,backward,work);
#endif
}
