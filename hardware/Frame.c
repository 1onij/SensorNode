#include "stm32f10x.h"
#include "Delay.h"
#include "Serial.h"
#include "Frame.h"

/* 数据帧打包：只管“字节怎么排”，排好交给 Serial_DMA_Send 发出去 */

/**
 * 发送一帧 24 字节的 AA 55 数据帧（多字节数都是高字节在前）
 * [0-1]   AA 55 帧头
 * [2]     数据长度 20
 * [3-14]  加速度 XYZ、陀螺仪 XYZ 原始值，各 2 字节
 * [15-16] 温度×10（int16_t）
 * [17-18] 湿度×10（int16_t）
 * [19-20] Roll×10（int16_t）
 * [21-22] Pitch×10（int16_t）
 * [23]    校验和：第 2~22 字节相加，取低 8 位
 */
void Frame_Send(int16_t ax, int16_t ay, int16_t az, int16_t gx, int16_t gy, int16_t gz, float temp, float humi, float Roll, float Pitch){
    uint8_t Buf[24];   // 局部数组：Serial_DMA_Send 会先拷进全局 TX_Buf，所以函数返回后失效也没关系
    uint32_t sum = 0;

    // float 不好直接发，×10 转成整数，保留一位小数：27.3℃ → 273
    int16_t t = (int16_t)(temp * 10);
    int16_t h = (int16_t)(humi * 10);
    int16_t r = (int16_t)(Roll * 10);
    int16_t p = (int16_t)(Pitch * 10);

    Buf[0] = 0xaa;   // 帧头
    Buf[1] = 0x55;
    Buf[2] = 20;     // 数据长度：20 字节
    // 每个 16 位数拆成两个字节：高字节右移 8 位，低字节强转只留低 8 位
    Buf[3] = (uint8_t)(ax>>8);
    Buf[4] = (uint8_t)(ax);
    Buf[5] = (uint8_t)(ay>>8);
    Buf[6] = (uint8_t)(ay);
    Buf[7] = (uint8_t)(az>>8);
    Buf[8] = (uint8_t)(az);
    Buf[9] = (uint8_t)(gx>>8);
    Buf[10] = (uint8_t)(gx);
    Buf[11] = (uint8_t)(gy>>8);
    Buf[12] = (uint8_t)(gy);
    Buf[13] = (uint8_t)(gz>>8);
    Buf[14] = (uint8_t)(gz);
    Buf[15] = (uint8_t)(t>>8);
    Buf[16] = (uint8_t)(t);
    Buf[17] = (uint8_t)(h>>8);
    Buf[18] = (uint8_t)(h);
    Buf[19] = (uint8_t)(r>>8);
    Buf[20] = (uint8_t)r;
    Buf[21] = (uint8_t)(p>>8);
    Buf[22] = (uint8_t)(p);
    // 校验和：第 2~22 字节相加；存进 uint8_t 的 Buf[23] 时自动只留低 8 位
    for(uint8_t i=2; i<23; i++){
        sum += Buf[i];
    }
    Buf[23] = sum;
    Serial_DMA_Send(Buf, 24);   // 交给 DMA 后台发送，函数立刻返回
}

/**
 * 按 VOFA+ JustFloat 格式发送 size 个 float
 * 格式：每个 float 4 字节（按内存原样发，小端）+ 帧尾 00 00 80 7F
 * 先把整帧拼进 buf，再调一次 DMA 发送；分两次调的话第二次要等第一次搬完，CPU 又要干等
 * buf 64 字节，size 最多 15：15×4 + 4 = 64
 */
void Frame_SendVofa(float *data, uint8_t size){
    if(size > 15) return;            // 超过就装不下，直接不发
    uint8_t *p = (uint8_t *)data;    // 地址不变，换成按字节看：p[0]~p[size*4-1] 就是所有 float 的字节
    uint8_t buf[64] = {0};
    for(uint16_t i=0; i<size*4; i++){
        buf[i] = p[i];
    }
    // 帧尾紧跟在数据后面
    buf[size * 4] = 0;
    buf[size * 4 + 1] = 0;
    buf[size * 4 + 2] = 0x80;
    buf[size * 4 + 3] = 0x7f;
    Serial_DMA_Send(buf, size * 4 +4);   // 8 个 float 时一共 36 字节



}
