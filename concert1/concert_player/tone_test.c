/* Standalone diagnostic: only PB4 / TIM3_CH1 emits a steady PWM.
 * B1 release cycles 1000 Hz -> 2000 Hz -> 500 Hz -> silence.
 * No MIDI decoder, note scheduling or repeated timer resets.
 */
#include "device_driver.h"

static volatile uint32_t millis;
void SysTick_Handler(void) { ++millis; }

void _Invalid_ISR(void)
{
    TIM3->CCER = 0U;
    GPIOB->BSRR = 1U << 20;
    GPIOB->MODER = (GPIOB->MODER & ~(3U << 8)) | (1U << 8);
    __disable_irq();
    for (;;) __WFI();
}

static void Tone(unsigned hz)
{
    TIM3->CR1 = 0U;
    TIM3->CCER = 0U;
    GPIOB->BSRR = 1U << 20;
    GPIOB->MODER = (GPIOB->MODER & ~(3U << 8)) | (1U << 8);
    GPIOA->BSRR = 1U << 21;
    if (!hz) return;
    TIM3->PSC = (TIMXCLK / 1000000U) - 1U;
    TIM3->ARR = 1000000U / hz - 1U;
    TIM3->CCR1 = (1000000U / hz) / 2U;
    TIM3->CNT = 0U;
    TIM3->EGR = TIM_EGR_UG;
    TIM3->SR = 0U;
    TIM3->CCER = TIM_CCER_CC1E;
    GPIOB->MODER = (GPIOB->MODER & ~(3U << 8)) | (2U << 8);
    TIM3->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
    GPIOA->BSRR = 1U << 5;
}

void Main(void)
{
    static const unsigned tones[4] = {1000U, 2000U, 500U, 0U};
    unsigned index = 0U;
    uint8_t previous = 0U, stable = 0U;
    uint32_t changed = 0U;
    SCB->CPACR |= 0xfU << 20;
    __DSB();
    __ISB();
    Clock_Init();
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    (void)RCC->APB1ENR;
    /* Hold all eight previously assigned buzzer signal pins LOW. */
    const uint32_t a_mask = (1U << 0) | (1U << 1) | (1U << 2) | (1U << 8);
    const uint32_t b_mask = (1U << 4) | (1U << 6) | (1U << 8) | (1U << 9);
    GPIOA->BSRR = a_mask << 16;
    GPIOB->BSRR = b_mask << 16;
    for (unsigned pin = 0U; pin < 16U; ++pin) {
        if (a_mask & (1U << pin)) {
            GPIOA->MODER = (GPIOA->MODER & ~(3U << (2U * pin))) | (1U << (2U * pin));
        }
        if (b_mask & (1U << pin)) {
            GPIOB->MODER = (GPIOB->MODER & ~(3U << (2U * pin))) | (1U << (2U * pin));
        }
    }
    GPIOB->OTYPER &= ~(1U << 4);
    GPIOB->PUPDR &= ~(3U << 8);
    GPIOB->AFR[0] = (GPIOB->AFR[0] & ~(15U << 16)) | (2U << 16);
    GPIOC->MODER &= ~(3U << 26);
    GPIOC->PUPDR = (GPIOC->PUPDR & ~(3U << 26)) | (1U << 26);
    GPIOA->MODER = (GPIOA->MODER & ~(3U << 10)) | (1U << 10);
    TIM3->DIER = 0U;
    TIM3->CCMR1 = (6U << 4) | (1U << 3);
    Tone(0U);
    SysTick_Config(SYSCLK / 1000U);
    for (;;) {
        uint32_t now = millis;
        uint8_t raw = (GPIOC->IDR & (1U << 13)) == 0U;
        if (raw != previous) { previous = raw; changed = now; }
        if (raw != stable && now - changed >= 30U) {
            stable = raw;
            if (!stable) { Tone(tones[index]); index = (index + 1U) % 4U; }
        }
        __WFI();
    }
}
