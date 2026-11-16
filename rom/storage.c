#include "card_io.h"
#include "main.h"
#include "storage.h"
#include "app_paths.h"
#include <string.h>

/* Shared by sequential read, write and readback operations. */
static u8 storage_buffer[PREFS_SIZE];
/* cc65 cannot address a >255-byte automatic Preferences object. */
static Preferences storage_candidate;

static u8 *slot_path(u8 slot) {
    return (u8 *)(slot ? TEO_USER_ROOT "/prefs1.bin" :
                         TEO_USER_ROOT "/prefs0.bin");
}

static u8 finish_file(u8 result) {
    u8 close_result = card_io_close(ed_cmd_file_close);
    return result ? result : close_result;
}

static u8 read_slot(u8 slot, Preferences *prefs) {
    u8 result;
    if (card_io_fault()) return CARD_IO_BLOCKED;
    result = ed_cmd_file_open(slot_path(slot), FA_READ);
    if (result) return result;
    if (ed_cmd_file_available() != PREFS_SIZE)
        return finish_file(STORAGE_BAD_DATA);
    result = ed_cmd_file_read(storage_buffer, PREFS_SIZE);
    result = finish_file(result);
    if (result) return result;
    if (!prefs_decode(prefs, storage_buffer, PREFS_SIZE))
        return STORAGE_BAD_DATA;
    /* A misplaced sequence would break safe alternation on the next save. */
    if ((prefs->sequence & 1) != slot) return STORAGE_BAD_DATA;
    return 0;
}

u8 storage_load(Preferences *prefs) {
    u8 slot, result, have_valid = 0, first_error = 0, bad_data = 0;

    if (card_io_fault()) return CARD_IO_BLOCKED;
    memset(prefs, 0, sizeof(*prefs));
    for (slot = 0; slot < 2; ++slot) {
        result = read_slot(slot, &storage_candidate);
        if (card_io_fault()) return result;
        if (!result) {
            if (!have_valid || prefs_newer(storage_candidate.sequence, prefs->sequence))
                *prefs = storage_candidate;
            have_valid = 1;
        } else if (result == STORAGE_BAD_DATA) {
            bad_data = 1;
        } else if (result != 4 && result != 5 && !first_error) {
            /* FatFs FR_NO_FILE / FR_NO_PATH mean a normal first launch. */
            first_error = result;
        }
    }
    /* I/O failure leaves the unseen slot ambiguous, even when its sibling is valid. */
    if (first_error) return first_error;
    return have_valid ? 0 : bad_data ? STORAGE_BAD_DATA : 0;
}

u8 storage_save(Preferences *prefs) {
    u32 next_sequence = (prefs->sequence + 1) & 0xffffffffUL;
    u8 slot, result, i;

    if (card_io_fault()) return CARD_IO_BLOCKED;
    storage_candidate = *prefs;
    storage_candidate.sequence = next_sequence;
    if (!prefs_encode(&storage_candidate, storage_buffer)) return STORAGE_BAD_DATA;
    result = ed_cmd_dir_make((u8 *)TEO_USER_ROOT);
    if (result && result != 8) return result; /* FatFs FR_EXIST is allowed. */
    slot = (u8)(next_sequence & 1);
    result = ed_cmd_file_open(slot_path(slot), FA_WRITE | FA_CREATE_ALWAYS);
    if (result) return result;
    result = ed_cmd_file_write(storage_buffer, PREFS_SIZE);
    result = finish_file(result);
    if (result) return result;

    result = read_slot(slot, &storage_candidate);
    if (result) return result;
    if (storage_candidate.sequence != next_sequence || storage_candidate.count != prefs->count ||
        storage_candidate.view != prefs->view || storage_candidate.theme != prefs->theme)
        return STORAGE_VERIFY_FAILED;
    for (i = 0; i < FAVORITES_MAX; ++i)
        if (storage_candidate.keys[i] != prefs->keys[i]) return STORAGE_VERIFY_FAILED;
    prefs->sequence = next_sequence;
    return 0;
}
