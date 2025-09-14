/*
 * CH32V003 SGM3732 Portable LED Light
 *
 * Aug 2025 by Li Mingjie
 *  - Email:  limingjie@outlook.com
 *  - GitHub: https://github.com/limingjie/
 */

#include <stdio.h>
#include <stdlib.h>
#include "ch32fun.h"
#include "button.h"

#define PIN_POWER_LED    PC1  // Power LED pin
#define PIN_LATCH        PC2  // Latch pin
#define PIN_MODE_BUTTON  PC2  // Mode button pin, same as latch pin
#define PIN_LEVEL_BUTTON PA2  // Set button pin
#define PIN_PWM          PC4  // PWM output pin

#define POWER_MONITORING_CYCLES     1000  // Every 5 seconds - button_debounce() = 5ms
#define POWER_VOLT_DIV_R_UP         2     // 22k or 10k   | 2:3 voltage divider
#define POWER_VOLT_DIV_R_DOWN       3     // 33k or 15k   | 5.5V / 5 x 3 = 3.3V
#define POWER_LOW_VOLT_THRESHOLD_MV 3000  // 3.0V
#define POWER_LOW_COUNT_THRESHOLD   3     // 3 times

#define PWM_FREQUENCY              ((uint32_t)5 * 1000)                         // 5kHz
#define PWM_CLOCKS_FULL_DUTY_CYCLE (FUNCONF_SYSTEM_CORE_CLOCK / PWM_FREQUENCY)  // 100% duty cycle
#define PWM_CLOCKS_ZERO_DUTY_CYCLE 0                                            // 0% duty cycle
#define PWM_SEQUENCE_ON            1
#define PWM_SEQUENCE_OFF           0
#define PWM_SEQUENCE_INCREASE      1
#define PWM_SEQUENCE_DECREASE      0

#define MORSE_CODE_DIT_DURATION_MS 150  // 100ms

// #define printf(...) (void)0  // Disable printf to save space

enum light_modes
{
    MODE_ON,
    MODE_DIMMING,
    MODE_BLINK,
    MODE_SOS,
    MODE_OFF
};

const char *mode_names[] = {"MODE_ON", "MODE_DIMMING", "MODE_BLINK", "MODE_SOS"};

uint8_t  current_mode           = 0;     // 5 modes: brightness, blink, dimming, sos, off
uint8_t  current_level          = 0;     // 0-7 levels of brightness, blink speed, dimming speed.
uint16_t current_pwm_duty_cycle = 0;     // Lights off
uint32_t pwm_update_interval_ms = 1000;  // systick interval in ms, set before initialize systick.
uint8_t  pwm_sequence           = 0;     // 0 - off / decrease; 1 - on / increase; 0-31 - sos sequence

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
    SysTick->CMP += FUNCONF_SYSTEM_CORE_CLOCK / 1000 * pwm_update_interval_ms;
    SysTick->SR = 0;

    // process mode and level changes
    switch (current_mode)
    {
        case MODE_DIMMING:
            TIM1->CH4CVR = PWM_CLOCKS_FULL_DUTY_CYCLE * current_pwm_duty_cycle / 100;
            (pwm_sequence == PWM_SEQUENCE_INCREASE) ? current_pwm_duty_cycle++ : current_pwm_duty_cycle--;
            if (current_pwm_duty_cycle >= 100 || current_pwm_duty_cycle <= 0)
            {
                pwm_sequence = !pwm_sequence;
            }
            break;
        case MODE_BLINK:
            TIM1->CH4CVR = (pwm_sequence == PWM_SEQUENCE_ON) ? PWM_CLOCKS_FULL_DUTY_CYCLE : PWM_CLOCKS_ZERO_DUTY_CYCLE;
            pwm_sequence = !pwm_sequence;
            break;
        case MODE_SOS:
            // From Wikipedia:
            // > The duration of a dah is three times the duration of a dit. Each dit or dah within an encoded character
            // > is followed by a period of signal absence, called a space, equal to the dit duration.
            // SOS Morse Code is the following sequence under a fixed systick.
            //   Morse Code - . . . --- --- --- . . . <-Wait->  | SOS   Time   = 0.15s x 24 = 3.6s
            //       Binary - 10101011101110111010101000000000  | Pause Time   = 0.15s x  8 = 1.2s
            //                |----+----|----+----|----+----|-  | Total Time   = 0.15s x 32 = 4.8s
            //     Sequence - 0    5   10   15   20   25   30   | No. Sequence = 32
            //           On - 0 2 4 678 012 456 8 0 2           | On  Sequence = 15
            //          Off -  1 3 5   9   3   7 9 1 345678901  | Off Sequence = 17
            TIM1->CH4CVR = (pwm_sequence >= 23 ||
                            ((pwm_sequence & 0x1) && pwm_sequence != 7 && pwm_sequence != 11 && pwm_sequence != 15))
                               ? PWM_CLOCKS_ZERO_DUTY_CYCLE
                               : PWM_CLOCKS_FULL_DUTY_CYCLE;

            pwm_update_interval_ms = MORSE_CODE_DIT_DURATION_MS;

            pwm_sequence++;
            pwm_sequence &= 0x1F;  // pwm_sequence %= 32;
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
    TIM1->ATRLR = PWM_CLOCKS_FULL_DUTY_CYCLE - 1;

    // Reload immediately
    TIM1->SWEVGR |= TIM_UG;

    // Enable CH4 output, positive pol
    TIM1->CCER |= TIM_CC4E | TIM_CC4NP;

    // CH2 Mode is output, PWM1 (CC1S = 00, OC1M = 110)
    TIM1->CHCTLR2 |= TIM_OC4M_2 | TIM_OC4M_1;

    // Set the Capture Compare Register value to 0% initially
    TIM1->CH4CVR = PWM_CLOCKS_ZERO_DUTY_CYCLE;

    // Enable TIM1 outputs
    TIM1->BDTR |= TIM_MOE;

    // Enable TIM1
    TIM1->CTLR1 |= TIM_CEN;
}

