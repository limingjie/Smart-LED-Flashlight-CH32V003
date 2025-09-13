#include "button.h"
#include <stdio.h>

#define BUTTON_DEBOUNCE_INTERVAL_MS         5    // 5ms
#define BUTTON_DEBOUNCE_STABLE_CYCLES       5    // 5ms x 5 = 25ms
#define BUTTON_DOUBLE_PRESS_CYCLE_THRESHOLD 50   // 5ms x 50  = 250ms
#define BUTTON_HOLD_CYCLE_THRESHOLD         200  // 5ms x 200 = 1000ms

// #define printf(...) (void)0  // Disable printf to save space

enum button_states
{
    STATE_WAIT_FOR_BUTTON_PRESS,
    STATE_BUTTON_PRESS_DEBOUNCE,
    STATE_WAIT_FOR_BUTTON_RELEASE,
    STATE_BUTTON_RELEASE_DEBOUNCE
};

void init_button(button_t *btn, uint8_t pin)
{
    funPinMode(pin, GPIO_CFGLR_IN_PUPD);
    funDigitalWrite(pin, FUN_HIGH);

    btn->pin                                = pin;
    btn->hold                               = 0;
    btn->press_cycles                       = 0;
    btn->debounce_cycles                    = 0;
    btn->cycles_since_single_press_released = 0;  // 0 - after double press released; >0 - cycles since last released
    btn->states = is_button_down(btn) ? STATE_WAIT_FOR_BUTTON_RELEASE : STATE_WAIT_FOR_BUTTON_PRESS;
}

uint8_t get_button_event(button_t *btn)
{
    uint8_t button_event = BUTTON_NONE;

    switch (btn->states)
    {
        case STATE_WAIT_FOR_BUTTON_PRESS:
            // Count cycles since last released to detect double press
            if (btn->cycles_since_single_press_released)
            {
                btn->cycles_since_single_press_released++;
            }

            if (is_button_down(btn))  // pressed
            {
                btn->debounce_cycles = 0;
                btn->states          = STATE_BUTTON_PRESS_DEBOUNCE;
                // printf("STATE_BUTTON_PRESS_DEBOUNCE\n");
            }
            else
            {
                if (btn->cycles_since_single_press_released >= BUTTON_DOUBLE_PRESS_CYCLE_THRESHOLD)
                {
                    btn->cycles_since_single_press_released = 0;
                    button_event                            = BUTTON_RELEASED;
                    printf("Emit BUTTON_RELEASED\n");
                }
            }
            break;
        case STATE_BUTTON_PRESS_DEBOUNCE:
            ++btn->debounce_cycles;
            if (is_button_down(btn))  // still pressed after debounce time
            {
                if (btn->debounce_cycles >= BUTTON_DEBOUNCE_STABLE_CYCLES)
                {
                    button_event = btn->cycles_since_single_press_released ? BUTTON_DOUBLE_PRESSED : BUTTON_PRESSED;
                    btn->press_cycles = 0;
                    btn->states       = STATE_WAIT_FOR_BUTTON_RELEASE;
                    printf("Emit %s\n",
                           btn->cycles_since_single_press_released ? "BUTTON_DOUBLE_PRESSED" : "BUTTON_PRESSED");
                    // printf("STATE_WAIT_FOR_BUTTON_RELEASE\n");
                }
            }
            else
            {
                btn->states = STATE_WAIT_FOR_BUTTON_PRESS;
                // printf("STATE_WAIT_FOR_BUTTON_PRESS\n");
            }
            break;
        case STATE_WAIT_FOR_BUTTON_RELEASE:
            ++btn->press_cycles;
            if (is_button_down(btn))  // holding
            {
                if (!btn->cycles_since_single_press_released && btn->press_cycles >= BUTTON_HOLD_CYCLE_THRESHOLD)
                {
                    if (btn->hold != 1)
                    {
                        btn->hold    = 1;
                        button_event = BUTTON_HOLD;
                        printf("Emit BUTTON_HOLD\n");
                    }
                }
            }
            else  // released
            {
                btn->debounce_cycles = 0;
                btn->states          = STATE_BUTTON_RELEASE_DEBOUNCE;
                // printf("STATE_BUTTON_RELEASE_DEBOUNCE\n");
            }
            break;
        case STATE_BUTTON_RELEASE_DEBOUNCE:
            ++btn->debounce_cycles;
            if (!is_button_down(btn))  // still released after debounce time
            {
                if (btn->debounce_cycles >= BUTTON_DEBOUNCE_STABLE_CYCLES)
                {
                    if (btn->cycles_since_single_press_released)
                    {
                        btn->cycles_since_single_press_released = 0;
                        button_event                            = BUTTON_DOUBLE_PRESS_RELEASED;
                        printf("Emit BUTTON_DOUBLE_PRESS_RELEASED\n");
                    }
                    else
                    {
                        if (btn->hold)
                        {
                            btn->hold    = 0;
                            button_event = BUTTON_HOLD_RELEASED;
                            printf("Emit BUTTON_HOLD_RELEASED\n");
                        }
                        else  // Button release after a single press, wait for enough cycles to confirm no double press
                        // before emitting BUTTON_RELEASED
                        {
                            btn->cycles_since_single_press_released = 1;
                        }
                    }

                    btn->states = STATE_WAIT_FOR_BUTTON_PRESS;
                    // printf("STATE_WAIT_FOR_BUTTON_PRESS\n");
                }
            }
            else
            {
                btn->states = STATE_WAIT_FOR_BUTTON_RELEASE;
                // printf("STATE_WAIT_FOR_BUTTON_RELEASE\n");
            }
            break;
    }

    return button_event;
}

void button_debounce(void)
{
    Delay_Ms(BUTTON_DEBOUNCE_INTERVAL_MS);
}

uint8_t is_button_down(button_t *btn)
{
    return funDigitalRead(btn->pin) == FUN_LOW;
}
