/* Read generated files through the production C readers and launch preflight. */
#include "catalog.h"
#include "find.h"
#include "game_meta.h"
#include "cheat_index.h"
#include "launch.h"
#include "card_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *root;
static FILE *opened;
static unsigned long length;
static unsigned bridge_calls;

u8 ed_cmd_file_open(u8 *path, u8 mode) {
    char filename[2048];
    if(opened || mode != FA_READ) abort();
    if(snprintf(filename, sizeof(filename), "%s/%s", root, (char *)path) >= sizeof(filename)) abort();
    opened = fopen(filename, "rb");
    if(!opened) return 4;
    fseek(opened, 0, SEEK_END);
    length = ftell(opened);
    fseek(opened, 0, SEEK_SET);
    return 0;
}
u32 ed_cmd_file_available(void) { if(!opened) abort(); return length - ftell(opened); }
u8 ed_cmd_file_set_ptr(u32 position) {
    if(!opened || position > length) return 1;
    return fseek(opened, position, SEEK_SET) ? 1 : 0;
}
u8 ed_cmd_file_read(void *data, u32 count) {
    if(!opened) abort();
    return fread(data, 1, count, opened) == count ? 0 : 1;
}
u8 ed_cmd_file_close(void) {
    int result;
    if(!opened) abort();
    result = fclose(opened); opened = 0;
    return result ? 1 : 0;
}
u8 ed_cmd_file_write(void *p, u32 n) { (void)p; (void)n; abort(); }
u8 ed_cmd_dir_make(u8 *p) { (void)p; abort(); }
u8 cd_guard_prepare(const CatalogRecord *r, const NativeBridgeTarget *t, u8 *guard) {
    (void)r; (void)t; *guard = 0; return 0;
}
u8 native_bridge_run(const char *path, const NativeBridgeTarget *t, NativeBridgeReport *report) {
    unsigned char buffer[4096];
    unsigned long total = 0, crc = 0xffffffffUL;
    unsigned i, j, n;
    (void)report;
    if(ed_cmd_file_open((u8 *)path, FA_READ)) abort();
    while((n = (unsigned)fread(buffer, 1, sizeof(buffer), opened)) != 0) {
        total += n;
        for(i = 0; i < n; ++i) {
            crc ^= buffer[i];
            for(j = 0; j < 8; ++j) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320UL : 0);
        }
    }
    ed_cmd_file_close();
    if(total != t->size || ((crc ^ 0xffffffffUL) & 0xffffffffUL) != t->crc32) abort();
    ++bridge_calls;
    return 0xef;
}
u8 native_bridge_run_guarded(const char *p, const NativeBridgeTarget *t, NativeBridgeReport *r, u8 g) {
    (void)g; return native_bridge_run(p, t, r);
}

#define CHECK(x) do { if(!(x)) { fprintf(stderr, "reader line %d: %s\n", __LINE__, #x); return 1; } } while(0)
int main(int argc, char **argv) {
    u16 count, matched, indices[1200], year, i;
    u8 category, work[544];
    CatalogRecord record;
    CatalogFilter filter;
    Preferences prefs;
    Recent recent;
    NativeBridgeReport report;
    NativeBridgeTarget target;
    CheatCoverage coverage;
    CHECK(argc == 2);
    root = argv[1];
    card_io_boot();
    CHECK(!catalog_init(&count));
    memset(&filter, 0, sizeof(filter));
    memset(&prefs, 0, sizeof(prefs));
    memset(&recent, 0, sizeof(recent));
    CHECK(!find_filter(&filter, &prefs, &recent, indices, &matched, work));
    printf("visible %u\n", matched);
    for(category = 1; category <= 6; ++category) {
        filter.category = category;
        CHECK(!find_filter(&filter, &prefs, &recent, indices, &matched, work));
        printf("category %u %u\n", category, matched);
    }
    for(i = 0; i < count; ++i) {
        CHECK(!catalog_record(i, &record));
        CHECK(!game_meta_year(i, pack_key(record.reserved), &year));
        CHECK(!cheat_index_read(i, pack_key(record.reserved), &coverage));
        if(record.kind == 3) continue;
        CHECK(!launch_metadata(i, &record, &target, &report));
        CHECK(launch_selected(i, &record, &report) == 0xef);
    }
    printf("launch-preflight %u\n", bridge_calls);
    CHECK(!opened);
    return 0;
}
