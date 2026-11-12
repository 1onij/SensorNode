#ifndef __SPI_H
#define __SPI_H 
#include "stm32f10x.h"

/* 硬件 SPI1 主机：PA4 CS，PA5 SCK，PA6 MISO，PA7 MOSI，模式 0 */

void My_SPI_Init(void);     // 初始化引脚和 SPI1，并使能
void My_SPI_Start(void);    // CS 拉低：开始
void My_SPI_Stop(void);     // CS 拉高：结束
void My_SPI_MasterTransmitRecieve(SPI_TypeDef *SPIx, uint8_t *pdataTx, uint8_t *pdataRx, uint16_t size);  // 批量收发 size 个字节
uint8_t My_SPI_SwapByte(SPI_TypeDef *SPIx, uint8_t Byte);   // 发 1 字节同时收 1 字节
#endif
