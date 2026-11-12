#include "stm32f10x.h"
#include "Serial.h"
#include "stdio.h"
/* 命令帧接收状态机用的变量（static：只在本文件里用）
 * 命令帧：AA 55 | LEN | CMD | DATA... | SUM，LEN = CMD+DATA 的字节数（1~8） */
static uint8_t RxState = 0, RxLen = 0, RxCount = 0, RxSum = 0;  // 状态 0 等AA/1 等55/2 收LEN/3 收CMD+DATA/4 比校验；长度；已收个数；累加的校验和
static uint8_t RxBuf[8];                        // RxBuf[0] = CMD，后面是 DATA
static volatile uint8_t Serial_PacketFlag = 0;  // 收到正确的一包置 1；中断里写、main 里读，所以加 volatile

/* DMA 发送用的变量 */
static uint8_t TX_Buf[64];    // DMA 发送缓冲区：必须是全局的，函数返回后 DMA 还要从这里搬；装的是字节，所以是 uint8_t
static uint8_t TX_Busy = 0;   // 1 = 发过一帧（要等上一帧的 TC4），0 = 从没发过（上电 TC4 是 0，等了会卡死）
/**
 * 初始化 USART1：115200 波特率，8 位数据，无校验，1 位停止位（8N1）
 * PA9 是 TX，要复用推挽输出（引脚由 USART 外设控制）
 * PA10 是 RX，上拉输入（空闲时线上是高电平）
 * 两个引脚模式不同，所以 GPIO_Init 要分别调用两次
 */
void USART1_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);  // GPIOA 和 USART1 都在 APB2 上

    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;    // TX：复用推挽输出
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &GPIO_InitStruct);             // 先把 PA9 配进硬件
   
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;      // RX：上拉输入
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10;

    GPIO_Init(GPIOA, &GPIO_InitStruct);             // 再配 PA10

    USART_InitTypeDef USART_InitStruct;
    USART_InitStruct.USART_BaudRate = 115200;                                   // 波特率
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 不用硬件流控
    USART_InitStruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;                // 收发都打开
    USART_InitStruct.USART_Parity = USART_Parity_No;                            // 无校验
    USART_InitStruct.USART_StopBits = USART_StopBits_1;                         // 1 位停止位
    USART_InitStruct.USART_WordLength = USART_WordLength_8b;                    // 8 位数据

    USART_Init(USART1, &USART_InitStruct);

    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);  // 开接收中断：每收到 1 个字节进一次中断

    NVIC_InitTypeDef NVIC_InitStruct;
    NVIC_InitStruct.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0;  // 抢占优先级 0（最高）：接收不及时会丢字节
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;
    NVIC_Init(&NVIC_InitStruct);

    USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);  // 发送请求交给 DMA：DR 一空（TXE），USART 就喊 DMA 送下一个字节

    USART_Cmd(USART1, ENABLE);  // 使能 USART1
}

/**
 * 串口 1 中断：硬件自动调用，每收到 1 个字节进来一次
 * 用 RxState 记住收到哪一步，一个字节推进一步；整包收完且校验对上才置 Serial_PacketFlag
 * 状态机必须放在 if (RXNE) 里面：别的原因进中断时 byte 没有意义，不能推进状态机
 */
void USART1_IRQHandler(void){
     uint8_t byte;

    if(USART_GetITStatus(USART1, USART_IT_RXNE) == SET){
        byte = USART_ReceiveData(USART1);  // 读 DR，同时硬件清掉 RXNE
        switch (RxState)
        {
            // 等帧头 AA
            case 0: if(byte == 0xaa){
            RxState = 1;
            }else{
            RxState = 0;
            }break;
            // 等 55；又来一个 AA 就当新的帧头，继续等 55
            case 1: if(byte == 0x55){
            RxState = 2;
            }else if(byte == 0xaa){
            RxState = 1;
            }else{
            RxState = 0;
            }break;
            // 收 LEN：0 或大于 8（RxBuf 装不下）就丢弃，回去等帧头
            case 2: if(byte == 0 || byte > 8){
            RxState = 0;
            }else{
            RxLen = byte;
            RxSum = byte;
            RxCount = 0;  // 校验和从 LEN 开始累加
            RxState = 3;
            }break;
            // 收 CMD 和 DATA，一边存一边累加校验和，收满 LEN 个就去比校验
            case 3: RxBuf[RxCount++] = byte;
            RxSum += byte;
            if(RxCount >= RxLen) RxState = 4;
            break;
            // 比校验：对上才置标志，不管对不对都回到等帧头
            case 4: if(RxSum == byte) Serial_PacketFlag = 1;
            RxState = 0;
            break;
        }

    }
        
    
   
}
/** 查询是否收到一包命令：返回 1 = 有新命令，读一次就清零（main 每圈调一次） */
uint8_t Get_Serial_PacketFlag(void){
    if (Serial_PacketFlag == 1) {
        Serial_PacketFlag = 0;
        return 1;
    }
    return 0;
}
/* 取命令和参数：RxBuf[0] 是 CMD，参数从 RxBuf[1] 开始 */
uint8_t Serial_GetCmd(void)          { return RxBuf[0]; }
uint8_t Serial_GetData(uint8_t i)    { return RxBuf[i + 1]; }   // 第 i 个参数，从 0 数

/**
 * 重写 C 库的 fputc：printf 格式化好字符串后，每个字符都会调一次它
 * 链接时优先用自己写的同名函数，所以 printf 的输出就被“接”到了 USART1 上
 * ch：要发的字符；f：输出流（只有串口一个出口，这里不用，但参数必须和 C 库一致）
 * 注意要在工程里勾选 MicroLIB，否则 printf 会走半主机模式卡死
 */
