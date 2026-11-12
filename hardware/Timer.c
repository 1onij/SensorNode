#include "stm32f10x.h"
#include "Delay.h"
#include "Timer.h"

/* TIM2 定时中断，产生 1ms 的系统节拍
 * 中断里只让 Tick 加 1，main 里用 GetTick() 看过了多少毫秒，到时间再去采数据 */

static volatile uint32_t Tick = 0;  // static：只能在本文件里直接改；volatile：中断里会改，每次都从内存重新读

/**
 * 初始化 TIM2：72MHz / (PSC+1) / (ARR+1) = 72MHz / 72 / 1000 = 1kHz，即 1ms 进一次中断
 */
void Timer_Init(void){
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);  // TIM2 在 APB1（APB1 分频后定时器时钟仍是 72MHz）

    TIM_TimeBaseInitTypeDef TIM_InitStruct = {0};
    TIM_InitStruct.TIM_CounterMode = TIM_CounterMode_Up;  // 向上计数：0 → ARR，溢出产生更新事件
    TIM_InitStruct.TIM_Period = 999;  // ARR：数 1000 下
    TIM_InitStruct.TIM_Prescaler = 71;  // PSC：72 分频 → 1MHz
    TIM_InitStruct.TIM_RepetitionCounter = 0;  // 只有高级定时器用，TIM2 填 0

    TIM_TimeBaseInit(TIM2, &TIM_InitStruct);
   
    TIM_ClearFlag(TIM2, TIM_FLAG_Update);  // TimeBaseInit 会置更新标志，先清掉，避免一开中断就进一次

    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);  // 允许“更新”事件产生中断


    NVIC_InitTypeDef NVIC_InitStruct = {0};
    NVIC_InitStruct.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 2;  // 抢占优先级
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;         // 响应优先级
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;

    NVIC_Init(&NVIC_InitStruct);
    TIM_Cmd(TIM2, ENABLE);  // 最后再启动计数
}

/**
 * TIM2 中断服务函数：名字必须和启动文件里的一模一样，不用声明、不用调用，硬件自动跳进来
 * 只做最少的事：Tick 加 1，然后清标志（不清的话退出后马上又进来，main 永远跑不了）
 */
void TIM2_IRQHandler(void){
    if(TIM_GetFlagStatus(TIM2, TIM_FLAG_Update) == SET){
        Tick++;
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);  // 清中断标志（注意：这里要清的应该是 Update，见聊天）
    }
}

/** 返回从 Timer_Init 到现在过了多少毫秒 */
uint32_t GetTick(void){
    return Tick;
}
