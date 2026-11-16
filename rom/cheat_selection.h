#ifndef TEOPCE_CHEAT_SELECTION_H
#define TEOPCE_CHEAT_SELECTION_H

/* Pure draft state. No filesystem, global settings, native writes or heap.
 * At most sixteen complete groups can fit the sixteen native record slots.
 * Zero-initialize, then observe each retained native group once. */
typedef struct {
    unsigned long ids[16];
    unsigned short bits[16];
    unsigned char costs[16];
    unsigned char count;
    /* Exact active group indexes explicitly accepted at own risk. */
    unsigned short risk;
} CheatSelection;

#define CHEAT_OFF 0
#define CHEAT_ON 1
#define CHEAT_MIXED 2
#define CHEAT_INVALID 255
#define CHEAT_LIMIT 1
#define CHEAT_HELD 2
#define CHEAT_BAD_STATE 3

unsigned char cheat_observe(CheatSelection *state, unsigned long id,
                           unsigned char cost, unsigned short bits);
unsigned char cheat_toggle(CheatSelection *state, unsigned long id,
                          unsigned char cost, unsigned char held_reason);
unsigned char cheat_disable(CheatSelection *state, unsigned long id);
unsigned char cheat_group_state(const CheatSelection *state, unsigned long id);
unsigned char cheat_slots(const CheatSelection *state);
unsigned char cheat_active(const CheatSelection *state);
#endif
