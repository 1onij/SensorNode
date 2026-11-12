#ifndef __W25Q64_H
#define __W25Q64_H 
#include "stm32f10x.h"

/* W25Q64 SPI Flash：8MB，页 256 字节，扇区 4KB */

void W25Q64_JEDEC_ID(uint8_t *Buf);                                     // 读 ID，应为 EF 40 17
void W25Q64_WriteEnable(void);                                          // 写使能，写/擦前调用
void W25Q64_WaitBusy(void);                                             // 等芯片忙完
void W25Q64_SectorErase(uint32_t Addr);                                 // 擦除 Addr 所在的 4KB 扇区
void W25Q64_PageProgram(uint32_t Addr, uint8_t *byte, uint16_t Count);  // 页写，不能跨页
void W25Q64_ReadData(uint32_t Addr, uint8_t *Data, uint32_t Count);     // 连续读，可跨页
#endif
