#include "button.h"
#include <stdio.h>

enum debounce_states
{
    STATE_WAIT_FOR_BTN_PRESS,
    STATE_BTN_PRESS_DEBOUNCE,
    STATE_WAIT_FOR_BTN_RELEASE,
    STATE_BTN_RELEASE_DEBOUNCE
};

enum button_states
{
    BTN_UP,
    BTN_DOWN
};

void init_button(button_t *btn, uint8_t pin)
{
    btn->pin                  = pin;
    btn->debounce_states      = STATE_WAIT_FOR_BTN_PRESS;
    btn->hold                 = 0;
    btn->cycle_count          = 0;
    btn->debounce_cycle_count = 0;

    if (funDigitalRead(pin) == FUN_LOW)
    {
        btn->debounce_states = STATE_WAIT_FOR_BTN_RELEASE;
        uint8_t button_event;
        while (1)  // Wait until button released
        {
            button_event = read_button_event(btn);
            if (button_event == BTN_RELEASE || button_event == BTN_HOLD_RELEASE)
            {
                break;
            }
        }
    }
}

uint8_t read_button_event(button_t *btn)
{
    uint8_t button_event = BTN_NONE;
    uint8_t button_state = (funDigitalRead(btn->pin) == FUN_LOW) ? BTN_DOWN : BTN_UP;

    switch (btn->debounce_states)
    {
        case STATE_WAIT_FOR_BTN_PRESS:
            if (button_state == BTN_DOWN)  // pressed
            {
                btn->debounce_cycle_count = 0;
                btn->debounce_states      = STATE_BTN_PRESS_DEBOUNCE;
                printf("STATE_BTN_PRESS_DEBOUNCE\n");
            }
            break;
        case STATE_BTN_PRESS_DEBOUNCE:
            ++btn->debounce_cycle_count;
            if (button_state == BTN_DOWN)  // holding
            {
                if (btn->debounce_cycle_count >= BTN_DEBOUNCE_STABLE_CYCLES)
                {
                    button_event         = BTN_PRESS;
                    btn->cycle_count     = 0;
                    btn->debounce_states = STATE_WAIT_FOR_BTN_RELEASE;
                    printf("STATE_WAIT_FOR_BTN_RELEASE\n");
                }
            }
            else
            {
                btn->debounce_states = STATE_WAIT_FOR_BTN_PRESS;
                printf("STATE_WAIT_FOR_BTN_PRESS\n");
            }
            break;
        case STATE_WAIT_FOR_BTN_RELEASE:
            ++btn->cycle_count;
            if (button_state == BTN_DOWN)  // holding
            {
                if (btn->cycle_count >= BTN_HOLD_CYCLES)
                {
                    btn->hold    = 1;
                    button_event = BTN_HOLD;
                    printf("Key hold!!! cycle = %d\n", btn->cycle_count);
                }
            }
            else  // released
            {
                btn->debounce_cycle_count = 0;
                btn->debounce_states      = STATE_BTN_RELEASE_DEBOUNCE;
                printf("STATE_BTN_RELEASE_DEBOUNCE\n");
            }
            break;
        case STATE_BTN_RELEASE_DEBOUNCE:
            ++btn->debounce_cycle_count;
            if (button_state == BTN_UP)  // still released
            {
                if (btn->debounce_cycle_count >= BTN_DEBOUNCE_STABLE_CYCLES)
                {
                    button_event         = btn->hold ? BTN_HOLD_RELEASE : BTN_RELEASE;
                    btn->cycle_count     = 0;
                    btn->hold            = 0;
                    btn->debounce_states = STATE_WAIT_FOR_BTN_PRESS;
                    printf("STATE_WAIT_FOR_BTN_PRESS\n");
                }
            }
            else
            {
                btn->debounce_states = STATE_WAIT_FOR_BTN_RELEASE;
                printf("STATE_WAIT_FOR_BTN_RELEASE\n");
            }
            break;
    }

    Delay_Ms(BTN_DEBOUNCE_INTERVAL_MS);

    return button_event;
}
