#include "stm32f10x.h"
#include "Delay.h"
#include "I2C.h"
#include "AHT20.h"

/* AHT20 温湿度传感器，挂在软件 I2C 上
 * 7 位地址 0x38，写地址 0x70，读地址 0x71
 * 它不是寄存器型器件：没有“寄存器地址”，直接发命令、直接读数据 */
#define AHT20Addr 0x38

/**
 * 读状态字节：起始 → 读地址 0x71 → 收 1 字节 → NACK → 停止
 * bit7 = 1 表示正在测量（忙），bit3 = 1 表示已校准
 */
uint8_t AHT20_ReadStatus(void){
    SoftI2C_Start();
    SoftI2C_SendByte((AHT20Addr<<1) | 1);   // 读地址 0x71
    SoftI2C_ReceiveAck(); 
    uint8_t Byte = SoftI2C_ReceiveByte();
    SoftI2C_SendAck(1);                     // 只要 1 个字节，回 NACK
    SoftI2C_Stop();
    return Byte;
}

/**
 * 初始化：上电后等 40ms，查校准位 bit3
 * 没校准就发初始化命令 0xBE 0x08 0x00，再等 10ms
 * 注意：这里不初始化 I2C 引脚，main 里要先调用 SoftI2C_Init（或 MPU6050_Init）
 */
void AHT20_Init(void){
    Delay_ms(40);                           // 手册要求上电后至少等 40ms
    uint8_t Byte = AHT20_ReadStatus();
    if((Byte & (0x01 << 3)) == 0){          // bit3 = 0：没校准
        SoftI2C_Start();
        SoftI2C_SendByte(AHT20Addr<<1);     // 写地址 0x70
        SoftI2C_ReceiveAck();
        SoftI2C_SendByte(0xbe);             // 初始化命令
        SoftI2C_ReceiveAck();
        SoftI2C_SendByte(0x08);             // 参数 1
        SoftI2C_ReceiveAck();
        SoftI2C_SendByte(0x00);             // 参数 2
        SoftI2C_ReceiveAck();
        SoftI2C_Stop();
        Delay_ms(10);
    }
}

/**
 * 触发一次测量：发 0xAC 0x33 0x00，然后等 80ms 让它测完
 * AHT20 每触发一次只测一次，想要新数据就要再触发
 */
void AHT20_Trigger(void){
        SoftI2C_Start();
        SoftI2C_SendByte(AHT20Addr<<1);     // 写地址 0x70
        SoftI2C_ReceiveAck();
        SoftI2C_SendByte(0xac);             // 触发测量命令
        SoftI2C_ReceiveAck();
        SoftI2C_SendByte(0x33);             // 参数 1
        SoftI2C_ReceiveAck();
        SoftI2C_SendByte(0x00);             // 参数 2
        SoftI2C_ReceiveAck();
        SoftI2C_Stop();                  
}

/**
 * 读温湿度：先查忙，不忙就连续读 6 个字节并换算
 * 返回 1 = 读到新数据，0 = 传感器还在忙（*Temp、*Humi 不变）
 *
 * 6 个字节的含义：
 *   Buf[0]                      状态
 *   Buf[1] Buf[2] Buf[3]高4位   湿度，共 20 位
 *   Buf[3]低4位 Buf[4] Buf[5]   温度，共 20 位
 * 换算：湿度 % = 原始值 / 2^20 × 100；温度 ℃ = 原始值 / 2^20 × 200 - 50
 */
uint8_t AHT20_GetData(float *Temp, float *Humi){
        uint8_t Byte = AHT20_ReadStatus();
        uint32_t Buf[6];                    // 用 32 位存，后面左移 12、16 位不会溢出
        if((Byte & (0x01<<7)) == 0){        // bit7 = 0：测完了
        SoftI2C_Start();
        SoftI2C_SendByte(AHT20Addr<<1 | 1); // 读地址 0x71
        SoftI2C_ReceiveAck();
        for (uint8_t i = 0; i < 6; i++)
        {
            Buf[i] = SoftI2C_ReceiveByte();
            if(i < 5){
                SoftI2C_SendAck(0);         // 前 5 个回 ACK：还要
            }else{
                SoftI2C_SendAck(1);         // 最后一个回 NACK：不要了
            }
        }  
        SoftI2C_Stop();
        uint32_t Humi1 = Buf[1]<<12 | Buf[2]<<4 | (Buf[3] & 0xf0)>>4;  // 湿度 20 位
        uint32_t Temp1 = (Buf[3] & 0x0f)<<16 | Buf[4]<<8 | Buf[5];     // 温度 20 位
    
        *Temp = (float)Temp1/ 1048576 * 200 -50;    // 1048576 = 2^20
        *Humi = (float)Humi1 / 1048576 * 100;
    return 1;

    }else{
        return 0;                           // 忙，这次不更新
    }
    
    
}
