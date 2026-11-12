#include "stm32f10x.h"  
#include "stdio.h"
#include "LED.h"
#include "Delay.h"
#include "Serial.h" 
#include "I2C.h"     
#include "OLED.h"      
#include "MPU6050.h" 
#include "AHT20.h"
#include "SPI.h"
#include "W25Q64.h"
#include "Timer.h"
#include "Frame.h"
#include "math.h"
#define Addr 0x0000000   // 零偏在 W25Q64 里的存放地址：第 0 扇区开头

/**
 * SensorNode 主程序：只管“什么时候做什么”
 * 初始化 → 读 / 校准零偏 → while(1) 里按 tick 轮流检查各任务，到时间才做，不用 Delay 干等
 */
int main(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);  // 中断分组：2 位抢占 + 2 位响应，整个程序只设一次
	LED_Init();
	// LED_ChangeStatus();
	USART1_Init();
	SoftI2C_Init();   // MPU6050、AHT20、OLED 共用 PB6/PB7 这一条 I2C 总线
	OLED_Init();
	MPU6050_Init();
	AHT20_Init();
	My_SPI_Init();    // 硬件 SPI1，给 W25Q64 用；加 My_ 前缀避免和标准库 SPI_Init 重名
	Timer_Init();     // TIM2 每 1ms 进一次中断，tick 加 1
	DMA1_Init();      // 串口发送 DMA，要在 USART1_Init 之后

	

    int16_t AccX, AccY, AccZ, GyroX, GyroY, GyroZ;   // MPU6050 原始值（补码，可以是负数）
	uint8_t  AhtBusy = 0, OledStep = 1,SendEnable = 1, SendVofaMode = 0;      // AhtBusy：0 = 空闲，1 = 已触发、在测量；OledStep：这次刷 OLED 第几项（1~8）；SendEnable：1 上报 / 0 停止；SendVofaMode：0 发 AA 55 帧 / 1 发 VOFA
	float Temp = 0, Humi = 0, ARoll = 0, APitch = 0, GRoll = 0, GPitch = 0, OffX, OffY, Roll = 0, Pitch = 0;  // A 开头：加速度算的角；G 开头：陀螺仪积分的角；Roll/Pitch：互补滤波后的角；OffX/OffY：陀螺仪零偏
    uint32_t LastLed = 0, LastMpu = 0, LastAht = 0, Now = 0, TrigTime, LastOled = 0, LastFrame = 0;  // 各任务上次执行的时刻（ms）；TrigTime：AHT20 触发测量的时刻



	// OLED 标签只在开机写一次，循环里只刷数字，省时间
	OLED_ShowString(1,1,"T:");
	OLED_ShowString(1,8,"H:");
	OLED_ShowString(2,1,"AR:");
	OLED_ShowString(2,8,"AP:");
	OLED_ShowString(3,1,"GR:");
	OLED_ShowString(3,9,"GP:");
	OLED_ShowString(4,1,"R:");
	OLED_ShowString(4,9,"P:");
	
	

	/* 零偏存储格式（10 字节）：A5 5A | OffX 的 4 字节 | OffY 的 4 字节
	 * 上电先读：有 A5 5A 标志说明存过，直接用；没有（新片子全是 FF）就校准一次再存进去 */
	uint8_t* P = (uint8_t*)&OffX;   // 把 float 当成 4 个字节来看，才能一个字节一个字节地存取
	uint8_t* Q = (uint8_t*)&OffY;
	uint8_t Buffer[10];

	W25Q64_ReadData(Addr, Buffer, 10);

	if(Buffer[0] == 0xa5 && Buffer[1] == 0x5a){
		// 存过：把 4 个字节拼回 float，跳过 2 秒校准
		for(uint8_t i=0; i<4; i++){
			P[i] = Buffer[i+2];
			Q[i] = Buffer[i+6];
		}

	}else{
		// 没存过：静止校准约 2 秒，再把结果写进 Flash
		MPU6050_Calibrate(&OffX, &OffY);
		Buffer[0] = 0xA5;
        Buffer[1] = 0X5A;
        for(uint8_t i=0; i<4; i++){
        Buffer[i+2] = P[i];
		Buffer[i+6] = Q[i];
        }
        W25Q64_SectorErase(Addr);  // Flash 要先擦再写
        W25Q64_PageProgram(Addr, Buffer, 10);  // 10 字节不跨页（一页 256 字节）
	}
	


	
	
	while (1)
	{
		Now = GetTick();   // 每圈取一次当前时刻；Now - LastXxx 用无符号减法，tick 溢出回绕也算得对

		// 任务 1：每 10ms 读 MPU6050，算姿态角
		if (Now - LastMpu >= 10){
			float dt = (Now - LastMpu) / 1000.0f;   // 真实间隔（秒），不写死 0.01：别的任务慢了，dt 也跟着准

			LastMpu = Now;
			MPU6050_GetData(&AccX, &AccY, &AccZ, &GyroX, &GyroY, &GyroZ);
			// 加速度算角度：只看重力方向，长时间准，但一晃就抖；57.3 是弧度转度
			ARoll = atan2(AccY, AccZ) * 57.3;
			APitch = atan2(-AccX, sqrt(AccY *AccY + AccZ * AccZ)) * 57.3;
			// 陀螺仪积分：角速度（原始值 / 16.4 = °/s）× dt 累加，短时间准、不抖，但会慢慢漂
			GRoll  += (GyroX - OffX) / 16.4 * dt;
            GPitch += (GyroY - OffY) / 16.4 * dt;
			// 互补滤波：98% 信陀螺仪（管短时间），2% 信加速度（把漂移慢慢拉回来）
			Roll  = 0.98f * (Roll  + (GyroX - OffX) / 16.4f * dt) + 0.02f * ARoll;
            Pitch = 0.98f * (Pitch + (GyroY - OffY) / 16.4f * dt) + 0.02f * APitch;
		}

		// 任务 2：每 20ms 往串口上报一帧，格式由 SendVofaMode 决定；DMA 发送，几微秒就返回
		if (Now - LastFrame >= 20 && SendEnable == 1){
			if(SendVofaMode == 1){
				LastFrame = Now;
			    float Buf[8] ={Temp, Humi, ARoll, APitch, GRoll, GPitch, Roll, Pitch};   // VOFA 通道 I0~I7
		        Frame_SendVofa(Buf, 8);
			}else{
			    LastFrame = Now;
		        Frame_Send(AccX, AccY, AccZ, GyroX, GyroY, GyroZ, Temp, Humi, Roll, Pitch);
		    }
		}

		// 任务 3：OLED 分 8 次刷新，每 12ms 只刷 1 项
		// 一次刷完 8 项要 100 多 ms，会把 MPU 任务卡住；拆开以后每圈只占一点时间
		switch (OledStep)
		{
			case 1: if(Now - LastOled >= 12){
				OLED_ShowFloat(1,3,Temp);
				LastOled = Now;
				OledStep++;
			}break;
			case 2: if(Now - LastOled >= 12){
				OLED_ShowFloat(1,10,Humi);
				LastOled = Now;
				OledStep++;
			}break;
			case 3: if(Now - LastOled >= 12){
				OLED_ShowSignedNum(2, 4, (int16_t)ARoll, 3);
				LastOled = Now;
				OledStep++;
			}break;
			case 4: if(Now - LastOled >= 12){
				OLED_ShowSignedNum(2, 11, (int16_t)APitch, 3);
				LastOled = Now;
				OledStep++;
			}break;
			case 5: if(Now - LastOled >= 12){
				OLED_ShowSignedNum(3, 4, (int16_t)GRoll, 3);
				LastOled = Now;
				OledStep++;
			}break;
			case 6: if(Now - LastOled >= 12){
				OLED_ShowSignedNum(3, 12, (int16_t)GPitch, 3);
				LastOled = Now;
				OledStep++;
			}break;
			case 7: if(Now - LastOled >= 12){
				OLED_ShowSignedNum(4, 4, (int16_t)Roll, 3);
				LastOled = Now;
				OledStep++;
			}break;
			case 8: if(Now - LastOled >= 12){
				OLED_ShowSignedNum(4, 12, (int16_t)Pitch, 3);
				LastOled = Now;
				OledStep = 1;   // 8 项刷完，下一轮从头开始
			}break;
		}

		// 任务 4：AHT20 温湿度，分两步走，不用 Delay(80) 干等
		// 第一步：空闲且离上次满 1 秒 → 触发测量，记下触发时刻
		if (AhtBusy == 0 && Now - LastAht >= 1000) {
			
            LastAht = Now;  
			AHT20_Trigger(); 
			TrigTime = Now;       
			AhtBusy = 1;
        }
		// 第二步：触发后满 80ms（手册给的测量时间）再去读；读成功才回到空闲，没好就下一圈再读
		if(AhtBusy == 1 && (Now - TrigTime) >= 80){

			if(AHT20_GetData(&Temp, &Humi)){
				AhtBusy = 0;
			}

		}

		// 任务 5：LED 心跳，500ms 翻转一次；灯不闪了说明程序卡死了
		if((Now - LastLed) >= 500){
			LastLed = Now;
			LED_ChangeStatus();
		}

		// 任务 6：处理串口命令，每圈都查；帧在中断里收，这里只看标志
		// 每条命令都回应答：结果 0 成功，2 不认识的命令
		if (Get_Serial_PacketFlag()) {        // 收到完整一帧
			uint8_t cmd = Serial_GetCmd();
            if (cmd == 0x01) {               // 0x01 开始上报
				SendEnable = 1;
				Serial_SendAck(cmd, 0);
	        }else if(cmd == 0x02){           // 0x02 停止上报
				SendEnable = 0;
				Serial_SendAck(cmd, 0);
			}
			else if(cmd == 0x03){            // 0x03 擦掉 Flash 里的零偏，下次上电重新校准
				W25Q64_SectorErase(Addr);
				Serial_SendAck(cmd, 0);
			}else if(cmd == 0x04){           // 0x04 在 AA 55 帧和 VOFA 之间切换
				if(SendVofaMode == 1){
					SendVofaMode = 0;
					Serial_SendAck(cmd, 0);
				}else{
					SendVofaMode = 1;
					Serial_SendAck(cmd, 0);
				}
			}else{                           // 不认识的命令
				Serial_SendAck(cmd, 2);
			}
        }
		
	}
}
