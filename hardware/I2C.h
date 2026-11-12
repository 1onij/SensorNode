#ifndef __I2C_H
#define __I2C_H 
#include "stm32f10x.h"

/* 软件 I2C：PB6 = SCL，PB7 = SDA，开漏输出
 * 只负责最小的动作（起始、停止、收发一个字节、收发应答），
 * “先发地址再发数据”这种组合由器件层（OLED、MPU6050、AHT20）自己拼 */

void SoftI2C_Init(void);              // 初始化 PB6/PB7，SCL、SDA 都拉高（空闲）
void SoftI2C_WriteSCL(uint8_t i);     // 写 SCL：1 释放（高），0 拉低
void SoftI2C_WriteSDA(uint8_t i);     // 写 SDA：1 释放（高），0 拉低
void SoftI2C_Start(void);             // 起始条件：SCL 高时 SDA 由高变低
void SoftI2C_Stop(void);              // 停止条件：SCL 高时 SDA 由低变高
uint8_t SoftI2C_ReadSDA(void);        // 读 SDA 实际电平
void SoftI2C_SendByte(uint8_t Byte);  // 发 1 个字节，高位先发
uint8_t SoftI2C_ReceiveAck(void);     // 收应答：0 = ACK（从机收到），1 = NACK
uint8_t SoftI2C_ReceiveByte(void);    // 收 1 个字节，高位先收
void SoftI2C_SendAck(uint8_t i);         // 发应答：0 = ACK（还要），1 = NACK（不要了）
#endif
