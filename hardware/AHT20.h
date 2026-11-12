#ifndef __AHT20_H
#define __AHT20_H 
#include "stm32f10x.h"

/* AHT20 温湿度传感器：软件 I2C，地址 0x38 */

uint8_t AHT20_ReadStatus(void);                     // 读状态字节（bit7 忙，bit3 已校准）
void AHT20_Init(void);                              // 上电等待 + 没校准就发初始化命令
void AHT20_Trigger(void);                           // 触发一次测量，内部等 80ms
uint8_t AHT20_GetData(float *Temp, float *Humi);    // 读温湿度，返回 1 成功、0 忙
#endif
