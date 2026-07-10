/*
*********************************************************************************************************
*
*	模块名称 : XY示波器波形显示
*	文件名称 : disp_wave_xy.h
*	版    本 : V1.0
*	说    明 : 头文件
*	修改记录 :
*		版本号  日期       作者    说明
*		v1.0    2013-02-01 armfly  首发
*
*	Copyright (C), 2014-2015, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __WAVE_DISP_XY_H
#define __WAVE_DISP_XY_H

#define VER_INFO	"X-Y Ver 1.0"


void DispFileXY_0(void);
void DispFileXY_1(uint32_t _Count);
void DispClearWinXY(void);

void DispRuleXY(void);
void DispFrameXY(void);

#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
