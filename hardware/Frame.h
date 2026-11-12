#ifndef __Frame_H
#define __Frame_H
#include "stm32f10x.h"

/* 数据帧打包：把数据排成字节，交给 Serial_DMA_Send 发出 */

void Frame_Send(int16_t ax, int16_t ay, int16_t az, int16_t gx, int16_t gy, int16_t gz, float temp, float humi, float Roll, float Pitch);  // 发一帧 AA 55 数据帧（24 字节）
void Frame_SendVofa(float *data, uint8_t size);  // 按 VOFA+ JustFloat 发 size 个 float，size 最多 15
#endif
