/*
*author:MCD Application Team
*date:2026-03-04
*/
#ifndef __BSP_RA8875_H
#define __BSP_RA8875_H

#include <stdint.h>

/* 
*FSMC 的 nEx（例如 NE1、NE2……）是低电平有效（active low）
*SN74LV139 也是低电平有效，AG是低电平使能，A+B高电平是1，选中的管脚输出低电平
*8080硬件总线 （安富莱RA8875屏缺省模式
*安富莱STM32-V5开发板接线方法：

*PD0/FSMC_D2
*PD1/FSMC_D3
*PD4/FSMC_NOE		--- 读控制信号，OE = Output Enable ， N 表示低有效
*PD5/FSMC_NWE		--- 写控制信号，WE = Output Enable ， N 表示低有效
*PD8/FSMC_D13
*PD9/FSMC_D14
*PD10/FSMC_D15
*PD13/FSMC_A18		--- 地址 RS
*PD14/FSMC_D0
*PD15/FSMC_D1

*PE4/FSMC_A20		--- 和主片选一起译码
*PE5/FSMC_A21		--- 和主片选一起译码
*PE7/FSMC_D4
*PE8/FSMC_D5
*PE9/FSMC_D6
*PE10/FSMC_D7
*PE11/FSMC_D8
*PE12/FSMC_D9
*PE13/FSMC_D10
*PE14/FSMC_D11
*PE15/FSMC_D12

*PG12/FSMC_NE4		--- 主片选（TFT, OLED 和 AD7606）

*PC3/TP_INT			--- 触摸芯片中断 （对于RA8875屏，是RA8875输出的中断)

---- 下面是 TFT LCD接口其他信号 （FSMC模式不使用）----
*PD3/LCD_BUSY		--- 触摸芯片忙       （RA8875屏是RA8875芯片的忙信号)
*PF6/LCD_PWM			--- LCD背光PWM控制  （RA8875屏无需此脚，背光由RA8875控制)

*PI10/TP_NCS			--- 触摸芯片的片选		(RA8875屏无需SPI接口触摸芯片）
*PB3/SPI3_SCK		--- 触摸芯片SPI时钟		(RA8875屏无需SPI接口触摸芯片）
*PB4/SPI3_MISO		--- 触摸芯片SPI数据线MISO(RA8875屏无需SPI接口触摸芯片）
*PB5/SPI3_MOSI		--- 触摸芯片SPI数据线MOSI(RA8875屏无需SPI接口触摸芯片）
*/
#define RA8875_BASE		((uint32_t)(0x60000000 | 0x00000000))
/* FSMC 16位总线模式下，FSMC_A18口线对应物理地址A19 */
#define RA8875_REG		*(volatile uint16_t *)(RA8875_BASE +  (1 << (18 + 1)))
#define RA8875_RAM		*(volatile uint16_t *)(RA8875_BASE)

#define RA8875_RAM_ADDR		RA8875_BASE

void     RA8875_Delaly1us(void);
void     RA8875_Delaly1ms(void);
uint16_t RA8875_ReadID(void);
void     RA8875_WriteCmd(uint8_t _ucRegAddr);
void     RA8875_WriteData(uint8_t _ucRegValue);
uint8_t  RA8875_ReadData(void);
void     RA8875_WriteData16(uint16_t _usRGB);
uint16_t RA8875_ReadData16(void);
uint8_t  RA8875_ReadStatus(void);
uint32_t RA8875_GetDispMemAddr(void);

/* 四线SPI接口模式 */
void RA8875_InitSPI(void);
void RA8875_HighSpeedSPI(void);
void RA8875_LowSpeedSPI(void);

uint8_t RA8875_ReadBusy(void);

#endif /* __RA8875_H */
