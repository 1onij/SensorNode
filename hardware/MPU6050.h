#ifndef __MPU6050_H
#define __MPU6050_H 
#include "stm32f10x.h"

/* MPU6050：软件 I2C，地址 0x68 */

void MPU6050_WriteReg(uint8_t RegAddr, uint8_t Data);   // 写一个寄存器
uint8_t MPU6050_ReadReg(uint8_t RegAddr);               // 读一个寄存器（WHO_AM_I 0x75 应读到 0x68）
void MPU6050_Init(void);                                // 初始化 I2C + 配置量程、滤波
void MPU6050_GetData(int16_t *AccX, int16_t *AccY, int16_t *AccZ, int16_t *GyroX, int16_t *GyroY, int16_t *GyroZ); // 读 6 轴原始值
void MPU6050_Calibrate(float *OffX, float *OffY);
#endif