void blink_power_led(uint8_t times)
{
    for (uint8_t i = 0; i < times; i++)
    {
        funDigitalWrite(PIN_POWER_LED, FUN_HIGH);  // Turn on LED
        Delay_Ms(100);
        funDigitalWrite(PIN_POWER_LED, FUN_LOW);  // Turn off LED
        Delay_Ms(100);
    }
}

void power_monitor(void)
{
    static uint8_t power_low_count = 0;
    uint16_t       adc_ref         = 0;  // Internal reference 1.2V
    uint16_t       adc_mon         = 0;
    uint32_t       adc_volt_mv;
    uint32_t       power_volt_mv;

    // Average 8 samples for better stability
    for (uint8_t i = 0; i < 8; i++)
    {
        adc_ref += funAnalogRead(ANALOG_8);  // Internal reference 1.2V
        adc_mon += funAnalogRead(ANALOG_6);  // A6 (PD6)
    }
    adc_ref /= 8;
    adc_mon /= 8;

    adc_volt_mv   = (uint32_t)adc_mon * 1200 / adc_ref;
    power_volt_mv = adc_volt_mv * (POWER_VOLT_DIV_R_UP + POWER_VOLT_DIV_R_DOWN) / POWER_VOLT_DIV_R_DOWN;

    printf("Vref: 1.2 V (%d) | Vadc: %ld mV (%d) | Vpower: %ld mV\n", adc_ref, adc_volt_mv, adc_mon, power_volt_mv);

    if (power_volt_mv < POWER_LOW_VOLT_THRESHOLD_MV)
    {
        if (++power_low_count >= POWER_LOW_COUNT_THRESHOLD)
        {
            printf("Battery too low! Powering off...\n");
            blink_power_led(10);
            funDigitalWrite(PIN_LATCH, FUN_LOW);  // Input pull-down
            // For debugging purpose only, code should not reach here if correctly shutdown.
            power_low_count = 0;
        }
    }
    else
    {
        power_low_count = 0;
    }
}

void update_led(void)
{
    printf("Change to %s, level %d\n", mode_names[current_mode], current_level);

    disable_systick();

    switch (current_mode)
    {
        case MODE_ON:
            TIM1->CH4CVR = (PWM_CLOCKS_FULL_DUTY_CYCLE * (8 - current_level)) >> 3;  // 12.5, 25%, 37.5%, ..., 100%
            break;
        case MODE_DIMMING:
            pwm_sequence           = PWM_SEQUENCE_DECREASE;     // Starts by decreasing brightness
            pwm_update_interval_ms = (current_level + 1) << 1;  // 2ms, 4ms, 6ms, ..., 16ms
            current_pwm_duty_cycle = 100;                       // 100% duty cycle
            systick_init();
            break;
        case MODE_BLINK:
            pwm_sequence           = PWM_SEQUENCE_ON;
            pwm_update_interval_ms = (current_level + 3) << 5;  // 96ms, 128ms, 160ms, ..., 320ms
            systick_init();
            break;
        case MODE_SOS:
            pwm_sequence           = 0;                           // Reset sequence
            TIM1->CH4CVR           = PWM_CLOCKS_ZERO_DUTY_CYCLE;  // Pause before SOS
            pwm_update_interval_ms = MORSE_CODE_DIT_DURATION_MS;
            systick_init();
            break;
        case MODE_OFF:
            TIM1->CH4CVR = PWM_CLOCKS_ZERO_DUTY_CYCLE;
            break;
    }
}

