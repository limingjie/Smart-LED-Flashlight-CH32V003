/*
 * CH32V003 SGM3732 Portable LED Light
 *
 * Aug 2025 by Li Mingjie
 *  - Email:  limingjie@outlook.com
 *  - GitHub: https://github.com/limingjie/
 */

#include <stdio.h>
#include "ch32fun.h"

uint8_t  power_led_pin                  = PC1;  // Power LED pin
uint8_t  button_and_latch_pin           = PC2;  // Latch and button pin
uint8_t  pwm_pin                        = PC4;  // PWM output pin
uint8_t  current_light_mode             = 0;    // 7 modes: 10%, 50%, 100%, 0.5Hz blink, 1Hz blink, 5Hz blink, 3s breath
uint32_t pwm_update_interval_ms         = 500;  // PWM update interval in ms
uint16_t pwm_dimming_current_duty_cycle = 0;
uint8_t  pwm_state                      = 0;  // 0 - off / decrease; 1 - on / increase

#define PWM_FREQUENCY              ((uint32_t)5 * 1000)                         // 5kHz
#define PWM_DUTY_CYCLE_100_PERCENT (FUNCONF_SYSTEM_CORE_CLOCK / PWM_FREQUENCY)  // 100% duty cycle
#define PWM_DUTY_CYCLE_50_PERCENT  (PWM_DUTY_CYCLE_100_PERCENT / 2)             // 50% duty cycle
#define PWM_DUTY_CYCLE_10_PERCENT  (PWM_DUTY_CYCLE_100_PERCENT / 10)            // 10% duty cycle

#define KEY_DEBOUNCE_INTERVAL      5    // 5ms
#define KEY_DEBOUNCE_STABLE_CYCLES 5    // 5ms x 5 = 25ms
#define KEY_LONG_PRESS_CYCLES      300  // 5ms x 300 = 1500ms

#define BATTERY_MONITOR_CYCLES           (1000 / KEY_DEBOUNCE_INTERVAL)
#define BATTERY_LOW_VOLTAGE_THRESHOLD_MV 3000  // 3.0V
#define BATTERY_LOW_COUNT_FOR_SHUTDOWN   3     // 3 times

