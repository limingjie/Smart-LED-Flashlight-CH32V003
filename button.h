#ifndef __BUTTON_H__
#define __BUTTON_H__

#include "ch32fun.h"

#define BTN_DEBOUNCE_INTERVAL_MS   5    // 5ms
#define BTN_DEBOUNCE_STABLE_CYCLES 5    // 5ms x 5 = 25ms
#define BTN_HOLD_CYCLES            300  // 5ms x 300 = 1500ms

// Button Lifecycle States
//  +----------------------------+         +----------------------------+
//  |  STATE_WAIT_FOR_BTN_PRESS  | <-----> |  STATE_BTN_PRESS_DEBOUNCE  |
//  +----------------------------+         +----------------------------+
//                ^                                      |
//                |                                      V
//  +----------------------------+         +----------------------------+
//  | STATE_BTN_RELEASE_DEBOUNCE | <-----> | STATE_WAIT_FOR_BTN_RELEASE |
//  +----------------------------+         +----------------------------+

enum button_events
{
    BTN_NONE,
    BTN_PRESS,
    BTN_HOLD,
    BTN_RELEASE,
    BTN_HOLD_RELEASE
};

typedef struct button
{
    uint8_t  pin;
    uint8_t  debounce_states;
    uint8_t  hold;
    uint16_t cycle_count;
    uint16_t debounce_cycle_count;
} button_t;

void    init_button(button_t *btn, uint8_t pin);
uint8_t read_button_event(button_t *btn);

#endif  // __BUTTON_H__
