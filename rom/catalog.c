#include "card_io.h"
#include "catalog.h"
#include <string.h>
#ifdef PREVIEW
#include "preview.h"
#endif

static u16 total;
static u32 generation;
static u8 initialized;
/* Shared by serial catalog operations; never retained between API calls. */
static union {
    u8 bytes[256];
    CatalogRecord record;
} workspace;
/* Expected metadata survives reuse of workspace by the serial index read. */
static u8 expected_index[CATALOG_PAGE_ROWS][8];

static u8 field_valid(const char *field, u8 width) {
    u8 i;
    if (!field[0]) return 0;
    for (i = 0; i < width; ++i) if (!field[i]) return 1;
    return 0;
}

static void record_index(const CatalogRecord *record, u8 row[8]) {
    row[0] = record->kind;
    row[1] = record->reserved[4];
    row[2] = record->reserved[6];
    row[3] = record->reserved[5];
    memcpy(row + 4, record->reserved, 4);
}

static u8 record_valid(const CatalogRecord *record) {
    u8 row[8];
    if (!field_valid(record->title, 88) || !field_valid(record->path, 128) ||
        !field_valid(record->genre, 20) || !field_valid(record->region, 12)) return 0;
    record_index(record, row);
    return pack_match(row, 0, 0, 0, 0) == 1;
}

#ifndef PREVIEW
/* Success leaves the one SDK file handle open; failures always close it. */
static u8 open_header(u8 index, u8 expect_generation, u16 *count, u32 *signature) {
    u8 error;
    u32 size;
    if (card_io_fault()) return CARD_IO_BLOCKED;
    error = ed_cmd_file_open((u8 *)(index ? TEO_METADATA_ROOT "/index.bin" : TEO_METADATA_ROOT "/catalog2.bin"), FA_READ);
    if (error) return error;
    size = ed_cmd_file_available();
    error = size < 16 ? CATALOG_ERR_FORMAT : ed_cmd_file_read(workspace.bytes, 16);
    if (!error && pack_header(workspace.bytes, size, index, count, signature))
        error = CATALOG_ERR_FORMAT;
    if (!error && expect_generation && (*count != total || *signature != generation))
        error = CATALOG_ERR_FORMAT;
    if (!error) return 0;
    card_io_close(ed_cmd_file_close);
    return error;
}
#endif

/* One index handle per operation; adjacent selected rows share one read. */
static u8 verify_index(const u16 *matches, u16 start, u8 amount) {
    u8 error = 0, row = 0, run, i;
    u16 index;
#ifndef PREVIEW
    u8 close_error;
    u16 entries, next = 0;
    u32 signature;
    error = open_header(1, 1, &entries, &signature);
    if (error) return error;
#endif
    while (row < amount) {
        index = matches[start + row];
        run = 1;
#ifdef PREVIEW
        error = preview_index(index, workspace.bytes);
#else
        while (row + run < amount && matches[start + row + run] == index + run) ++run;
        if (index != next) error = ed_cmd_file_set_ptr(16UL + (u32)index * 8);
        if (!error) error = ed_cmd_file_read(workspace.bytes, (u16)run * 8);
#endif
        if (error) break;
        for (i = 0; i < run; ++i) {
            if (!memcmp(workspace.bytes + (u16)i * 8, expected_index[row + i], 8)) continue;
            error = CATALOG_ERR_FORMAT; break;
        }
        if (error) break;
        row += run;
#ifndef PREVIEW
        next = index + run;
#endif
    }
#ifndef PREVIEW
    close_error = card_io_close(ed_cmd_file_close);
    if (!error) error = close_error;
#endif
    return error;
}

u8 catalog_init(u16 *count) {
    u16 entries;
#ifndef PREVIEW
    u16 index_entries;
    u32 signature, index_signature;
    u8 error;
#endif
    total = 0;
    generation = 0;
    initialized = 0;
    if (!count) return CATALOG_ERR_INPUT;
    *count = 0;
    if (card_io_fault()) return CARD_IO_BLOCKED;
#ifdef PREVIEW
    entries = preview_count();
    if (entries > CATALOG_MAX) return CATALOG_ERR_FORMAT;
#else
    error = open_header(0, 0, &entries, &signature);
    if (error) return error;
    error = card_io_close(ed_cmd_file_close);
    if (error) return error;
    error = open_header(1, 0, &index_entries, &index_signature);
    if (error) return error;
    error = card_io_close(ed_cmd_file_close);
    if (error) return error;
    if (entries != index_entries || signature != index_signature)
        return CATALOG_ERR_FORMAT;
    generation = signature;
#endif
    total = entries;
    initialized = 1;
    *count = entries;
    return 0;
}

u8 catalog_identity(u16 *count, u32 *signature) {
    if (count) *count = 0;
    if (signature) *signature = 0;
    if (!count || !signature || !initialized) return CATALOG_ERR_INPUT;
    *count = total;
    *signature = generation;
    return 0;
}

static u8 filter_valid(const CatalogFilter *filter, const Preferences *prefs) {
    if (!filter || filter->medium > 2 || filter->genre >= GENRE_COUNT || filter->region > 4 ||
        filter->favorites > 1 ||
        (filter->letter && filter->letter != '#' &&
         (filter->letter < 'A' || filter->letter > 'Z'))) return 0;
    if (filter->favorites && (!prefs || prefs->count > FAVORITES_MAX)) return 0;
    return 1;
}

