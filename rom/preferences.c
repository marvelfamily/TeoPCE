#include "preferences.h"
#include <string.h>

static unsigned long get32(const unsigned char *p) {
    return (unsigned long)p[0] | ((unsigned long)p[1] << 8) |
        ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}
static void put32(unsigned char *p, unsigned long value) {
    p[0] = (unsigned char)value; p[1] = (unsigned char)(value >> 8);
    p[2] = (unsigned char)(value >> 16); p[3] = (unsigned char)(value >> 24);
}
static unsigned long checksum(const unsigned char *p) {
    unsigned short i;
    unsigned long hash = 2166136261UL;
    for (i = 0; i < PREFS_SIZE - 4; ++i)
        hash = ((hash ^ p[i]) * 16777619UL) & 0xffffffffUL;
    return hash;
}
unsigned char prefs_has(const Preferences *prefs, unsigned long key) {
    unsigned char i;
    for (i = 0; i < prefs->count && i < FAVORITES_MAX; ++i)
        if (prefs->keys[i] == key) return 1;
    return 0;
}
unsigned char prefs_toggle(Preferences *prefs, unsigned long key) {
    unsigned char i, j;
    if (prefs->count > FAVORITES_MAX) return 1;
    for (i = 0; i < prefs->count; ++i) {
        if (prefs->keys[i] != key) continue;
        for (j = i; j + 1 < prefs->count; ++j) prefs->keys[j] = prefs->keys[j + 1];
        prefs->keys[--prefs->count] = 0;
        return 0;
    }
    if (prefs->count == FAVORITES_MAX) return 1;
    prefs->keys[prefs->count++] = key;
    return 0;
}
unsigned char prefs_encode(const Preferences *prefs, unsigned char *data) {
    unsigned char i;
    if (prefs->count > FAVORITES_MAX || prefs->view > 1 || prefs->theme >= THEME_COUNT) return 0;
    memset(data, 0, PREFS_SIZE);
    memcpy(data, "TPRF", 4);
    data[4] = 1; data[5] = prefs->count; data[6] = prefs->view; data[7] = prefs->theme;
    put32(data + 8, prefs->sequence);
    for (i = 0; i < prefs->count; ++i) put32(data + 12 + (unsigned short)i * 4, prefs->keys[i]);
    put32(data + PREFS_SIZE - 4, checksum(data));
    return 1;
}
unsigned char prefs_decode(Preferences *prefs, const unsigned char *data, unsigned short size) {
    unsigned char i, j;
    unsigned long key;
    if (size != PREFS_SIZE || memcmp(data, "TPRF", 4) || data[4] != 1 ||
        data[5] > FAVORITES_MAX || data[6] > 1 || data[7] >= THEME_COUNT ||
        get32(data + PREFS_SIZE - 4) != checksum(data)) return 0;
    for (i = 0; i < data[5]; ++i) {
        key = get32(data + 12 + (unsigned short)i * 4);
        for (j = 0; j < i; ++j)
            if (key == get32(data + 12 + (unsigned short)j * 4)) return 0;
    }
    memset(prefs, 0, sizeof(*prefs));
    prefs->count = data[5]; prefs->view = data[6]; prefs->theme = data[7];
    prefs->sequence = get32(data + 8);
    for (i = 0; i < prefs->count; ++i) prefs->keys[i] = get32(data + 12 + (unsigned short)i * 4);
    return 1;
}
unsigned char prefs_newer(unsigned long candidate, unsigned long current) {
    unsigned long distance = (candidate - current) & 0xffffffffUL;
    return distance != 0 && distance < 0x80000000UL;
}
