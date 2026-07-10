/*
*********************************************************************************************************
*
*	模块名称 : 2CH波形显示
*	文件名称 : disp_wave.c
*	版    本 : V1.0
*	说    明 : 本模块处理波形绘制和擦除
*	修改记录 :
*		版本号  日期       作者    说明
*		v1.0    2014-09-23 armfly  首发
*
*	Copyright (C), 2014-2015, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/


/* Includes ------------------------------------------------------------------*/
#include "stdio.h"
#include "stm32f4xx.h"
#include "wave_disp_2ch.h"
#include "form_dso_xy.h"
#include "form_dso_2ch.h"
#include "bsp_tft_lcd.h"
#include "param.h"

/******************** 定义2CH 波形窗口的大小和偏移位置 ********************/
#define WIN1_WIDTH	600		//700
#define WIN1_HEIGHT	156

#define WIN1_LEFT	48
#define WIN1_TOP	54

#define WIN2_WIDTH	WIN1_WIDTH
#define WIN2_HEIGHT	180
#define WIN2_LEFT	WIN1_LEFT
#define WIN2_TOP	242

#define GRID_Y		30		/* Y轴，大栅格25像素，内分5个小点 */
#define DOT_Y		6		/* 每小点间隔6像素 */

#define GRID_X		50		/* X轴，大栅格50像素, 内分10个小点 */
#define DOT_X		5		/* 每小点间隔5像素 */

//#define CH1_GRID_Y_VALUE	40	/* CH1 Y轴一大格对应多少伏电压 */
//#define CH2_GRID_Y_VALUE	200	/* CH2 Y轴一大格对应多少伏电压 */
//#define CH1_GRID_Y_VALUE	g_Ch1GridTable[g_DSO.Ch1GridIndex]	/* CH1 Y轴一大格对应多少伏电压 */
//#define CH2_GRID_Y_VALUE	g_Ch2GridTable[g_DSO.Ch2GridIndex]	/* CH2 Y轴一大格对应多少伏电压 */

/******************** 函数声明 ********************/

void DrawPoints(uint16_t *x, uint16_t *y, uint16_t _usSize, uint16_t _usColor);
void DrawPoints2(uint16_t *x, uint16_t *y, uint16_t *x1, uint16_t *y1, uint16_t _usSize,
	uint16_t _usSize2, uint16_t _usColor);
void DrawPoints3(uint16_t *x1, uint16_t *y1, uint16_t *x2, uint16_t *y2, uint16_t _usSize,
	uint16_t _usColor1, uint16_t _usColor2);
void DrawPoints4(uint16_t *x1, uint16_t *y1, uint16_t *x2, uint16_t *y2, uint16_t _usSize, uint16_t _usColor);
static int32_t Ch1AdcToPixel(int16_t _adc);
static int32_t Ch2AdcToPixel(int16_t _adc);

DSO_T g_DSO;	/* 全局变量，是一个结构体 */

int32_t CH1_GRID_Y_VALUE;
int32_t CH2_GRID_Y_VALUE;

/*
*********************************************************************************************************
*	函 数 名: DispFrame
*	功能说明: 显示波形窗口的边框和刻度线
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void DispFrame1(void)
{
	uint16_t x, y;

	/* 调整波形框的尺寸 */
	#if 1
		/* 绘制一个实线矩形框 x, y, h, w */
		LCD_DrawRect(WIN1_LEFT - 1, WIN1_TOP - 1, WIN1_HEIGHT + 2, WIN1_WIDTH + 2, CL_BLUE6);

		/* 绘制垂直刻度点 */
		for (x = 1; x < WIN1_WIDTH / GRID_X; x++)
		{
			for (y = 1; y < WIN1_HEIGHT / DOT_Y; y++)
			{
				LCD_PutPixel(WIN1_LEFT + (x * GRID_X), WIN1_TOP + (y * DOT_Y), WIN_GRID_COLOR);
			}
		}

		/* 绘制水平刻度点 */
		for (y = 1; y < WIN1_HEIGHT / GRID_Y + 1; y++)
		{
			for (x = 1; x < WIN1_WIDTH / DOT_X; x++)
			{
				LCD_PutPixel(WIN1_LEFT + (x * DOT_X), WIN1_TOP + (y * GRID_Y) - 2 * DOT_Y, WIN_GRID_COLOR);
			}
		}
	#else 		
		/* 绘制一个实线矩形框 x, y, h, w */
		LCD_DrawRect(WIN1_LEFT - 1, WIN1_TOP - 1, WIN1_HEIGHT + 2, WIN1_WIDTH + 2, CL_BLUE6);

		/* 绘制垂直刻度点 */
		for (x = 1; x < WIN1_WIDTH / GRID_X; x++)
		{
			for (y = 1; y < WIN1_HEIGHT / DOT_Y; y++)
			{
				LCD_PutPixel(WIN1_LEFT + (x * GRID_X), WIN1_TOP + (y * DOT_Y), WIN_GRID_COLOR);
			}
		}

		/* 绘制水平刻度点 */
		for (y = 1; y < WIN1_HEIGHT / GRID_Y; y++)
		{
			for (x = 1; x < WIN1_WIDTH / DOT_X; x++)
			{
				LCD_PutPixel(WIN1_LEFT + (x * DOT_X), WIN1_TOP + (y * GRID_Y), WIN_GRID_COLOR);
			}
		}
	#endif
}

