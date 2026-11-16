#include "card_io.h"
/* Isolated, single-threaded CPU bridge. SD files are READ-only.
 * Never installs a game using the published SDK; only starts native HUC_SYS.
 * The unchanged owned stub calls the native install/start ABI after init. */
#include "native_bridge.h"
#include "cd_guard_packet.h"
extern const CdGuardPacket *cd_guard_packet(void);
#include <string.h>
/* Bridge code/tables share bank11; resident wrappers own the mapping. */
#ifdef __CC65__
#pragma code-name(push,"BNK11")
#pragma rodata-name(push,"BNK11")
#endif
#include "stub_bytes.h"
#define MENU_BYTES 0x20000UL
#define NATIVE_CRC 0x3d7d8293UL
#define SLOT_BYTES 2048
#define HOOK_MEMORY (ADDR_MENU + 0x51c8UL)
#define SLOT_MEMORY (ADDR_MENU + 0x9000UL)
#define SAVE_ACK 0x01848000UL
/* The only module BSS. Caller owns path, expected metadata and report. */
static u8 scratch[64];
static const u8 original_hook[3]={0x20,0x9f,0x7c};
static const u8 replacement_hook[3]={0x20,0x00,0xd0};
static const u8 slot_header[64]={
    0x54,0x4e,0x42,0x31,1,0,64,0,0,0xd0,0xc8,0x91,0,4,0,0,
    0x11,0x18,0xd6,0xe0,0x07,0x40,0xea,0x6b,
    0xfd,0xe0,0x3d,0x2d,0x5e,0xd8,0x65,0x72,
    0x4b,0x82,0xd3,0x4f,0x35,0xf3,0xcb,0x58,
    0x4c,0xdf,0xce,0xe9,0x75,0x81,0x11,0xef,
    0,0,0,0,182,0,0,8,0,0,0,0,0,0,0,0
};
/* Native menu profiles: two audited fixed images; selection stays caller-local. */
#include "stub_fw26_bytes.h"
#define FW26_NATIVE_CRC 0x3a8c8ed7UL
#define PROFILE_HOOK_MEMORY(profile) ((profile)?(ADDR_MENU+0x51ffUL):HOOK_MEMORY)
#define PROFILE_ORIGINAL_HOOK(profile) ((profile)?fw26_original_hook:original_hook)
static const u8 fw26_original_hook[3]={0x20,0xd6,0x7c};
static const u8 fw26_slot_header[64]={
    0x54,0x4e,0x42,0x31,0x01,0x00,0x40,0x00,0x00,0xd0,0xff,0x91,0x00,0x04,0x00,0x00,
    0xa1,0x80,0x3c,0xb4,0x90,0xa7,0x4d,0xce,0xfd,0xe9,0x9b,0x59,0xfb,0x8e,0x16,0xfb,
    0x62,0x4e,0x09,0x54,0x74,0xc7,0xf2,0x2e,0xb0,0xa8,0xec,0xff,0x61,0x73,0xcb,0xf3,
    0x00,0x00,0x00,0x00,0xb6,0x00,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
#include "cd_guard_bytes.h"
#define CD_UPLOAD (ADDR_MENU+0x14c86UL)
static const u8 cd_original[]={0x20,0x64,0xf0};
static const u8 cd_hook[]={0x20,0x00,0xd1};
/* End native menu profiles. */
static u8 path_length(const char *path) {
    u16 n,segment=0,start=0;
    u8 c;
    if(!path) return 0;
    for(n=0;n<=NATIVE_BRIDGE_MAX_PATH;++n) {
        c=(u8)path[n];
        if(!c) break;
        if(c<32 || c>126 || c=='\\' || c==':' || c=='*' || c=='?' ||
           c=='"' || c=='<' || c=='>' || c=='|') return 0;
        if(c=='/') {
            if(!segment || path[start]==' ' || path[n-1]==' ' || path[n-1]=='.' ||
               (segment==1 && path[start]=='.') ||
               (segment==2 && path[start]=='.' && path[start+1]=='.')) return 0;
            segment=0;start=n+1;
        } else ++segment;
    }
    if(n>NATIVE_BRIDGE_MAX_PATH || segment<=4 || path[start]==' ' ||
       path[n-1]==' ' || path[n-1]=='.') return 0;
    /* Native catalog media: unchanged native ABI accepts original PCE/CUE. */
    c=(u8)path[n-3]|32;
    if(path[n-4]!='.' || ((u8)path[n-1]|32)!='e' || (c!='p' && c!='c') ||
       ((u8)path[n-2]|32)!=(c=='p'?'c':'u')) return 0;
    /* End native catalog media. */
    return (u8)n;
}
static u32 path_crc32(const char *path,u8 length) {
    u32 crc=0xffffffffUL;
    u8 bit;
    while(length--) {
        crc^=(u8)*path++;
        for(bit=0;bit<8;++bit) crc=(crc>>1)^((crc&1)?0xedb88320UL:0);
    }
    return crc^0xffffffffUL;
}
static u8 slot_byte(u16 offset,const char *path,u8 length,u32 crc,u8 profile,u8 guard) {
    if(guard) {
        if(offset<sizeof(cd_guard_stub))return cd_guard_stub[offset];
        if(guard==4) {
            const CdGuardPacket *packet=cd_guard_packet();
            if(offset>=0x100 && offset<0x100+sizeof(cd_callback_4))return cd_callback_4[offset-0x100];
            if(offset>=0x200 && offset<0x200+sizeof(cd_disarm_4))return cd_disarm_4[offset-0x200];
            if(offset>=0x500 && offset<0x580)return cd_packet_byte(packet,1,(u8)(offset-0x500));
            if(offset>=0x580 && offset<0x600)return cd_packet_byte(packet,0,(u8)(offset-0x580));
            if(offset==0x600)return cd_packet_size(packet,0);
            if(offset==0x601)return cd_packet_size(packet,1);
        } else {
            if(offset>=0x100 && offset<0x100+cd_callback_size[guard-1])return cd_callback[guard-1][offset-0x100];
            if(offset>=0x200 && offset<0x200+sizeof(cd_disarm))return cd_disarm[offset-0x200];
            if(offset>=0x500 && offset<0x500+cd_guard_size[guard-1])return cd_guard[guard-1][offset-0x500];
            if(offset>=0x580 && offset<0x580+cd_expected_size[guard-1])return cd_expected[guard-1][offset-0x580];
        }
    }else if(offset<sizeof(native_bridge_stub)) return profile?native_bridge_stub_fw26[offset]:native_bridge_stub[offset];
    if(offset>=0x300 && offset<0x340) {
        offset-=0x300;
        if(offset==14) return length;
        if(guard && offset==52)return sizeof(cd_guard_stub);
        if(offset>=48 && offset<52) return (u8)(crc>>((offset-48)*8));
        return profile?fw26_slot_header[offset]:slot_header[offset];
    }
    if(offset>=0x400 && offset<=0x400+length) return (u8)path[offset-0x400];
    return 0;
}
static u8 close_with(u8 error) {
    u8 result=card_io_close(ed_cmd_file_close);
    return error?error:result;
}
static u8 file_zero(u16 length,u8 mismatch) {
    u8 error,i,amount;
    while(length) {
        amount=length>sizeof(scratch)?sizeof(scratch):(u8)length;
        error=ed_cmd_file_read(scratch,amount);
        if(error) return error;
        for(i=0;i<amount;++i) if(scratch[i]) return mismatch;
        length-=amount;
    }
    return 0;
}
static u8 registry_guard(void) {
    u8 error=ed_cmd_file_open((u8*)"edturbo/sysdata/registry.dat",FA_READ);
    if(error) return error;
    if(ed_cmd_file_available()!=1193) return close_with(NATIVE_BRIDGE_ERR_REGISTRY);
    error=ed_cmd_file_set_ptr(0x49aUL);
    if(!error) error=ed_cmd_file_read(scratch,1);
    if(!error && scratch[0]) error=NATIVE_BRIDGE_ERR_REGISTRY;
    return close_with(error);
}

/* Native master status: exact pinned OS registry format, read-only UI query. */
static u8 cheat_state_impl(void) {
#ifdef PREVIEW
    return NATIVE_CHEATS_UNKNOWN;
#else
    u16 at=0;
    u8 error,state=NATIVE_CHEATS_UNKNOWN,n,i,value,master=0;
    u8 sum=0xaa,fold=0x55,version_lo=0,version_hi=0,tail_lo=0,tail_hi=0;
    if(card_io_fault())return NATIVE_CHEATS_UNKNOWN;
    error=ed_cmd_file_open((u8*)"edturbo/sysdata/registry.dat",FA_READ);
    if(error)return NATIVE_CHEATS_UNKNOWN;
    if(ed_cmd_file_available()!=1193UL)error=NATIVE_BRIDGE_ERR_REGISTRY;
    /* Native721E reads04A9 bytes;9FBF folds04A7 bytes from seedsAA/55.
     * Footer stores fold,sum; native72AC also requires version word04A5=1. */
    while(!error && at<1193U) {
        n=1193U-at>sizeof(scratch)?sizeof(scratch):(u8)(1193U-at);
        error=ed_cmd_file_read(scratch,n);
        if(!error)for(i=0;i<n;++i,++at) {
            value=scratch[i];
            if(at<0x4a7U){sum+=value;fold+=sum;}
            if(at==0x497U)master=value;
            else if(at==0x4a5U)version_lo=value;
            else if(at==0x4a6U)version_hi=value;
            else if(at==0x4a7U)tail_lo=value;
            else if(at==0x4a8U)tail_hi=value;
        }
    }
    if(!error && version_lo==1 && !version_hi && tail_lo==fold && tail_hi==sum)
        state=master?NATIVE_CHEATS_ON:NATIVE_CHEATS_OFF;
    if(close_with(error))state=NATIVE_CHEATS_UNKNOWN;
    return state;
#endif
}
/* End native master status. */

static u8 journal_guard(const char *path,u16 size,u8 allow_missing) {
    u8 error=ed_cmd_file_open((u8*)path,FA_READ);
    /* FR_NO_FILE=4 is allowed only for the optional HUC file. The registry
     * guard has just proved this same edturbo/sysdata parent is accessible.
     * FR_NO_PATH and every other error remain fatal. */
    if(error) return allow_missing && error==4 ? 0 : error;
    if(ed_cmd_file_available()!=size) return close_with(NATIVE_BRIDGE_ERR_JOURNAL);
    error=file_zero(size,NATIVE_BRIDGE_ERR_JOURNAL);
    return close_with(error);
}
static u8 target_guard(const char *path,const NativeBridgeTarget *expected,
                       NativeBridgeReport *report) {
    u32 crc=0;
    u8 error=ed_cmd_file_open((u8*)path,FA_READ);
    if(error) return error;
    report->detail=ed_cmd_file_available();
    if(report->detail!=expected->size) return close_with(NATIVE_BRIDGE_ERR_TARGET);
    error=ed_cmd_file_crc(expected->size,&crc);
    report->detail=crc;
    if(!error && crc!=expected->crc32) error=NATIVE_BRIDGE_ERR_TARGET;
    return close_with(error);
}
/* Caller holds the one open, readonly native file from this gate through DMA. */
static u8 native_guard(NativeBridgeReport *report,u8 *profile,u8 guard) {
    u8 error;
    u32 crc=0;
    if(ed_cmd_file_available()!=MENU_BYTES) return NATIVE_BRIDGE_ERR_MENU;
    error=ed_cmd_file_crc(MENU_BYTES,&crc);report->detail=crc;
    if(error) return error;
    if(crc==NATIVE_CRC) *profile=0;
    else if(crc==FW26_NATIVE_CRC) *profile=1;
    else return NATIVE_BRIDGE_ERR_MENU;
    error=ed_cmd_file_set_ptr(*profile?0x51ffUL:0x51c8UL);
    if(!error) error=ed_cmd_file_read(scratch,3);
    if(error) return error;
    if(memcmp(scratch,PROFILE_ORIGINAL_HOOK(*profile),3)) return NATIVE_BRIDGE_ERR_MENU;
    error=ed_cmd_file_set_ptr(0x9000UL);
    if(!error) error=file_zero(SLOT_BYTES,NATIVE_BRIDGE_ERR_MENU);
    if(error) return error;
    error=ed_cmd_file_set_ptr(0x1fff0UL);
    if(!error) error=file_zero(16,NATIVE_BRIDGE_ERR_MENU);
    if(error) return error;
    if(guard) {
        if(!*profile)return NATIVE_BRIDGE_ERR_MENU;
        error=ed_cmd_file_set_ptr(0x14c86UL);
        if(!error)error=ed_cmd_file_read(scratch,3);
        if(error)return error;
        if(memcmp(scratch,cd_original,3))return NATIVE_BRIDGE_ERR_MENU;
    }
    return ed_cmd_file_set_ptr(0);
}
static u8 memory_equal(u32 address,const u8 *data,u8 length) {
    u8 i;
    for(i=0;i<length;++i) scratch[i]=(u8)~data[i];
    ed_cmd_mem_rd(address,scratch,length);
    return !memcmp(scratch,data,length);
}
static u8 memory_zero(u32 address,u16 length) {
    u8 i,amount;
    while(length) {
        amount=length>sizeof(scratch)?sizeof(scratch):(u8)length;
        memset(scratch,0xff,amount);
        ed_cmd_mem_rd(address,scratch,amount);
        for(i=0;i<amount;++i) if(scratch[i]) return 0;
        address+=amount;length-=amount;
    }
    return 1;
}
static void write_slot(const char *path,u8 length,u32 crc,u8 profile,u8 guard) {
    u16 offset;
    u8 i;
    for(offset=0;offset<SLOT_BYTES;offset+=sizeof(scratch)) {
        for(i=0;i<sizeof(scratch);++i) scratch[i]=slot_byte(offset+i,path,length,crc,profile,guard);
        ed_cmd_mem_wr(SLOT_MEMORY+offset,scratch,sizeof(scratch));
    }
}
static u8 verify_slot(const char *path,u8 length,u32 crc,u8 profile,u8 guard) {
    u16 offset;
    u8 i;
    for(offset=0;offset<SLOT_BYTES;offset+=sizeof(scratch)) {
        /* Void SDK reads have no status: poison expected bytes first so a
         * missing/short transfer cannot pass on stale zero padding. */
        for(i=0;i<sizeof(scratch);++i) scratch[i]=(u8)~slot_byte(offset+i,path,length,crc,profile,guard);
        ed_cmd_mem_rd(SLOT_MEMORY+offset,scratch,sizeof(scratch));
        for(i=0;i<sizeof(scratch);++i)
            if(scratch[i]!=slot_byte(offset+i,path,length,crc,profile,guard)) return 0;
    }
    return 1;
}
static void rollback(NativeBridgeReport *report,u8 profile,u8 guard) {
    u16 offset;
    report->rollback_flags=NATIVE_BRIDGE_ROLLBACK_ATTEMPTED;
    if(guard) {
        report->rollback_flags|=NATIVE_BRIDGE_ROLLBACK_GUARDED;
        ed_cmd_mem_wr(CD_UPLOAD,(void*)cd_original,3);
        if(memory_equal(CD_UPLOAD,cd_original,3))report->rollback_flags|=NATIVE_BRIDGE_ROLLBACK_GUARD_OK;
    }
    ed_cmd_mem_wr(PROFILE_HOOK_MEMORY(profile),(void*)PROFILE_ORIGINAL_HOOK(profile),3);
    if(memory_equal(PROFILE_HOOK_MEMORY(profile),PROFILE_ORIGINAL_HOOK(profile),3)) report->rollback_flags|=NATIVE_BRIDGE_ROLLBACK_HOOK_OK;
    memset(scratch,0,sizeof(scratch));
    for(offset=0;offset<SLOT_BYTES;offset+=sizeof(scratch))
        ed_cmd_mem_wr(SLOT_MEMORY+offset,scratch,sizeof(scratch));
    if(memory_zero(SLOT_MEMORY,SLOT_BYTES)) report->rollback_flags|=NATIVE_BRIDGE_ROLLBACK_SLOT_OK;
}
static u8 run_impl(const char *path,const NativeBridgeTarget *expected,
                     NativeBridgeReport *report,u8 guard) {
    u8 length,error,opened=0,patch_started=0,profile=0;
    u32 crc;
    CartCfg cfg;
    if(!report) return NATIVE_BRIDGE_ERR_INPUT;
    memset(report,0,sizeof(*report));report->stage=1;
    if(card_io_fault()){report->error=CARD_IO_BLOCKED;return CARD_IO_BLOCKED;}
    length=path_length(path);
    if(guard>4 || (guard==4 && (!cd_guard_packet() || cd_packet_validate(cd_guard_packet()))) || !length || !expected || !expected->size || expected->size>NATIVE_BRIDGE_MAX_TARGET) {
        error=NATIVE_BRIDGE_ERR_INPUT;goto failed;
    }
    crc=path_crc32(path,length);
    report->stage=2;
    ed_init();ed_cmd_status(scratch);
    if(scratch[0]!=STATUS_KEY || scratch[1]!=PROTOCOL_ID || scratch[2]!=DEVID_TE_PRO) {
        error=NATIVE_BRIDGE_ERR_DEVICE;goto failed;
    }
    report->stage=3;error=registry_guard();if(error) goto failed;
    report->stage=4;
    error=journal_guard("edturbo/sysdata/bram_exp.dat",2052,guard!=0);if(error) goto failed;
    error=journal_guard("edturbo/sysdata/bram_huc.dat",8196,1);if(error) goto failed;
    report->stage=5;scratch[0]=0;ed_cmd_mem_rd(SAVE_ACK,scratch,1);report->detail=scratch[0];
    if(scratch[0]!=0xa0) {error=NATIVE_BRIDGE_ERR_ACK;goto failed;}
    report->stage=6;error=target_guard(path,expected,report);if(error) goto failed;
    report->stage=7;error=ed_cmd_file_open((u8*)"edturbo/menu.dat",FA_READ);if(error) goto failed;
    opened=1;error=native_guard(report,&profile,guard);if(error) goto failed;
    report->stage=8;error=ed_cmd_file_read_mem(ADDR_MENU,MENU_BYTES);
    error=close_with(error);opened=0;if(error) goto failed;
    if((guard && !memory_equal(CD_UPLOAD,cd_original,3)) || !memory_equal(PROFILE_HOOK_MEMORY(profile),PROFILE_ORIGINAL_HOOK(profile),3) || !memory_zero(SLOT_MEMORY,SLOT_BYTES) ||
       !memory_zero(ADDR_MENU+0x1fff0UL,16)) {error=NATIVE_BRIDGE_ERR_MEMORY;goto failed;}
    report->stage=9;patch_started=1;
    write_slot(path,length,crc,profile,guard);
    if(!verify_slot(path,length,crc,profile,guard)) {error=NATIVE_BRIDGE_ERR_PATCH;goto failed;}
    report->stage=10;
    if(guard) {
        ed_cmd_mem_wr(CD_UPLOAD,(void*)cd_hook,3);
        if(!memory_equal(CD_UPLOAD,cd_hook,3)){error=NATIVE_BRIDGE_ERR_PATCH;goto failed;}
    }
    ed_cmd_mem_wr(PROFILE_HOOK_MEMORY(profile),(void*)replacement_hook,3);
    if(!memory_equal(PROFILE_HOOK_MEMORY(profile),replacement_hook,3)) {error=NATIVE_BRIDGE_ERR_PATCH;goto failed;}
    report->stage=11;
    memset(&cfg,0,sizeof(cfg));cfg.reserved[0]=cfg.reserved[1]=0x80;
    cfg.huc_type=HUC_SYS;cfg.exp_type=EXP_OFF;
    ed_app_start(&cfg);
    error=NATIVE_BRIDGE_ERR_START;
failed:
    if(opened) error=close_with(error);
    if(patch_started) rollback(report,profile,guard);
    report->error=error;
    return error;
}

#ifdef __CC65__
#pragma code-name(pop)
#pragma rodata-name(pop)
#endif
u8 native_bridge_cheat_state(void) {
    u8 result;
#ifdef __CC65__
    u8 bank=sysGetBank();sysSetBank(15);
#endif
    result=cheat_state_impl();
#ifdef __CC65__
    sysSetBank(bank);
#endif
    return result;
}
u8 native_bridge_run_guarded(const char *path,const NativeBridgeTarget *expected,NativeBridgeReport *report,u8 guard) {
    u8 result;
#ifdef __CC65__
    u8 bank=sysGetBank();sysSetBank(15);
#endif
    result=run_impl(path,expected,report,guard);
#ifdef __CC65__
    sysSetBank(bank);
#endif
    return result;
}
u8 native_bridge_run(const char *path,const NativeBridgeTarget *expected,NativeBridgeReport *report) {
    return native_bridge_run_guarded(path,expected,report,0);
}
