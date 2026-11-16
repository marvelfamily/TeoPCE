#ifndef TEOPCE_CATALOG_H
#define TEOPCE_CATALOG_H

#include "main.h"
#include "preferences.h"
#include "pack.h"
#include "app_paths.h"

#define CATALOG_ERR_INPUT 0xf0
#define CATALOG_ERR_FORMAT 0xf1
#define CATALOG_ERR_RANGE 0xf2
#define CATALOG_ERR_CATEGORY 0xf3
#define CATEGORY_COUNT 7
#define CATALOG_PAGE_ROWS 12
#define CATALOG_TITLE_WIDTH 24

typedef struct {
    char title[88];
    char path[128];
    char genre[20];
    char region[12];
    u8 kind;
    u8 reserved[7];
} CatalogRecord;
typedef char catalog_record_is_256_bytes[(sizeof(CatalogRecord) == 256) ? 1 : -1];

typedef struct {
    u8 medium;
    u8 genre;
    u8 letter;
    u8 region;
    u8 favorites;
    u8 recent;
    u8 cheats;
    u8 category;
    char prefix[13];
} CatalogFilter;

u8 catalog_init(u16 *count);
u8 catalog_identity(u16 *count, u32 *signature);
u8 catalog_filter(const CatalogFilter *filter, const Preferences *prefs,
                  u16 *matches, u16 *matched);
u8 catalog_record(u16 index, CatalogRecord *record);
u8 catalog_page(const u16 *matches, u16 matched, u16 start, u8 rows,
                char titles[12][24], u32 keys[12], u8 *loaded);

#endif
