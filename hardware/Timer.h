#ifndef __Timer_H
#define __Timer_H 
#include "stm32f10x.h"

/* TIM2：1ms 节拍 */

void Timer_Init(void);      // 初始化 TIM2，1ms 中断一次
uint32_t GetTick(void);     // 读当前毫秒数
#endif
