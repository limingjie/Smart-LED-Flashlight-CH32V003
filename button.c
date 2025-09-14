#include "button.h"
#include <stdio.h>

#define BUTTON_DEBOUNCE_INTERVAL_MS   5    // 5ms
#define BUTTON_DEBOUNCE_STABLE_CYCLES 5    // 5ms x 5 = 25ms
#define BUTTON_RELEASE_STABLE_CYCLES  50   // 5ms x 50  = 250ms
#define BUTTON_HOLD_STABLE_CYCLES     200  // 5ms x 200 = 1000ms

#define printf(...) (void)0  // Disable printf to save flash

enum button_states
{
    STATE_WAIT_FOR_BUTTON_PRESS,
    STATE_BUTTON_PRESS_DEBOUNCE,
    STATE_WAIT_FOR_BUTTON_RELEASE,
    STATE_BUTTON_RELEASE_DEBOUNCE
};

void init_button(button_t *button, uint8_t pin)
{
    funPinMode(pin, GPIO_CFGLR_IN_PUPD);
    funDigitalWrite(pin, FUN_HIGH);

    button->pin                     = pin;
    button->is_held                 = 0;
    button->hold_cycles             = 0;
    button->debounce_cycles         = 0;  // Debounce cycles during stable state (pressed or released)
    button->post_release_cycles     = 0;  // Cycles after previous button release
    button->consecutive_press_count = 0;  // Count the number of presses if within the threshold
    button->state = is_button_down(button) ? STATE_WAIT_FOR_BUTTON_RELEASE : STATE_WAIT_FOR_BUTTON_PRESS;
}

uint8_t get_button_event(button_t *button)
{
    uint8_t button_event = BUTTON_NONE;

    switch (button->state)
    {
        case STATE_WAIT_FOR_BUTTON_PRESS:
            ++button->post_release_cycles;
            if (is_button_down(button))  // Pressed
            {
                button->debounce_cycles = 0;
                button->state           = STATE_BUTTON_PRESS_DEBOUNCE;
                // printf("STATE_BUTTON_PRESS_DEBOUNCE\n");
            }
            else  // Not pressed
            {
                // Cycles since previous button release exceeds the cycles to confirm a release
                if (button->post_release_cycles > BUTTON_RELEASE_STABLE_CYCLES)
                {
                    if (button->is_held)
                    {
                        button->is_held = 0;
                        button_event    = BUTTON_HOLD_RELEASED;
                        printf("Emit BUTTON_HOLD_RELEASED\n");
                    }
                    else
                    {
                        switch (button->consecutive_press_count)
                        {
                            case 0:  // No button pressed
                                break;
                            case 1:
                                button_event = BUTTON_RELEASED;
                                printf("Emit BUTTON_RELEASED\n");
                                break;
                            case 2:
                                button_event = BUTTON_DOUBLE_PRESS_RELEASED;
                                printf("Emit BUTTON_DOUBLE_PRESS_RELEASED\n");
                                break;
                            case 3:
                                button_event = BUTTON_TRIPLE_PRESS_RELEASED;
                                printf("Emit BUTTON_TRIPLE_PRESS_RELEASED\n");
                                break;
                            default:  // Extend here for more consecutive presses if needed
                                button_event = BUTTON_MORE_PRESS_RELEASED;
                                printf("Emit BUTTON_MORE_PRESS_RELEASED, presses = %d\n",
                                       button->consecutive_press_count);
                                break;
                        }
                    }
                    button->consecutive_press_count = 0;
                }
            }
            break;
        case STATE_BUTTON_PRESS_DEBOUNCE:
            ++button->debounce_cycles;
            ++button->post_release_cycles;
            if (is_button_down(button))  // Still pressed during debounce
            {
                // Still pressed after debounce time
                if (button->debounce_cycles >= BUTTON_DEBOUNCE_STABLE_CYCLES)
                {
                    // printf("Consecutive press count: %d\n", button->consecutive_press_count + 1);
                    switch (++button->consecutive_press_count)
                    {
                        case 0:  // No button pressed
                            break;
                        case 1:
                            button_event = BUTTON_PRESSED;
                            printf("Emit BUTTON_PRESSED\n");
                            break;
                        case 2:
                            button_event = BUTTON_DOUBLE_PRESSED;
                            printf("Emit BUTTON_DOUBLE_PRESSED\n");
                            break;
                        case 3:
                            button_event = BUTTON_TRIPLE_PRESSED;
                            printf("Emit BUTTON_TRIPLE_PRESSED\n");
                            break;
                        default:  // Extend here for more consecutive presses if needed
                            button_event = BUTTON_MORE_PRESSED;
                            printf("Emit BUTTON_MORE_PRESSED, presses = %d\n", button->consecutive_press_count);
                            break;
                    }

                    button->hold_cycles = 0;
                    button->state       = STATE_WAIT_FOR_BUTTON_RELEASE;
                    // printf("STATE_WAIT_FOR_BUTTON_RELEASE\n");
                }
            }
            else  // Unstable during debounce, go back to wait for press state
            {
                button->state = STATE_WAIT_FOR_BUTTON_PRESS;
                // printf("STATE_WAIT_FOR_BUTTON_PRESS\n");
            }
            break;
        case STATE_WAIT_FOR_BUTTON_RELEASE:
            ++button->hold_cycles;
            if (is_button_down(button))  // Not released yet
            {
                // Emit hold event if no consecutive press and exceeded hold threshold.
                // Note: Hold event is only emitted once. To continuously emitting hold event, extend here.
                if (button->consecutive_press_count < 2  // 0 - Hold on power on. 1 - Press and hold after power on.
                    && (button->is_held != 1 && button->hold_cycles >= BUTTON_HOLD_STABLE_CYCLES))
                {
                    button->is_held = 1;
                    button_event    = BUTTON_HOLD;
                    printf("Emit BUTTON_HOLD\n");
                }
            }
            else  // released
            {
                button->debounce_cycles = 0;
                button->state           = STATE_BUTTON_RELEASE_DEBOUNCE;
                // printf("STATE_BUTTON_RELEASE_DEBOUNCE\n");
            }
            break;
        case STATE_BUTTON_RELEASE_DEBOUNCE:
            ++button->debounce_cycles;
            if (!is_button_down(button))  // Still released during debounce
            {
                // Release stabilized. Reset post release cycles and start counting for consecutive press detection.
                if (button->debounce_cycles >= BUTTON_DEBOUNCE_STABLE_CYCLES)
                {
                    button->post_release_cycles = 0;
                    button->state               = STATE_WAIT_FOR_BUTTON_PRESS;
                    // printf("STATE_WAIT_FOR_BUTTON_PRESS\n");
                }
            }
            else  // Unstable during debounce, go back to wait for release state
            {
                button->state = STATE_WAIT_FOR_BUTTON_RELEASE;
                // printf("STATE_WAIT_FOR_BUTTON_RELEASE\n");
            }
            break;
    }

    return button_event;
}

void debounce_delay(void)
{
    Delay_Ms(BUTTON_DEBOUNCE_INTERVAL_MS);
}

uint8_t is_button_down(button_t *button)
{
    return funDigitalRead(button->pin) == FUN_LOW;
}
