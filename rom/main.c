#include "ui_hint.h"
#include "main.h"
#include "device.h"
#include "trace.h"
#include "catalog.h"
#include "input.h"
#include "navigation.h"
#include "preferences.h"
#include "pack.h"
#include "picture.h"
#include "launch.h"
#include "cd_guard.h"
#include "cheat_groups.h"
#include "cheat_store.h"
#include "cheat_recovery.h"
#include "cheat_legacy.h"
#include "game_info.h"
#include "game_meta.h"
#include "cheat_index.h"
#include "description.h"
#include "card_io.h"
#include "library_ui.h"
#ifndef PREVIEW
#include "storage.h"
#else
#include "preview.h"
#endif
#include <string.h>

#define LIST_ROWS 12
#define GRID_ROWS 6
#define GRID_COLS 3
#define GRID_CAPTION_WIDTH 11
#define LIST_TITLE_WIDTH 22
#define SCREEN_LIST 0
#define SCREEN_DETAIL 1
#define SCREEN_FILTER 2
#define SCREEN_INFO 3
#define SCREEN_CHEATS 4
#define SCREEN_ACTIONS 5
#define SCREEN_RETURN 6
#define SCREEN_START 7
#define SCREEN_START_ERROR 8
#define SCREEN_CHEAT_RECOVERY 9
#define SCREEN_CARD_RESTART 10

static CatalogRecord current;
static u16 matches[CATALOG_MAX];
static char titles[LIST_ROWS][24];
static u32 page_keys[LIST_ROWS];
static Preferences prefs;
static CatalogFilter filter, previous_filter;
static LibraryUI library;
static Recent recent;
static u16 query_anchor;
static Cursor cursor;
static InputState input;
static u8 text_buffer[CHEAT_STORE_WORK_BYTES];
typedef char description_workspace_fits[(CHEAT_STORE_WORK_BYTES>=DESCRIPTION_WORK_BYTES)?1:-1];
static u16 total, cheat_lines, cheat_page;
static Description description;
static u8 screen, page_loaded, art_loaded, pending_detail, settle_frames;
static u8 picture_kind, current_ready;
static u8 browse_title_offset, browse_title_wait;
static u8 previous_view, previous_theme, save_error, error_code;
static u8 favorites_changed, prefs_load_error, recent_changed;
static u8 action_parent, action_row, cheat_parent, cover_missing, start_parent;
static const char *notice;
static u8 notice_frames;
static NativeBridgeReport launch_report;
static u8 launch_error, native_blocked;
static u8 group_mode,saved_codes;
static const char saved_on[]="Saved codes: ON";
static const char saved_off[]="Saved codes: OFF";
static const char saved_unknown[]="Saved codes: ?";

static const char *media[] = {"ALL MEDIA", "HUCARD", "CD-ROM", "SUPERGRAFX"};
static const u16 theme_colors[THEME_COUNT][5] = {
    {0x001, 0x16d, 0x1ff, 0x196, 0x0f8},
    {0x000, 0x152, 0x1ff, 0x17a, 0x179},
    {0x001, 0x16d, 0x1ff, 0x1af, 0x1c7},
    {0x000, 0x1b1, 0x1b6, 0x124, 0x179}
};

