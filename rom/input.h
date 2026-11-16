#ifndef TEOPCE_INPUT_H
#define TEOPCE_INPUT_H

/*
 * Zero-initialize before first use and poll once per video frame.
 * Bits 0..3 are Up, Right, Down, Left; bits 4..7 are I, II, Select, Run.
 * Action bits are rising edges only. Directions fire immediately, then at
 * held frames 18, 22, ... 90, 92, 94, ... . Opposite axes cancel; valid
 * diagonals retain both bits so the caller can choose its navigation policy.
 */
typedef struct {
    unsigned char previous;
    unsigned char direction;
    unsigned char held;
    unsigned char repeat;
} InputState;

unsigned char input_poll(InputState *state, unsigned char buttons);

#endif
