#include "stm32f10x.h"
#include "OLED.h"
#include "Delay.h"
#include "I2C.h"
#include "OLED_Font.h"
#define OLEDAddr 0x3C   // OLED 的 7 位 I2C 地址；发送时左移一位加读写位，写 = 0x78

/* SSD1306 显存：128 列 x 8 页，每页高 8 像素
 * 写 1 个数据字节 = 点亮当前光标那一列竖着的 8 个点（bit0 在上，bit7 在下）
 * 写完光标自动右移 1 列，但不会自动换页
 * I2C 上地址后面的第一个字节是控制字节：0x00 表示后面是命令，0x40 表示后面是数据 */

/** 写命令：起始 → 地址(写) → 0x00(后面是命令) → 命令 → 停止，每个字节后收一次应答 */
void OLED_WriteCommand(uint8_t Command){
    SoftI2C_Start();
    SoftI2C_SendByte(OLEDAddr<<1);  // 0x3C << 1 | 0 = 0x78，写
    SoftI2C_ReceiveAck();
    SoftI2C_SendByte(0x00);             // 控制字节：命令
    SoftI2C_ReceiveAck();
    SoftI2C_SendByte(Command);
    SoftI2C_ReceiveAck();
    SoftI2C_Stop();
}

/** 写 1 个数据字节：和写命令一样，只是控制字节换成 0x40 */
void OLED_WriteByte(uint8_t Byte){
    SoftI2C_Start();
    SoftI2C_SendByte(OLEDAddr<<1);
    SoftI2C_ReceiveAck();
    SoftI2C_SendByte(0x40);             // 控制字节：数据
    SoftI2C_ReceiveAck();
    SoftI2C_SendByte(Byte);
    SoftI2C_ReceiveAck();
    SoftI2C_Stop();
}

/**
 * 连续写 Count 个数据字节：0x40 之后 OLED 会把后面所有字节都当数据
 * 起始、地址、控制字节只发一次，比逐个调 WriteByte 快约 3 倍
 * Data 用 const：字库存在 Flash 里是只读的，这个函数也只读不改
 */
void OLED_WriteDataBurst(const uint8_t *Data, uint8_t Count){
    SoftI2C_Start();
    SoftI2C_SendByte(OLEDAddr<<1);
    SoftI2C_ReceiveAck();
    SoftI2C_SendByte(0x40);             // 控制字节：后面全是数据
    SoftI2C_ReceiveAck();
    for(uint8_t i=0; i<Count; i++){
        SoftI2C_SendByte(Data[i]);
        SoftI2C_ReceiveAck();
    }
    SoftI2C_Stop();
}
/**
 * OLED 初始化：先初始化 I2C，等 OLED 上电稳定，再按顺序发配置命令
 * 两个字节的命令（如 8D 14）分两次发：前一个是命令，后一个是它的参数
 * 每一项都明确写一遍，不依赖上电默认值（烧录只复位单片机，OLED 不会复位）
 */
void OLED_Init(void){
    SoftI2C_Init();      // OLED 用 I2C，由它负责初始化 I2C
    Delay_ms(100);       // 等 OLED 上电稳定

    OLED_WriteCommand(0xAE);  // 关显示（配置期间先关掉）

    OLED_WriteCommand(0xD5);  // 设置时钟分频 / 振荡频率
    OLED_WriteCommand(0x80);  //   默认值

    OLED_WriteCommand(0xA8);  // 设置多路复用率
    OLED_WriteCommand(0x3F);  //   64 行（0x3F = 63）

    OLED_WriteCommand(0xD3);  // 设置显示偏移
    OLED_WriteCommand(0x00);  //   不偏移

    OLED_WriteCommand(0x40);  // 起始行 = 第 0 行

    OLED_WriteCommand(0xA1);  // 左右方向正常（A0 左右镜像）
    OLED_WriteCommand(0xC8);  // 上下方向正常（C0 上下镜像）

    OLED_WriteCommand(0xDA);  // 设置 COM 引脚配置
    OLED_WriteCommand(0x12);  //   128x64 用 0x12

    OLED_WriteCommand(0x81);  // 设置对比度（亮度）
    OLED_WriteCommand(0xCF);  //   0x00 ~ 0xFF

    OLED_WriteCommand(0xD9);  // 设置预充电周期
    OLED_WriteCommand(0xF1);

    OLED_WriteCommand(0xDB);  // 设置 VCOMH 电压
    OLED_WriteCommand(0x30);

    OLED_WriteCommand(0xA4);  // 按显存内容显示（A5 = 全屏点亮，测试用）
    OLED_WriteCommand(0xA6);  // 正常显示（A7 = 反色）

    OLED_WriteCommand(0x8D);  // 设置电荷泵
    OLED_WriteCommand(0x14);  //  开启（不开屏幕不亮）

    OLED_WriteCommand(0xAF);  // 开显示

    OLED_Clear();             // 清掉显存里上次程序留下的内容
    //OLED_WriteCommand(0xa5);//main里使用开启屏幕全亮
}

/**
 * 设置光标位置（页寻址模式）
 * page：页 0~7（从 0 开始）；X：列 0~127
 * 页命令 B0~B7：高 5 位 10110 是命令名，低 3 位放页号 → 0xB0 + page
 * 列地址要 7 位，一条命令只能放 4 位，所以拆两条：
 *   0x10 | 高 4 位（10h~1Fh = 设置列地址高 4 位）
 *   0x00 | 低 4 位（00h~0Fh = 设置列地址低 4 位）
 */
