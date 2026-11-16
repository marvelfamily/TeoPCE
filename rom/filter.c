#include <string.h>

unsigned char entry_matches(const char *title, const char *genre, unsigned char kind,
                            unsigned char medium, const char *genre_filter,
                            unsigned char letter_filter) {
    unsigned char first = (unsigned char) title[0];
    if (medium && medium != kind) return 0;
    if (*genre_filter && strcmp(genre, genre_filter) != 0) return 0;
    if (!letter_filter) return 1;
    if (first >= 'a' && first <= 'z') first -= 'a' - 'A';
    if (letter_filter == '#') return first < 'A' || first > 'Z';
    return first == letter_filter;
}