#define say ui_text
/* Preserve word boundaries when they fit, and retain every title byte. */
static void say_wrapped(u8 x, u8 y, const char *s, u8 width, u8 rows) {
    u8 length, space, end;
    const char *rest;
    for (; *s && rows; --rows, ++y) {
        while (*s == ' ') ++s;
        length = 0; space = 0;
        while (length < width && s[length] && s[length] != '\n') {
            if (s[length] == ' ') space = length;
            ++length;
        }
        end = length;
        if (length == width && s[length] && s[length] != ' ' &&
                s[length] != '\n' && space) {
            rest = s + space;
            while (*rest == ' ') ++rest;
            if (strlen(rest) <= (u16)(rows - 1) * width) end = space;
        }
        say(x, y, s, end); s += end;
        while (*s == ' ' || *s == '\n') ++s;
    }
}
static void clear_line(u8 y) { gSetPal(PAL_01); gFillRow(' ', 1, y, 38); }
static u8 page_size(void) { return prefs.view ? GRID_ROWS : LIST_ROWS; }
static u8 filters_active(void) {
    return filter.medium || filter.genre || filter.letter || filter.region || filter.favorites || filter.recent || filter.cheats || filter.category || filter.prefix[0];
}
#ifndef PREVIEW
#endif
static void announce(const char *message) { notice = message; notice_frames = 120; }
static void footer(const char *first, const char *second) {
    ui_hint_footer(first,second);
}
static void heading(const char *label) {
    gSetPal(PAL_OK); gFillRow(' ', 1, 1, 38);
    say(2, 1, label, 36);
    gSetPal(PAL_01);
}
static void draw_counter(void) {
    gSetPal(PAL_OK); gFillRow(' ', 28, 1, 10);
    gSetXY(28, 1); gAppendNum(cursor.count ? cursor.selected + 1 : 0);
    gAppendChar('/'); gAppendNum(cursor.count);
}
#ifdef __CC65__
#ifdef PREVIEW
#pragma rodata-name(push,"BNK14")
#else
#pragma rodata-name(push,"BNK07")
#endif
#endif
static const char status_prefs_read_error[] = "PREFS READ ERROR / RESTART TO RETRY";
static const char status_prefs_save_error[] = "PREFS NOT SAVED / SD ERROR ";
static const char status_page_help[] = "LEFT/RIGHT Page / HOLD to scroll";
static const char card_restart_heading[] = "TEOPCE / SD RESTART REQUIRED";
static const char card_restart_close[] = "SD FILE CLOSE WAS NOT VERIFIED";
static const char card_restart_power[] = "Turn the console off, then on.";
static const char card_restart_blocked[] = "SD access, Start and Return are blocked.";
static const char card_restart_close_label[] = "Close error:";
static const char card_restart_footer[] = "Restart console to continue";
static const char device_key_mismatch[] = "STATUS KEY MISMATCH";
static const char device_protocol_mismatch[] = "PROTOCOL MISMATCH";
static const char device_id_mismatch[] = "DEVICE ID MISMATCH";
static const char device_heading[] = "TEOPCE 0.9.13 / DEVICE STATUS";
static const char device_error_label[] = "Error code:";
static const char device_got_label[] = "GOT:";
static const char device_expect[] = "EXPECT: 5A 02 20 --";
static const char device_last_byte[] = "LAST BYTE = COMMAND STATUS";
static const char device_no_library[] = "No library files were opened.";
static const char device_retry[] = "I Retry";
static const char device_reset[] = "Reset to return to the stock menu";
static const char return_heading[] = "TEOPCE 0.9.13 / RETURN TO EVERDRIVE";
static const char return_question[] = "Return to the EverDrive menu?";
static const char return_note[] = "Choose games in the EverDrive menu.";
static const char return_footer[] = "I Return  II Cancel";
#ifdef __CC65__
#pragma rodata-name(pop)
#ifdef PREVIEW
#pragma code-name(push,"BNK14")
#pragma rodata-name(push,"BNK14")
#else
#pragma code-name(push,"BNK07")
#pragma rodata-name(push,"BNK07")
#endif
#endif
static void status_line_impl(void) {
    clear_line(23); gSetPal(PAL_00);
    if (notice_frames && notice) say(1, 23, notice, 38);
    else if (prefs_load_error) say(1, 23, status_prefs_read_error, 38);
    else if (save_error) {
        say(1, 23, status_prefs_save_error, 30);
        gSetXY(31, 23); gAppendHex8(save_error);
    } else if(current_ready&&saved_codes==CHEAT_SAVED_ON)say(1,23,saved_on,38);
    else if(current_ready&&saved_codes==CHEAT_SAVED_UNKNOWN)say(1,23,saved_unknown,38);
    else say(1, 23, status_page_help, 38);
}
static void apply_theme_impl(void) {
    const u16 *c = theme_colors[prefs.theme];
    gSetColor(4, c[0]); gSetColor(5, c[1]);
    gSetColor(20, c[0]); gSetColor(21, c[2]);
    gSetColor(36, c[3]); gSetColor(37, c[0]);
    gSetColor(52, c[0]); gSetColor(53, c[4]);
}
static void draw_card_restart_impl(void) {
    screen=SCREEN_CARD_RESTART;gCleanScreen();heading(card_restart_heading);
    gSetPal(PAL_ER);say(1,8,card_restart_close,38);
    gSetPal(PAL_01);say(1,11,card_restart_power,38);
    say(1,14,card_restart_blocked,38);
    gSetPal(PAL_00);say(1,20,card_restart_close_label,15);gSetXY(15,20);gAppendHex8(card_io_fault());
    footer(card_restart_footer,"");
}
static void device_error_screen_impl(const u8 *status, u8 code) {
    u8 i;
    const char *reason = code == DEVICE_ERR_KEY ? device_key_mismatch :
        code == DEVICE_ERR_PROTOCOL ? device_protocol_mismatch : device_id_mismatch;
    gCleanScreen(); heading(device_heading);
    gSetPal(PAL_01); say(2, 3, device_error_label, 20);
    gSetXY(15, 3); gAppendHex8(code);
    gSetPal(PAL_ER); say(2, 5, reason, 35);
    gSetPal(PAL_01); say(2, 8, device_got_label, 5); gSetXY(10, 8);
    for (i = 0; i < 4; ++i) { gAppendHex8(status[i]); gAppendChar(' '); }
    say(2, 10, device_expect, 35);
    say(2, 13, device_last_byte, 35);
    say(2, 16, device_no_library, 35);
    footer(device_retry, device_reset);
    while (!(sysJoyWait() & JOY_B)) {}
}
static void draw_return_confirm_impl(void) {
    gCleanScreen(); heading(return_heading);
    gSetPal(PAL_01); say(2, 8, return_question, 36);
    gSetPal(PAL_00); say(2, 11, return_note, 36);
    footer(return_footer, "");
}
#ifdef __CC65__
#pragma rodata-name(pop)
#pragma code-name(pop)
#ifdef PREVIEW
#define MAIN_RENDER_BANK 18
#else
#define MAIN_RENDER_BANK 11
#endif
#endif
static void apply_theme(void) {
#ifdef __CC65__
    u8 bank=sysGetBank();sysSetBank(MAIN_RENDER_BANK);
#endif
    apply_theme_impl();
#ifdef __CC65__
    sysSetBank(bank);
#endif
}
static void draw_card_restart(void) {
#ifdef __CC65__
    u8 bank=sysGetBank();sysSetBank(MAIN_RENDER_BANK);
#endif
    draw_card_restart_impl();
#ifdef __CC65__
    sysSetBank(bank);
#endif
}
static void device_error_screen(const u8 *status, u8 code) {
#ifdef __CC65__
    u8 bank=sysGetBank();sysSetBank(MAIN_RENDER_BANK);
#endif
    device_error_screen_impl(status,code);
#ifdef __CC65__
    sysSetBank(bank);
#endif
}
static void draw_return_confirm(void) {
#ifdef __CC65__
    u8 bank=sysGetBank();sysSetBank(MAIN_RENDER_BANK);
#endif
    draw_return_confirm_impl();
#ifdef __CC65__
    sysSetBank(bank);
#endif
}
static void status_line(void) {
#ifdef __CC65__
    u8 bank = sysGetBank();
#ifdef PREVIEW
    sysSetBank(18);
#else
    sysSetBank(11);
#endif
    status_line_impl();
    sysSetBank(bank);
#else
    status_line_impl();
#endif
}
static void stop_for_card_fault(void) {
    if(!card_io_fault())return;
    draw_card_restart();for(;;)gVsync();
}
static void error_screen(const char *message, u8 code) {
    stop_for_card_fault();
    gCleanScreen(); heading("TEOPCE 0.9.13 / CARD ERROR");
    gSetPal(PAL_ER); say(2, 6, message, 35);
    gSetPal(PAL_01); say(2, 9, "Error code:", 20);
    gSetXY(15, 9); gAppendHex8(code);
    say(2, 13, "Check the SD card and menu files.", 36);
    say(2, 15, "Nothing has been launched.", 36);
    footer("I Retry", "Reset the console to leave this screen");
    while (!(sysJoyWait() & JOY_B)) {}
}
static u8 save_preferences(void) {
    if(card_io_fault()){save_error=CARD_IO_BLOCKED;return save_error;}
    /* An unread slot may contain a newer sequence. Never overwrite it with defaults. */
    if (prefs_load_error) { save_error = prefs_load_error; return save_error; }
#ifdef PREVIEW
    save_error = 0;
#else
    save_error = storage_save(&prefs);
#endif
    return save_error;
}
static u8 load_current(void) {
    u8 result;
    if (!cursor.count) { memset(&current, 0, sizeof(current)); current_ready = 0; return 0; }
    result = catalog_record(matches[cursor.selected], &current);
    if (result) { current_ready = 0; return result; }
    if (pack_key(current.reserved) != page_keys[cursor.selected - cursor.top]) {
        current_ready = 0; return CATALOG_ERR_FORMAT;
    }
    current_ready = 1;
    saved_codes=cheat_store_saved_status(matches[cursor.selected],&current,text_buffer);
    return 0;
}
static u8 refresh_page(void) {
    u8 result;
    result = catalog_page(matches, cursor.count, cursor.top, page_size(),
                          titles, page_keys, &page_loaded);
    art_loaded = 0; current_ready = 0; pending_detail = 1; settle_frames = 0;
    browse_title_offset = 0; browse_title_wait = 50;
    return result;
}
static void blank_picture(u8 x, u8 y, const char *label) {
    gSetPal(PAL_00); gFillRect(' ', x, y, 12, 9);
    say(x + 1, y + 3, label, 10);
}
static u8 read_art(u16 index, u8 which, u8 slot, u8 x, u8 y) {
    if(card_io_fault())return CARD_IO_BLOCKED;
#ifdef PREVIEW
    return preview_picture(index, which, slot, x, y);
#else
    return picture_draw(index, which, slot, x, y);
#endif
}
/* Return the picture actually shown: 0 box art, 1 game screen, 2 missing/error. */
static u8 draw_art(u16 index, u8 which, u8 slot, u8 x, u8 y) {
    u8 result = read_art(index, which, slot, x, y);
    if ((result == 4 || result == 5) && which == 0) {
        which = 1;
        result = read_art(index, which, slot, x, y);
    }
    if (result) {
        blank_picture(x, y, result == 4 || result == 5 ? "NO IMAGE" : "ART ERROR");
        return 2;
    }
    return which;
}
static void draw_row(u8 row) {
    u16 position = cursor.top + row;
    if (row >= page_loaded) return;
    gSetPal(position == cursor.selected ? PAL_OK : PAL_01);
    gFillRow(' ', 1, 5 + row, 24);
    gSetXY(1, 5 + row);
    gAppendChar(position == cursor.selected ? '>' : ' ');
    gAppendChar(prefs_has(&prefs, page_keys[row]) ? '*' : ' ');
    say(3, 5 + row, position == cursor.selected && current_ready ? current.title + browse_title_offset : titles[row], LIST_TITLE_WIDTH);
}
static void draw_grid_caption(u8 slot) {
    u8 x = 1 + (slot % GRID_COLS) * 13;
    u8 y = slot < GRID_COLS ? 14 : 24;
    u8 selected = cursor.top + slot == cursor.selected;
    const char *title;
    if (slot >= page_loaded) return;
    if (!(notice_frames && notice && slot >= GRID_COLS)) {
        gSetPal(selected ? PAL_OK : PAL_01);
        gFillRow(' ', x, y, 12);
        title = selected && current_ready ? current.title : titles[slot];
        if (selected && current_ready) say(x, y, title + browse_title_offset, GRID_CAPTION_WIDTH);
        else say(x, y, title, GRID_CAPTION_WIDTH);
        if (prefs_has(&prefs, page_keys[slot])) {
            gSetXY(x + GRID_CAPTION_WIDTH, y); gAppendChar('*');
        }
    }
    gSetPal(selected ? PAL_OK : PAL_00);
    gFillCol('|', x - 1, slot < GRID_COLS ? 5 : 15, 9);
    gFillCol('|', x + 12, slot < GRID_COLS ? 5 : 15, 9);
}

