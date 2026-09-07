#include "device_driver.h"

void Clock_Init(void)
{
    RCC->CR |= (1U << 0);
    while(!Macro_Check_Bit_Set(RCC->CR, 1));

    /* 96메가헤르츠 뽑기 */
    RCC->APB1ENR |= (1U << 28);
    (void)RCC->APB1ENR;
    Macro_Write_Block(PWR->CR, 0x3, 0x3, 14);

    FLASH->ACR = (1U<<10)|(1U<<9)|(1U<<8)|(0x3U<<0);

    RCC->PLLCFGR = (4U<<24)|(0U<<22)|(0U<<16)|(96U<<6)|(8U<<0);

    Macro_Set_Bit(RCC->CR, 24);
    while(!Macro_Check_Bit_Set(RCC->CR, 25));
    while(!Macro_Check_Bit_Set(PWR->CSR, 14));


    RCC->CFGR = (0U<<13)|(4U<<10)|(0U<<4);

    Macro_Write_Block(RCC->CFGR, 0x3, 0x2, 0);
    while(Macro_Extract_Area(RCC->CFGR, 0x3, 2) != 0x2);

    SystemCoreClockUpdate();
}
