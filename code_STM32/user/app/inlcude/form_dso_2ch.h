/*
*********************************************************************************************************
*
*	模块名称 : 双通道示波器界面
*	文件名称 : form_dso_2ch.h
*	版    本 : V1.0
*	说    明 : 头文件
*
*	Copyright (C), 2014-2015, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __FORM_DSO_2CH_H
#define __FORM_DSO_2CH_H

#include "bsp_font.h"
#include "arm_math.h"
/* 5V时对应的加速度，单位m/(s*s) */
//#define ACCELE		490	   /*  100mv - 9.8m/(s*s)  100mv对应9.8米每二次方秒 */
#define TEST_LENGTH_SAMPLES 1024 
/* 采样频率调节 */
#define FREQNUM		8

/* 定义界面结构 */
typedef struct
{
	FONT_T FontBlack;	/* 静态的文字 */
	FONT_T FontBlue;	/* 变化的文字字体 */
	FONT_T FontBtn;		/* 按钮的字体 */
	FONT_T FontBox;		/* 分组框标题字体 */

	BUTTON_T Btn0;
	BUTTON_T Btn1;
	BUTTON_T Btn2;
	BUTTON_T Btn3;
	BUTTON_T Btn4;
	BUTTON_T Btn5;
	
	//LABEL_T Div;		/* 采样分度 */
	LABEL_T Freq;		/* 采样频率 */
	LABEL_T Volt;		/* 当前电压值 */
	LABEL_T Offset;		/* 加速偏移 */
	LABEL_T PeakValue;	/* 峰值 */
	LABEL_T PeakToPeak;	/* 峰峰值 */
	LABEL_T RmsValue;	/* 有效值 */
	LABEL_T Average;	/* 平均值 */
	LABEL_T Warn;		/* 速度报警状态 */
	LABEL_T WarnFreq;	/* 速度报警频率 */
	LABEL_T WarnPeak;	/* 报警峰值 */

	LABEL_T LabelFreq;	/* 10kHz */
	LABEL_T LabelStatus;	/* 采集状态 */
		
	uint32_t SampleFreq;	/* 采样频率  Hz */
	uint32_t SampleTime;	/* 采样时间  秒 */

	uint8_t TimeStep;		/* 时间步距离， 间隔多少个样本取样一次，用于水平缩放波形 */

	uint8_t Samping;		/* 1表示正在采集 */

	uint32_t SampleCountNow;	/* 已采集的样本个数 */
	uint32_t SampleCountWant;	/* 期望采集的样本个数 0表示无限制 */

	uint32_t TimeCount;		/* 秒计数器，用于显示 */

	uint32_t VoltVpp;	/*　电压峰值 */
	uint32_t CurrVpp;	/*　电流峰值 */

	uint32_t VoltRms;	/*　正弦波电压有效值 */
	uint32_t CurrRms;	/*　正弦波电压有效值 */
	
	int32_t VoltAvg;	/* 电压平均值 */
	int32_t CurrAvg;	/* 电流平均值 */

}FormAD_T;

/* 窗体背景色 */
#define FORM_BACK_COLOR		CL_BTN_FACE

void DSO_Main(void);
//extern float32_t testOutput[TEST_LENGTH_SAMPLES2]; 
extern float32_t SpeedInput_fft[TEST_LENGTH_SAMPLES];
extern FormAD_T *FormAD;

extern const uint16_t g_Freq[FREQNUM];
extern uint8_t g_FreqId;

#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/

