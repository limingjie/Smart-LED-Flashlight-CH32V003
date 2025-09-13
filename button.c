#include "button.h"
#include <stdio.h>

#define BUTTON_DEBOUNCE_INTERVAL_MS   5    // 5ms
#define BUTTON_DEBOUNCE_STABLE_CYCLES 5    // 5ms x 5 = 25ms
#define BUTTON_HOLD_CYCLE_THRESHOLD   200  // 5ms x 200 = 1000ms

#define printf(...) (void)0  // Disable printf to save space

enum button_states
{
    STATE_WAIT_FOR_BUTTON_PRESS,
    STATE_BUTTON_PRESS_DEBOUNCE,
    STATE_WAIT_FOR_BUTTON_RELEASE,
    STATE_BUTTON_RELEASE_DEBOUNCE
};

void init_button(button_t *btn, uint8_t pin)
{
    btn->pin             = pin;
    btn->hold            = 0;
    btn->press_cycles    = 0;
    btn->debounce_cycles = 0;
    btn->states = (funDigitalRead(pin) == FUN_HIGH) ? STATE_WAIT_FOR_BUTTON_PRESS : STATE_WAIT_FOR_BUTTON_RELEASE;
}

uint8_t get_button_event(button_t *btn)
{
    uint8_t button_event   = BUTTON_NONE;
    uint8_t is_button_down = (funDigitalRead(btn->pin) == FUN_LOW);

    switch (btn->states)
    {
        case STATE_WAIT_FOR_BUTTON_PRESS:
            if (is_button_down)  // pressed
            {
                btn->debounce_cycles = 0;
                btn->states          = STATE_BUTTON_PRESS_DEBOUNCE;
                printf("STATE_BUTTON_PRESS_DEBOUNCE\n");
            }
            break;
        case STATE_BUTTON_PRESS_DEBOUNCE:
            ++btn->debounce_cycles;
            if (is_button_down)  // still pressed after debounce time
            {
                if (btn->debounce_cycles >= BUTTON_DEBOUNCE_STABLE_CYCLES)
                {
                    button_event      = BUTTON_PRESSED;
                    btn->press_cycles = 0;
                    btn->states       = STATE_WAIT_FOR_BUTTON_RELEASE;
                    printf("STATE_WAIT_FOR_BUTTON_RELEASE\n");
                }
            }
            else
            {
                btn->states = STATE_WAIT_FOR_BUTTON_PRESS;
                printf("STATE_WAIT_FOR_BUTTON_PRESS\n");
            }
            break;
        case STATE_WAIT_FOR_BUTTON_RELEASE:
            ++btn->press_cycles;
            if (is_button_down)  // holding
            {
                if (btn->press_cycles >= BUTTON_HOLD_CYCLE_THRESHOLD)
                {
                    if (btn->hold != 1)
                    {
                        btn->hold    = 1;
                        button_event = BUTTON_HOLD;
                        printf("Button hold!!!\n");
                    }
                }
            }
            else  // released
            {
                btn->debounce_cycles = 0;
                btn->states          = STATE_BUTTON_RELEASE_DEBOUNCE;
                printf("STATE_BUTTON_RELEASE_DEBOUNCE\n");
            }
            break;
        case STATE_BUTTON_RELEASE_DEBOUNCE:
            ++btn->debounce_cycles;
            if (!is_button_down)  // still released after debounce time
            {
                if (btn->debounce_cycles >= BUTTON_DEBOUNCE_STABLE_CYCLES)
                {
                    button_event      = btn->hold ? BUTTON_HOLD_RELEASED : BUTTON_RELEASED;
                    btn->press_cycles = 0;
                    btn->hold         = 0;
                    btn->states       = STATE_WAIT_FOR_BUTTON_PRESS;
                    printf("STATE_WAIT_FOR_BUTTON_PRESS\n");
                }
            }
            else
            {
                btn->states = STATE_WAIT_FOR_BUTTON_RELEASE;
                printf("STATE_WAIT_FOR_BUTTON_RELEASE\n");
            }
            break;
    }

    return button_event;
}

void button_debounce(void)
{
    Delay_Ms(BUTTON_DEBOUNCE_INTERVAL_MS);
}
