#include "stm32f10x.h"
#include "I2C.h"
#include "Delay.h"

/* 软件 I2C：用 GPIO 翻转模拟 I2C 时序
 * 核心规则：SCL 低的时候改 SDA，SCL 高的时候读 SDA（高电平期间 SDA 保持不变）
 * 唯一的例外是起始和停止：它们故意在 SCL 高时改变 SDA，用来标记一次通信的开始和结束 */

/**
 * 初始化：PB6（SCL）、PB7（SDA）开漏输出，默认都输出 1（释放，被上拉成高电平 = 总线空闲）
 * 为什么用开漏：I2C 是多个器件共用一根线，从机要能把 SDA 拉低应答。
 * 开漏输出 1 只是“松手”，不会和从机的拉低打架；推挽输出 1 会硬顶成高电平，和从机冲突
 */
void SoftI2C_Init(void){
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_OD;       // 开漏输出
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;

    GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_WriteBit(GPIOB, GPIO_Pin_6, Bit_SET);  // SCL 释放
    GPIO_WriteBit(GPIOB, GPIO_Pin_7, Bit_SET);  // SDA 释放，总线空闲
}

/** 写 SCL：1 释放（高），0 拉低；每次翻转后延时 1us，给从机反应时间 */
void SoftI2C_WriteSCL(uint8_t i){
    if(i == 1){
        GPIO_WriteBit(GPIOB, GPIO_Pin_6, Bit_SET);
    }else{
        GPIO_WriteBit(GPIOB, GPIO_Pin_6, Bit_RESET);
    }
    Delay_us(1);
}

/** 写 SDA：1 释放（高），0 拉低；调用者要保证只传 0 或 1 */
void SoftI2C_WriteSDA(uint8_t i){
    if(i == 1){
        GPIO_WriteBit(GPIOB, GPIO_Pin_7, Bit_SET);
    }else{
        GPIO_WriteBit(GPIOB, GPIO_Pin_7, Bit_RESET);
    }
    Delay_us(1);
}

/**
 * 起始条件：SCL 高电平期间，SDA 从高变低
 * 先把两根线都拉高（防止上一次停在别的状态），再拉低 SDA，最后拉低 SCL 准备发数据
 */
void SoftI2C_Start(void){
    SoftI2C_WriteSDA(1);
    SoftI2C_WriteSCL(1);
    SoftI2C_WriteSDA(0);    // SCL 高时 SDA 下降 = 起始
    SoftI2C_WriteSCL(0);    // 拉低 SCL，之后才能改 SDA 发数据
}

/**
 * 停止条件：SCL 高电平期间，SDA 从低变高
 * 进来时 SCL 已经是低的，所以可以直接先把 SDA 拉低
 */
void SoftI2C_Stop(void){
    SoftI2C_WriteSDA(0);
    SoftI2C_WriteSCL(1);
    SoftI2C_WriteSDA(1);    // SCL 高时 SDA 上升 = 停止，总线回到空闲
}

/**
 * 读 SDA 的实际电平：读输入寄存器 IDR
 * 开漏模式下输入电路一直开着，不用切换成输入模式就能读
 * 读之前主机必须先释放 SDA（写 1），否则读到的永远是自己拉的 0
 */
uint8_t SoftI2C_ReadSDA(void){
    uint8_t status;
    status = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_7);
    return status;
}

/**
 * 发送 1 个字节：高位先发，共 8 位
 * 每一位：SCL 低时把 SDA 设成这一位 → SCL 拉高（从机在高电平期间读）→ SCL 拉低
 * Byte & (0x01 << i) 的结果是 0 或“那一位的值”（如 0x80），不一定是 1，所以用 != 0 判断
 */
void SoftI2C_SendByte(uint8_t Byte){
    for(int8_t i=7; i>=0; i--){             // i 从 7 到 0，高位先发；用有符号数，否则 i>=0 永远成立
        if((Byte & (0x01<<i)) != 0){
            SoftI2C_WriteSDA(1);
        }else{
            SoftI2C_WriteSDA(0);
        }
        SoftI2C_WriteSCL(1);    // 从机读这一位
        SoftI2C_WriteSCL(0);    // 准备下一位
    }
}

/**
 * 接收应答（第 9 个时钟）：主机松开 SDA，让从机来控制
 * 从机收到了会把 SDA 拉低（ACK = 0），没收到或不存在则 SDA 被上拉保持高（NACK = 1）
 * 返回：0 = ACK，1 = NACK
 */
uint8_t SoftI2C_ReceiveAck(void){
    SoftI2C_WriteSDA(1);                    // 主机释放 SDA
    SoftI2C_WriteSCL(1);                    // SCL 高，读应答
    uint8_t status = SoftI2C_ReadSDA();
    SoftI2C_WriteSCL(0);                    // 第 9 个时钟结束，从机松开 SDA
    return status;
}

/**
 * 接收 1 个字节：高位先收
 * 先释放 SDA 交给从机（只需一次），然后每一位：SCL 拉高 → 读 SDA → SCL 拉低（从机趁低电平放下一位）
 * 读到 1 就用 |= 把对应位置 1，其他位不动
 */
uint8_t SoftI2C_ReceiveByte(void){
    SoftI2C_WriteSDA(1);                    // 释放 SDA，让从机发数据
    uint8_t Byte = 0x00;
    for(int8_t i=7; i>=0; i--){
        SoftI2C_WriteSCL(1);
        if(SoftI2C_ReadSDA() == 1){
          Byte |= (0x01<<i);                // 这一位是 1
        }
        SoftI2C_WriteSCL(0);
    }
    return Byte;
}

/**
 * 发送应答：主机收完一个字节后告诉从机“还要不要”
 * i = 0：ACK，继续发下一个字节；i = 1：NACK，不要了（读最后一个字节后发 NACK）
 * 和发 1 位数据一样：SCL 低时写 SDA → SCL 拉高 → SCL 拉低
 */
void SoftI2C_SendAck(uint8_t i){
    SoftI2C_WriteSDA(i);
    SoftI2C_WriteSCL(1);
    SoftI2C_WriteSCL(0);
}
