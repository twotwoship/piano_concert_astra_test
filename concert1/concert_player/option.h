#ifndef OPTION_H
#define OPTION_H
#define SYSCLK 96000000U
#define HCLK SYSCLK
#define PCLK1 (HCLK / 2U)
#define PCLK2 HCLK
#define TIMXCLK (PCLK1 * 2U)
#define TIM1CLK PCLK2
#endif
