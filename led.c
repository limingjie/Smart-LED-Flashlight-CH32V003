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

#define POWER_MONITORING_CYCLES 1000  // Every 5 seconds - button_debounce() = 5ms
#define POWER_VOLTDIV_R_UP      100   // 10k    | 10k + 14.7k voltage divider
#define POWER_VOLTDIV_R_DOWN    147   // 14.7k  | 5.5V / 24.7 x 14.7 = 3.27V
#define POWER_LOW_VOLT_THLD_MV  3000  // 3.0V
#define POWER_LOW_COUNT_THLD    3     // 3 times

#define PWM_FREQUENCY              ((uint32_t)5 * 1000)                         // 5kHz
#define PWM_CLOCKS_FULL_DUTY_CYCLE (FUNCONF_SYSTEM_CORE_CLOCK / PWM_FREQUENCY)  // 100% duty cycle
#define PWM_CLOCKS_ZERO_DUTY_CYCLE 0                                            // 0% duty cycle
#define PWM_MODE_ON                1
#define PWM_MODE_OFF               0
#define PWM_MODE_INCREASE          1
#define PWM_MODE_DECREASE          0

#define MODE_BRIGHTNESS 0
#define MODE_BLINK      1
#define MODE_DIMMING    2
#define MODE_SOS        3
#define MODE_OFF        4

