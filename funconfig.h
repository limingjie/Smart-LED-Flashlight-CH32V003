#ifndef _FUNCONFIG_H
#define _FUNCONFIG_H

// Clock Calculation
//
// SYSCLK (System Clock) = PLLCLK or HSI or HSE
// HCLK (HB Bus Peripheral Clock) = SYSCLK / Prescaler_Divisor
//
// Use HSI
// HCLK = 24MHz x (USE_PLL ? 2 : 1) / Prescaler_Divisor
//
// Refer to the follow code from ch32v003fun to calculate system clock settings
// #if FUNCONF_SYSTEM_CORE_CLOCK == 48000000
//     #define FUNCONF_CLOCK_DIV RCC_HPRE_DIV1
//     #define FUNCONF_USE_PLL   1
// #elif FUNCONF_SYSTEM_CORE_CLOCK == 24000000
//     #define FUNCONF_CLOCK_DIV RCC_HPRE_DIV1
//     #define FUNCONF_USE_PLL   0
// #elif FUNCONF_SYSTEM_CORE_CLOCK == 16000000
//     #define FUNCONF_CLOCK_DIV RCC_HPRE_DIV3
//     #define FUNCONF_USE_PLL   1
// #elif FUNCONF_SYSTEM_CORE_CLOCK == 12000000
//     #define FUNCONF_CLOCK_DIV RCC_HPRE_DIV2
//     #define FUNCONF_USE_PLL   0
// #elif FUNCONF_SYSTEM_CORE_CLOCK == 8000000
//     #define FUNCONF_CLOCK_DIV RCC_HPRE_DIV3
//     #define FUNCONF_USE_PLL   0
// #elif FUNCONF_SYSTEM_CORE_CLOCK == 6000000
//     #define FUNCONF_CLOCK_DIV RCC_HPRE_DIV4
//     #define FUNCONF_USE_PLL   0
// #elif FUNCONF_SYSTEM_CORE_CLOCK == 4000000
//     #define FUNCONF_CLOCK_DIV RCC_HPRE_DIV6
//     #define FUNCONF_USE_PLL   0
// #elif FUNCONF_SYSTEM_CORE_CLOCK == 3000000
//     #define FUNCONF_CLOCK_DIV RCC_HPRE_DIV8
//     #define FUNCONF_USE_PLL   0
// #elif FUNCONF_SYSTEM_CORE_CLOCK == 1500000
//     #define FUNCONF_CLOCK_DIV RCC_HPRE_DIV16
//     #define FUNCONF_USE_PLL   0
// #elif FUNCONF_SYSTEM_CORE_CLOCK == 750000
//     #define FUNCONF_CLOCK_DIV RCC_HPRE_DIV32
//     #define FUNCONF_USE_PLL   0
// #elif FUNCONF_SYSTEM_CORE_CLOCK == 375000
//     #define FUNCONF_CLOCK_DIV RCC_HPRE_DIV64
//     #define FUNCONF_USE_PLL   0
// #elif FUNCONF_SYSTEM_CORE_CLOCK == 187500
//     #define FUNCONF_CLOCK_DIV RCC_HPRE_DIV128
//     #define FUNCONF_USE_PLL   0
// #elif FUNCONF_SYSTEM_CORE_CLOCK == 93750
//     #define FUNCONF_CLOCK_DIV RCC_HPRE_DIV256
//     #define FUNCONF_USE_PLL   0
// #else
//     #warning Unsupported system clock frequency, using internal 48MHz
//     #define FUNCONF_CLOCK_DIV RCC_HPRE_DIV1
//     #define FUNCONF_USE_PLL   1
//     #undef FUNCONF_SYSTEM_CORE_CLOCK
//     #define FUNCONF_SYSTEM_CORE_CLOCK 48000000
// #endif
//
// Set in ch32fun.c
// 1728:   RCC->CFGR0 = RCC_HPRE_DIV3;         // HCLK = SYSCLK / 3

#define CH32V003                  1
#define FUNCONF_USE_PLL           0        // Don't use PLL - Phase-Locked Loop - clock multiplier.
#define FUNCONF_USE_HSI           1        // Use HSI - Internal High-Frequency RC Oscillator
#define FUNCONF_USE_HSE           0        // Use HSE - External High-Frequency Oscillator
#define FUNCONF_SYSTEM_CORE_CLOCK 1500000  // Computed Clock in Hz - 24MHz / 16 = 1.5MHz
#define FUNCONF_SYSTICK_USE_HCLK  1        // Set SYSTICK to use HCLK or HCLK/8.
#define FUNCONF_USE_DEBUGPRINTF   1

#endif
