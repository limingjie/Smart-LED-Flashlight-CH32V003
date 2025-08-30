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
// Refer to the follow code from ch32v003fun
// ```
// #elif FUNCONF_SYSTEM_CORE_CLOCK == 750000  // 750kHz
//     #define FUNCONF_CLOCK_DIV RCC_HPRE_DIV32
//     #define FUNCONF_USE_PLL   0
// ```
//
// Set in ch32fun.c
// 1728:   RCC->CFGR0 = RCC_HPRE_DIV32;         // HCLK = SYSCLK / 32

#define CH32V003                  1
#define FUNCONF_USE_PLL           0       // Don't use PLL - Phase-Locked Loop - clock multiplier.
#define FUNCONF_USE_HSI           1       // Use HSI - Internal High-Frequency RC Oscillator
#define FUNCONF_USE_HSE           0       // Use HSE - External High-Frequency Oscillator
#define FUNCONF_SYSTEM_CORE_CLOCK 750000  // Computed Clock in Hz - 24MHz / 32 = 750kHz
#define FUNCONF_SYSTICK_USE_HCLK  1       // Set SYSTICK to use HCLK or HCLK/8.
#define FUNCONF_USE_DEBUGPRINTF   1

#endif
