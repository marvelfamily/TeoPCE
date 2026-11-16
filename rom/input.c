#include "input.h"

unsigned char input_poll(InputState *state, unsigned char buttons) {
    unsigned char event;
    unsigned char direction;

    event = (unsigned char)(buttons & ~state->previous & 0xf0);
    state->previous = buttons;
    direction = buttons & 0x0f;
    if ((direction & 0x05) == 0x05) direction &= 0x0a;
    if ((direction & 0x0a) == 0x0a) direction &= 0x05;

    if (!direction) {
        state->direction = 0;
        state->held = 0;
        state->repeat = 0;
    } else if (direction != state->direction) {
        state->direction = direction;
        state->held = 0;
        state->repeat = 18;
        event |= direction;
    } else {
        /* Saturation keeps a long hold fast without an 8-bit rollover gap. */
        if (state->held < 90) ++state->held;
        if (state->repeat) --state->repeat;
        if (!state->repeat) {
            event |= direction;
            state->repeat = state->held >= 90 ? 2 : 4;
        }
    }
    return event;
}
