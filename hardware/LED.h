#ifndef __LED_H
#define __LED_H 

/* 板载 LED：PC13，低电平亮（LED 另一端接 3.3V） */

void LED_Init(void);          // 初始化 PC13，默认点亮
void LED_ChangeStatus(void);  // 翻转 LED 状态（亮 ↔ 灭）


#endif // !__LED_H
