/*
 * CH32V003 SGM3732 Portable LED Light
 *
 * Aug 2025 by Li Mingjie
 *  - Email:  limingjie@outlook.com
 *  - GitHub: https://github.com/limingjie/
 */

#include <stdio.h>
#include "ch32fun.h"

uint8_t  power_led_pin       = PA2;  // LED pin
uint8_t  button_pin          = PC1;  // Latch and button pin
uint8_t  pwm_pin             = PC4;  // PWM output pin
uint32_t pwm_update_delay_ms = 500;  // LED blink delay in ms
uint8_t  pwm_state           = 0;
uint16_t pwm_dim_cycles      = 0;
uint16_t pwm_dim_cycle_delta = 30;
uint8_t  pwm_blink_state     = FUN_LOW;

void systick_init(void)
{
    SysTick->CTLR = 0;
    NVIC_EnableIRQ(SysTicK_IRQn);
    SysTick->CMP  = FUNCONF_SYSTEM_CORE_CLOCK / 1000 * pwm_update_delay_ms;
    SysTick->CNT  = 0;
    SysTick->CTLR = SYSTICK_CTLR_STE | SYSTICK_CTLR_STIE | SYSTICK_CTLR_STCLK;
}

void disable_systick(void)
{
    NVIC_DisableIRQ(SysTicK_IRQn);
}

void SysTick_Handler(void) __attribute__((interrupt));
void SysTick_Handler(void)
{
    // SysTick maintains accurate 20ms period
    SysTick->CMP += FUNCONF_SYSTEM_CORE_CLOCK / 1000 * pwm_update_delay_ms;
    SysTick->SR = 0;

    // process PWM
    switch (pwm_state)
    {
        case 3:
        case 4:
        case 5:
            TIM1->CH4CVR    = pwm_blink_state ? 150 : 0;  // blink
            pwm_blink_state = !pwm_blink_state;
            break;
        case 6:  // Dimming
            TIM1->CH4CVR = pwm_dim_cycles;
            pwm_dim_cycles += pwm_dim_cycle_delta;
            if (pwm_dim_cycles >= 150 || pwm_dim_cycles <= 0)
            {
                pwm_dim_cycle_delta = -pwm_dim_cycle_delta;
            }
            break;
    }
}

void tim1_pwm_init(void)
{
    // Enable GPIOC and TIM1
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_TIM1;

    // PC4 is T1CH4, 10MHz Output alt func, push-pull
    GPIOC->CFGLR &= ~(0xf << (4 * 4));
    GPIOC->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_PP_AF) << (4 * 4);

    // Reset TIM1 to init all regs
    RCC->APB2PRSTR |= RCC_APB2Periph_TIM1;
    RCC->APB2PRSTR &= ~RCC_APB2Periph_TIM1;

    // Prescaler
    TIM1->PSC = 0x0000;

    // Auto Reload - sets period
    TIM1->ATRLR = 149;

    // Reload immediately
    TIM1->SWEVGR |= TIM_UG;

    // Enable CH4 output, positive pol
    TIM1->CCER |= TIM_CC4E | TIM_CC4NP;

    // CH2 Mode is output, PWM1 (CC1S = 00, OC1M = 110)
    TIM1->CHCTLR2 |= TIM_OC4M_2 | TIM_OC4M_1;

    // Set the Capture Compare Register value to 50% initially
    TIM1->CH4CVR = 0;

    // Enable TIM1 outputs
    TIM1->BDTR |= TIM_MOE;

    // Enable TIM1
    TIM1->CTLR1 |= TIM_CEN;
}

// Key states
// TODO: Create a library
//  +----------------------------+         +----------------------------+
//  |  STATE_WAIT_FOR_KEY_PRESS  | <-----> |  STATE_KEY_PRESS_DEBOUNCE  |
//  +----------------------------+         +----------------------------+
//                ^                                      |
//                |                                      V
//  +----------------------------+         +----------------------------+
//  | STATE_KEY_RELEASE_DEBOUNCE | <-----> | STATE_WAIT_FOR_KEY_RELEASE |
//  +----------------------------+         +----------------------------+

enum debounce_states
{
    STATE_WAIT_FOR_KEY_PRESS,
    STATE_KEY_PRESS_DEBOUNCE,
    STATE_WAIT_FOR_KEY_RELEASE,
    STATE_KEY_RELEASE_DEBOUNCE
};

enum key_states
{
    KEY_UP,
    KEY_DOWN
};

enum key_events
{
    KEY_NONE,
    KEY_PRESS,
    KEY_RELEASE,
    KEY_LONG_PRESS_RELEASE
};

#define DEBOUNCE_INTERVAL      5    // 5ms
#define DEBOUNCE_STABLE_CYCLES 5    // 5ms x 5 = 25ms
#define LONG_PRESS_CYCLES      600  // 5ms x 600 = 3000ms