uint8_t  current_mode               = 0;     // 5 modes: brightness, blink, dimming, sos, off
uint8_t  current_level              = 10;    // 1 - 10 levels of brightness, blink speed, dimming speed
uint16_t current_pwm_duty_cycle_pct = 0;     // Lights off
uint32_t pwm_update_interval_ms     = 1000;  // systick interval in ms, set before initialize systick.
uint8_t  pwm_mode                   = 0;     // 0 - off / decrease; 1 - on / increase
uint8_t  SOS_sequence               = 0;

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

    // process light modes and settings
    switch (current_mode)
    {
        case MODE_BLINK:  // blink
            TIM1->CH4CVR = (pwm_mode == PWM_MODE_ON) ? PWM_CLOCKS_FULL_DUTY_CYCLE : PWM_CLOCKS_ZERO_DUTY_CYCLE;
            pwm_mode     = !pwm_mode;
            break;
        case MODE_DIMMING:  // dimming
            TIM1->CH4CVR = PWM_CLOCKS_FULL_DUTY_CYCLE * current_pwm_duty_cycle_pct / 100;
            (pwm_mode == PWM_MODE_INCREASE) ? current_pwm_duty_cycle_pct++ : current_pwm_duty_cycle_pct--;
            if (current_pwm_duty_cycle_pct >= 100 || current_pwm_duty_cycle_pct <= 0)
            {
                pwm_mode = !pwm_mode;
            }
            break;
        case MODE_SOS:
            // The duration of a dah is three times the duration of a dit. Each dit or dah within an encoded character
            // is followed by a period of signal absence, called a space, equal to the dit duration.
            // So, with a fixed cycle, SOS Morse Code is the following sequence of on and off.
            //   Morse Code - . . . --- --- --- . . .[3s wait]
            //       Binary - 10101011101110111010101000000000
            //         Time - |----+----|----+----|----+----|-
            //           On - 0 2 4 678 012 456 8 0 2
            //          Off -  1 3 5   9   3   7 9 1 345678901
            if (SOS_sequence >= 23 ||
                (SOS_sequence % 2 && SOS_sequence != 7 && SOS_sequence != 11 && SOS_sequence != 15))
            {
                TIM1->CH4CVR = PWM_CLOCKS_ZERO_DUTY_CYCLE;
            }
            else
            {
                TIM1->CH4CVR = PWM_CLOCKS_FULL_DUTY_CYCLE;
            }

            if (SOS_sequence++ > 31)
            {
                SOS_sequence = 0;
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
    power_volt_mv = adc_volt_mv * (POWER_VOLTDIV_R_UP + POWER_VOLTDIV_R_DOWN) / POWER_VOLTDIV_R_DOWN;

    printf("Vref: 1.2 V (%d) | Vadc: %ld mV (%d) | Vpower: %ld mV\n", adc_ref, adc_volt_mv, adc_mon, power_volt_mv);

    if (power_volt_mv < POWER_LOW_VOLT_THLD_MV)
    {
        if (++power_low_count >= POWER_LOW_COUNT_THLD)
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
    printf("Change to mode %d, level %d\n", current_mode, current_level);

    disable_systick();

    switch (current_mode)
    {
        case MODE_BRIGHTNESS:
            TIM1->CH4CVR = PWM_CLOCKS_FULL_DUTY_CYCLE / 10 * current_level;  // 10, 20, ..., 100%
            break;
        case MODE_BLINK:
            pwm_mode               = PWM_MODE_ON;
            pwm_update_interval_ms = current_level * 50;  // 50ms, 100ms, 150ms, ..., 500ms
            systick_init();
            break;
        case MODE_DIMMING:
            pwm_mode                   = PWM_MODE_DECREASE;  // Starts by decreasing brightness
            pwm_update_interval_ms     = current_level * 2;
            current_pwm_duty_cycle_pct = 100;  // Current brightness level = 100
            systick_init();
            break;
        case MODE_SOS:
            SOS_sequence           = 0;
            TIM1->CH4CVR           = PWM_CLOCKS_ZERO_DUTY_CYCLE;
            pwm_update_interval_ms = 333;
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

    // Run first power monitoring on startup
    power_monitor();

    // Init TIM1 for PWM
    tim1_pwm_init();
    TIM1->CH4CVR = PWM_CLOCKS_FULL_DUTY_CYCLE;  // Starts with mode 0 - 100% brightness

    // Init buttons
    // funPinMode(PIN_MODE_BUTTON, GPIO_CFGLR_IN_PUPD); // Share the same pin as latch, already set above.
    // funDigitalWrite(PIN_MODE_BUTTON, FUN_HIGH);
    funPinMode(PIN_LEVEL_BUTTON, GPIO_CFGLR_IN_PUPD);
    funDigitalWrite(PIN_LEVEL_BUTTON, FUN_HIGH);

    button_t mode_button;
    init_button(&mode_button, PIN_MODE_BUTTON);
    button_t level_button;
    init_button(&level_button, PIN_LEVEL_BUTTON);

    uint16_t power_monitoring_cycles = 0;
    while (1)
    {
        if (++power_monitoring_cycles >= POWER_MONITORING_CYCLES)
        {
            power_monitor();
            power_monitoring_cycles = 0;
        }

        switch (get_button_event(&mode_button))
        {
            case BUTTON_HOLD:  // Turn off lights
                printf("Turn off LEDs.\n");
                disable_systick();
                current_mode  = MODE_OFF;
                current_level = 0;
                update_led();
                break;
            case BUTTON_HOLD_RELEASED:  // Power off the entire circuit
                printf("Powering off...\n");
                funDigitalWrite(PIN_LATCH, FUN_LOW);  // Input pull-down

                // These 2 lines are for debugging purpose only, code should not reach here if correctly shutdown.
                // However, if the MCU is powered by WCH-LinkE, the code will keep running. Then, the key event loop
                // would detect the power off pull down as a key down, then there would be endless BUTTON_HOLD events.
                Delay_Ms(100);
                funDigitalWrite(PIN_LATCH, FUN_HIGH);  // Input pull-up
                current_mode  = MODE_BRIGHTNESS;
                current_level = 10;
                update_led();
                break;
            case BUTTON_RELEASED:  // Change light mode
                printf("Mode button released.\n");

                // Move to next mode
                current_mode++;
                current_mode %= 4;
                current_level = 10;  // Reset setting to max
                update_led();
                break;
        }

        switch (get_button_event(&level_button))
        {
            case BUTTON_RELEASED:  // Change light setting
                printf("Set button released.\n");
                current_level--;
                if (current_level <= 0 || current_level > 10)
                {
                    current_level = 10;  // Reset to max
                }

                update_led();
                break;
            case BUTTON_HOLD:  // Change light setting to min or max
                printf("Set button hold.\n");
                current_level = (current_level > 5) ? 1 : 10;
                update_led();
                break;
        }

        button_debounce();
    }
}