/*
*********************************************************************************************************
*	函 数 名: DispFrame2
*	功能说明: 显示波形窗口的边框和刻度线
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void DispFrame2(void)
{
	uint16_t x, y;

	/* 绘制一个实线矩形框 x, y, h, w */
	LCD_DrawRect(WIN2_LEFT - 1, WIN2_TOP - 1, WIN2_HEIGHT + 2, WIN2_WIDTH + 2, CL_BLUE6);

	/* 绘制垂直刻度点 */
	for (x = 1; x < WIN2_WIDTH / GRID_X; x++)
	{
		for (y = 1; y < WIN2_HEIGHT / DOT_Y; y++)
		{
			LCD_PutPixel(WIN2_LEFT + (x * GRID_X), WIN2_TOP + (y * DOT_Y), WIN_GRID_COLOR);
		}
	}

	/* 绘制水平刻度点 */
	for (y = 1; y < WIN2_HEIGHT / GRID_Y; y++)
	{
		for (x = 1; x < WIN2_WIDTH / DOT_X; x++)
		{
			LCD_PutPixel(WIN2_LEFT + (x * DOT_X), WIN2_TOP + (y * GRID_Y), WIN_GRID_COLOR);
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: DispRule1
*	功能说明: 显示波形窗口的标尺文本
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void DispRule1(void)
{
	uint8_t i;
	FONT_T font;
	char buf[32];

	font.FontCode = FC_ST_16;
	font.BackColor = CL_BLUE5;	/* 和背景色相同 */
	font.FrontColor = CL_WHITE;
	font.Space = 0;
	
	for (i = 0; i < 5; i++)
	{
		int32_t count;
		count = 2 * (g_DSO.Ch1GridValue / 1000) - i * (g_DSO.Ch1GridValue / 1000);
		if (count >= 0)
		{
			sprintf(buf, "%d", count);
		}
		else 
		{
			sprintf(buf, "-%d", -count);
		}
		LCD_DispStrEx(WIN1_LEFT - 36, WIN1_TOP - 2 + GRID_Y * i + 2 * DOT_Y,  buf, &font, 33, 2);
	}
}

/*
*********************************************************************************************************
*	函 数 名: DispRule2
*	功能说明: 显示波形窗口的标尺文本
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void DispRule2(void)
{
	uint8_t i;
	FONT_T font;
	char buf[32];
	int32_t count;
	uint16_t grad;
	
	font.FontCode = FC_ST_16;
	font.BackColor = CL_BLUE5;	/* 和背景色相同 */
	font.FrontColor = CL_WHITE;
	font.Space = 0;
	
	/* 速度刻度 */
	for (i = 0; i < 7; i++)
	{

		count = (3 - i) * g_DSO.Ch2GridValue;
		
		if (count >= 0)
		{
			sprintf(buf, "%4d", count);
			LCD_DispStrEx(WIN2_LEFT - 36, WIN2_TOP - 10 + GRID_Y * i,  buf, &font, 33, 2);
		}
		else 
		{
			sprintf(buf, "%5d", count);
			LCD_DispStrEx(WIN2_LEFT - 44, WIN2_TOP - 10 + GRID_Y * i,  buf, &font, 33, 2);
		}
	}
	
	
	/* fft刻度 */
	font.FrontColor = CL_YELLOW;
	
	grad = 100;	//50 * FormAD->SampleFreq / TEST_LENGTH_SAMPLES;
		
	for (i = 0; i < 13; i++)
	{
		count = i * grad;
		
		sprintf(buf, "%3d", count);
		LCD_DispStrEx(WIN2_LEFT - 28 + (i * 50), WIN2_TOP + WIN2_HEIGHT + 4,  buf, &font, 33, 2);
	}
	
	for (i = 0; i < 6; i++)
	{
		count = (5 - i) * g_DSO.Ch3GridValue;
		
		sprintf(buf, "%-5d", count);
		LCD_DispStrEx(WIN2_LEFT + WIN2_WIDTH + 2, WIN2_TOP - 10 + GRID_Y * i,  buf, &font, 33, 2);
	}
}

/*
*********************************************************************************************************
*	函 数 名: AdcToVoltage
*	功能说明: ADC值转换为真实电压(0.1 V)
*	形    参: _adc : ADC值, 有符号数
*	返 回 值: 电压值(1mV单位)
*********************************************************************************************************
*/
int32_t AdcToVoltage(int16_t _adc)
{
	int32_t m;

	m = _adc *  g_DSO.VoltRate;
	m = m / 10000;
	return m ;
}

/*
*********************************************************************************************************
*	函 数 名: AdcToCurrent
*	功能说明: ADC值转换为真实电流(A)
*	形    参: _adc : ADC值, 有符号数
*	返 回 值: 电压值(0.1A单位)
*********************************************************************************************************
*/
int32_t AdcToCurrent(int16_t _adc)
{
	int32_t m;

	m = _adc *  g_DSO.CurrRate;
	m = m / 10000;
	return m ;
}

/* ADC转换为加速度，单位为0.001 m / (s * s) */
int32_t AdcToAccele(int16_t _adc)
{
	int32_t m;
	uint32_t accele = 0;

	m = AdcToVoltage(_adc);				/* 此时m的单位也为0.001V */
	accele = g_tParam.Rate_5V * m / 5;
	return accele;
}


/*
*********************************************************************************************************
*	函 数 名: Ch1AdcToPixel
*	功能说明: ADC值转换为像素单位
*	形    参: _adc : ADC值, 有符号数
*	返 回 值: 电压值
*********************************************************************************************************
*/
static int32_t Ch1AdcToPixel(int16_t _adc)
{
	/*
		GRID_Y = 30

		CH1_GRID_Y_VALUE	40	  CH1 Y轴一大格对应多少V电压 0.1V 单位
		CH2_GRID_Y_VALUE	200	  CH2 Y轴一大格对应多少A电流 0.1A 单位
	*/
	int32_t m;

	m = AdcToAccele(_adc);			/* 此时m的单位也为0.001 m / (s * s) */
	m = (m *  GRID_Y) / CH1_GRID_Y_VALUE;
	return m;
}

/*
*********************************************************************************************************
*	函 数 名: Ch2AdcToPixel
*	功能说明: ADC值转换为像素单位
*	形    参: _adc : ADC值, 有符号数
*	返 回 值: 电压值
*********************************************************************************************************
*/
static int32_t Ch2AdcToPixel(int16_t _adc)
{
	/*
		GRID_Y = 30

		CH1_GRID_Y_VALUE	40	  CH1 Y轴一大格对应多少V电压
		CH2_GRID_Y_VALUE	200	  CH2 Y轴一大格对应多少A电流
	*/
	//return (AdcToCurrent(_adc) * GRID_Y) / CH2_GRID_Y_VALUE;  --- 编译器不支持这种写法：负数除法

	int32_t m;

	m = AdcToCurrent(_adc);
	m = (m *  GRID_Y) / CH2_GRID_Y_VALUE;
	return m;
}

/*
*********************************************************************************************************
*	函 数 名: SpeedToPixel
*	功能说明: 速度转换成点
*	形    参: _speed : 速度值, 单位为0.1 mm / (s * s)
*	返 回 值: 电压值
*********************************************************************************************************
*/
static int32_t SpeedToPixel(int32_t _speed)
{
	int32_t m;
	
	m = _speed * GRID_Y / g_DSO.Ch2GridValue;
	
	return m;
}

/*
*********************************************************************************************************
*	函 数 名: FFTToPixel
*	功能说明: 傅立叶波形转换成点
*	形    参: _speed : 速度值, 单位为0.1 mm / (s * s)
*	返 回 值: 电压值
*********************************************************************************************************
*/
static int32_t FFTToPixel(uint16_t _fft)
{
	int32_t m;
	
	m = _fft * GRID_Y / g_DSO.Ch3GridValue;
	
	return m;
}


/*
*********************************************************************************************************
*	函 数 名: DispWave1
*	功能说明: 显示CH1 波形
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/

void DispWave1(void)
{
	int16_t i;		/* 有符号数 */
	uint16_t *px_new, *py_new, *pCount_new;
	uint16_t *px_old, *py_old, *pCount_old;
	int16_t iTemp;
	
	static uint8_t s_DispFirst = 0;		/* 用于第一次调用时刷屏 */

	if (s_DispFirst == 0)
	{
		s_DispFirst = 1;
		//LCD_ClrScr(BACK_COLOR);  			/* 清屏，背景蓝色 */
	}

	if (g_DSO.BufUsed == 0)
	{
		g_DSO.BufUsed = 1;

		px_new = g_DSO.xCh1Buf1;
		py_new = g_DSO.yCh1Buf1;
		pCount_new = &g_DSO.Ch1InvlidCount1;

		px_old = g_DSO.xCh1Buf2;
		py_old = g_DSO.yCh1Buf2;
		pCount_old = &g_DSO.Ch1InvlidCount2;
	}
	else
	{
		g_DSO.BufUsed = 0;

		px_new = g_DSO.xCh1Buf2;
		py_new = g_DSO.yCh1Buf2;
		pCount_new =  &g_DSO.Ch1InvlidCount2;

		px_old = g_DSO.xCh1Buf1;
		py_old = g_DSO.yCh1Buf1;
		pCount_old = &g_DSO.Ch1InvlidCount1;
	}


	g_DSO.Ch1VOffset = WIN1_HEIGHT / 2;

	*pCount_new = 0;
	
	CH1_GRID_Y_VALUE = g_DSO.Ch1GridValue;  /* Y轴1大格对应多少，目前单位为0.1mm */
	
	#if 1
		for (i = 0; i < VIEW_POINT_NUM; i++)
		{
			/* Ch2Buf[] 中是有符号数 */
			//iTemp = g_DSO.Ch2VOffset + (g_DSO.Ch2Buf[i * g_DSO.AdcStep + 1])  / g_DSO.Ch2Zoom;
			iTemp = g_DSO.Ch1VOffset + Ch1AdcToPixel(g_DSO.Ch1Buf[i + TEST_LENGTH_SAMPLES - VIEW_POINT_NUM]);	/* 将浮点数强制转换成16位有符号整数 */

			px_new[i] = WIN1_LEFT + i;
			
			if ((iTemp < WIN1_HEIGHT) && (iTemp > 0))
			{
				iTemp = WIN1_HEIGHT - iTemp - 1;	/* 绘图函数的坐标左上角是零点（0，0）； 显示坐标做下角是零点. 因此Y轴需倒向 */
				py_new[i] = iTemp + WIN1_TOP;

				//(*pCount_new)++;	/* 只统计窗口内的点个数 */
			}
			else if (iTemp >= WIN1_HEIGHT)
			{
				py_new[i] = WIN1_TOP;
			}
			else 
			{
				py_new[i] = WIN1_TOP + WIN1_HEIGHT - 2;
			}
			
		}
	
		DrawPoints3(px_old, py_old, px_new, py_new, VIEW_POINT_NUM, WIN_BACK_COLOR, CL_YELLOW);
	#else 
	//	CH2_GRID_Y_VALUE =	g_DSO.Ch2GridValue;
		for (i = 0; i < VIEW_POINT_NUM; i++)
		{
			/* Ch1Buf[] 中是有符号数 */
			//iTemp = g_DSO.Ch1VOffset + (g_DSO.Ch1Buf[i * g_DSO.AdcStep + 1])  / g_DSO.Ch1Zoom;
			iTemp = g_DSO.Ch1VOffset + Ch1AdcToPixel(g_DSO.Ch1Buf[i + TEST_LENGTH_SAMPLES - VIEW_POINT_NUM]);

			if ((iTemp < WIN1_HEIGHT) && (iTemp > 0))
			{
				px_new[*pCount_new] = WIN1_LEFT + i ;		/* px_new[0] = 55 ；  px_new[1] = 56 ...*/

				iTemp = WIN1_HEIGHT - iTemp - 1;	/* 绘图函数的坐标左上角是零点（0，0）； 显示坐标做下角是零点. 因此Y轴需倒向 */
				py_new[*pCount_new] = iTemp + WIN1_TOP;		/* py_new[0] = 第一个电压值对应的屏幕上Y轴的位置；  py_new[1] 对应第二个*/

				(*pCount_new)++;	/* 只统计窗口内的点个数 */
			}
			else
			{
				;	/* 显示窗口之外的点抛弃 */
			}
		}
		
			/* 清除上帧波形 */
		DrawPoints2(px_old, py_old, px_new, py_new, *pCount_old, *pCount_new, WIN_BACK_COLOR);

		/* 显示更新的波形 */
		DrawPoints(px_new, py_new, *pCount_new, CL_YELLOW);	//WIN_WAVE_COLOR
	#endif
}

