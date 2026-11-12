#include "stm32f10x.h"
#include "LED.h"

/**
 * 初始化 LED：PC13 开漏输出
 * 开漏输出 0 时把引脚拉到地，LED 有电流就亮；输出 1 时引脚断开，LED 灭
 * 初始化后输出低电平，LED 默认点亮
 */
void LED_Init(void){
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);  // GPIOC 挂在 APB2 上

    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_OD;   // 开漏输出（推挽也可以）
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_13;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;   // LED 不需要快速翻转，2MHz 够用

    GPIO_Init(GPIOC, &GPIO_InitStruct);
    GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_RESET);   // 输出低电平，LED 亮
}

/**
 * 翻转 LED：读输出寄存器 ODR 看当前输出的是什么，再输出相反的值
 * 用 ReadOutputDataBit（读 ODR，“我让它输出什么”），不是 ReadInputDataBit（读 IDR，“引脚实际电平”）
 */
void LED_ChangeStatus(void){
    if(GPIO_ReadOutputDataBit(GPIOC, GPIO_Pin_13) == SET){
        GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_RESET);   // 原来灭 → 亮
    }else{
        GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_SET);     // 原来亮 → 灭
    }
}