#ifdef __CC65__
#ifdef PREVIEW
#pragma code-name(push,"BNK14")
#pragma rodata-name(push,"BNK14")
#else
#pragma code-name(push,"BNK07")
#pragma rodata-name(push,"BNK07")
#endif
#endif
/* Only the selected full record is needed; animation never reads SD or writes history. */
static void tick_browse_title_impl(void) {
    u8 length, width = prefs.view ? GRID_CAPTION_WIDTH : LIST_TITLE_WIDTH;
    if (!current_ready || (prefs.view && notice_frames && notice && cursor.selected - cursor.top >= GRID_COLS)) return;
    length = (u8)strlen(current.title);
    if (length <= width) return;
    if (browse_title_wait) { --browse_title_wait; return; }
    if (browse_title_offset < length - width) {
        ++browse_title_offset;
        browse_title_wait = browse_title_offset == length - width ? 50 : 5;
    } else { browse_title_offset = 0; browse_title_wait = 50; }
    if (prefs.view) draw_grid_caption((u8)(cursor.selected - cursor.top));
    else draw_row((u8)(cursor.selected - cursor.top));
}

static void draw_browse_title_impl(void) {
    library_browse_context(current_ready?&current:0,&filter,&prefs);
    gSetPal(PAL_01);
    if(cursor.count&&!current_ready)say_wrapped(1,2,titles[cursor.selected-cursor.top],38,3);
    else if(!cursor.count)say(1,2,filter.favorites&&!prefs.count?"NO FAVORITES YET":filter.recent?(recent.read_error?"RECENT HISTORY READ ERROR":"NO RECENTLY OPENED GAMES"):"NO GAMES MATCH THESE FILTERS",38);
    draw_counter();
    if(save_error||recent.read_error||recent.write_error){gSetPal(PAL_ER);say(24,1,"SD!",3);}
    if(prefs.view&&notice_frames&&notice){clear_line(24);gSetPal(PAL_00);say(1,24,notice,38);}
}
#ifdef __CC65__
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
static void draw_browse_title(void){
#ifdef __CC65__
    u8 bank=sysGetBank();
#ifdef PREVIEW
    sysSetBank(18);
#else
    sysSetBank(11);
#endif
    draw_browse_title_impl();sysSetBank(bank);
#else
    draw_browse_title_impl();
#endif
}
static void tick_browse_title(void) {
#ifdef __CC65__
    u8 bank = sysGetBank(); sysSetBank(MAIN_RENDER_BANK);
#endif
    tick_browse_title_impl();
#ifdef __CC65__
    sysSetBank(bank);
#endif
}
static void draw_browse(void) {
    u8 row, x, y;
    gCleanScreen();
    heading(prefs.view ? "TEO GRID" : "TEO LIST");
    draw_browse_title();
    gSetPal(PAL_00);
    if (!cursor.count) {
        gSetPal(PAL_01);
        if (filter.favorites && !prefs.count) {
            say(3, 9, "Your favorites list is empty.", 36);
            say(3, 11, "II Clear, I Details, then SEL Menu", 36);
            say(3, 13, "Choose ADD TO FAVORITES.", 36);
        } else if(filter.recent){
            if(recent.read_error){
                say(3,9,"Saved history could not be read.",36);
                say(3,11,"New openings are session-only.",36);
                say(3,13,"Restart to read saved history.",36);
            }else{
                say(3,9,"No recently opened games match.",36);
                say(3,11,"II All games, I Details or RUN Play.",36);
            }
        } else {
            say(3, 9, "Try clearing a filter.", 34);
            say(3, 11, "SELECT opens filters and settings.", 34);
        }
    } else if (prefs.view) {
        for (row = 0; row < page_loaded; ++row) {
            x = 1 + (row % GRID_COLS) * 13; y = row < GRID_COLS ? 5 : 15;
            blank_picture(x, y, "LOADING"); draw_grid_caption(row);
        }
        /* Draw the active caption last, preserving the selected frame. */
        draw_grid_caption((u8)(cursor.selected - cursor.top));
    } else {
        for (row = 0; row < page_loaded; ++row) draw_row(row);
        gSetPal(PAL_00); gFillCol('|', 25, 5, 17);
        blank_picture(27, 5, "LOADING");
        status_line();
    }
    footer("",cursor.count?(filters_active()?"I Details  II All  SEL Find  RUN Play":"I Details  II Back  SEL Find  RUN Play"):(filters_active()?"II Clear  SELECT Find / filters":"II Back  SELECT Find / filters"));
    if(!cursor.count&&save_error){gSetPal(PAL_ER);say(1,17,"PREFS NOT SAVED / THIS SESSION ONLY",38);}
    draw_browse_title();
}
static void draw_release_year(u16 index, u8 x, u8 y, u8 width) {
    u16 year = 0;
    u8 result = game_meta_year(index, pack_key(current.reserved), &year);
    if (result) {
        gSetPal(PAL_ER); say(x, y, "YEAR ERROR", width);
    } else if (!year) {
        gSetPal(PAL_01); gFillRow(' ', x, y, width);
    } else {
        gSetPal(PAL_01); say(x, y, "YEAR ", width);
        gSetXY(x + 5, y); gAppendNum(year);
    }
}
static void draw_side(void) {
    u8 row;
    if (!current_ready) return;
    draw_browse_title();
    if (prefs.view) { draw_grid_caption((u8)(cursor.selected - cursor.top)); return; }
    draw_row((u8)(cursor.selected - cursor.top));
    for (row = 15; row <= 21; ++row) {
        gSetPal(PAL_01); gFillRow(' ', 27, row, 12);
    }
    gSetPal(PAL_ER); say(27, 15, media[current.kind], 12);
    gSetPal(PAL_00); say(27, 17, current.region, 12);
    gSetPal(PAL_01);
    say(27, 19, current.genre, 12);
    draw_release_year(matches[cursor.selected], 27, 21, 12);
    if(saved_codes==CHEAT_SAVED_ON){gSetPal(PAL_00);say(27,16,"CODES SET",12);}
    status_line();
}
static u8 update_browse_selection(u16 previous, u16 previous_top) {
    u8 result;
    if (previous_top != cursor.top) {
        result = refresh_page(); if (result) return result;
        draw_browse(); return 0;
    }
    current_ready = 0; browse_title_offset = 0; browse_title_wait = 50;
    if (prefs.view) {
        draw_grid_caption((u8)(previous - cursor.top));
        draw_grid_caption((u8)(cursor.selected - cursor.top));
    } else {
        draw_row((u8)(previous - cursor.top));
        draw_row((u8)(cursor.selected - cursor.top));
        blank_picture(27, 5, "LOADING");
        gSetPal(PAL_01); gFillRect(' ', 27, 14, 12, 8);
    }
    current_ready = 0; draw_browse_title(); pending_detail = 1; settle_frames = 0;
    return 0;
}
static u8 change_selection(short delta) {
    u16 previous = cursor.selected, previous_top = cursor.top;
    if (!cursor_move(&cursor, delta, page_size())) return 0;
    return update_browse_selection(previous, previous_top);
}
static void toggle_favorite(void) {
    u32 key;
    u8 had;
    if (!cursor.count) return;
    key = page_keys[cursor.selected - cursor.top];
    had = prefs_has(&prefs, key);
    if (prefs_toggle(&prefs, key)) { announce("FAVORITES FULL (64)"); return; }
    favorites_changed = 1;
    if (save_preferences())
        announce(had ? "FAVORITE REMOVED / SESSION ONLY" : "FAVORITE ADDED / SESSION ONLY");
    else announce(had ? "REMOVED FROM FAVORITES" : "ADDED TO FAVORITES");
}
static u8 apply_filter(void) {
    u16 count = 0, previous_id = 0xffff, previous_position = cursor.selected, i;
    u8 result, found = 0;
    if(screen==SCREEN_FILTER)previous_id=query_anchor;
    else if (cursor.count) previous_id = matches[cursor.selected];
    else previous_id=query_anchor;
    if(previous_id!=0xffff)query_anchor=previous_id;
    result = find_filter(&filter, &prefs, &recent, matches, &count,text_buffer);
    if (result) return result;
    cursor.count = count; cursor.selected = 0;
    for (i = 0; i < count; ++i) {
        if (matches[i] != previous_id) continue;
        cursor.selected = i; found = 1; break;
    }
    if (!found && favorites_changed && filter.favorites && count)
        cursor.selected = previous_position < count ? previous_position : count - 1;
    cursor.top = (cursor.selected / page_size()) * page_size();
    return refresh_page();
}
/* Resolve history to a visible CoreGrafx entry, then open the unfiltered library. */
static u8 apply_startup_resume(void) {
    u16 candidates = 0;
    u8 result;
    memset(&cursor, 0, sizeof(cursor));
    memset(&filter, 0, sizeof(filter));
    query_anchor = 0xffff;
    if (recent.count && !recent.read_error) {
        filter.recent = 1;
        result = find_filter(&filter, &prefs, &recent, matches, &candidates, text_buffer);
        memset(&filter, 0, sizeof(filter));
        if (result) return result;
        if (candidates) query_anchor = matches[0];
    }
    return apply_filter();
}
static void load_description(void) {
    description_open(&description,matches[cursor.selected],text_buffer);
}
static void restore_description(void) {
    description_read(&description,text_buffer);
}
#ifdef __CC65__
#ifdef PREVIEW
#define MAIN_DESCRIPTION_BANK 16
#pragma code-name(push,"BNK12")
#pragma rodata-name(push,"BNK12")
#else
#define MAIN_DESCRIPTION_BANK 10
#pragma code-name(push,"BNK06")
#pragma rodata-name(push,"BNK06")
#endif
#endif
static const char desc_unavailable[]="No description available.";
static const char desc_empty[]="Description file is empty.";
static const char desc_failed[]="Description could not be read.";
static const char desc_read_error[]="Read error:";
static const char desc_position[]="TEXT ";
static void draw_description_impl(void) {
    u16 offset=0,end;u8 row,col,c,label;
    for(row=18;row<=24;++row)clear_line(row);
    gSetPal(PAL_01);
    if(description.status!=DESCRIPTION_READY){
        say(1,18,description.status==DESCRIPTION_MISSING?desc_unavailable:
            description.status==DESCRIPTION_EMPTY?desc_empty:desc_failed,38);
        if(description.status==DESCRIPTION_ERROR){say(1,20,desc_read_error,14);gSetXY(15,20);gAppendHex8(description.error);}
        return;
    }
    for(row=0;row<DESCRIPTION_ROWS;++row){
        label=!strncmp((char*)text_buffer+offset,"Developer:",10)||
              !strncmp((char*)text_buffer+offset,"Publisher:",10)?11:0;
        gSetPal(label?PAL_00:PAL_01);gSetXY(1,18+row);col=0;
        while(text_buffer[offset]&&text_buffer[offset]!='\n'&&col<DESCRIPTION_COLUMNS){
            if(label&&col==label)gSetPal(PAL_01);
            c=text_buffer[offset++];gAppendChar(c);++col;
        }
        if(text_buffer[offset]=='\n')++offset;
    }
    gSetPal(PAL_00);say(1,24,desc_position,5);gSetXY(6,24);gAppendNum(description.line+1);gAppendChar('-');
    end=description.line+DESCRIPTION_ROWS;if(end>description.lines)end=description.lines;
    gAppendNum(end);gAppendChar('/');gAppendNum(description.lines);
    if(end==description.lines){gAppendChar(' ');gAppendChar('E');gAppendChar('N');gAppendChar('D');}
}
#ifdef __CC65__
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
static void draw_description(void){
#ifdef __CC65__
 u8 bank=sysGetBank();sysSetBank(MAIN_DESCRIPTION_BANK);
#endif
 draw_description_impl();
#ifdef __CC65__
 sysSetBank(bank);
#endif
}

