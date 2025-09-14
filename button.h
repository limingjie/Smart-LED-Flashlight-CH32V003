#ifndef __BUTTON_H__
#define __BUTTON_H__

#include "ch32fun.h"

// Button Lifecycle States
//  +-------------------------------+         +-------------------------------+
//  |  STATE_WAIT_FOR_BUTTON_PRESS  |         |  STATE_BUTTON_PRESS_DEBOUNCE  |
//  |  - Wait for next button press | <-----> |  - Debouncing button press    |
//  |  - Emit button release events |         |  - Emit BUTTON_PRESSED event  |
//  +-------------------------------+         +-------------------------------+
//                  ^                                         |
//                  |                                         V
//  +-------------------------------+         +-------------------------------+
//  | STATE_BUTTON_RELEASE_DEBOUNCE |         | STATE_WAIT_FOR_BUTTON_RELEASE |
//  | - Debouncing button release   | <-----> | - Wait for button release     |
//  |                               |         | - Emit BUTTON_HOLD event      |
//  +-------------------------------+         +-------------------------------+

enum button_events
{
    BUTTON_NONE,
    BUTTON_PRESSED,
    BUTTON_RELEASED,
    BUTTON_DOUBLE_PRESSED,
    BUTTON_DOUBLE_PRESS_RELEASED,
    BUTTON_TRIPLE_PRESSED,
    BUTTON_TRIPLE_PRESS_RELEASED,
    BUTTON_MORE_PRESSED,         // More than 3 consecutive presses
    BUTTON_MORE_PRESS_RELEASED,  // More than 3 consecutive press releases
    BUTTON_HOLD,
    BUTTON_HOLD_RELEASED
};

typedef struct button
{
    uint8_t  pin;
    uint8_t  state;
    uint8_t  is_held;
    uint16_t hold_cycles;
    uint16_t debounce_cycles;
    uint8_t  consecutive_press_count;
    uint32_t post_release_cycles;
} button_t;

void    init_button(button_t *button, uint8_t pin);
uint8_t get_button_event(button_t *button);
void    debounce_delay(void);
uint8_t is_button_down(button_t *button);

#endif  // __BUTTON_H__
