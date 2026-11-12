#ifndef __DELAY_H
#define __DELAY_H

/* 阻塞延时：CPU 在里面干等。只在初始化、I2C 时序这种很短的地方用，主循环里用 Timer 的 tick 不阻塞调度 */

void Delay_us(uint32_t us);  // 微秒延时
void Delay_ms(uint32_t ms);  // 毫秒延时
void Delay_s(uint32_t s);    // 秒延时

#endif