static void CaltPeakAvg(void);
/* 计算速度波形Y轴每隔的值 */
void Get_SpeedMax(void)
{
	uint16_t i;
	int16_t max = -32767, min = 32767;
	float value;
	float avg_sum = 0;
	float filter_sum = 0;
	int64_t rms_sum = 0;
	

	/* 求滤波成分 */
	for (i = TEST_LENGTH_SAMPLES - VIEW_POINT_NUM; i < TEST_LENGTH_SAMPLES; i++)
	{
		value = g_DSO.Ch2Buf[i];
		
		if (value < min)
		{
			min = value;
		}
		
		if (value > max)
		{
			max = value;
		}
		
		filter_sum += value;
	}
	
	/* 求平均值(滤波直流成分) */
	g_DSO.Filter = (int32_t)filter_sum / VIEW_POINT_NUM;
	
	if (max < 0)
	{
		max = -max;
	}
	
	if (min < 0)
	{
		min = -min;
	}
	
	if (max > min)
	{
		g_DSO.Ch2GridValue = max;
	}
	else
	{
		g_DSO.Ch2GridValue = min;
	}
	
	/* 滤除直流 */
	if (g_DSO.Filter >= 0)
	{
		g_DSO.Ch2GridValue = g_DSO.Ch2GridValue - g_DSO.Filter;
	}
	else 
	{
		g_DSO.Ch2GridValue = g_DSO.Ch2GridValue - (-g_DSO.Filter);		// - (-g_DSO.Average)
	}
	
	#if 1
	max = -32767;
	min = 32767;
	
	for (i = TEST_LENGTH_SAMPLES - VIEW_POINT_NUM; i < TEST_LENGTH_SAMPLES; i++)		/* 滤波。再求最大值、最小值 */
	{
		if (g_DSO.Filter >= 0)
		{
			g_DSO.Ch2Buf[i] = g_DSO.Ch2Buf[i] - g_DSO.Filter;
		}
		else 
		{
			g_DSO.Ch2Buf[i] = g_DSO.Ch2Buf[i] - g_DSO.Filter;			// - (-g_DSO.Average)
		}
		
		value = g_DSO.Ch2Buf[i];
		
		if (value < min)
		{
			min = value;
		}
		
		if (value > max)
		{
			max = value;
		}
		
		avg_sum += g_DSO.Ch2Buf[i];
		
		
		/* 平方和(求速度有效值) */
		rms_sum += g_DSO.Ch2Buf[i] * g_DSO.Ch2Buf[i];
	}
	
	/* (求速度有效值) */
	rms_sum = rms_sum / VIEW_POINT_NUM;
	g_DSO.RmsValue = sqrt(rms_sum);				/* 有效值 */
	
	if (max < 0)
	{
		max = -max;
	}
	
	if (min < 0)
	{
		min = -min;
	}	
	#endif

	g_DSO.PeakValue = g_DSO.Ch2GridValue;			/* 峰值 */
//	g_DSO.PeakToPeak = max + min;					/* 峰峰值 */
	
	/* 2015-12-23 速度1的平均值*/
	//g_tSData.NowSpeed = g_DSO.PeakValue;
	
	/* 求平均值(速度均值) */
	//	g_DSO.Average = avg_sum / VIEW_POINT_NUM;
	
	CaltPeakAvg();			/* 计算峰值平均值 */
	
	g_DSO.old_speed = 0;
	
	
	g_DSO.Ch2GridValue = (g_DSO.Ch2GridValue / 3);
	g_DSO.Ch2GridValue = (g_DSO.Ch2GridValue * 12) / 10;
	g_DSO.Ch2GridValue = (g_DSO.Ch2GridValue / 2 + 1) * 2;
	
	if (g_DSO.Ch2GridValue < 10)
	{
		g_DSO.Ch2GridValue = 10;
	}
}

