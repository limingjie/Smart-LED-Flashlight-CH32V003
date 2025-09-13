#ifndef __BUTTON_H__
#define __BUTTON_H__

#include "ch32fun.h"

// Button Lifecycle States
//  +-------------------------------+         +-------------------------------+
//  |  STATE_WAIT_FOR_BUTTON_PRESS  | <-----> |  STATE_BUTTON_PRESS_DEBOUNCE  |
//  +-------------------------------+         +-------------------------------+
//                  ^                                         |
//                  |                                         V
//  +-------------------------------+         +-------------------------------+
//  | STATE_BUTTON_RELEASE_DEBOUNCE | <-----> | STATE_WAIT_FOR_BUTTON_RELEASE |
//  +-------------------------------+         +-------------------------------+

enum button_events
{
    BUTTON_NONE,
    BUTTON_PRESSED,
    BUTTON_RELEASED,
    BUTTON_DOUBLE_PRESSED,
    BUTTON_DOUBLE_PRESS_RELEASED,
    BUTTON_HOLD,
    BUTTON_HOLD_RELEASED
};

typedef struct button
{
    uint8_t  pin;
    uint8_t  states;
    uint8_t  hold;
    uint16_t press_cycles;
    uint16_t debounce_cycles;
    uint8_t  cycles_since_single_press_released;
} button_t;

void    init_button(button_t *btn, uint8_t pin);
uint8_t get_button_event(button_t *btn);
void    button_debounce(void);
uint8_t is_button_down(button_t *btn);

#endif  // __BUTTON_H__
