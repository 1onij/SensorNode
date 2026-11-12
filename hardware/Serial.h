#ifndef __Serial_H
#define __Serial_H

/* USART1：PA9 TX，PA10 RX，115200 8N1
 * Serial.c 里重写了 fputc，printf 的输出会从 USART1 发出去（需要勾选 MicroLIB）
 * 发送走 DMA1 通道 4；接收用 RXNE 中断 + 状态机解析命令帧 */

void USART1_Init(void);  // 初始化 USART1 和引脚，开接收中断和 TX 的 DMA 请求
void Serial_SendArray(uint8_t *Array, uint16_t Length);  // 逐字节发送，等发完才返回（DMA 版之前用的，现在不用）
uint8_t Get_Serial_PacketFlag(void);  // 收到完整且校验正确的命令包返回 1（读后清零）
uint8_t Serial_GetCmd(void);  // 取命令字节 CMD
uint8_t Serial_GetData(uint8_t i);  // 取第 i 个参数（从 0 数）
void Serial_SendAck(uint8_t Cmd, uint8_t Result);  // 回应答帧：AA 55 02 (Cmd|0x80) Result SUM
void DMA1_Init(void);  // 初始化发送 DMA（DMA1 通道 4），在 USART1_Init 之后调用
void Serial_DMA_Send(uint8_t *Data, uint16_t Len);  // DMA 后台发送，立刻返回；Len 最多 64
#endif
