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

#define PIN_POWER_LED   PC1  // Power LED pin
#define PIN_LATCH       PC2  // Latch pin
#define PIN_MODE_BUTTON PC2  // Mode button pin, same as latch pin
#define PIN_SET_BUTTON  PA2  // Set button pin
#define PIN_PWM         PC4  // PWM output pin

#define POWER_MONITORING_CYCLES (5000 / BTN_DEBOUNCE_INTERVAL_MS)  // Every 5 seconds
#define POWER_VOLTDIV_R_UP      100                                // 10k    | 10k + 14.7k voltage divider
#define POWER_VOLTDIV_R_DOWN    147                                // 14.7k  | 5.5V / 24.7 x 14.7 = 3.27V
#define POWER_LOW_VOLT_THLD_MV  3000                               // 3.0V
#define POWER_LOW_COUNT_THLD    3                                  // 3 times

#define PWM_FREQUENCY                ((uint32_t)5 * 1000)                         // 5kHz
#define PWM_TICKS_100_PCT_DUTY_CYCLE (FUNCONF_SYSTEM_CORE_CLOCK / PWM_FREQUENCY)  // 100% duty cycle
#define PWM_TICKS_0_PCT_DUTY_CYCLE   0                                            // 0% duty cycle
#define PWM_MODE_ON                  1
#define PWM_MODE_OFF                 0
#define PWM_MODE_INCREASE            1
#define PWM_MODE_DECREASE            0

uint8_t  current_light_mode         = 0;     // 3 modes: brightness, blink, dimming
uint8_t  current_light_setting      = 10;    // 1 - 10 levels of brightness, blink speed, dimming speed
uint16_t current_pwm_duty_cycle_pct = 0;     // Lights off
uint32_t pwm_update_interval_ms     = 1000;  // systick interval in ms, set before initialize systick.
uint8_t  pwm_mode                   = 0;     // 0 - off / decrease; 1 - on / increase

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
    switch (current_light_mode)
    {
        case 1:  // blink
            TIM1->CH4CVR = (pwm_mode == PWM_MODE_ON) ? PWM_TICKS_100_PCT_DUTY_CYCLE : PWM_TICKS_0_PCT_DUTY_CYCLE;
            pwm_mode     = !pwm_mode;
            break;
        case 2:  // dimming
            TIM1->CH4CVR = PWM_TICKS_100_PCT_DUTY_CYCLE * current_pwm_duty_cycle_pct / 100;
            (pwm_mode == PWM_MODE_INCREASE) ? current_pwm_duty_cycle_pct++ : current_pwm_duty_cycle_pct--;
            if (current_pwm_duty_cycle_pct >= 100 || current_pwm_duty_cycle_pct <= 0)
            {
                pwm_mode = !pwm_mode;
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
    TIM1->ATRLR = PWM_TICKS_100_PCT_DUTY_CYCLE - 1;

    // Reload immediately
    TIM1->SWEVGR |= TIM_UG;

    // Enable CH4 output, positive pol
    TIM1->CCER |= TIM_CC4E | TIM_CC4NP;

    // CH2 Mode is output, PWM1 (CC1S = 00, OC1M = 110)
    TIM1->CHCTLR2 |= TIM_OC4M_2 | TIM_OC4M_1;

    // Set the Capture Compare Register value to 0% initially
    TIM1->CH4CVR = PWM_TICKS_0_PCT_DUTY_CYCLE;

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

void change_light_setting(uint8_t mode, uint8_t setting)
{
    printf("Change to mode %d, setting %d\n", mode, setting);
    disable_systick();  // Disable systick during mode change to avoid interference

    switch (mode)
    {
        case 0:
            TIM1->CH4CVR = PWM_TICKS_100_PCT_DUTY_CYCLE / 10 * setting;  // 10, 20, ..., 100%
            break;
        case 1:
            pwm_mode = PWM_MODE_ON;
            // TIM1->CH4CVR           = PWM_TICKS_100_PCT_DUTY_CYCLE;  // 100% brightness
            pwm_update_interval_ms = setting * 50;  // 50ms, 100ms, 150ms, ..., 500ms
            systick_init();                         // Re-initiate systick, apply updated interval.
            break;
        case 2:
            pwm_mode = PWM_MODE_DECREASE;  // Starts by decreasing brightness
            // TIM1->CH4CVR               = PWM_TICKS_100_PCT_DUTY_CYCLE;  // 100% brightness
            current_pwm_duty_cycle_pct = 100;  // Current brightness level = 100
            pwm_update_interval_ms     = setting * 2;
            systick_init();  // Re-initiate systick, apply updated interval.
            break;
    }
}

int main()
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
    TIM1->CH4CVR = PWM_TICKS_100_PCT_DUTY_CYCLE;  // Starts with mode 0 - 100% brightness

    // Init buttons
    // funPinMode(PIN_MODE_BUTTON, GPIO_CFGLR_IN_PUPD); // Share the same pin as latch, already set above.
    // funDigitalWrite(PIN_MODE_BUTTON, FUN_HIGH);
    funPinMode(PIN_SET_BUTTON, GPIO_CFGLR_IN_PUPD);
    funDigitalWrite(PIN_SET_BUTTON, FUN_HIGH);

    button_t mode_button;
    button_t set_button;
    init_button(&mode_button, PIN_MODE_BUTTON);
    init_button(&set_button, PIN_SET_BUTTON);

    uint16_t power_monitoring_cycles = 0;
    uint8_t  button_event;
    while (1)
    {
        if (++power_monitoring_cycles >= POWER_MONITORING_CYCLES)
        {
            power_monitor();
            power_monitoring_cycles = 0;
        }

        button_event = read_button_event(&mode_button);
        if (button_event == BTN_HOLD)  // Turn off lights
        {
            disable_systick();
            TIM1->CH4CVR = PWM_TICKS_0_PCT_DUTY_CYCLE;
            printf("Turn off LEDs.\n");
        }
        else if (button_event == BTN_HOLD_RELEASE)  // Power off the entire circuit
        {
            printf("Powering off...\n");
            funDigitalWrite(PIN_LATCH, FUN_LOW);  // Input pull-down

            // These 2 lines are for debugging purpose only, code should not reach here if correctly shutdown.
            // However, if the MCU is powered by WCH-LinkE, the code will keep running. Then, the key event loop would
            // detect the power off pull down as a key down, then there would be endless BTN_HOLD events.
            Delay_Ms(100);
            funDigitalWrite(PIN_LATCH, FUN_HIGH);  // Input pull-up
        }
        else if (button_event == BTN_RELEASE)  // Change light mode
        {
            printf("Mode button released.\n");

            // Move to next mode
            current_light_mode++;
            current_light_mode %= 3;
            current_light_setting = 10;  // Reset setting to max
            change_light_setting(current_light_mode, current_light_setting);
        }

        button_event = read_button_event(&set_button);
        if (button_event == BTN_RELEASE)  // Change light setting
        {
            printf("Set button released.\n");
            current_light_setting--;
            if (current_light_setting <= 0 || current_light_setting > 10)
            {
                current_light_setting = 10;  // Reset to max
            }

            change_light_setting(current_light_mode, current_light_setting);
        }
        else if (button_event == BTN_HOLD_RELEASE)
        {
            printf("Set button hold.\n");
            current_light_setting -= 3;
            if (current_light_setting <= 0 || current_light_setting > 10)
            {
                current_light_setting = 10;  // Reset to max
            }

            change_light_setting(current_light_mode, current_light_setting);
        }
    }
}
