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

#ifndef __FORM_DSO_XY_H
#define __FORM_DSO_XY_H

#include "bsp_font.h"

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

	LABEL_T LabelFreq;	/* 10kHz */
	LABEL_T LabelTime;	/* 120s	*/
	LABEL_T LabelT1;	/* 1s	*/
	LABEL_T LabelT2;	/* 1s	*/

	LABEL_T LabelProgress;	/*　时间分度 */

	LABEL_T LabelVolt0;	/*　电压 */
	LABEL_T LabelCurr0;	/*　电流 */

	LABEL_T LabelVolt;	/*　电压有效值 */
	LABEL_T LabelCurr;	/*　电流有效值 */
	LABEL_T LabelVolt2;	/*　电压有效值 */
	LABEL_T LabelCurr2;	/*　电流有效值 */

	LABEL_T LabelPower0;	/*　电压有效值 */
	LABEL_T LabelPower2;	/*　电流有效值 */

	char FileName[13];
	uint32_t FileSize;
	uint32_t SampleFreq;	/* 采样频率  Hz */
	uint32_t SampleTime;	/* 采样时间  秒 */

	uint8_t TimeStep;		/* 时间步距离， 间隔多少个样本取样一次，用于水平缩放波形 */

	int16_t AvgVolt;	/* 电压均值 */
	int16_t AvgCurr;	/* 电流均值 */

	uint32_t SqlVolt;	/* 电压均方差值 */
	uint32_t SqlCurr;	/* 电流均方差值 */

	uint32_t RmsVolt;	/* 电压有效值 */
	uint32_t RmsCurr;	/* 电流有效值 */

	uint32_t Power;	/* 功率w */
	
	LABEL_T YEMA;
}FormXY_T;

typedef struct
{
	uint8_t AdcFileName[14 + 1];		/* 当前ADC记录数据的文件名 adc01.dat */
	uint32_t AdcFileSize;				/* 当前ADC记录数据的文件名 adc01.dat */
	uint32_t T1;
	uint32_t T2;
	uint32_t SampleFreq;	/* 采样频率  Hz */
	uint32_t SampleTime;	/* 采样时间  秒 */
	char Mode[3];
}FILE_MSG_T;

/* 窗体背景色 */
#define FORM_BACK_COLOR		CL_BTN_FACE

void DSO_XY(void);

//extern FormXY_T *FormXY;
extern FILE_MSG_T g_tFile;

#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/

