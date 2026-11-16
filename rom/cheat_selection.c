#include "cheat_selection.h"
#if defined(__CC65__)
#include "main.h"
#define cheat_observe preview_observe_impl
#define cheat_toggle preview_toggle_impl
#define cheat_disable preview_disable_impl
#define cheat_group_state preview_state_impl
#define cheat_slots preview_slots_impl
#define cheat_active preview_active_impl
#ifdef PREVIEW
#define SELECTION_BANK 15
#pragma code-name(push,"BNK11")
#else
#define SELECTION_BANK 14
#pragma code-name(push,"BNK10")
#endif
#endif

static unsigned short full_bits(unsigned char cost) {
    return (unsigned short)((1UL << cost) - 1UL);
}

static unsigned char valid(const CheatSelection *s) {
    unsigned char i, j, slots = 0;
    if (!s || s->count > 16 || (s->risk & ~(unsigned short)((1UL << s->count) - 1UL))) return 0;
    for (i = 0; i < s->count; ++i) {
        if (!s->costs[i] || s->costs[i] > 16 || !s->bits[i] ||
            (s->bits[i] & full_bits(s->costs[i])) != s->bits[i]) return 0;
        slots += s->costs[i];
        if (slots > 16) return 0;
        for (j = 0; j < i; ++j) if (s->ids[j] == s->ids[i]) return 0;
    }
    return 1;
}

static unsigned char find(const CheatSelection *s, unsigned long id) {
    unsigned char i;
    for (i = 0; i < s->count; ++i) if (s->ids[i] == id) return i;
    return 16;
}

unsigned char cheat_slots(const CheatSelection *s) {
    unsigned char i, total = 0;
    if (!valid(s)) return CHEAT_INVALID;
    for (i = 0; i < s->count; ++i) total += s->costs[i];
    return total;
}

unsigned char cheat_active(const CheatSelection *s) {
    unsigned char i, bit, total = 0;
    unsigned short bits;
    if (!valid(s)) return CHEAT_INVALID;
    for (i = 0; i < s->count; ++i) {
        bits = s->bits[i];
        for (bit = 0; bit < s->costs[i]; ++bit) {
            total += (unsigned char)(bits & 1);
            bits >>= 1;
        }
    }
    return total;
}

unsigned char cheat_group_state(const CheatSelection *s, unsigned long id) {
    unsigned char i;
    if (!valid(s)) return CHEAT_INVALID;
    i = find(s, id);
    if (i == 16) return CHEAT_OFF;
    return s->bits[i] == full_bits(s->costs[i]) ? CHEAT_ON : CHEAT_MIXED;
}

unsigned char cheat_observe(CheatSelection *s, unsigned long id,
                           unsigned char cost, unsigned short bits) {
    unsigned char i;
    if (!valid(s) || !cost || cost > 16 ||
        (bits & full_bits(cost)) != bits || find(s, id) != 16) return CHEAT_BAD_STATE;
    if (!bits) return 0;
    if (cheat_slots(s) + cost > 16) return CHEAT_LIMIT;
    i = s->count;
    s->ids[i] = id; s->costs[i] = cost; s->bits[i] = bits;
    ++s->count;
    return 0;
}

unsigned char cheat_disable(CheatSelection *s, unsigned long id) {
    unsigned char i, j;
    if (!valid(s)) return CHEAT_BAD_STATE;
    i = find(s, id);
    if (i == 16) return 0;
    s->risk = (unsigned short)((s->risk & ((1UL << i) - 1UL)) |
               ((s->risk >> 1) & ~((1UL << i) - 1UL)));
    --s->count;
    for (j = i; j < s->count; ++j) {
        s->ids[j] = s->ids[j + 1];
        s->costs[j] = s->costs[j + 1];
        s->bits[j] = s->bits[j + 1];
    }
    s->ids[s->count] = 0; s->costs[s->count] = 0; s->bits[s->count] = 0;
    return 0;
}

unsigned char cheat_toggle(CheatSelection *s, unsigned long id,
                          unsigned char cost, unsigned char held_reason) {
    unsigned char i;
    if (!valid(s) || !cost || cost > 16) return CHEAT_BAD_STATE;
    if (held_reason) return CHEAT_HELD;
    i = find(s, id);
    if (i == 16) return cheat_observe(s, id, cost, full_bits(cost));
    if (s->costs[i] != cost) return CHEAT_BAD_STATE;
    if (s->bits[i] != full_bits(cost)) {
        s->bits[i] = full_bits(cost);
        return 0;
    }
    return cheat_disable(s, id);
}

#if defined(__CC65__)
#pragma code-name(pop)
#endif
#if defined(__CC65__)
#undef cheat_observe
#undef cheat_toggle
#undef cheat_disable
#undef cheat_group_state
#undef cheat_slots
#undef cheat_active
/* These routines touch only RAM; no active TCG/art data are read while mapped. */
unsigned char cheat_observe(CheatSelection*s,unsigned long id,unsigned char cost,unsigned short bits){unsigned char bank=sysGetBank(),r;sysSetBank(SELECTION_BANK);r=preview_observe_impl(s,id,cost,bits);sysSetBank(bank);return r;}
unsigned char cheat_toggle(CheatSelection*s,unsigned long id,unsigned char cost,unsigned char held){unsigned char bank=sysGetBank(),r;sysSetBank(SELECTION_BANK);r=preview_toggle_impl(s,id,cost,held);sysSetBank(bank);return r;}
unsigned char cheat_disable(CheatSelection*s,unsigned long id){unsigned char bank=sysGetBank(),r;sysSetBank(SELECTION_BANK);r=preview_disable_impl(s,id);sysSetBank(bank);return r;}
unsigned char cheat_group_state(const CheatSelection*s,unsigned long id){unsigned char bank=sysGetBank(),r;sysSetBank(SELECTION_BANK);r=preview_state_impl(s,id);sysSetBank(bank);return r;}
unsigned char cheat_slots(const CheatSelection*s){unsigned char bank=sysGetBank(),r;sysSetBank(SELECTION_BANK);r=preview_slots_impl(s);sysSetBank(bank);return r;}
unsigned char cheat_active(const CheatSelection*s){unsigned char bank=sysGetBank(),r;sysSetBank(SELECTION_BANK);r=preview_active_impl(s);sysSetBank(bank);return r;}
#endif