void OLED_SetCursor(uint8_t page, uint8_t X){
    OLED_WriteCommand(0xb0 + page);
    OLED_WriteCommand(0x10 | (X>>4));
    OLED_WriteCommand(0x00 | (X & 0x0F));
}

/**
 * 清屏：8 页，每页从第 0 列开始写 128 个 0
 * 写数据后列地址自动 +1，所以每页只需设置一次光标；换页不会自动换，要手动设
 * 只在初始化时调用一次，速度不敏感，所以没有改成 Burst
 */
void OLED_Clear(void){
    for(uint8_t page=0; page<8; page++){
        OLED_SetCursor(page, 0);
        for(uint16_t X=0; X<128; X++){
            OLED_WriteByte(0X00);
        }
    }
}

/**
 * 显示一个字符（8x16 字体）
 * Line 1~4，Column 1~16（从 1 开始）
 * 一个字高 16 点 = 占 2 页：上半在第 (Line-1)*2 页，下半在下一页
 * 一个字宽 8 点：从第 (Column-1)*8 列开始
 * 字库 OLED_F8x16 从空格 ' '（ASCII 32）开始排，所以字符在第 Char - ' ' 行
 * 每行 16 字节：[0]~[7] 是上半部分，[8]~[15] 是下半部分
 */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char){
    OLED_SetCursor((Line-1)*2 , (Column-1)*8);              // 光标到上半页
   
    
    OLED_WriteDataBurst(&OLED_F8x16[Char - ' '][0], 8);      // 写上半 8 个字节
    
    OLED_SetCursor((Line-1)*2 + 1 , (Column-1)*8);          // 光标到下半页，同一列

    OLED_WriteDataBurst(&OLED_F8x16[Char - ' '][0] + 8, 8);       // 写下半 8 个字节
    
}

/**
 * 显示字符串：一个字符一个字符调 ShowChar，每个字往右挪 1 列
 * 字符串以 '\0' 结尾，读到 '\0' 就停
 */
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String){
    int i = 0;
    while(String[i] != '\0'){
        OLED_ShowChar(Line, Column + i, String[i]);
        i++;
    }
}

/** 求 X 的 Y 次方，ShowNum / ShowHexNum 拆位时用 */
uint32_t OLED_Pow(uint8_t X, uint8_t Y){
    uint32_t Result = 1;
    while (Y--)
    {
        Result *= X;
    }   
    return Result;

}

/**
 * 显示无符号十进制数，固定显示 Length 位（不足前面补 0）
 * 取第 i 位（从左数）：Num / 10^(Length-1-i) % 10，例如 123 的百位 = 123 / 100 % 10 = 1
 * 数字 n 变成字符：n + '0'
 */
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Num, uint8_t Length){
    for(uint8_t i=0; i<Length; i++){
        uint8_t ch = Num / OLED_Pow(10, Length - 1 - i) % 10 + '0';
        OLED_ShowChar(Line, Column + i, ch);
    }
}

/**
 * 显示有符号数：第 1 位显示 '+' 或 '-'，后面 Length 位是绝对值
 * 只有负数才取反变成正数，再交给 ShowNum
 */
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Num, uint8_t Length){
    if(Num >= 0){
        OLED_ShowChar(Line, Column, '+');
    }else{
        OLED_ShowChar(Line, Column, '-');
        Num = - Num;
    }
  
   OLED_ShowNum(Line, Column + 1, Num, Length);
}

/**
 * 显示一位小数，格式 ±XX.X，共占 5 位：
 *   Column 符号，+1 +2 整数，+3 小数点，+4 小数位
 * 步骤：判断符号并显示 → 变成正数 → 乘 10 加 0.5 四舍五入成整数 t → 整数部分 t/10，小数位 t%10
 * 加 0.5 是因为 C 转整数是直接截断，例如 26.66 × 10 = 266.6，不加会变成 266（显示 26.6）
 * 整数和小数都从 t 算，保证 9.96 这种进位后显示 10.0
 */
void OLED_ShowFloat(uint8_t Line, uint8_t Column, float Num){
      if(Num >= 0){
        OLED_ShowChar(Line, Column, '+');
    }else{
        OLED_ShowChar(Line, Column, '-');
        Num = - Num;
    }
    uint32_t t = (uint32_t)((Num * 10) + 0.5f);     // 四舍五入到一位小数，放大 10 倍成整数
    uint16_t t1 = t / 10;                           // 整数部分
    uint8_t t2 = t % 10;                            // 小数位

    OLED_ShowNum(Line, Column + 1 , t1, 2);
    OLED_ShowChar(Line, Column + 3, '.');
    OLED_ShowNum(Line, Column + 4, t2, 1);
}

/**
 * 显示十六进制数，固定 Length 位
 * 拆位和 ShowNum 一样，只是 10 换成 16；每一位是 0~15：
 *   0~9  → '0'~'9'（+ '0'）
 *   10~15 → 'A'~'F'（- 10 + 'A'）
 */
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Num, uint8_t Length){
    for(uint8_t i=0; i<Length; i++){
        uint8_t ch = Num / OLED_Pow(16, Length - 1 - i) % 16 ;
        if(ch  < 10){
            ch += '0';
        }else
        {
            ch  = ch - 10 +'A';
        }
        OLED_ShowChar(Line, Column + i, ch);
    }
    

}