/*  2015-12-23  计算峰值平均值 */
static void CaltPeakAvg(void)		
{
	uint16_t num;
	uint16_t SampleFreq;
	uint16_t i, j;
	int16_t PMax = -32767, PMin = 32767;	/* 速度峰值 */
	int32_t AMax = -32767, AMin = 32767; 	/* 加速度峰值 */
	float Svalue;
	float Avalue;
	uint32_t sum;
	uint32_t Asum;
	
	SampleFreq = g_Freq[g_FreqId];
	
	num =  SampleFreq * 10 / g_DSO.RangeMax;		/* num为振动一个周期占的采样点数量,g_DSO.RangeMax精确到0.1 所以要 *10 */
	
	/* 2015-12-23 1024个数据峰值平均值 */
	/* 
		num表示一个振动周期的采样点数,VIEW_POINT_NUM / num 表示600个样本有几个这样的振动周期。
		每次循环num个样本，用来分析一个周期的最大最小值。最后将最大最小值加到sum中来求平均值
	*/
	/* 说明：PMax 和 PMin 其实把滤直流成分中和掉了。 因为 PMax + PMin在滤波前和滤波后的值是不变的 */
	sum = 0;
	Asum = 0;
	for (i = 0; i < TEST_LENGTH_SAMPLES / num; i++)		
	{
		if (num * (i + 1) > TEST_LENGTH_SAMPLES)		/* 处理异常值 */
		{
			break;
		}
		
	/* 1.求速度峰值平均值 */	
		{	
			for (j = num * i; j < num * (i + 1); j++)
			{	
				Svalue = g_DSO.Ch2Buf[j];
				
				if (Svalue < PMin)
				{
					PMin = Svalue;
				}
				
				if (Svalue > PMax)
				{
					PMax = Svalue;
				}
			}
			
			if (PMax < 0)					
			{
				PMax = (-PMax);				/* 得出一个周期的最大值 */
			}
			
			if (PMin < 0)
			{
				PMin = (-PMin);				/* 得出一个周期的最小值 */
			}
			
			sum = sum + PMax + PMin;		/* 求的1024个样本中的峰值总和，PMax + PMin 的值不受滤直流影响,因此正好能得到想要的数据 */
		}
		
	/* 2.求加速度峰值平均值 */	
		{	
			for (j = num * i; j < num * (i + 1); j++)
			{	
				Avalue = AdcToAccele(g_DSO.Ch1Buf[j]);
				
				if (Avalue < AMin)
				{
					AMin = Avalue;
				}
				
				if (Avalue > AMax)
				{
					AMax = Avalue;
				}
			}
			
			if (AMax < 0)					
			{
				AMax = (-AMax);				/* 得出一个周期的最大值 */
			}
			
			if (AMin < 0)
			{
				AMin = (-AMin);				/* 得出一个周期的最小值 */
			}
			
			Asum = Asum + AMax + AMin;		/* 求的1024个样本中的峰值总和，PMax + PMin 的值不受滤直流影响,因此正好能得到想要的数据 */
		}
	}	
	
	/* 振动速度1的1024个显示数据峰值平均值 */
	#if 0 //TOTO
	g_tSData.NowSpeed1 = sum / (i * 2);			/* 速度峰值平均值 */
	g_tSData.NowAccel1 = Asum / (i * 2);			/* 加速度峰值平均值 */
	#endif
}
	

