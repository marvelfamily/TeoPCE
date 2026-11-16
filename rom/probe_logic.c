#include "main.h"
#include "device.h"
#include <string.h>

/* Research probe only. All files are opened read-only; scratch is volatile PSRAM. */
#define PROBE_HUC 1
#define PROBE_CD 2
#define PROBE_BAD_INPUT 0xe0
#define PROBE_BAD_SIZE 0xe1
#define PROBE_MAX_ROM 0x100000UL
#define PROBE_SELF_SIZE 0x20000UL

static u8 self_path[512];
static u8 marker[8];
static CartCfg cfg;

u8 probe_check_device(const u8 *status) {
    return device_check(status);
}

static u8 file_size(u8 *path, u32 *size) {
    u8 error = ed_cmd_file_open(path, FA_READ);
    if (error) return error;
    *size = ed_cmd_file_available();
    return ed_cmd_file_close();
}

static u8 load_file(u8 *path, u32 address, u32 max_size, u32 exact_size) {
    u8 error, close_error;
    u32 size;
    error = ed_cmd_file_open(path, FA_READ);
    if (error) return error;
    size = ed_cmd_file_available();
    if (!size || size > max_size || (exact_size && size != exact_size)) {
        ed_cmd_file_close();
        return PROBE_BAD_SIZE;
    }
    error = ed_cmd_file_read_mem(address, size);
    close_error = ed_cmd_file_close();
    return error ? error : close_error;
}

static void configure(u8 huc, u8 exp) {
    memset(&cfg, 0, sizeof(cfg));
    /* The upstream CartCfg's reserved bytes are volume registers on PRO. */
    cfg.reserved[0] = 0x80;
    cfg.reserved[1] = 0x80;
    cfg.huc_type = huc;
    cfg.exp_type = exp;
}

u8 probe_begin(u8 mode) {
    u8 error;
    u32 size;
    if (mode != PROBE_HUC && mode != PROBE_CD) return PROBE_BAD_INPUT;
    error = file_size((u8 *)(mode == PROBE_HUC ? "teo-probe/huc.pce" : "teo-probe/bios.pce"), &size);
    if (error) return error;
    if (!size || size > PROBE_MAX_ROM || (mode == PROBE_CD && size != 0x40000UL))
        return PROBE_BAD_SIZE;
    if (mode == PROBE_CD) {
        error = file_size((u8 *)"teo-probe/cd/240pSuite.cue", &size);
        if (error) return error;
        if (!size) return PROBE_BAD_SIZE;
    }
    error = ed_cmd_rom_path(self_path, 0);
    if (error) return error;
    error = load_file(self_path, ADDR_MENU, PROBE_SELF_SIZE, PROBE_SELF_SIZE);
    if (error) return error;
    memcpy(marker, "TEOPB1", 6);
    marker[6] = mode;
    marker[7] = mode ^ 0xff;
    ed_cmd_mem_wr(ADDR_GP_BUFF, marker, sizeof(marker));
    configure(HUC_SYS, EXP_OFF);
    ed_app_start(&cfg);
    return 0;
}

u8 probe_resume(void) {
    u8 mode, error;
    ed_cmd_mem_rd(ADDR_GP_BUFF, marker, sizeof(marker));
    if (memcmp(marker, "TEOPB1", 6) || (marker[6] != PROBE_HUC && marker[6] != PROBE_CD) ||
        marker[7] != (u8)(marker[6] ^ 0xff)) return 0;
    mode = marker[6];
    memset(marker, 0, sizeof(marker));
    ed_cmd_mem_wr(ADDR_GP_BUFF, marker, sizeof(marker));
    error = load_file((u8 *)(mode == PROBE_HUC ? "teo-probe/huc.pce" : "teo-probe/bios.pce"),
                      ADDR_ROM, PROBE_MAX_ROM, mode == PROBE_CD ? 0x40000UL : 0);
    if (error) return error;
    if (mode == PROBE_CD) {
        error = ed_cmd_cd_mount((u8 *)"teo-probe/cd/240pSuite.cue");
        if (error) return error;
    }
    configure(mode == PROBE_CD ? HUC_SSC : HUC_STD, mode == PROBE_CD ? EXP_CDR : EXP_OFF);
    ed_app_start(&cfg);
    return 0;
}
