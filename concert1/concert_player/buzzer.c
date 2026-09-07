#include "device_driver.h"
#include "buzzer.h"
#include "generated/score_data.h"

typedef struct {
    TIM_TypeDef *timer;
    GPIO_TypeDef *port;
    uint8_t pin, af, channel;
} Buzzer;

/* 부저마다 타이머를 하나씩 사용: 8개 음높이가 완전히 독립적입니다. */
static const Buzzer buzzers[8] = {
    {TIM1,  GPIOA, 8, 1, 1}, /* D7,  CN9-8  */
    {TIM2,  GPIOA, 0, 1, 1}, /* A0,  CN8-1  */
    {TIM3,  GPIOB, 4, 2, 1}, /* D5,  CN9-6  */
    {TIM4,  GPIOB, 6, 2, 1}, /* D10, CN5-3  */
    {TIM5,  GPIOA, 1, 2, 2}, /* A1,  CN8-2: CH2 사용 */
    {TIM9,  GPIOA, 2, 3, 1}, /* D1,  CN9-2: UART2 사용 안 함 */
    {TIM10, GPIOB, 8, 3, 1}, /* D15, CN5-10 */
    {TIM11, GPIOB, 9, 3, 1}, /* D14, CN5-9  */
};

static void Pin_Mode(const Buzzer *b, uint32_t mode)
{
    uint32_t shift = 2U * b->pin;
    b->port->MODER = (b->port->MODER & ~(3U << shift)) | (mode << shift);
}

void Buzzer_Set(uint8_t voice, uint8_t note_plus_one)
{
    if (voice >= 8U) return;
    const Buzzer *b = &buzzers[voice];
    TIM_TypeDef *t = b->timer;
    t->CR1 = 0U;
    t->CCER = 0U;
    /* 무음 때 실제 핀을 LOW 출력으로 고정합니다. */
    b->port->BSRR = 1U << (b->pin + 16U);
    Pin_Mode(b, 1U);
    if (note_plus_one == 0U || note_plus_one > 128U) return;

    const PitchTimer *pitch = &pitch_timers[note_plus_one - 1U];
    t->PSC = pitch->prescaler;
    t->ARR = pitch->reload;
    uint32_t duty = ((uint32_t)pitch->reload + 1U) / 2U;
    if (b->channel == 2U) t->CCR2 = duty;
    else t->CCR1 = duty;
    t->CNT = 0U;
    t->EGR = TIM_EGR_UG;
    t->SR = 0U;
    if (t == TIM1) t->BDTR = TIM_BDTR_MOE;
    t->CCER = 1U << ((b->channel - 1U) * 4U);
    Pin_Mode(b, 2U);
    t->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
}

void Buzzer_Silence(void)
{
    for (uint8_t i = 0; i < 8U; ++i) Buzzer_Set(i, 0U);
}

void Buzzer_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN | RCC_APB1ENR_TIM3EN |
                    RCC_APB1ENR_TIM4EN | RCC_APB1ENR_TIM5EN;
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN | RCC_APB2ENR_TIM9EN |
                    RCC_APB2ENR_TIM10EN | RCC_APB2ENR_TIM11EN;
    (void)RCC->APB2ENR;
    for (uint8_t i = 0; i < 8U; ++i) {
        const Buzzer *b = &buzzers[i];
        uint32_t shift = b->pin * 2U;
        b->port->OTYPER &= ~(1U << b->pin);
        b->port->PUPDR &= ~(3U << shift);
        b->port->OSPEEDR &= ~(3U << shift);
        uint32_t af_shift = (b->pin % 8U) * 4U;
        b->port->AFR[b->pin / 8U] =
            (b->port->AFR[b->pin / 8U] & ~(15U << af_shift)) | ((uint32_t)b->af << af_shift);
        b->timer->CR1 = 0U;
        b->timer->CR2 = 0U;
        b->timer->DIER = 0U;
        b->timer->CCER = 0U;
        /* PWM mode 1 + CCR preload. TIM5만 CH2 필드 사용. */
        b->timer->CCMR1 = b->channel == 2U ? ((6U << 12) | (1U << 11))
                                                    : ((6U << 4) | (1U << 3));
    }
    Buzzer_Silence();
}