/*
*********************************************************************************************************
*	函 数 名: DispWave2
*	功能说明: 显示CH2 波形
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void DispWave2(void)
{
	int16_t i;		/* 有符号数 */
	uint16_t *px_new, *py_new, *pCount_new;
	uint16_t *px_old, *py_old, *pCount_old;
	int16_t iTemp;

	static uint8_t s_DispFirst = 0;		/* 用于第一次调用时刷屏 */

	if (s_DispFirst == 0)
	{
		s_DispFirst = 1;
		//LCD_ClrScr(BACK_COLOR);  			/* 清屏，背景蓝色 */
	}

	if (g_DSO.BufUsed == 0)
	{
		//g_DSO.BufUsed = 1;

		px_new = g_DSO.xCh2Buf1;
		py_new = g_DSO.yCh2Buf1;
		pCount_new = &g_DSO.Ch2InvlidCount1;

		px_old = g_DSO.xCh2Buf2;
		py_old = g_DSO.yCh2Buf2;
		pCount_old = &g_DSO.Ch2InvlidCount2;
	}
	else
	{
		//g_DSO.BufUsed = 0;

		px_new = g_DSO.xCh2Buf2;
		py_new = g_DSO.yCh2Buf2;
		pCount_new =  &g_DSO.Ch2InvlidCount2;

		px_old = g_DSO.xCh2Buf1;
		py_old = g_DSO.yCh2Buf1;
		pCount_old = &g_DSO.Ch2InvlidCount1;
	}

	/* 计算显示坐标数组 */
	//if (g_tParam.VoltZoom > 8
	//g_DSO.AdcStep = 1;		/* 取样间隔 */

	g_DSO.Ch2VOffset = WIN2_HEIGHT / 2;
	*pCount_new = 0;

	CH2_GRID_Y_VALUE = g_DSO.Ch2GridValue;
#if 1	
	for (i = 0; i < VIEW_POINT_NUM; i++)
	{
		/* Ch2Buf[] 中是有符号数 */
		//iTemp = g_DSO.Ch2VOffset + (g_DSO.Ch2Buf[i * g_DSO.AdcStep + 1])  / g_DSO.Ch2Zoom;
		iTemp = g_DSO.Ch2VOffset + SpeedToPixel((int32_t)g_DSO.Ch2Buf[i + TEST_LENGTH_SAMPLES - VIEW_POINT_NUM]);		/* 将浮点数强制转换成16位有符号整数 */

		px_new[i] = WIN2_LEFT + i;
		
		if ((iTemp < WIN2_HEIGHT) && (iTemp > 0))
		{
			iTemp = WIN2_HEIGHT - iTemp - 1;	/* 绘图函数的坐标左上角是零点（0，0）； 显示坐标做下角是零点. 因此Y轴需倒向 */
			py_new[i] = iTemp + WIN2_TOP;

			//(*pCount_new)++;	/* 只统计窗口内的点个数 */
		}
		else if (iTemp >= WIN2_HEIGHT)
		{
			py_new[i] = WIN2_TOP;
		}
		else 
		{
			py_new[i] = WIN2_TOP + WIN2_HEIGHT - 2;
		}
		
	}
	
	DrawPoints3(px_old, py_old, px_new, py_new, VIEW_POINT_NUM, WIN_BACK_COLOR, WIN_WAVE_COLOR);
#else 
	for (i = 0; i < VIEW_POINT_NUM; i++)
	{
		/* Ch2Buf[] 中是有符号数 */
		//iTemp = g_DSO.Ch2VOffset + (g_DSO.Ch2Buf[i * g_DSO.AdcStep + 1])  / g_DSO.Ch2Zoom;
		iTemp = g_DSO.Ch2VOffset + SpeedToPixel((int32_t)g_DSO.Ch2Buf[i + TEST_LENGTH_SAMPLES - VIEW_POINT_NUM]);		/* 将浮点数强制转换成16位有符号整数 */

		if ((iTemp < WIN2_HEIGHT) && (iTemp > 0))
		{
			px_new[*pCount_new] = WIN2_LEFT + i;

			iTemp = WIN2_HEIGHT - iTemp - 1;	/* 绘图函数的坐标左上角是零点（0，0）； 显示坐标做下角是零点. 因此Y轴需倒向 */
			py_new[*pCount_new] = iTemp + WIN2_TOP;

			(*pCount_new)++;	/* 只统计窗口内的点个数 */
		}
		else
		{
			;	/* 显示窗口之外的点抛弃 */
		}
	}
	
	/* 清除上帧波形 */
	DrawPoints2(px_old, py_old, px_new, py_new, *pCount_old, *pCount_new, WIN_BACK_COLOR);

	/* 显示更新的波形 */
	DrawPoints(px_new, py_new, *pCount_new, WIN_WAVE_COLOR);
#endif
}


/*
*********************************************************************************************************
*	函 数 名: DispWave3
*	功能说明: 显示FFT 波形
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void DispWave3(void)
{
	int16_t i;		/* 有符号数 */
	uint16_t *px_new, *py_new, *pCount_new;
	uint16_t *px_old, *py_old, *pCount_old;
	int16_t iTemp;

	static uint8_t s_DispFirst = 0;		/* 用于第一次调用时刷屏 */

	if (s_DispFirst == 0)
	{
		s_DispFirst = 1;
		//LCD_ClrScr(BACK_COLOR);  			/* 清屏，背景蓝色 */
	}

	if (g_DSO.BufUsed == 0)
	{
		//g_DSO.BufUsed = 1;

		px_new = g_DSO.xCh3Buf1;
		py_new = g_DSO.yCh3Buf1;
		pCount_new = &g_DSO.Ch3InvlidCount1;

		px_old = g_DSO.xCh3Buf2;
		py_old = g_DSO.yCh3Buf2;
		pCount_old = &g_DSO.Ch3InvlidCount2;
	}
	else
	{
		//g_DSO.BufUsed = 0;

		px_new = g_DSO.xCh3Buf2;
		py_new = g_DSO.yCh3Buf2;
		pCount_new =  &g_DSO.Ch3InvlidCount2;

		px_old = g_DSO.xCh3Buf1;
		py_old = g_DSO.yCh3Buf1;
		pCount_old = &g_DSO.Ch3InvlidCount1;
	}

	/* 计算显示坐标数组 */
	//if (g_tParam.VoltZoom > 8
	//g_DSO.AdcStep = 1;		/* 取样间隔 */

	g_DSO.Ch2VOffset = WIN2_HEIGHT / 6;
	*pCount_new = 0;

	CH2_GRID_Y_VALUE = g_DSO.Ch2GridValue;
	
