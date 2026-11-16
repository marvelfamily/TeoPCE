#ifndef TEOPCE_PREFERENCES_H
#define TEOPCE_PREFERENCES_H
#define PREFS_SIZE 272
#define FAVORITES_MAX 64
#define THEME_COUNT 4
typedef struct {
    unsigned char count, view, theme;
    unsigned long sequence;
    unsigned long keys[FAVORITES_MAX];
} Preferences;
unsigned char prefs_toggle(Preferences *prefs, unsigned long key);
unsigned char prefs_has(const Preferences *prefs, unsigned long key);
unsigned char prefs_encode(const Preferences *prefs, unsigned char *data);
unsigned char prefs_decode(Preferences *prefs, const unsigned char *data, unsigned short size);
unsigned char prefs_newer(unsigned long candidate, unsigned long current);
#endif
