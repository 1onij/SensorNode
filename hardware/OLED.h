#ifndef __OLED_H
#define __OLED_H 
#include "stm32f10x.h"

/* 0.96 寸 OLED（SSD1306，128x64），软件 I2C，地址 0x3C
 * 显示坐标：Line 1~4 行，Column 1~16 列（8x16 字体，从 1 开始数） */

void OLED_WriteCommand(uint8_t Command);                       // 写 1 个命令字节
void OLED_WriteByte(uint8_t Byte);                             // 写 1 个数据字节（竖着 8 个点）
void OLED_WriteDataBurst(const uint8_t *Data, uint8_t Count);  // 连续写 Count 个数据字节
void OLED_Init(void);                                          // 初始化 I2C 和 OLED，并清屏
void OLED_SetCursor(uint8_t page, uint8_t X);                  // 设置光标：页 0~7，列 0~127
void OLED_Clear(void);                                         // 清屏（全黑）
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String);                   // 显示字符串
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Num, uint8_t Length);      // 显示无符号十进制数，Length 位
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Num, uint8_t Length); // 显示有符号数，符号占 1 位
void OLED_ShowFloat(uint8_t Line, uint8_t Column, float Num);                       // 显示一位小数，格式 ±XX.X，占 5 位
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Num, uint8_t Length);   // 显示十六进制数，Length 位
#endif