#if 0
	for (i = 0; i < VIEW_POINT_NUM; i++)
	{
		uint16_t uX;
		uX = i * FormAD->SampleFreq	/ 2000;	//50 * FormAD->SampleFreq / TEST_LENGTH_SAMPLES
		
		/* Ch2Buf[] 中是有符号数 */
		iTemp = g_DSO.Ch2VOffset + FFTToPixel(SpeedInput_fft[i]);

		px_new[i] = (WIN2_LEFT + uX);	//(WIN2_LEFT + i)	
			
		if ((iTemp < WIN2_HEIGHT) && (iTemp > 0) && uX < 600)
		{
			iTemp = WIN2_HEIGHT - iTemp - 1;	/* 绘图函数的坐标左上角是零点（0，0）； 显示坐标做下角是零点. 因此Y轴需倒向 */
			py_new[i] = iTemp + WIN2_TOP;
		}
		else if (iTemp >= WIN1_HEIGHT && uX < 600)
		{
			py_new[i] = WIN1_TOP;
		}
		else if (iTemp <= 0 && uX < 600)
		{
			py_new[i] = WIN1_TOP + WIN1_HEIGHT - 2;
		}
		else 
		{
			;
		}
	}
	
	DrawPoints3(px_old, py_old, px_new, py_new, VIEW_POINT_NUM, WIN_BACK_COLOR, CL_YELLOW);
#else 
	for (i = 0; i < 512; i++)		/* fft傅里叶只得到512个数据。其他88个数据手动补0 */
	{
		uint16_t uX;
		uX = i * FormAD->SampleFreq	/ 2000;	//50 * FormAD->SampleFreq / TEST_LENGTH_SAMPLES
		
		/* Ch2Buf[] 中是有符号数 */
		iTemp = g_DSO.Ch2VOffset + FFTToPixel(SpeedInput_fft[i]);

		if ((iTemp < WIN2_HEIGHT) && (iTemp > 0) && uX < 600)
		{
			px_new[*pCount_new] = (WIN2_LEFT + uX);	//(WIN2_LEFT + i)	

			iTemp = WIN2_HEIGHT - iTemp - 1;	/* 绘图函数的坐标左上角是零点（0，0）； 显示坐标做下角是零点. 因此Y轴需倒向 */
			py_new[*pCount_new] = iTemp + WIN2_TOP;

			(*pCount_new)++;	/* 只统计窗口内的点个数 */
		}
		else
		{
			;	/* 显示窗口之外的点抛弃 */
		}
	}
	
	/* 因为波形已拉升，后面的波形可能被抛弃了。所以要补上最后一个点 */
	if ((*pCount_new) - 2 >= 0)
	{
		px_new[(*pCount_new) - 2] = WIN2_LEFT + 599;
		py_new[(*pCount_new) - 2] = WIN2_TOP + WIN2_HEIGHT - g_DSO.Ch2VOffset - 1;
	}
	
	/* 清除上帧波形 */
	//DrawPoints2(px_old, py_old, px_new, py_new, *pCount_old, *pCount_new, WIN_BACK_COLOR);
	DrawPoints4(px_old, py_old, px_new, py_new, *pCount_old, WIN_BACK_COLOR);
	
	/* 显示更新的波形 */
	DrawPoints(px_new, py_new, *pCount_new, CL_YELLOW);
#endif
}




/*
*********************************************************************************************************
*	函 数 名: Fill_WaveBack
*	功能说明: 填充波形底色
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void Fill_WaveBack(void)
{
	/* 填充波形窗口背景颜色*/
	LCD_Fill_Rect(WIN2_LEFT - 1, WIN2_TOP - 1, WIN2_HEIGHT + 2, WIN2_WIDTH + 2, CL_BLACK);
	
	/* 填充波形窗口背景颜色*/
	#if 1
		LCD_Fill_Rect(WIN1_LEFT - 1, WIN1_TOP - 1, WIN1_HEIGHT + 2, WIN1_WIDTH + 2, CL_BLACK);
	#else
		LCD_Fill_Rect(WIN1_LEFT - 1, WIN1_TOP - 1, WIN1_HEIGHT + 2, WIN1_WIDTH + 2, CL_BLACK);
	#endif

}

/*
*********************************************************************************************************
*	函 数 名: DispDSO
*	功能说明: 刷新整个窗口
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void Disp_Uint(uint16_t _uX, uint16_t _uY,char *_uint);
void DispDSO(void)
{
	DispFrame1();	/* 绘制刻度框 */
	DispFrame2();	/* 绘制刻度框 */

	
	Disp_Uint(WIN1_LEFT, WIN1_TOP - 20, "加速度(m / (s * s))");
	Disp_Uint(WIN2_LEFT, WIN2_TOP - 20, "速度(mm / s)");
//	DispChInfo();	/* 显示通道信息(幅度，时间档位) */
	
	Get_SpeedMax();	/* 处理速度数据 */
	
	DispRule1();
	DispRule2();

	DispWave1();	/* 显示波形1 */
	DispWave2();	/* 显示波形2 */
	DispWave3();	/* fft波形 */
}

/*
*********************************************************************************************************
*	函 数 名: Disp_Uint
*	功能说明: 显示表图单位
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void Disp_Uint(uint16_t _uX, uint16_t _uY,char *_uint)
{
	FONT_T font;

	font.FontCode = FC_ST_16;
	font.BackColor = CL_BLUE5;	/* 和背景色相同 */
	font.FrontColor = CL_WHITE;
	font.Space = 0;
	
	LCD_DispStrEx(_uX, _uY, _uint, &font, 33, 2);
}

