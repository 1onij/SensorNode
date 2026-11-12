#include "stm32f10x.h"
#include "Delay.h"
#include "I2C.h"

/* MPU6050 六轴传感器（三轴加速度 + 三轴陀螺仪），挂在软件 I2C 上
 * 7 位地址 0x68（AD0 接地），写地址 0xD0，读地址 0xD1
 * MPU6050 是“寄存器型”器件：所有操作都是读/写某个寄存器 */
#define MPU6050Addr 0x68

/**
 * 写寄存器：起始 → 写地址 → 寄存器地址 → 数据 → 停止
 * 每发一个字节都要收一次应答（这里没判断应答结果，调试时可以拿返回值看）
 */
void MPU6050_WriteReg(uint8_t RegAddr, uint8_t Data){
    SoftI2C_Start();
    SoftI2C_SendByte(MPU6050Addr<<1);       // 写地址 0xD0
    SoftI2C_ReceiveAck();
    SoftI2C_SendByte(RegAddr);              // 要写哪个寄存器
    SoftI2C_ReceiveAck();
    SoftI2C_SendByte(Data);                 // 写进去的值
    SoftI2C_ReceiveAck();
    SoftI2C_Stop();

}

/**
 * 读寄存器：分两段
 * 第一段：起始 → 写地址 → 寄存器地址（先“指定”要读哪个寄存器）
 * 第二段：重复起始 → 读地址 → 收 1 字节 → 发 NACK（告诉从机不要了）→ 停止
 */
uint8_t MPU6050_ReadReg(uint8_t RegAddr){
    SoftI2C_Start();
    SoftI2C_SendByte(MPU6050Addr<<1);       // 写地址 0xD0
    SoftI2C_ReceiveAck();
    SoftI2C_SendByte(RegAddr);              // 指定寄存器
    SoftI2C_ReceiveAck();
    SoftI2C_Start();                        // 重复起始：不停止，直接切换成读
    SoftI2C_SendByte(MPU6050Addr<<1 | 1);   // 读地址 0xD1
    SoftI2C_ReceiveAck();
    uint8_t Byte = SoftI2C_ReceiveByte();
    SoftI2C_SendAck(1);                     // 只读 1 个字节，回 NACK
    SoftI2C_Stop();
    return Byte;
}

/**
 * 初始化：先初始化 I2C 引脚，再配置 6 个寄存器
 */
void MPU6050_Init(void){
    SoftI2C_Init();
    MPU6050_WriteReg(0x6b, 0x01);   // PWR_MGMT_1：解除睡眠，时钟选 X 轴陀螺仪
    MPU6050_WriteReg(0x6c, 0x00);   // PWR_MGMT_2：6 个轴都不待机
    MPU6050_WriteReg(0x19, 0x09);   // SMPLRT_DIV：采样分频 10
    MPU6050_WriteReg(0x1a, 0x06);   // CONFIG：数字低通滤波最平滑档
    MPU6050_WriteReg(0x1b, 0x18);   // GYRO_CONFIG：±2000°/s，16.4 LSB 对应 1°/s
    MPU6050_WriteReg(0x1c, 0x18);   // ACCEL_CONFIG：±16g，2048 LSB 对应 1g
}

/**
 * 读 6 轴原始数据：每个轴 16 位，存在两个寄存器里，高字节在前
 * 拼法：高字节 << 8 | 低字节；结果是补码，所以输出用 int16_t（可以是负数）
 * 加速度 0x3B~0x40，温度 0x41~0x42（跳过），陀螺仪 0x43~0x48
 */
void MPU6050_GetData(int16_t *AccX, int16_t *AccY, int16_t *AccZ, int16_t *GyroX, int16_t *GyroY, int16_t *GyroZ){
    uint8_t AccX1 = MPU6050_ReadReg(0x3b);  // ACCEL_XOUT_H
    uint8_t AccX2 = MPU6050_ReadReg(0x3C);  // ACCEL_XOUT_L
    *AccX = AccX1<<8 | AccX2;
    uint8_t AccY1 = MPU6050_ReadReg(0x3D);
    uint8_t AccY2 = MPU6050_ReadReg(0x3E);
    *AccY = AccY1<<8 | AccY2;
    uint8_t AccZ1 = MPU6050_ReadReg(0x3F);
    uint8_t AccZ2 = MPU6050_ReadReg(0x40);
    *AccZ = AccZ1<<8 | AccZ2;               // 平放时约 +2048（1g）

    uint8_t GyroX1 = MPU6050_ReadReg(0x43); // GYRO_XOUT_H
    uint8_t GyroX2 = MPU6050_ReadReg(0x44); // GYRO_XOUT_L
    *GyroX = GyroX1<<8 | GyroX2;
    uint8_t GyroY1 = MPU6050_ReadReg(0x45);
    uint8_t GyroY2 = MPU6050_ReadReg(0x46);
    *GyroY = GyroY1<<8 | GyroY2;
    uint8_t GyroZ1 = MPU6050_ReadReg(0x47);
    uint8_t GyroZ2 = MPU6050_ReadReg(0x48);
    *GyroZ = GyroZ1<<8 | GyroZ2;            // 静止时接近 0
}
/**
 * 陀螺仪零偏校准：板子静止时陀螺仪本应读到 0，实际有一个固定偏移（零偏）
 * 不减掉的话积分会一直累加，角度越漂越远（互补滤波后 Roll 固定差约 9° 就是它造成的）
 * 静止采 200 次、每次隔 10ms（共约 2 秒），求平均当零偏；只校 X、Y 两个轴（Roll、Pitch 用）
 * OffX / OffY：输出参数，用指针把结果带回去
 */
void MPU6050_Calibrate(float *OffX, float *OffY){
    int16_t AccX, AccY, AccZ, GyroX, GyroY, GyroZ;
    int32_t SumX = 0, SumY = 0;   // 200 个 int16 相加会超过 int16 范围，用 int32
    for(uint16_t i=0; i<200; i++){
        MPU6050_GetData(&AccX, &AccY, &AccZ, &GyroX, &GyroY, &GyroZ);
        Delay_ms(10);
        SumX += GyroX;
        SumY += GyroY;
    }
    *OffX = SumX / 200.0f;   // 除以 200.0f 而不是 200：按浮点除，保留小数
    *OffY = SumY / 200.0f;
}
