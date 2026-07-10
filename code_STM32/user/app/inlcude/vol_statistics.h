/*
*********************************************************************************************************
*
*	模块名称 : X-y示波器界面
*	文件名称 : form_dso_xy.h
*	版    本 : V1.0
*	说    明 : 头文件
*
*	Copyright (C), 2014-2015, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __VOL_STATISTICS_H
#define __VOL_STATISTICS_H

#include "main.h"
#include "wave_disp_2ch.h"
#include "math.h"
#include "stdlib.h"
#include "form_dso_xy.h"
#include "bsp_tft_lcd.h"

//uint16_t n = (6 * g_DSO.YGridValue) / 10;



/* 定义界面结构 */
typedef struct
{
	FONT_T FontBlack;	/* 静态的文字 */
	FONT_T FontBlue;	/* 变化的文字字体 */
	FONT_T FontBtn;		/* 按钮的字体 */
	FONT_T FontBox;		/* 分组框标题字体 */

	GROUP_T Box1;

	LABEL_T LabelV;		/*　电压标尺单位 */
	LABEL_T LabelA;		/*　电流标尺单位 */

	LABEL_T Label1;		/* 平均值 */
	LABEL_T Label2;		/* 有效值 */
	LABEL_T Label3;		/* 方差 */
	LABEL_T Label4;		/* 最大值 */
	LABEL_T Label5;		/* 最小值 */
	
	
	LABEL_T LabelStatus;	/* 采集状态 */

	LABEL_T LabelProgress;	/*　时间分度 */

	LABEL_T LabelVolt0;	/*　电压 */
	LABEL_T LabelCurr0;	/*　电流 */


	LABEL_T LabelMaxVolt;	/*　电压最大值 */
	LABEL_T LabelMinVolt;	/*　电压最小值*/
	LABEL_T LabelAvgVolt;	/*　电压均值 */
	//LABEL_T LabelCurr;	/*　电流有效值 */
	LABEL_T LabelRmsVolt;	/*　电压有效值 */
	//LABEL_T LabelCurr2;	/*　电流有效值 */
	LABEL_T LabelSqlVolt;	/*  电压均方差值 */


	char FileName[13];
	uint32_t FileSize;

	uint8_t TimeStep;		/* 时间步距离， 间隔多少个样本取样一次，用于水平缩放波形 */

	int16_t AvgVolt;	/* 电压均值 */
	//int16_t AvgCurr;	/* 电流均值 */

	uint32_t RmsVolt;	/* 电压有效值 */
	//uint32_t RmsCurr;	/* 电流有效值 */
	
	uint32_t SqlVolt;	/* 电压均方差值 */
	//uint32_t SqlCurr;	/* 电流均方差值 */

	uint32_t MaxVolt;	/* 电压最大值 */
	int32_t MinVolt;	/* 电流最小值 */
	
	
	LABEL_T YEMA;
}FormV_T;


typedef struct
{
	int32_t EachVol_L[150];
	int32_t EachVol_R[150];
}VCOUNT_T;


void DSO_V(void);
void Disp_ACDC(uint8_t flag);
void DispErr(void);

#endif
