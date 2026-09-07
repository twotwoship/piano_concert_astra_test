#include "device_driver.h"
#include "buzzer.h"
#include "player.h"

static volatile uint32_t millis;
void SysTick_Handler(void) { ++millis; }

void _Invalid_ISR(void)
{
    Buzzer_Silence();
    __disable_irq();
    for (;;) __WFI();
}

static void Button_Update(uint32_t now)
{
    static uint8_t raw_previous, stable, long_fired;
    static uint32_t changed_at, pressed_at;
    /* B1: PC13, 눌렀을 때 LOW. 30ms 디바운스. */
    uint8_t raw = (GPIOC->IDR & (1U << 13)) == 0U;
    if (raw != raw_previous) { raw_previous = raw; changed_at = now; }
    if (raw != stable && now - changed_at >= 30U) {
        stable = raw;
        if (stable) { pressed_at = now; long_fired = 0U; }
        else if (!long_fired) Player_Toggle(now);
    }
    if (stable && !long_fired && now - pressed_at >= 1000U) {
        long_fired = 1U;
        Player_Next(now);
    }
}

static void LED_Update(uint32_t now)
{
    PlayerState s = Player_State();
    uint8_t on;
    if (s == PLAYER_PLAYING) on = 1U;
    else if (s == PLAYER_PAUSED) on = (now % 1000U) < 500U;
    else if (s == PLAYER_ERROR) on = (now % 150U) < 75U;
    else {
        /* 대기: 2초마다 현재 악장 번호만큼 깜빡임. */
        uint32_t phase = now % 2000U;
        on = phase < (uint32_t)(Player_Movement() + 1U) * 300U && phase % 300U < 120U;
    }
    GPIOA->BSRR = on ? (1U << 5) : (1U << 21);
}

void Main(void)
{
    SCB->CPACR |= (0xfU << 20);
    __DSB();
    __ISB();
    Clock_Init();
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN;
    (void)RCC->AHB1ENR;
    GPIOC->MODER &= ~(3U << 26);
    GPIOC->PUPDR = (GPIOC->PUPDR & ~(3U << 26)) | (1U << 26);
    GPIOA->MODER = (GPIOA->MODER & ~(3U << 10)) | (1U << 10);
    Buzzer_Init();
    Player_Init();
    SysTick_Config(SYSCLK / 1000U);
    for (;;) {
        uint32_t now = millis;
        Button_Update(now);
        Player_Update(now);
        LED_Update(now);
        __WFI();
    }
}
