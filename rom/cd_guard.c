/* Exact-revision launch preflight. No image/profile/save writes or persistent game RAM. */
#include "cd_guard.h"
#include "cheat_store_codec.h"
#include "card_io.h"
#include <string.h>
static CdGuardPacket packet;
static u8 packet_ready;
#ifdef __CC65__
#pragma code-name(push,"BNK10")
#pragma rodata-name(push,"BNK10")
#endif
typedef struct { u16 address; u8 value; } GuardRow;
typedef struct { const char *name; u32 size,crc; } GuardTrack;
typedef struct {
    u32 key,cue_size,cue_crc;
    const GuardTrack *tracks;
    u8 track_count;
    u16 signature_start;
    u8 signature[8],rows_count;
    const GuardRow *rows;
    u8 groups_count;
    const u32 *group_masks;
} GuardProfile;
#include "cd_guard_profiles.h"
static const u8 guard_vectors[6]={0x36,0xe7,0x70,0xe8,0xb3,0xe6};
static u8 finish(u8 error) {
    u8 e=card_io_close(ed_cmd_file_close);
    return e?CARD_IO_BLOCKED:error;
}
static u8 asset(const char *path,u32 bytes,u32 expected,u8 failure) {
    u8 e;u32 crc=0;
    e=ed_cmd_file_open((u8*)path,FA_READ);if(e)return failure;
    if(ed_cmd_file_available()!=bytes)return finish(failure);
    e=ed_cmd_file_crc(bytes,&crc);
    return finish(e||crc!=expected?failure:0);
}
static u8 selection(const char *path,u8 *candidate,const u8 *cache,u8 cached_count) {
    u8 line[64],row[26],e,c,n=0,p1=0,timer=0,other=0,first=0,count=0;
    u32 left,address;
    if(cache)left=cached_count;
    else {
        e=ed_cmd_file_open((u8*)path,FA_READ);if(e)return e==4?0:CD_GUARD_ERR_SELECTION;
        left=ed_cmd_file_available();if(left>1024)return finish(CD_GUARD_ERR_SELECTION);
    }
    while(left--) {
        if(cache){memcpy(row,cache+(u16)count*26,26);++count;}
        else {
        e=ed_cmd_file_read(&c,1);if(e)return finish(e);
        if(n>=63)return finish(CD_GUARD_ERR_SELECTION);
        line[n++]=c;
        if(c!='\n')continue;
        /* An all-off CD transaction exports a three-byte blank carrier. */
        if(n==3&&!memcmp(line," \r\n",3)){n=0;continue;}
        if(++count>16||!cs_parse_line(line,n,row))return finish(CD_GUARD_ERR_SELECTION);
        n=0;
        }
        if(!row[5])continue;
        address=(u32)row[0]|((u32)row[1]<<8)|((u32)row[2]<<16)|((u32)row[3]<<24);
        if(address==0xf82961UL||address==0x1f0961UL) {
            ++p1;if(!first)first=1;
            if(address!=0xf82961UL||row[4]!=0x23)other=1;
        }else if(address==0xf826f0UL&&row[4]==0x76) {
            ++timer;if(!first)first=2;
        }else other=1;
    }
    e=cache?0:finish(n?CD_GUARD_ERR_SELECTION:0);if(e)return e;
    if(!p1)return 0;
    if(p1!=1||timer>1||other)return CD_GUARD_ERR_SELECTION;
    *candidate=timer?(first==1?2:3):1;return 0;
}
static u8 profile_selection(const char *path,const GuardProfile *profile,u8 *candidate,const u8 *cache,u8 cached_count) {
    u8 line[64],row[26],e,c,n=0,count=0,i,matched=0,other=0;
    u32 left,address,selected_rows=0,covered=0;
    cd_packet_init(&packet,profile->signature_start,profile->signature,guard_vectors,profile->key==0x4f214820UL?2:0);
    if(cache)left=cached_count;
    else {
        e=ed_cmd_file_open((u8*)path,FA_READ);if(e)return e==4?0:CD_GUARD_ERR_SELECTION;
        left=ed_cmd_file_available();if(left>1024)return finish(CD_GUARD_ERR_SELECTION);
    }
    while(left--) {
        if(cache){memcpy(row,cache+(u16)count*26,26);++count;}
        else {
        e=ed_cmd_file_read(&c,1);if(e)return finish(e);
        if(n>=63)return finish(CD_GUARD_ERR_SELECTION);
        line[n++]=c;if(c!='\n')continue;
        if(n==3&&!memcmp(line," \r\n",3)){n=0;continue;}
        if(++count>16||!cs_parse_line(line,n,row))return finish(CD_GUARD_ERR_SELECTION);
        n=0;
        }
        if(!row[5])continue;
        address=(u32)row[0]|((u32)row[1]<<8)|((u32)row[2]<<16)|((u32)row[3]<<24);
        if(address<0xf82000UL||address>0xf83fffUL){other=1;continue;}
        for(i=0;i<profile->rows_count;++i)
            if(profile->rows[i].address==(u16)address&&profile->rows[i].value==row[4])break;
        if(i==profile->rows_count){other=1;continue;}
        matched=1;selected_rows|=1UL<<i;
        if(cd_packet_add(&packet,(u16)address,row[4]))return cache?CD_GUARD_ERR_SELECTION:finish(CD_GUARD_ERR_SELECTION);
    }
    e=cache?0:finish(n?CD_GUARD_ERR_SELECTION:0);if(e||!matched)return e;
    for(i=0;i<profile->groups_count;++i)
        if((selected_rows&profile->group_masks[i])==profile->group_masks[i])covered|=profile->group_masks[i];
    if(other||covered!=selected_rows||cd_packet_validate(&packet))return CD_GUARD_ERR_SELECTION;
    *candidate=4;return 0;
}
static u8 prepare(const CatalogRecord *record,const NativeBridgeTarget *target,u8 *mode) {
    char path[160];u8 e,candidate=0,i;
    const GuardProfile *profile=0;
    u32 key;
    *mode=0;packet_ready=0;
    if(card_io_fault())return CARD_IO_BLOCKED;
    if(!record||!target)return CD_GUARD_ERR_IMAGE;
    if(record->kind!=2)return 0;
    key=(u32)record->reserved[0]|((u32)record->reserved[1]<<8)|((u32)record->reserved[2]<<16)|((u32)record->reserved[3]<<24);
    if(key!=0x4f214820UL) {
        for(i=0;i<GUARD_PROFILE_COUNT;++i)if(guard_profiles[i].key==key){profile=&guard_profiles[i];break;}
        if(!profile)return 0;
    }
    if(!cs_stock_path(record->path,path))return CD_GUARD_ERR_SELECTION;
    e=profile?profile_selection(path,profile,&candidate,0,0):selection(path,&candidate,0,0);if(e)return e;
    /* Fixed P1 modes keep their exact path. Only when none matched may the
     * separately pinned P2 profile select the generic, bounded packet. */
    if(!candidate && key==0x4f214820UL) {
        for(i=0;i<GUARD_PROFILE_COUNT;++i)if(guard_profiles[i].key==key){profile=&guard_profiles[i];break;}
        if(profile){e=profile_selection(path,profile,&candidate,0,0);if(e)return e;}
    }
    if(!candidate)return 0;
    if(target->size!=(profile?profile->cue_size:1655UL)||target->crc32!=(profile?profile->cue_crc:0x7b531a22UL))return CD_GUARD_ERR_IMAGE;
    if(native_bridge_cheat_state()!=NATIVE_CHEATS_ON)return card_io_fault()?CARD_IO_BLOCKED:CD_GUARD_ERR_STATE;
    e=asset("edturbo/bios/System Card 3.0 Japan.pce",262144UL,0x6d9a73efUL,CD_GUARD_ERR_BIOS);if(e)return e;
    /* launch_metadata has bounded and validated this catalog path. */
    if(strlen(record->path)>127||!strrchr(record->path,'/'))return CD_GUARD_ERR_IMAGE;
    strcpy(path,record->path);
    if(profile) {
        for(i=0;i<profile->track_count;++i) {
            strcpy(strrchr(path,'/')+1,profile->tracks[i].name);
            e=asset(path,profile->tracks[i].size,profile->tracks[i].crc,CD_GUARD_ERR_IMAGE);if(e)return e;
        }
    }else {
        strcpy(strrchr(path,'/')+1,"Track 02.bin");
        e=asset(path,5828256UL,0xd2ee10e6UL,CD_GUARD_ERR_IMAGE);if(e)return e;
    }
    *mode=candidate;packet_ready=candidate==4;return 0;
}


