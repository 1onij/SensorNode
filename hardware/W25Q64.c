#include "stm32f10x.h"
#include "Delay.h"
#include "W25Q64.h"
#include "SPI.h"

/* W25Q64：8MB SPI Flash，地址 24 位（0x000000 ~ 0x7FFFFF）
 * 每个操作的套路都一样：CS 拉低 → 发指令 → 发/收若干字节 → CS 拉高
 * 结构：页 256 字节，扇区 4KB（16 页），块 64KB
 * 三条规则：
 *   1. 写和擦之前必须先写使能（每次都要，做完自动失效）
 *   2. 写和擦之后要等忙结束
 *   3. Flash 只能把 1 改成 0，所以写之前要先擦（擦完全是 0xFF） */

/**
 * 读 JEDEC ID（指令 0x9F）：依次返回厂商 ID、存储类型、容量
 * W25Q64 应读到 EF 40 17，用来验证 SPI 通不通
 */
void W25Q64_JEDEC_ID(uint8_t *Buf){
    My_SPI_Start();
    My_SPI_SwapByte(SPI1, 0x9f);            // 发指令，收回来的没用
    Buf[0] = My_SPI_SwapByte(SPI1, 0xff);   // 厂商 ID：0xEF（华邦）
    Buf[1] = My_SPI_SwapByte(SPI1, 0xff);   // 存储类型：0x40
    Buf[2] = My_SPI_SwapByte(SPI1, 0xff);   // 容量：0x17（2^23 = 8MB）
    My_SPI_Stop();
}

/**
 * 写使能（指令 0x06）：写和擦之前必须调用
 * 必须单独一次“拉低…拉高”，芯片在 CS 拉高时才真正生效
 */
void W25Q64_WriteEnable(void){
    My_SPI_Start();
    My_SPI_SwapByte(SPI1, 0x06);
    My_SPI_Stop();
}

/**
 * 等忙（指令 0x05 读状态寄存器 1）：bit0 BUSY = 1 表示正在写/擦
 * CS 保持低时，每发一个 0xFF 就换回一次最新的状态，不用重发 0x05
 * 注意：没加超时，芯片没接好时会卡在这里
 */
void W25Q64_WaitBusy(void){
    My_SPI_Start();
    My_SPI_SwapByte(SPI1, 0x05);
    while(1){
        uint8_t Byte = My_SPI_SwapByte(SPI1, 0xff);
        if((Byte & 0x01) == 0) break;       // BUSY = 0：空闲了
    }
    My_SPI_Stop();
} 

/**
 * 扇区擦除（指令 0x20）：把地址所在的整个 4KB 扇区擦成 0xFF
 * 例如传 0x001234，实际擦的是 0x001000 ~ 0x001FFF
 */
void W25Q64_SectorErase(uint32_t Addr){
    W25Q64_WriteEnable();
    My_SPI_Start();
    My_SPI_SwapByte(SPI1, 0x20);
    My_SPI_SwapByte(SPI1, Addr>>16);        // 地址高字节（传进 uint8_t 参数时自动只留低 8 位）
    My_SPI_SwapByte(SPI1, Addr>>8);         // 地址中字节
    My_SPI_SwapByte(SPI1, Addr);            // 地址低字节
    My_SPI_Stop();
    W25Q64_WaitBusy();                      // 擦一个扇区要几十毫秒
}

/**
 * 页编程（指令 0x02）：从 Addr 开始写 Count 个字节
 * 限制：最多 256 - (Addr % 256) 个，不能跨页；超了会绕回本页开头覆盖
 * 写之前这片区域要先擦过
 */
void W25Q64_PageProgram(uint32_t Addr, uint8_t *byte, uint16_t Count){
    W25Q64_WriteEnable();
    My_SPI_Start();
    My_SPI_SwapByte(SPI1, 0x02);
    My_SPI_SwapByte(SPI1, Addr>>16);
    My_SPI_SwapByte(SPI1, Addr>>8);
    My_SPI_SwapByte(SPI1, Addr);
    for(uint16_t i=0; i<Count; i++){
        My_SPI_SwapByte(SPI1, byte[i]);     // 发数据，收回来的没用
    }
    My_SPI_Stop();                          // CS 拉高后芯片才开始真正写入
    W25Q64_WaitBusy();
}

/**
 * 读数据（指令 0x03）：从 Addr 开始连续读 Count 个字节到 Data[]
 * 不用写使能、不用等忙，也没有 256 的限制，地址自动往后加，可以跨页一直读
 */
void W25Q64_ReadData(uint32_t Addr, uint8_t *Data, uint32_t Count){
    My_SPI_Start();
    My_SPI_SwapByte(SPI1, 0x03);
    My_SPI_SwapByte(SPI1, Addr>>16);
    My_SPI_SwapByte(SPI1, Addr>>8);
    My_SPI_SwapByte(SPI1, Addr);
    for(uint32_t i=0; i<Count; i++){
       Data[i] = My_SPI_SwapByte(SPI1, 0xff);  // 发 0xFF 占位，把数据换回来
    }
    My_SPI_Stop();
}