static void draw_detail(void) {
    u8 loaded;
    gCleanScreen(); heading("TEOPCE 0.9.13 / GAME DETAILS");
    gSetPal(PAL_01); say_wrapped(1, 3, current.title, 38, 3);
    blank_picture(2, 6, "LOADING");
    gSetPal(PAL_00); say(16, 6, "MEDIA", 22);
    gSetPal(PAL_ER); say(16, 7, media[current.kind], 22);
    gSetPal(PAL_00); say(16, 9, "REGION", 22);
    gSetPal(PAL_01); say(16, 10, current.region, 22);
    gSetPal(PAL_00); say(16, 12, "GENRE", 22);
    gSetPal(PAL_01); say(16, 13, current.genre, 22);
    draw_release_year(matches[cursor.selected], 16, 14, 22);
    gSetPal(PAL_ER); say(16, 15, prefs_has(&prefs, pack_key(current.reserved)) ? "* FAVORITE" : "SELECT: GAME MENU", 22);
    if (save_error) say(16, 16, prefs_load_error ? "PREFS READ ONLY" : "SD SAVE FAILED", 22);
    else {gSetPal(PAL_00);game_cheat_summary(matches[cursor.selected],pack_key(current.reserved),16,16);}
    gSetPal(PAL_00);say(1,17,notice_frames&&notice?notice:saved_codes==CHEAT_SAVED_ON?saved_on:saved_codes==CHEAT_SAVED_OFF?saved_off:saved_unknown,38);
    loaded = draw_art(matches[cursor.selected], picture_kind, 0, 2, 6);
    /* Keep the requested cover mode when the displayed image is a fallback. */
    if (picture_kind == 0 && loaded == 1) cover_missing = 1;
    else if (picture_kind == 0 && loaded == 0) cover_missing = 0;
    gSetPal(PAL_ER);
    say(2, 16, loaded == 1 ? "GAME SCREEN" : loaded == 0 ?
        (!strcmp(current.region, "English") ? "ORIGINAL ART" : "ARTWORK") : "NO PICTURE", 13);
    restore_description();draw_description();
    footer("I Picture  II Back  SELECT Game menu", "UP/DOWN Text  L/R Game  RUN Play");
}
static void draw_filters(void) {
    library_draw(&library,&filter,&prefs,&recent);
    if(library.mode==LIBRARY_OPTIONS&&save_error){gSetPal(PAL_ER);say(2,22,prefs_load_error?"SD! SETTINGS READ ONLY / RESTART":"SD! SETTINGS NOT SAVED / SESSION ONLY",36);}
}
static u8 cycle(u8 value, u8 count, u8 backward) {
    if (backward) return value ? value - 1 : count - 1;
    return value + 1 == count ? 0 : value + 1;
}
static void open_filters(void) {
    previous_filter = filter; previous_view = prefs.view; previous_theme = prefs.theme;
    query_anchor=cursor.count?matches[cursor.selected]:0xffff;
    library_open(&library,&filter);screen = SCREEN_FILTER; draw_filters();
}
/* Actions rendering shares the file-information bank; all inputs are RAM or
 * resident strings and nested bank wrappers restore this bank before returning. */