int main(void)
{
    SystemInit();
    Delay_Ms(50);

    funGpioInitAll();

    // Power on latch by input pull-up
    funPinMode(PIN_LATCH, GPIO_CFGLR_IN_PUPD);
    funDigitalWrite(PIN_LATCH, FUN_HIGH);

    // Init LED pin
    funPinMode(PIN_POWER_LED, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
    funDigitalWrite(PIN_POWER_LED, FUN_LOW);

    // Init ADC for battery voltage monitoring
    funAnalogInit();
    funPinMode(PD6, GPIO_CFGLR_IN_ANALOG);

    // Init TIM1 for PWM
    tim1_pwm_init();
    // TIM1->CH4CVR = PWM_CLOCKS_FULL_DUTY_CYCLE;  // Starts with MODE_ON - 100% brightness
    update_led();

    button_t mode_button;
    init_button(&mode_button, PIN_MODE_BUTTON);
    button_t level_button;
    init_button(&level_button, PIN_LEVEL_BUTTON);

    uint16_t power_monitoring_cycles = 0;
    while (1)
    {
        switch (get_button_event(&mode_button))
        {
            case BUTTON_HOLD:  // Enter SOS mode directly
                if (current_mode != MODE_SOS)
                {
                    current_mode = MODE_SOS;
                    update_led();
                }
                break;
            case BUTTON_RELEASED:  // Light mode +
                // printf("Mode button released.\n");
                // Move to next mode
                current_mode++;
                if (current_mode == MODE_OFF)
                {
                    // printf("Powering off...\n");
                    funDigitalWrite(PIN_LATCH, FUN_LOW);  // Input pull-down

                    // These following lines are for debugging purpose only, code should not reach here if correctly
                    // shutdown. However, if the MCU is powered by WCH-LinkE, the code will keep running. And, the
                    // button processing logic would detect the power off pull down as a button down, then there would
                    // be endless BUTTON_HOLD events.
                    Delay_Ms(100);
                    funDigitalWrite(PIN_LATCH, FUN_HIGH);  // Input pull-up
                    current_mode  = MODE_ON;
                    current_level = 0;
                    update_led();
                    break;
                }

                current_level = 0;  // Reset setting to max
                update_led();
                break;
            case BUTTON_DOUBLE_PRESS_RELEASED:  // Light mode -
            case BUTTON_TRIPLE_PRESS_RELEASED:
            case BUTTON_MORE_PRESS_RELEASED:
                if (current_mode > MODE_ON)
                {
                    current_mode--;
                    current_level = 0;  // Reset setting to max
                    update_led();
                }
                break;
        }

        switch (get_button_event(&level_button))
        {
            case BUTTON_RELEASED:  // Light setting +
                // printf("Set button released.\n");
                if (current_mode != MODE_SOS)  // Do not interfere with SOS mode
                {
                    current_level++;
                    current_level %= 8;  // 0 - 7
                    update_led();
                }
                break;
            case BUTTON_DOUBLE_PRESS_RELEASED:  // Light setting -
            case BUTTON_TRIPLE_PRESS_RELEASED:
            case BUTTON_MORE_PRESS_RELEASED:
                // printf("Set button double press released.\n");
                if (current_mode != MODE_SOS)  // Do not interfere with SOS mode
                {
                    if (current_level > 0)
                    {
                        current_level--;
                    }
                    else
                    {
                        current_level = 7;
                    }
                    update_led();
                }
                break;
            case BUTTON_HOLD:  // Change light setting to min or max
                // printf("Set button hold.\n");
                if (current_mode != MODE_SOS)  // Do not interfere with SOS mode
                {
                    current_level = (current_level & 0x4) ? 0 : 7;  // 0/1/2/3 -> 7; 4/5/6/7 -> 0
                    update_led();
                }
                break;
        }

        if (current_mode != MODE_SOS)  // In an SOS, do not check power to avoid interference
        {
            if (++power_monitoring_cycles >= POWER_MONITORING_CYCLES)
            {
                power_monitor();
                power_monitoring_cycles = 0;
            }
        }

        debounce_delay();
    }
}
