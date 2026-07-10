/*
*********************************************************************************************************
*
*	模块名称 : 应用程序参数模块
*	文件名称 : param.h
*	版    本 : V1.0
*	说    明 : 头文件
*
*	Copyright (C), 2012-2013, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __PARAM_H
#define __PARAM_H

#include <stdint.h>

/* 下面2行宏只能选择其一 */
#define PARAM_SAVE_TO_EEPROM			/* 参数存储到外部的EEPROM (AT24C128) */
//#define PARAM_SAVE_TO_FLASH		/* 参数存储到CPU内部Flash */

#ifdef PARAM_SAVE_TO_EEPROM
	#define PARAM_ADDR		0			/* 参数区地址 */
#endif

#ifdef PARAM_SAVE_TO_FLASH
	#define PARAM_ADDR		ADDR_FLASH_SECTOR_3			/* 0x0800C000 中间的16KB扇区用来存放参数 */
	//#define PARAM_ADDR	 ADDR_FLASH_SECTOR_11		/* 0x080E0000 Flash最后128K扇区用来存放参数 */
#endif

#define PARAM_VER			0x00000108				/* 参数版本 107*/

/* 全局参数 */
typedef struct
{
	uint32_t ParamVer;			/* 参数区版本控制（可用于程序升级时，决定是否对参数区进行升级） */

	/* LCD背光亮度 */
	uint8_t ucBackLight;

	/* 触摸屏校准参数 */
	//{
		uint8_t TouchDirection;	/* 屏幕方向 0-3  0表示横屏，1表示横屏180° 2表示竖屏 3表示竖屏180° */

		uint8_t XYChange;		/* X, Y 是否交换， 1表示i切换，0表示不切换  */

		uint16_t usAdcX1;	/* 左上角 */
		uint16_t usAdcY1;
		uint16_t usAdcX2;	/* 右下角 */
		uint16_t usAdcY2;
		uint16_t usAdcX3;	/* 左下角 */
		uint16_t usAdcY3;
		uint16_t usAdcX4;	/* 右上角 */
		uint16_t usAdcY4;

		uint16_t usLcdX1;	/* 校准时，屏幕坐标 */
		uint16_t usLcdY1;	/* 校准时，屏幕坐标 */
		uint16_t usLcdX2;	/* 校准时，屏幕坐标 */
		uint16_t usLcdY2;	/* 校准时，屏幕坐标 */
		uint16_t usLcdX3;	/* 校准时，屏幕坐标 */
		uint16_t usLcdY3;	/* 校准时，屏幕坐标 */
		uint16_t usLcdX4;	/* 校准时，屏幕坐标 */
		uint16_t usLcdY4;	/* 校准时，屏幕坐标 */
	//}
	
	uint8_t Addr;			/* RS485地址 */
	uint16_t Baud;			/* RS485波特率 */
	
	uint16_t Alarm;			/* 报警速度值 */
	int16_t OffsetAdc;			/* 加速度偏移 */
	uint16_t Rate_5V;		/* 5V对应的倍率 */
	uint8_t OffsetStep;	/* 偏移步距 */
	
	uint8_t Mode;		/* 工作模式 */
}
PARAM_T;

extern PARAM_T g_tParam;

void LoadParam(void);
void SaveParam(void);

#endif