u8 catalog_filter(const CatalogFilter *filter, const Preferences *prefs,
                  u16 *matches, u16 *matched) {
    u16 index, found;
    u8 error, amount, i, match;
    u8 *row;
#ifndef PREVIEW
    u8 close_error;
    u16 entries, available;
    u32 signature;
#endif
    if (!matched) return CATALOG_ERR_INPUT;
    *matched = 0;
    if (card_io_fault()) return CARD_IO_BLOCKED;
    if (!initialized || (!matches && total) || !filter_valid(filter, prefs))
        return CATALOG_ERR_INPUT;
#ifndef PREVIEW
    error = open_header(1, 1, &entries, &signature);
    if (error) return error;
#endif
    index = found = 0;
    error = 0;
    while (index < total) {
#ifdef PREVIEW
        amount = 1;
        error = preview_index(index, workspace.bytes);
#else
        available = total - index;
        amount = available > 32 ? 32 : (u8)available;
        error = ed_cmd_file_read(workspace.bytes, (u16)amount * 8);
#endif
        if (error) break;
        for (i = 0; i < amount; ++i) {
            row = workspace.bytes + (u16)i * 8;
            match = pack_match(row, filter->medium, filter->genre, filter->letter, filter->region);
            if (match == 2) { error = CATALOG_ERR_FORMAT; break; }
            /* Hide preserved SGX records without renumbering asset or favorite IDs. */
            if (match && row[0] != 3 && (!filter->favorites || prefs_has(prefs, pack_key(row + 4))))
                matches[found++] = index + i;
        }
        if (error) break;
        index += amount;
    }
#ifndef PREVIEW
    close_error = card_io_close(ed_cmd_file_close);
    if (!error) error = close_error;
#endif
    if (!error) *matched = found;
    return error;
}

/* The caller owns the open catalog handle in a cartridge build. */
static u8 read_record(u16 index, CatalogRecord *record) {
    u8 error;
#ifdef PREVIEW
    error = preview_record(index, record);
#else
    error = ed_cmd_file_set_ptr(16UL + (u32)index * 256);
    if (!error) error = ed_cmd_file_read(record, 256);
#endif
    if (!error && !record_valid(record)) error = CATALOG_ERR_FORMAT;
    return error;
}

u8 catalog_record(u16 index, CatalogRecord *record) {
    u8 error;
#ifndef PREVIEW
    u8 close_error;
    u16 entries;
    u32 signature;
#endif
    if (!record) return CATALOG_ERR_INPUT;
    memset(record, 0, sizeof(*record));
    if (card_io_fault()) return CARD_IO_BLOCKED;
    if (!initialized) return CATALOG_ERR_INPUT;
    if (index >= total) return CATALOG_ERR_RANGE;
#ifndef PREVIEW
    error = open_header(0, 1, &entries, &signature);
    if (error) return error;
#endif
    error = read_record(index, record);
#ifndef PREVIEW
    close_error = card_io_close(ed_cmd_file_close);
    if (!error) error = close_error;
#endif
    if (!error) {
        record_index(record, expected_index[0]);
        error = verify_index(&index, 0, 1);
    }
    if (error) memset(record, 0, sizeof(*record));
    return error;
}

static void clear_page(char titles[12][24], u32 keys[12]) {
    if (titles) memset(titles, 0, CATALOG_PAGE_ROWS * CATALOG_TITLE_WIDTH);
    if (keys) memset(keys, 0, CATALOG_PAGE_ROWS * sizeof(u32));
}

u8 catalog_page(const u16 *matches, u16 matched, u16 start, u8 rows,
                char titles[12][24], u32 keys[12], u8 *loaded) {
    CatalogRecord *record = &workspace.record;
    u16 i;
    u8 amount, done, column, error;
#ifndef PREVIEW
    u8 close_error;
    u16 entries;
    u32 signature;
#endif
    clear_page(titles, keys);
    if (!loaded) return CATALOG_ERR_INPUT;
    *loaded = 0;
    if (card_io_fault()) return CARD_IO_BLOCKED;
    if (!initialized || !titles || !keys || (!matches && matched) ||
        !rows || rows > CATALOG_PAGE_ROWS || matched > total || start > matched)
        return CATALOG_ERR_INPUT;
    for (i = 0; i < matched; ++i)
        if (matches[i] >= total) return CATALOG_ERR_RANGE;
    if (start == matched) return 0;
    amount = matched - start > rows ? rows : (u8)(matched - start);
#ifndef PREVIEW
    error = open_header(0, 1, &entries, &signature);
    if (error) return error;
#endif
    error = 0;
    for (done = 0; done < amount; ++done) {
        error = read_record(matches[start + done], record);
        if (error) break;
        record_index(record, expected_index[done]);
        for (column = 0; column < CATALOG_TITLE_WIDTH - 1 && record->title[column]; ++column)
            titles[done][column] = record->title[column];
        keys[done] = pack_key(record->reserved);
    }
#ifndef PREVIEW
    close_error = card_io_close(ed_cmd_file_close);
    if (!error) error = close_error;
#endif
    if (!error) error = verify_index(matches, start, amount);
    if (error) clear_page(titles, keys);
    else *loaded = amount;
    return error;
}