/*
*********************************************************************************************************
*	函 数 名: InitDSO
*	功能说明: 初始化DSO全局参数变量
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void InitDSO(void)
{
	/* 波形绘图缓冲区清空 */
	g_DSO.Ch1InvlidCount1 = 0;
	g_DSO.Ch1InvlidCount2 = 0;

	g_DSO.Ch2InvlidCount1 = 0;
	g_DSO.Ch2InvlidCount2 = 0;

#if 0
	g_DSO.Ch1VScale = 1000;	/* 缺省是1V */
	g_DSO.Ch2VScale = 1000;	/* 缺省是1V */

	g_DSO.Ch1VOffset = 0; 	/* 通道1 GND线位置 */
	g_DSO.Ch2VOffset = 0; /* 通道2 GND线位置 */

	g_DSO.SampleFreq = 1000000;		/* 采样频率，固定为1M */

	g_DSO.Ch1Zoom = 1;
	g_DSO.Ch2Zoom = 1;

	g_DSO.DrawMode = DRAW_LINE;			/* 波形绘制模式 */
	g_DSO.PenSize = PEN_X2;			/* 画笔粗细 */

	g_DSO.AdcStep = 1;		/* 取样间隔 */
#endif

	g_DSO.Ch1GridIndex = 0;
	g_DSO.Ch2GridIndex = 0;
}