/* Uses the same selection rules as launch preflight, before the draft is saved.
 * The caller's 26-byte rows are in RAM; no card or native state is touched. */
static u8 check_rows(const CatalogRecord *record,const NativeBridgeTarget *target,const u8 *cache,u8 count) {
    u8 i,e=0,candidate=0;u32 key;const GuardProfile *profile=0;
    packet_ready=0;
    if(!record||!target||!cache||count>16)return CD_GUARD_ERR_SELECTION;
    if(record->kind!=2)return 0;
    key=(u32)record->reserved[0]|((u32)record->reserved[1]<<8)|((u32)record->reserved[2]<<16)|((u32)record->reserved[3]<<24);
    for(i=0;i<count;++i)if(cache[(u16)i*26+5]>1)return CD_GUARD_ERR_SELECTION;
    if(key==0x4f214820UL){e=selection(0,&candidate,cache,count);if(e)return e;}
    if(!candidate){
        for(i=0;i<GUARD_PROFILE_COUNT;++i)if(guard_profiles[i].key==key){profile=&guard_profiles[i];break;}
        if(profile){e=profile_selection(0,profile,&candidate,cache,count);if(e)return e;}
    }
    if(candidate&&(target->size!=(profile?profile->cue_size:1655UL)||target->crc32!=(profile?profile->cue_crc:0x7b531a22UL)))return CD_GUARD_ERR_IMAGE;
    return 0;
}

#ifdef __CC65__
#pragma code-name(pop)
#pragma rodata-name(pop)
#endif
u8 cd_guard_prepare(const CatalogRecord *record,const NativeBridgeTarget *target,u8 *mode) {
    u8 e;
#ifdef __CC65__
    u8 bank=sysGetBank();sysSetBank(14);
#endif
    e=prepare(record,target,mode);
#ifdef __CC65__
    sysSetBank(bank);
#endif
    return e;
}

const CdGuardPacket *cd_guard_packet(void) { return packet_ready?&packet:0; }

u8 cd_guard_check_rows(const CatalogRecord *record,const NativeBridgeTarget *target,const u8 *cache,u8 count) {
    u8 e;
#ifdef __CC65__
    u8 bank=sysGetBank();sysSetBank(14);
#endif
    e=check_rows(record,target,cache,count);
#ifdef __CC65__
    sysSetBank(bank);
#endif
    return e;
}