uint8_t read_button_state(void)
{
    static uint8_t  debounce_state           = STATE_WAIT_FOR_KEY_PRESS;
    static uint8_t  key_long_press           = 0;
    static uint16_t key_cycle_count          = 0;
    static uint16_t key_debounce_cycle_count = 0;

    uint8_t key_event = KEY_NONE;
    uint8_t key_state = (funDigitalRead(button_pin) == FUN_LOW) ? KEY_DOWN : KEY_UP;
    switch (debounce_state)
    {
        case STATE_WAIT_FOR_KEY_PRESS:
            if (key_state == KEY_DOWN)  // pressed
            {
                key_debounce_cycle_count = 0;
                debounce_state           = STATE_KEY_PRESS_DEBOUNCE;
                printf("STATE_KEY_PRESS_DEBOUNCE\n");
            }
            break;
        case STATE_KEY_PRESS_DEBOUNCE:
            ++key_debounce_cycle_count;
            if (key_state == KEY_DOWN)  // still pressed
            {
                if (key_debounce_cycle_count >= DEBOUNCE_STABLE_CYCLES)
                {
                    key_event       = KEY_PRESS;
                    key_cycle_count = 0;
                    key_long_press  = 0;
                    debounce_state  = STATE_WAIT_FOR_KEY_RELEASE;
                    printf("STATE_WAIT_FOR_KEY_RELEASE\n");
                }
            }
            else
            {
                debounce_state = STATE_WAIT_FOR_KEY_PRESS;
                printf("STATE_WAIT_FOR_KEY_PRESS\n");
            }
            break;
        case STATE_WAIT_FOR_KEY_RELEASE:
            ++key_cycle_count;
            if (key_state == KEY_UP)  // released
            {
                if (key_cycle_count >= LONG_PRESS_CYCLES)
                {
                    key_long_press = 1;
                    printf("Long Press!!! cycle = %d\n", key_cycle_count);
                }
                key_debounce_cycle_count = 0;
                debounce_state           = STATE_KEY_RELEASE_DEBOUNCE;
                printf("STATE_KEY_RELEASE_DEBOUNCE\n");
            }
            break;
        case STATE_KEY_RELEASE_DEBOUNCE:
            ++key_debounce_cycle_count;
            if (key_state == KEY_UP)  // still released
            {
                if (key_debounce_cycle_count >= DEBOUNCE_STABLE_CYCLES)
                {
                    key_event      = key_long_press ? KEY_LONG_PRESS_RELEASE : KEY_RELEASE;
                    debounce_state = STATE_WAIT_FOR_KEY_PRESS;
                    printf("STATE_WAIT_FOR_KEY_PRESS\n");
                }
            }
            else
            {
                debounce_state = STATE_WAIT_FOR_KEY_RELEASE;
                printf("STATE_WAIT_FOR_KEY_RELEASE\n");
            }
            break;
    }

    Delay_Ms(DEBOUNCE_INTERVAL);

    return key_event;
}

int main()
{
    SystemInit();
    // Delay_Ms(100);

    funGpioInitAll();

    // Power on latch
    funPinMode(button_pin, GPIO_CFGLR_IN_PUPD);
    funDigitalWrite(button_pin, FUN_HIGH);  // Input pull-up

    // Init LED pin
    funPinMode(power_led_pin, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
    funDigitalWrite(power_led_pin, FUN_LOW);

    // Init TIM1 for PWM
    tim1_pwm_init();

    // If the key is held after power on, wait until it is released.
    // TODO: There should be a more elegant way...
    if (funDigitalRead(button_pin) == FUN_LOW)
    {
        while (1)
        {
            if (funDigitalRead(button_pin) == FUN_HIGH)
            {
                Delay_Ms(5);
                if (funDigitalRead(button_pin) == FUN_HIGH)
                {
                    break;
                }
            }
            Delay_Ms(5);
        }
    }

    while (1)
    {
        uint8_t key_event = read_button_state();
        if (key_event == KEY_LONG_PRESS_RELEASE)
        {
            funDigitalWrite(button_pin, FUN_LOW);  // Input pull-down
        }
        else if (key_event == KEY_RELEASE)
        {
            printf("Key Released.");

            pwm_state++;
            pwm_state %= 7;

            // process PWM
            switch (pwm_state)
            {
                case 0:
                    TIM1->CH4CVR = 15;  // 10% duty cycle
                    disable_systick();
                    break;
                case 1:
                    TIM1->CH4CVR = 75;  // 50% duty cycle
                    disable_systick();
                    break;
                case 2:
                    TIM1->CH4CVR = 150;  // 100% duty cycle
                    disable_systick();
                    break;
                case 3:
                    pwm_blink_state     = FUN_HIGH;
                    TIM1->CH4CVR        = 0;     // 0% duty cycle
                    pwm_update_delay_ms = 1000;  // Reset blink delay
                    systick_init();              // Re-initiate systick, change LED blink interval.
                    break;
                case 4:
                    pwm_blink_state     = FUN_HIGH;
                    TIM1->CH4CVR        = 0;    // 0% duty cycle
                    pwm_update_delay_ms = 500;  // Reset blink delay
                    systick_init();             // Re-initiate systick, change LED blink interval.
                    break;
                case 5:
                    pwm_blink_state     = FUN_HIGH;
                    TIM1->CH4CVR        = 0;    // 0% duty cycle
                    pwm_update_delay_ms = 100;  // Reset blink delay
                    systick_init();             // Re-initiate systick, change LED blink interval.
                    break;
                case 6:
                    TIM1->CH4CVR        = 0;  // 0% duty cycle
                    pwm_dim_cycles      = 0;
                    pwm_dim_cycle_delta = 1;
                    pwm_update_delay_ms = 10;  // Reset blink delay
                    systick_init();            // Re-initiate systick, change LED blink interval.
                    break;
            }
        }
    }
}
