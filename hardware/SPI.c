#include "stm32f10x.h"
#include "Delay.h"

/* 硬件 SPI1，主机模式，接 W25Q64
 * PA4 = CS（软件控制），PA5 = SCK，PA6 = MISO，PA7 = MOSI
 * SPI 是全双工：每发出 1 个字节的同时，一定会收回 1 个字节 */

/**
 * 初始化 GPIO 和 SPI1
 */
void My_SPI_Init(void){
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1, ENABLE);  // GPIOA 和 SPI1 都在 APB2

    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;    // SCK、MOSI 由 SPI 外设控制：复用推挽
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;

    GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;   // CS 自己用软件拉高拉低：普通推挽输出
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;

    GPIO_Init(GPIOA, &GPIO_InitStruct);
    GPIO_WriteBit(GPIOA, GPIO_Pin_4, Bit_SET);      // CS 默认高：不选中

    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;      // MISO 是输入：上拉输入
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6;

    GPIO_Init(GPIOA, &GPIO_InitStruct);

    SPI_InitTypeDef SPI_InitStruct;
    SPI_InitStruct.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_128;  // 72MHz / 128 ≈ 562kHz
    SPI_InitStruct.SPI_CPHA = SPI_CPHA_1Edge;  // 第一个边沿采样 ┐
    SPI_InitStruct.SPI_CPOL = SPI_CPOL_Low;    // 空闲时 SCK 低 ┘ 合起来是模式 0，W25Q64 支持
    SPI_InitStruct.SPI_Mode = SPI_Mode_Master;  // 主机：由 STM32 产生 SCK
    SPI_InitStruct.SPI_CRCPolynomial = 7;  // 不用 CRC，填默认值
    SPI_InitStruct.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStruct.SPI_Direction = SPI_Direction_2Lines_FullDuplex;  // 全双工：发一个字节同时收一个
    SPI_InitStruct.SPI_FirstBit = SPI_FirstBit_MSB;  // 高位先发
    SPI_InitStruct.SPI_NSS = SPI_NSS_Soft;  // 片选用软件控制（PA4），不用硬件 NSS
    
    SPI_Init(SPI1, &SPI_InitStruct);
    
    SPI_NSSInternalSoftwareConfig(SPI1, SPI_NSSInternalSoft_Set);  // 内部 NSS 置高，否则主机会出错掉回从机模式
    SPI_Cmd(SPI1, ENABLE);                          // 开一次就一直开着
}

/** 开始一次通信：CS 拉低，选中从机 */
void My_SPI_Start(void){
    GPIO_WriteBit(GPIOA, GPIO_Pin_4, Bit_RESET); 
}

/** 结束一次通信：CS 拉高，取消选中（W25Q64 在这一刻才执行写使能、擦除、页写） */
void My_SPI_Stop(void){
    GPIO_WriteBit(GPIOA, GPIO_Pin_4, Bit_SET); 
}

/**
 * 批量收发：一边发 pdataTx[]，一边把收到的存进 pdataRx[]，共 size 个字节
 * 流水线写法：先发第 0 个，之后每次“发下一个 → 收上一个”，最后再收最后一个
 * 注意 size 不能为 0
 */
void My_SPI_MasterTransmitRecieve(SPI_TypeDef *SPIx, uint8_t *pdataTx, uint8_t *pdataRx, uint16_t size){
    SPI_I2S_SendData(SPIx, pdataTx[0]);
    for(uint16_t i=0; i<size-1; i++){
        while (SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_TXE) == RESET);    // 等发送缓冲空
        SPI_I2S_SendData(SPIx, pdataTx[i + 1]);
        while (SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_RXNE) == RESET);   // 等收到上一个字节
        pdataRx[i] = SPI_I2S_ReceiveData(SPIx);
    }
    while (SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_RXNE) == RESET);       // 收最后一个
    pdataRx[size - 1] = SPI_I2S_ReceiveData(SPIx);
    
}

/**
 * 交换 1 个字节：发出 Byte，同时收回 1 个字节并返回
 * 只想发：不管返回值；只想收：发 0xFF 占位
 */
uint8_t My_SPI_SwapByte(SPI_TypeDef *SPIx, uint8_t Byte){
    while (SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_TXE) == RESET);    // 等发送缓冲空（TXE = 1）
    SPI_I2S_SendData(SPIx, Byte);                                      // 写 DR，硬件开始发
    while (SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_RXNE) == RESET);   // 等收完 8 位（RXNE = 1）
    Byte = SPI_I2S_ReceiveData(SPIx);                                  // 读 DR，同时清 RXNE
    return Byte;
}
 