void systick_init(void)
{
    SysTick->CTLR = 0;
    NVIC_EnableIRQ(SysTicK_IRQn);
    SysTick->CMP  = FUNCONF_SYSTEM_CORE_CLOCK / 1000 * pwm_update_interval_ms;
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
    SysTick->CMP += FUNCONF_SYSTEM_CORE_CLOCK / 1000 * pwm_update_interval_ms;
    SysTick->SR = 0;

    // process PWM
    switch (current_light_mode)
    {
        case 3:  // 0.5Hz blink
        case 4:  // 1Hz blink
        case 5:  // 5Hz blink
            TIM1->CH4CVR = pwm_state ? PWM_DUTY_CYCLE_100_PERCENT : 0;
            pwm_state    = !pwm_state;
            break;
        case 6:  // Dimming
            TIM1->CH4CVR = PWM_DUTY_CYCLE_100_PERCENT * pwm_dimming_current_duty_cycle / 100;
            pwm_dimming_current_duty_cycle += pwm_state ? 1 : -1;
            if (pwm_dimming_current_duty_cycle >= 100 || pwm_dimming_current_duty_cycle <= 0)
            {
                pwm_state = !pwm_state;
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
    TIM1->ATRLR = PWM_DUTY_CYCLE_100_PERCENT - 1;

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

uint8_t read_button_state(void)
{
    static uint8_t  debounce_state           = STATE_WAIT_FOR_KEY_PRESS;
    static uint8_t  key_long_press           = 0;
    static uint16_t key_cycle_count          = 0;
    static uint16_t key_debounce_cycle_count = 0;

    uint8_t key_event = KEY_NONE;
    uint8_t key_state = (funDigitalRead(button_and_latch_pin) == FUN_LOW) ? KEY_DOWN : KEY_UP;
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
                if (key_debounce_cycle_count >= KEY_DEBOUNCE_STABLE_CYCLES)
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
                if (key_cycle_count >= KEY_LONG_PRESS_CYCLES)
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
                if (key_debounce_cycle_count >= KEY_DEBOUNCE_STABLE_CYCLES)
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

    Delay_Ms(KEY_DEBOUNCE_INTERVAL);

    return key_event;
}

int main()
{
    SystemInit();
    // Delay_Ms(100);

    funGpioInitAll();

    // Power on latch
    funPinMode(button_and_latch_pin, GPIO_CFGLR_IN_PUPD);
    funDigitalWrite(button_and_latch_pin, FUN_HIGH);  // Input pull-up

    // Init LED pin
    funPinMode(power_led_pin, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
    funDigitalWrite(power_led_pin, FUN_LOW);

    // Init ADC for battery voltage monitoring
    funAnalogInit();
    funPinMode(PD6, GPIO_CFGLR_IN_ANALOG);

    // Init TIM1 for PWM
    tim1_pwm_init();
    // TIM1->CH4CVR = PWM_DUTY_CYCLE_10_PERCENT;  // 10% brightness

    // If the key is held after power on, wait until it is released.
    // TODO: There should be a more elegant way...
    if (funDigitalRead(button_and_latch_pin) == FUN_LOW)
    {
        while (1)
        {
            if (funDigitalRead(button_and_latch_pin) == FUN_HIGH)
            {
                Delay_Ms(5);
                if (funDigitalRead(button_and_latch_pin) == FUN_HIGH)
                {
                    break;
                }
            }
            Delay_Ms(5);
        }
    }

    uint16_t battery_monitor_cycle_count = 0;
    uint8_t  battery_low_count           = 0;

    while (1)
    {
        battery_monitor_cycle_count++;
        if (battery_monitor_cycle_count >= BATTERY_MONITOR_CYCLES)
        {
            battery_monitor_cycle_count = 0;

            uint16_t v_ref  = funAnalogRead(ANALOG_8);  // Internal reference 1.2V
            uint16_t v_adc6 = funAnalogRead(ANALOG_6);  // PD6

            uint16_t battery_voltage_mv = (uint16_t)((uint32_t)v_adc6 * 1200 / v_ref * (9786 + 9961) / 9786);

            printf("V_ref: 1.2 V (%d), V_A7: %d mV (%d)\n", v_ref, battery_voltage_mv, v_adc6);

            if (battery_voltage_mv < BATTERY_LOW_VOLTAGE_THRESHOLD_MV)
            {
                if (++battery_low_count >= BATTERY_LOW_COUNT_FOR_SHUTDOWN)
                {
                    for (uint8_t i = 0; i < 10; i++)
                    {
                        funDigitalWrite(power_led_pin, FUN_HIGH);  // Turn on LED
                        Delay_Ms(100);
                        funDigitalWrite(power_led_pin, FUN_LOW);  // Turn off LED
                        Delay_Ms(100);
                    }

                    printf("Battery too low! Powering off...\n");
                    funDigitalWrite(button_and_latch_pin, FUN_LOW);  // Input pull-down
                }
            }
            else
            {
                battery_low_count = 0;
            }
        }

        uint8_t key_event = read_button_state();
        if (key_event == KEY_LONG_PRESS_RELEASE)
        {
            funDigitalWrite(button_and_latch_pin, FUN_LOW);  // Input pull-down
        }
        else if (key_event == KEY_RELEASE)
        {
            printf("Key Released.");

            current_light_mode++;
            current_light_mode %= 7;

            // process PWM
            switch (current_light_mode)
            {
                case 0:
                    TIM1->CH4CVR = PWM_DUTY_CYCLE_10_PERCENT;  // 10% brightness
                    disable_systick();
                    break;
                case 1:
                    TIM1->CH4CVR = PWM_DUTY_CYCLE_50_PERCENT;  // 50% brightness
                    disable_systick();
                    break;
                case 2:
                    TIM1->CH4CVR = PWM_DUTY_CYCLE_100_PERCENT;  // 100% brightness
                    disable_systick();
                    break;
                case 3:
                    pwm_state              = FUN_HIGH;
                    TIM1->CH4CVR           = PWM_DUTY_CYCLE_10_PERCENT;  // 0% brightness
                    pwm_update_interval_ms = 1000;                       // Reset update interval
                    pwm_state              = 0;                          // Starts off
                    systick_init();                                      // Re-initiate systick, apply updated interval.
                    break;
                case 4:
                    pwm_state              = FUN_HIGH;
                    TIM1->CH4CVR           = PWM_DUTY_CYCLE_10_PERCENT;  // 0% brightness
                    pwm_update_interval_ms = 500;                        // Reset update interval
                    pwm_state              = 0;                          // Starts off
                    systick_init();                                      // Re-initiate systick, apply updated interval.
                    break;
                case 5:
                    pwm_state              = FUN_HIGH;
                    TIM1->CH4CVR           = PWM_DUTY_CYCLE_10_PERCENT;  // 0% brightness
                    pwm_update_interval_ms = 100;                        // Reset update interval
                    pwm_state              = 0;                          // Starts off
                    systick_init();                                      // Re-initiate systick, apply updated interval.
                    break;
                case 6:
                    TIM1->CH4CVR                   = 0;   // 0% brightness
                    pwm_dimming_current_duty_cycle = 0;   // Current brightness level = 0
                    pwm_state                      = 1;   // Starts by increasing brightness
                    pwm_update_interval_ms         = 15;  // Brightness update interval
                    systick_init();                       // Re-initiate systick, apply updated interval.
                    break;
            }
        }
    }
}