int fputc(int ch, FILE* f){
    USART_SendData(USART1, ch);                                     // 把字符放进发送数据寄存器 DR
    while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);    // 等 TXE = 1，DR 空了才能放下一个
    return ch;
}
/**
 * 逐字节发送一串数据：每个字节都等 TXE，发完才返回（CPU 一直陪着等）
 * 改用 DMA 后已经没有地方调用，留着做对比和调试用
 * 注意：DMA 正在发的时候不要调它，两边会抢 DR
 */
void Serial_SendArray(uint8_t *Array, uint16_t Length){
    for(uint16_t i=0; i<Length; i++){
        USART_SendData(USART1, Array[i]);
        while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);  // 等 DR 空了再放下一个
    }
}

/**
 * 回一帧应答，告诉电脑命令执行结果
 * 格式和命令帧一样：AA 55 | LEN=02 | CMD | 结果 | SUM，共 6 字节
 * Cmd：收到的命令，应答的 CMD = Cmd | 0x80（最高位置 1，一看就知道是回复）
 * Result：0 成功，2 不认识的命令
 * 例：停止上报成功 → AA 55 02 82 00 84
 */
void Serial_SendAck(uint8_t Cmd, uint8_t Result){
    uint8_t Buf[6];   // 局部数组也没关系：Serial_DMA_Send 会先拷进全局的 TX_Buf
    uint8_t sum = 0;

    Buf[0] = 0xaa;        // 帧头
    Buf[1] = 0x55;
    Buf[2] = 2;           // LEN：CMD + 结果，共 2 字节
    Buf[3] = Cmd | 0x80;  // 应答命令字，0x02 → 0x82
    Buf[4] = Result;
    // 校验和：LEN + CMD + 结果，sum 是 uint8_t，超过 255 自动只留低 8 位
    for(uint8_t i=2; i<5; i++){
        sum += Buf[i];
    }
    Buf[5] = sum;


    Serial_DMA_Send(Buf, 6);
}

/**
 * 初始化 USART1 发送用的 DMA（DMA1 通道 4 固定对应 USART1_TX，查参考手册 DMA 请求映射表）
 * 从哪搬：内存 TX_Buf；搬到哪：USART1->DR；每次 1 字节
 * 只配置不开通道：一上电就开的话，DR 是空的，DMA 会马上把 TX_Buf 里的 0 发出去
 * 要在 USART1_Init 之后调用
 */
void DMA1_Init(void){
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);  // DMA 挂在 AHB 总线上，不是 APB

    DMA_InitTypeDef DMA_InitStruct;
    DMA_InitStruct.DMA_BufferSize = 36;                         // 先随便填，每次发送前用 DMA_SetCurrDataCounter 重设
    DMA_InitStruct.DMA_DIR = DMA_DIR_PeripheralDST;             // 外设是目的：内存 → 外设
    DMA_InitStruct.DMA_M2M = DISABLE;                           // 不用内存到内存，等 USART 的请求一个一个搬（规范写法是 DMA_M2M_Disable，数值一样）
    DMA_InitStruct.DMA_MemoryBaseAddr = (uint32_t)TX_Buf;       // 内存那端：数组地址是 32 位的数，所以转 uint32_t
    DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;        // 数组搬完一个往后挪 1 字节
    DMA_InitStruct.DMA_Mode = DMA_Mode_Normal;                  // 搬完就停，不循环；下一帧要“重新上膛”
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR;  // 外设那端：串口数据寄存器
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;   // DR 只有一个地址，不自增
    DMA_InitStruct.DMA_Priority = DMA_Priority_Medium;

    DMA_Init(DMA1_Channel4, &DMA_InitStruct);  // 把结构体写进通道 4 的寄存器
}

/**
 * 用 DMA 发送一串字节：交代完立刻返回，不等发完，DMA 在后台搬
 * Data：要发的数据（可以是调用者的局部数组，这里会拷一份）；Len：字节数，不能超过 TX_Buf 的 64
 * 上一帧还没搬完时，会先在第一步等它搬完，所以连续调用也不会把帧搞乱
 * 发 36 字节在 115200 下线上要约 3.1ms，用 DMA 后 CPU 只花几微秒
 */
void Serial_DMA_Send(uint8_t *Data, uint16_t Len){
    if(Len > 64) return;
    // ① 等：发过才等上一帧的 TC4（搬完标志）；TX_Buf 只有一个，没搬完就写新数据会把帧搞乱
    if(TX_Busy == 1){
        while(DMA_GetFlagStatus(DMA1_FLAG_TC4) == RESET);
    }
    // ② 清：TC4 是硬件置 1、软件清 0，不清的话下次一看就以为已经搬完了
    DMA_ClearFlag(DMA1_FLAG_TC4);
    // ③ 关：通道开着时改不了计数器 CNDTR
    DMA_Cmd(DMA1_Channel4, DISABLE);
    // ④ 拷：DMA 只认初始化时给的 TX_Buf 地址；调用者的局部数组函数返回后就失效了
    for(uint16_t i = 0; i < Len; i++){
        TX_Buf[i] = Data[i];
    }
    // ⑤ 设：Normal 模式搬完后计数是 0，不会自己恢复；每帧长度也不一样（24 / 36 / 6）
    DMA_SetCurrDataCounter(DMA1_Channel4, Len);
    
    // ⑥ 开：DR 正好空着，USART 马上喊 DMA，开始在后台搬
    DMA_Cmd(DMA1_Channel4, ENABLE);

    // ⑦ 记：发过了，下次调用要先等这一帧的 TC4
    TX_Busy = 1;

}