/*
*********************************************************************************************************
*	函 数 名: DrawPoints
*	功能说明: 绘制样本点，显示波形。
*	形    参：
*			x, y     ：坐标数组
*			_usColor ：颜色
*	返 回 值: 无
*********************************************************************************************************
*/
void DrawPoints(uint16_t *x, uint16_t *y, uint16_t _usSize, uint16_t _usColor)
{
	uint16_t i;

	if (g_DSO.DrawMode == DRAW_LINE)		/* 连线模式 (其实采样点足够密集的话，无需连线模式很占资源) */
	{
		switch (g_DSO.PenSize)
		{
			case PEN_X1:	/* 单笔画 */
				for (i = 0 ; i < _usSize - 1; i++)
				{
					LCD_DrawLine(x[i], y[i], x[i + 1], y[i + 1], _usColor);
				}
				break;

			case PEN_X2:	/* 双笔画 */
				for (i = 0 ; i < _usSize - 2; i++)
				{
					LCD_DrawLine(x[i], y[i], x[i + 1], y[i + 1], _usColor);
					LCD_DrawLine(x[i], y[i]+1, x[i + 1], y[i + 1]+1, _usColor);
				}
				break;

			case  PEN_X3:	/* 三笔画 */
				for (i = 0 ; i < _usSize - 3; i++)
				{
					LCD_DrawLine(x[i], y[i], x[i + 1], y[i + 1], _usColor);
					LCD_DrawLine(x[i], y[i]+1, x[i + 1], y[i + 1]+1, _usColor);
					LCD_DrawLine(x[i], y[i]+2, x[i + 1], y[i + 1]+2, _usColor);
				}
				break;
		}
	}
	else	/* 打点模式 */
	{
		switch (g_DSO.PenSize)
		{
			case PEN_X1:	/* 单笔画 */
				for (i = 0 ; i < _usSize - 1; i++)
				{
					LCD_PutPixel(x[i], y[i], _usColor);
				}
				break;

			case PEN_X2:	/* 双笔画 */
				for (i = 0 ; i < _usSize - 1; i++)
				{
					LCD_PutPixel(x[i], y[i], _usColor);
					LCD_PutPixel(x[i]+1, y[i], _usColor);
					LCD_PutPixel(x[i], y[i]+1, _usColor);
					LCD_PutPixel(x[i]+1, y[i]+1, _usColor);
				}
				break;


			case  PEN_X3:	/* 三笔画 */
				for (i = 0 ; i < _usSize - 1; i++)
				{
					LCD_PutPixel(x[i], y[i], _usColor);
					LCD_PutPixel(x[i]+1, y[i], _usColor);
					LCD_PutPixel(x[i]+2, y[i], _usColor);

					LCD_PutPixel(x[i], y[i], _usColor);
					LCD_PutPixel(x[i], y[i]+1, _usColor);
					LCD_PutPixel(x[i], y[i]+2, _usColor);

					LCD_PutPixel(x[i]+1, y[i]+1, _usColor);
					LCD_PutPixel(x[i]+1, y[i]+2, _usColor);
					LCD_PutPixel(x[i]+2, y[i]+1, _usColor);
					LCD_PutPixel(x[i]+2, y[i]+2, _usColor);
				}
				break;
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: DrawPoints2
*	功能说明: 用于擦除上个波形，只擦除不需要的点，尽量避免闪烁
*	形    参：
*			x, y     ：等待擦除的点坐标数组
*			x1, y1   : 新波形的点坐标数组
*			_usColor ：颜色
*	返 回 值: 无
*********************************************************************************************************
*/
void DrawPoints2(uint16_t *x, uint16_t *y, uint16_t *x1, uint16_t *y1, uint16_t _usSize,
	uint16_t _usSize2, uint16_t _usColor)
{
	uint16_t i;

	if (g_DSO.DrawMode == DRAW_LINE)		/* 连线模式 (其实采样点足够密集的话，无需连线模式很占资源) */
	{
		/* 画线模式非常复杂，不支持选择性地擦除 */
		switch (g_DSO.PenSize)
		{
			case PEN_X1:	/* 单笔画 */
				for (i = 0 ; i < _usSize - 1; i++)
				{
					LCD_DrawLine(x[i], y[i], x[i + 1], y[i + 1], _usColor);
				}
				break;

			case PEN_X2:	/* 双笔画 */
				for (i = 0 ; i < _usSize - 2; i++)
				{
					LCD_DrawLine(x[i], y[i], x[i + 1], y[i + 1], _usColor);
					LCD_DrawLine(x[i], y[i]+1, x[i + 1], y[i + 1]+1, _usColor);
				}
				break;

			case  PEN_X3:	/* 三笔画 */
				for (i = 0 ; i < _usSize - 3; i++)
				{
					LCD_DrawLine(x[i], y[i], x[i + 1], y[i + 1], _usColor);
					LCD_DrawLine(x[i], y[i]+1, x[i + 1], y[i + 1]+1, _usColor);
					LCD_DrawLine(x[i], y[i]+2, x[i + 1], y[i + 1]+2, _usColor);
				}
				break;
		}
	}
	else	/* 打点模式 */
	{
		switch (g_DSO.PenSize)
		{
			case PEN_X1:	/* 单笔画 */
				for (i = 0 ; i < _usSize - 1; i++)
				{
					if ((x[i] != x1[i]) || (y[i] != y1[i]) || (_usSize != _usSize2))
					{
						LCD_PutPixel(x[i], y[i], _usColor);
					}
				}
				break;

			case PEN_X2:	/* 双笔画 */
				for (i = 0 ; i < _usSize - 1; i++)
				{
					if ((x[i] != x1[i]) || (y[i] != y1[i]) || (_usSize != _usSize2))
					{
						LCD_PutPixel(x[i], y[i], _usColor);
						LCD_PutPixel(x[i]+1, y[i], _usColor);
						LCD_PutPixel(x[i], y[i]+1, _usColor);
						LCD_PutPixel(x[i]+1, y[i]+1, _usColor);
					}
				}
				break;


			case  PEN_X3:	/* 三笔画 */
				for (i = 0 ; i < _usSize - 1; i++)
				{
					if ((x[i] != x1[i]) || (y[i] != y1[i]) || (_usSize != _usSize2))
					{
						LCD_PutPixel(x[i], y[i], _usColor);
						LCD_PutPixel(x[i]+1, y[i], _usColor);
						LCD_PutPixel(x[i]+2, y[i], _usColor);

						LCD_PutPixel(x[i], y[i], _usColor);
						LCD_PutPixel(x[i], y[i]+1, _usColor);
						LCD_PutPixel(x[i], y[i]+2, _usColor);

						LCD_PutPixel(x[i]+1, y[i]+1, _usColor);
						LCD_PutPixel(x[i]+1, y[i]+2, _usColor);
						LCD_PutPixel(x[i]+2, y[i]+1, _usColor);
						LCD_PutPixel(x[i]+2, y[i]+2, _usColor);
					}
				}
				break;
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: DrawPoints3
*	功能说明: 画点去点同时进行，先清除第2条线，画第1条线。清x2，再画x1
*	形    参：
*			x, y     ：等待擦除的点坐标数组
*			x1, y1   : 新波形的点坐标数组
*			_usColor ：颜色
*	返 回 值: 无
*********************************************************************************************************
*/
void DrawPoints3(uint16_t *x1, uint16_t *y1, uint16_t *x2, uint16_t *y2, uint16_t _usSize,
	uint16_t _usColor1, uint16_t _usColor2)
{
	uint16_t i;

	if (g_DSO.DrawMode == DRAW_LINE)		/* 连线模式 (其实采样点足够密集的话，无需连线模式很占资源) */
	{
		/* 清除第一条线 */
		LCD_DrawLine(x1[0], y1[0], x1[0 + 1], y1[0 + 1], _usColor1);
		LCD_DrawLine(x1[0], y1[0]+1, x1[0 + 1], y1[0 + 1]+1, _usColor1);
		
		/* 双笔画 */
		for (i = 0 ; i < _usSize - 3; i++)
		{
			//if (x1[i] == x2[i] && y1[i] == y2[i] && x1[i + 1] == x2[i + 1] && y1[i + 1] == y2[i + 1])
			
			/* 取消一个点的波形 */
			LCD_DrawLine(x1[i+1], y1[i+1], x1[i + 2], y1[i + 2], _usColor1);
			LCD_DrawLine(x1[i+1], y1[i+1]+1, x1[i + 2], y1[i + 2]+1, _usColor1);
			
			//for (j = 0; j < 1000; j++);
			/* 画新波形 */
			LCD_DrawLine(x2[i], y2[i], x2[i + 1], y2[i + 1], _usColor2);
			LCD_DrawLine(x2[i], y2[i]+1, x2[i + 1], y2[i + 1]+1, _usColor2);
		}
		
		/* 画最后一条线 */
		LCD_DrawLine(x2[_usSize-3], y2[_usSize-3], x2[_usSize-2], y2[_usSize-2], _usColor2);
		LCD_DrawLine(x2[_usSize-3], y2[_usSize-3]+1, x2[_usSize-2], y2[_usSize-2]+1, _usColor2);
#if 0		
		for (i = 0 ; i < _usSize2 - 2; i++)
		{
			/* 画新波形 */
			LCD_DrawLine(x2[i], y2[i], x2[i + 1], y2[i + 1], _usColor2);
			LCD_DrawLine(x2[i], y2[i]+1, x2[i + 1], y2[i + 1]+1, _usColor2);
		}
#endif
	}
}

/*
*********************************************************************************************************
*	函 数 名: DrawPoints4
*	功能说明: 如果线与之前的相同，则不去除（FFT专用）
*	形    参：
*			x, y     ：等待擦除的点坐标数组
*			x1, y1   : 新波形的点坐标数组
*			_usColor ：颜色
*	返 回 值: 无
*********************************************************************************************************
*/
void DrawPoints4(uint16_t *x1, uint16_t *y1, uint16_t *x2, uint16_t *y2, uint16_t _usSize, uint16_t _usColor)
{
	uint16_t i;

	if (g_DSO.DrawMode == DRAW_LINE)		/* 连线模式 (其实采样点足够密集的话，无需连线模式很占资源) */
	{
		/* 双笔画 */
		for (i = 0 ; i < _usSize - 2; i++)
		{
			if (x1[i] == x2[i] && y1[i] == y2[i] && x1[i + 1] == x2[i + 1] && y1[i + 1] == y2[i + 1])
			{
				;
			}
			else
			{
				/* 取消一个点的波形 */
				LCD_DrawLine(x1[i], y1[i], x1[i + 1], y1[i + 1], _usColor);
				LCD_DrawLine(x1[i], y1[i]+1, x1[i + 1], y1[i + 1]+1, _usColor);
			}
			//for (j = 0; j < 1000; j++);
			/* 画新波形 */
		//	LCD_DrawLine(x2[i], y2[i], x2[i + 1], y2[i + 1], _usColor2);
		//	LCD_DrawLine(x2[i], y2[i]+1, x2[i + 1], y2[i + 1]+1, _usColor2);
		}
	}
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