#ifdef __CC65__
#ifdef PREVIEW
#pragma code-name(push,"BNK13")
#else
#pragma code-name(push,"BNK04")
#endif
#endif
#ifdef __CC65__
#ifdef PREVIEW
#pragma rodata-name(push,"BNK13")
#else
#pragma rodata-name(push,"BNK04")
#endif
#endif
static const char action_labels[9][32]={
 "START GAME (RESTART REQUIRED)","START GAME","START GAME (UNSUPPORTED)",
 "REMOVE FROM FAVORITES","ADD TO FAVORITES","CHEATS","FILE INFORMATION",
 "RETURN (RESTART REQUIRED)","RETURN TO EVERDRIVE"
};
#ifdef __CC65__
#pragma rodata-name(pop)
#endif
static void draw_actions_impl(void) {
    u8 row;
    const char *label;
    gCleanScreen(); heading("TEOPCE 0.9.13 / GAME ACTIONS");
    gSetPal(PAL_01); say_wrapped(1, 3, current.title, 38, 3);
    for (row = 0; row < 5; ++row) {
        label = action_labels[row == 0 ? (native_blocked ? 0 :
                (current.kind == 1 || current.kind == 2) ? 1 : 2) :
                row == 1 ? (prefs_has(&prefs, pack_key(current.reserved)) ? 3 : 4) :
                row == 2 ? 5 : row == 3 ? 6 : native_blocked ? 7 : 8];
        gSetPal(row == action_row ? PAL_OK : PAL_01);
        gFillRow(' ', 2, 8 + row * 2, 36);
        if(row==action_row){gSetXY(2,8+row*2);gAppendChar('>');}
        if(row==2)game_cheat_summary(matches[cursor.selected],pack_key(current.reserved),3,12);
        else say(3, 8 + row * 2, label, 34);
    }
    gSetPal(PAL_00);say(2,19,saved_codes==CHEAT_SAVED_ON?saved_on:saved_codes==CHEAT_SAVED_OFF?saved_off:saved_unknown,36);
    say(2, 20, "EverDrive launches the selected game.", 36);
    say(2, 22, current.kind==2?"CD presets apply on TeoPCE launch.":"Codes are matched to the exact ROM.", 36);
    footer("I Choose  II Back", "UP/DOWN Action");
}
#ifdef __CC65__
#pragma code-name(pop)
#endif
static void draw_actions(void){
#ifdef __CC65__
 u8 bank=sysGetBank();
#ifdef PREVIEW
 sysSetBank(17);
#else
 sysSetBank(8);
#endif
 draw_actions_impl();sysSetBank(bank);
#else
 draw_actions_impl();
#endif
}
static u8 cheat_transition_blocked(void){
 if(!(cheat_store_flags()&(CHEAT_STORE_PENDING|CHEAT_STORE_UNCERTAIN)))return 0;
 screen=SCREEN_CHEAT_RECOVERY;cheat_recovery_draw();return 1;
}
static void draw_start_confirm(void){game_start_draw(&current,0);}
static void draw_start_loading(void){game_start_draw(&current,1);}
static void draw_start_error(void) {
    const char *message;
    gCleanScreen(); heading(native_blocked ? "TEOPCE 0.9.13 / RESTART REQUIRED" : "TEOPCE 0.9.13 / START ERROR");
    gSetPal(PAL_01); say_wrapped(1, 2, current.title, 38, 3);
    if (native_blocked) {
        gSetPal(PAL_ER); say(1, 8, launch_report.stage==LAUNCH_STAGE_CLOSE ? "SD FILE CLOSE WAS NOT VERIFIED" : "NATIVE RECOVERY WAS NOT VERIFIED", 38);
        gSetPal(PAL_01); say(1, 11, "Turn the console off, then on.", 38);
        say(1, 14, "Starting games and Return are blocked.", 38);
    } else {
        message = launch_error == CHEAT_STORE_ERR_HELD ? "SAVED CHEAT IS NOW HELD" :
            launch_error == CD_GUARD_ERR_SELECTION ? "UNSUPPORTED MIX / USE FEWER CODES" :
            launch_error == CD_GUARD_ERR_IMAGE ? "PROTECTION NEEDS MATCHING DISC" :
            launch_error == CD_GUARD_ERR_BIOS ? "PROTECTION NEEDS JAPAN SYSTEM 3.0" :
            launch_error == CD_GUARD_ERR_STATE ? "ENABLE EVERDRIVE CHEATS FIRST" :
            launch_error == NATIVE_BRIDGE_ERR_MENU ? "EVERDRIVE OS NOT SUPPORTED" :
            launch_error == LAUNCH_ERR_UNSUPPORTED ? (current.kind == 3 ?
            "SUPERGRAFX LAUNCH IS NOT SUPPORTED" : "THIS FILE TYPE CANNOT BE STARTED") :
            launch_error == LAUNCH_ERR_METADATA ? "Launch data could not be verified." :
            launch_error == LAUNCH_ERR_IDENTITY ? "The selected game does not match." :
            launch_error == LAUNCH_ERR_PREVIEW ? "PREVIEW: START IS CARTRIDGE ONLY" : "GAME COULD NOT BE STARTED";
        gSetPal(PAL_ER); say(1, 8, message, 38);
        gSetPal(PAL_01); say(1, 11, launch_error == CHEAT_STORE_ERR_HELD ? "Open Cheats / SEL All off / RUN Save" :
            launch_error == NATIVE_BRIDGE_ERR_MENU ? "UPDATE TEOPCE FOR THIS OS" :
            launch_error == LAUNCH_ERR_UNSUPPORTED ?
            "Use RETURN TO EVERDRIVE for this game." : "No retry is performed automatically.", 38);
        say(1, 14, "II returns to the Game menu.", 38);
    }
    gSetPal(PAL_00); say(1, 20, "Error:", 8); gSetXY(9, 20); gAppendHex8(launch_error);
    say(1, 21, "Stage:", 8); gSetXY(9, 21); gAppendHex8(launch_report.stage);
    footer(launch_report.stage==LAUNCH_STAGE_CLOSE ? "Restart console to continue" : "II Back", "");
}
static void decline_start(u8 code) {
    if (!native_blocked) { memset(&launch_report, 0, sizeof(launch_report)); launch_error = code; }
    screen = SCREEN_START_ERROR; draw_start_error();
}
static void record_opened(void){
    u32 key=pack_key(current.reserved);u8 result;
    if(card_io_fault())return;
    if(recent_rank(&recent,key)!=0)recent_changed=1;
    result=recent_opened(&recent,key,text_buffer);
    /* History verification reuses the same workspace as the description window. */
    if(screen==SCREEN_DETAIL&&!card_io_fault()){restore_description();draw_description();}
    if(result&&!card_io_fault()){
        announce("RECENT HISTORY / SESSION ONLY");gSetPal(PAL_ER);
        say(1,screen==SCREEN_DETAIL?17:23,notice,38);
    }
}
static void open_actions(u8 parent) {
    action_parent = parent; action_row = 0;
    screen = SCREEN_ACTIONS; draw_actions();record_opened();
}
/* Shared entry: fast Play and the menu route keep the same native gates. */
static void request_start(u8 parent) {
    start_parent = parent;
    if (parent != SCREEN_ACTIONS) { action_parent = parent; action_row = 0; }
    if (native_blocked) decline_start(launch_error);
    else if (cheat_transition_blocked()) return;
    else if (current.kind != 1 && current.kind != 2) decline_start(LAUNCH_ERR_UNSUPPORTED);
    else {
        screen = SCREEN_START; draw_start_confirm();
        if (parent != SCREEN_ACTIONS) record_opened();
    }
}
static void draw_info(void){game_info_draw(&current);}
static void draw_cheats(void){cheat_legacy_draw(&current,text_buffer,&cheat_page,&cheat_lines);}
static void open_cheats(void) {
    u8 result;
    if(cheat_transition_blocked())return;
    cheat_page=0;
    if(current.kind!=1&&current.kind!=2){group_mode=0;draw_cheats();return;}
    group_mode=1;
    gCleanScreen();heading("LOADING CHEAT GROUPS");
    result=cheat_draft_open(matches[cursor.selected],&current,text_buffer);
    if(result==4){group_mode=0;draw_cheats();}
    else cheat_draft_draw();
}
static u8 begin_detail(void) {
    u8 result = load_current();
    if (result) return result;
    picture_kind = 0; cover_missing = 0; load_description(); screen = SCREEN_DETAIL; draw_detail();record_opened();
    return 0;
}
static u8 return_to_browse(void) {
    u8 result;
    screen = SCREEN_LIST;
    result = (favorites_changed&&filter.favorites)||(recent_changed&&filter.recent)?apply_filter():refresh_page();
    favorites_changed = recent_changed = 0;
    if (!result) draw_browse();
    return result;
}
static u8 browser_loop(void) {
    u8 event, slot, x, y, art_status;
    u16 previous, previous_top;
    short delta;
    for (;;) {
        gVsync();
        if(card_io_fault()){
            if(screen!=SCREEN_CARD_RESTART){screen=SCREEN_CARD_RESTART;draw_card_restart();}
            continue;
        }
        event = input_poll(&input, sysJoyRead());
        if (notice_frames && --notice_frames == 0) {
            if (screen == SCREEN_LIST) { if (prefs.view){clear_line(24);for(slot=GRID_COLS;slot<page_loaded;++slot)draw_grid_caption(slot);if(cursor.count)draw_grid_caption((u8)(cursor.selected-cursor.top));draw_browse_title();}else status_line(); }
            else if (screen == SCREEN_DETAIL){clear_line(17);gSetPal(PAL_00);say(1,17,saved_codes==CHEAT_SAVED_ON?saved_on:saved_codes==CHEAT_SAVED_OFF?saved_off:saved_unknown,38);}
        }
        if (screen == SCREEN_LIST) {
            if (event & JOY_SEL) { open_filters(); continue; }
            if ((event & JOY_STA) && cursor.count) {
                error_code = load_current(); if (error_code) return error_code;
                request_start(SCREEN_LIST); continue;
            }
            if ((event & JOY_B) && cursor.count) {
                error_code = begin_detail(); if (error_code) return error_code;
                continue;
            }
            if (event & JOY_A) {
                if (filters_active()) {
                    memset(&filter, 0, sizeof(filter));
                    error_code = apply_filter(); if (error_code) return error_code;
                    announce("BACK TO ALL GAMES"); draw_browse();
                } else {
                    announce("ALL GAMES / LIBRARY ROOT");
                    if (!prefs.view) status_line();
                }
                continue;
            }
            delta = 0;
            if (event & JOY_U) delta = prefs.view ? -GRID_COLS : -1;
            else if (event & JOY_D) delta = prefs.view ? GRID_COLS : 1;
            else if (event & JOY_L) delta = prefs.view ? -1 : -LIST_ROWS;
            else if (event & JOY_R) delta = prefs.view ? 1 : LIST_ROWS;
            if (delta) { error_code = change_selection(delta); if (error_code) return error_code; }
            if (pending_detail && cursor.count) {
                if (delta) settle_frames = 0;
                else if (++settle_frames >= 6) {
                    error_code = load_current(); if (error_code) return error_code;
                    draw_side();
                    if (!prefs.view) {
                        art_status = draw_art(matches[cursor.selected], 0, 0, 27, 5);
                        gSetPal(PAL_00); gFillRow(' ', 27, 14, 12);
                        say(27, 14, art_status == 1 ? "GAME SCREEN" : art_status == 0 ? "ARTWORK" : "NO PICTURE", 12);
                    }
                    pending_detail = 0;
                }
            }
            if (prefs.view && cursor.count) {
                for (slot = 0; slot < page_loaded; ++slot) {
                    if (art_loaded & (1 << slot)) continue;
                    x = 1 + (slot % GRID_COLS) * 13; y = slot < GRID_COLS ? 5 : 15;
                    draw_art(matches[cursor.top + slot], 0, slot, x, y);
                    art_loaded |= (1 << slot);
                    break;
                }
            }
            tick_browse_title();
        } else if (screen == SCREEN_FILTER) {
            if(!event)continue;
            error_code=library_event(&library,&filter,&prefs,event);
            if(error_code==LIBRARY_CANCEL){
                filter=previous_filter;prefs.view=previous_view;prefs.theme=previous_theme;apply_theme();
                if(library.mode==LIBRARY_ERROR)error_code=apply_filter();
                else{cursor_move(&cursor,0,page_size());error_code=refresh_page();}
                if(error_code)return error_code;screen=SCREEN_LIST;draw_browse();
            }else if(error_code==LIBRARY_APPLY){
                gCleanScreen();heading("FINDING GAMES...");error_code=apply_filter();
                if(error_code){library_error(&library,error_code);draw_filters();continue;}
                if(prefs.view!=previous_view||prefs.theme!=previous_theme)save_preferences();
                screen=SCREEN_LIST;draw_browse();
            }else{apply_theme();draw_filters();}
        } else if (screen == SCREEN_DETAIL) {
            if (event & JOY_A) {
                error_code = return_to_browse(); if (error_code) return error_code;
            }
            else if (event & JOY_B) {
                if (cover_missing) announce("NO BOX ART / SHOWING GAME SCREEN");
                else picture_kind ^= 1;
                draw_detail();
            }
            else if (event & JOY_SEL) {
                open_actions(SCREEN_DETAIL);
            } else if (event & JOY_STA) { request_start(SCREEN_DETAIL); }
            else if (event & (JOY_U|JOY_D)) {
                description_move(&description,(event&JOY_U)!=0,text_buffer);draw_description();
            } else if ((event & (JOY_L | JOY_R)) && cursor.count) {
                previous = cursor.selected; previous_top = cursor.top;
                cursor_move(&cursor, event & JOY_L ? -1 : 1, page_size());
                if (previous != cursor.selected) {
                    if (previous_top != cursor.top) { error_code = refresh_page(); if (error_code) return error_code; }
                    error_code = load_current(); if (error_code) return error_code;
                    cover_missing = 0; load_description(); draw_detail();record_opened();
                }
            }
        } else if (screen == SCREEN_ACTIONS) {
            if (event & JOY_A) {
                if (action_parent == SCREEN_LIST) {
                    error_code = return_to_browse(); if (error_code) return error_code;
                } else { screen = SCREEN_DETAIL; draw_detail(); }
            } else if (event & JOY_U) { action_row = cycle(action_row, 5, 1); draw_actions(); }
            else if (event & JOY_D) { action_row = cycle(action_row, 5, 0); draw_actions(); }
            else if (event & JOY_B) {
                if (action_row == 0) {
                    request_start(SCREEN_ACTIONS);
                } else if (action_row == 1) {
                    toggle_favorite();
                    if (action_parent == SCREEN_LIST) {
                        error_code = return_to_browse(); if (error_code) return error_code;
                    } else { screen = SCREEN_DETAIL; draw_detail(); }
                } else if (action_row == 2) {
                    cheat_parent = SCREEN_ACTIONS; screen = SCREEN_CHEATS;
                    open_cheats();
                } else if (action_row == 3) { screen = SCREEN_INFO; draw_info(); }
                else if (native_blocked) decline_start(launch_error);
                else if(cheat_transition_blocked())continue;
                else { screen = SCREEN_RETURN; draw_return_confirm(); }
            }
        } else if (screen == SCREEN_START) {
            if (event & JOY_A) {
                if (start_parent == SCREEN_LIST) {
                    error_code = return_to_browse(); if (error_code) return error_code;
                } else if (start_parent == SCREEN_DETAIL) { screen = SCREEN_DETAIL; draw_detail(); }
                else { screen = SCREEN_ACTIONS; draw_actions(); }
            }
            else if (event & JOY_B) {
                if (native_blocked) decline_start(launch_error);
                else if(cheat_transition_blocked())continue;
                else {
                    memset(&launch_report, 0, sizeof(launch_report));
                    draw_start_loading();
                    launch_error = cheat_store_prepare_launch(matches[cursor.selected], &current, text_buffer);
                    if(launch_error){if(cheat_transition_blocked())continue;decline_start(launch_error);continue;}
                    launch_error = launch_selected(matches[cursor.selected], &current, &launch_report);
                    if (!launch_error) launch_error = NATIVE_BRIDGE_ERR_START;
                    if(launch_report.stage==LAUNCH_STAGE_CLOSE)native_blocked=1;
                    if ((launch_report.rollback_flags & NATIVE_BRIDGE_ROLLBACK_ATTEMPTED) &&
                            launch_report.rollback_flags != 7 && launch_report.rollback_flags != 31) native_blocked = 1;
                    screen = SCREEN_START_ERROR; draw_start_error();
                }
            }
        } else if (screen == SCREEN_START_ERROR) {
            if ((event & JOY_A) && launch_report.stage!=LAUNCH_STAGE_CLOSE) { screen = SCREEN_ACTIONS; draw_actions(); }
        } else if (screen == SCREEN_RETURN) {
            if (event & JOY_A) {
                screen = SCREEN_ACTIONS; draw_actions();
            } else if (event & JOY_B) {
                if (native_blocked) { decline_start(launch_error); continue; }
                if(cheat_transition_blocked())continue;
                error_code = device_return_to_menu();
                screen = SCREEN_ACTIONS; draw_actions();
                gSetPal(error_code ? PAL_ER : PAL_00);
                say(2, 23, error_code ? "RETURN FAILED / STILL IN TEOPCE" :
                        "PREVIEW: RETURN IS DISABLED", 36);
            }
        } else if(screen==SCREEN_CHEAT_RECOVERY){
            if(cheat_recovery_event(event,text_buffer)){screen=SCREEN_ACTIONS;draw_actions();}
        } else if (screen == SCREEN_INFO) {
            if (event & JOY_A) {
                screen = SCREEN_ACTIONS; draw_actions();
            }
            else if (event & JOY_B) {
                cheat_parent = SCREEN_INFO; screen = SCREEN_CHEATS; open_cheats();
            }
        } else {
            if (group_mode) {
                error_code=cheat_draft_event(event);
                if(cheat_transition_blocked())continue;
                if(error_code) {
                    if(error_code>=2)saved_codes=error_code==3?CHEAT_SAVED_OFF:CHEAT_SAVED_ON;
                    if(cheat_parent==SCREEN_ACTIONS){screen=SCREEN_ACTIONS;draw_actions();}
                    else {screen=SCREEN_INFO;draw_info();}
                    if(error_code>=2){gSetPal(PAL_OK);say(1,23,error_code==3?"ALL CHEATS OFF / SAVED":"CHEAT SELECTION SAVED",38);}
                }
            } else if (event & JOY_A) {
                if (cheat_parent == SCREEN_ACTIONS) { screen = SCREEN_ACTIONS; draw_actions(); }
                else { screen = SCREEN_INFO; draw_info(); }
            }
            else if ((event & JOY_R) && (u32)(cheat_page + 1) * 17 < cheat_lines) { ++cheat_page; draw_cheats(); }
            else if ((event & JOY_L) && cheat_page) { --cheat_page; draw_cheats(); }
        }
    }
}
void main(void) {
    u8 result;
#ifndef PREVIEW
    u8 status[4];
#endif
    sysInit(); card_io_boot(); apply_theme();query_anchor=0xffff;
    for (;;) {
        stop_for_card_fault();
#ifndef PREVIEW
        ed_init(); ed_cmd_status(status);
        trace_boot(1, 0, 0, status);
        result = device_check(status);
        if (result) {
            device_error_screen(status, result);
            continue;
        }
#endif
        result = catalog_init(&total);
        trace_boot(2, result, result ? 0 : total, 0);
        if (result) { error_screen("Catalog / index could not be opened", result); continue; }
        result=cheat_store_init(text_buffer);cheat_recovery_reset();
        stop_for_card_fault();
        if(cheat_transition_blocked()){
            memset(&input,0,sizeof(input));
            for(;;){gVsync();stop_for_card_fault();if(cheat_recovery_event(input_poll(&input,sysJoyRead()),text_buffer))break;}
        }

#ifdef PREVIEW
        memset(&prefs, 0, sizeof(prefs)); save_error = 0;
#else
        save_error = storage_load(&prefs);
#endif
        trace_boot(3, save_error, 0, 0);
        prefs_load_error = save_error;
        recent_load(&recent,text_buffer);stop_for_card_fault();recent_changed=0;
        apply_theme();
        memset(&input, 0, sizeof(input));
        screen = SCREEN_LIST;
        result = apply_startup_resume();
        trace_boot(4, result, result ? 0 : cursor.count, 0);
        if (result) { error_screen("Catalog filter could not be read", result); continue; }
        draw_browse();
        trace_boot(5, 0, cursor.count, 0);
        result = browser_loop();
        trace_boot(0xff, result, 0, 0);
        error_screen("SD data could not be read", result);
    }
}
