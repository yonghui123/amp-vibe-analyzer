/*
*********************************************************************************************************
*
*	模块名称 : 2CH波形显示
*	文件名称 : disp_wave.c
*	版    本 : V1.0
*	说    明 : 本模块处理波形绘制和擦除。 可以使用2*VIEW_POINT_NUM 的样本点来描绘X-Y波形。
*	修改记录 :
*		版本号  日期       作者    说明
*		v1.0    2014-09-23 armfly  首发
*
*	Copyright (C), 2014-2015, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/


/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include "stm32f4xx.h"
#include "bsp_font.h"
#include "wave_disp_xy.h"
#include "wave_disp_2ch.h"
#include "bsp_tft_lcd.h"

/******************** 定义X-Y波形窗口的大小和偏移位置 ********************/
/* X-Y 刻度表 */
#define Y_RULE_NUM	9
#define X_RULE_NUM	13

#define WIN_WIDTH	600
#define WIN_HEIGHT	400

#define WIN_LEFT	50
#define WIN_TOP		30

#define WIN_GRID_Y	50		/* Y轴，大栅格25像素，内分5个小点 */
#define WIN_DOT_Y	5		/* 每小点间隔6像素 */

#define WIN_GRID_X	50		/* X轴，大栅格50像素, 内分10个小点 */
#define WIN_DOT_X	5		/* 每小点间隔5像素 */

//#define WIN_Y_GRID_VALUE	30	/* Y 轴一大格对应多少伏电压 */
//#define WIN_X_GRID_VALUE	100	/* X 轴一大格对应多少A电流 */

#define WIN_X_GRID_VALUE 	g_DSO.XGridValue	/* X 轴一大格对应多少A电流 */
#define WIN_Y_GRID_VALUE 	g_DSO.YGridValue	/* Y 轴一大格对应多少伏电压 */


/******************** 函数声明 ********************/

static int32_t YAdcToPixel(int16_t _adc);
static int32_t XAdcToPixel(int16_t _adc);

extern void DrawPoints(uint16_t *x, uint16_t *y, uint16_t _usSize, uint16_t _usColor);
extern void DrawPoints2(uint16_t *x, uint16_t *y, uint16_t *x1, uint16_t *y1, uint16_t _usSize,
	uint16_t _usSize2, uint16_t _usColor);

extern int32_t AdcToVoltage(int16_t _adc);
extern int32_t AdcToCurrent(int16_t _adc);


/*
*********************************************************************************************************
*	函 数 名: DispFrameXY
*	功能说明: 显示波形窗口的边框和刻度线
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void DispFrameXY(void)
{
	uint16_t x, y;

	/* 绘制一个实线矩形框 x, y, h, w */
	LCD_DrawRect(WIN_LEFT - 1, WIN_TOP - 1, WIN_HEIGHT + 2, WIN_WIDTH + 2, WIN_GRID_COLOR);

	/* 绘制垂直刻度点 */
	for (x = 1; x < WIN_WIDTH / WIN_GRID_X; x++)
	{
		for (y = 1; y < WIN_HEIGHT / WIN_DOT_Y; y++)
		{
			LCD_PutPixel(WIN_LEFT + (x * WIN_GRID_X), WIN_TOP + (y * WIN_DOT_Y), WIN_GRID_COLOR);
		}
	}

	/* 绘制水平刻度点 */
	for (y = 1; y < WIN_HEIGHT / WIN_GRID_Y; y++)
	{
		for (x = 1; x < WIN_WIDTH / WIN_DOT_X; x++)
		{
			LCD_PutPixel(WIN_LEFT + (x * WIN_DOT_X), WIN_TOP + (y * WIN_GRID_Y), WIN_GRID_COLOR);
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: DispRuleXY
*	功能说明: 显示波形窗口的标尺文本
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void DispRuleXY(void)
{
	uint8_t i;
	FONT_T font;
	char buf[32];

	font.FontCode = FC_ST_16;
	font.BackColor = CL_BTN_FACE;	/* 和背景色相同 */
	font.FrontColor = CL_BLACK;
	font.Space = 0;

	for (i = 0; i < Y_RULE_NUM; i++)
	{
		sprintf(buf, "%d", ((4 - i) * g_DSO.YGridValue) / 10);

		LCD_DispStrEx(WIN_LEFT - 36, WIN_TOP - 6 + WIN_GRID_Y * i,  buf, &font, 33, 2);
	}

	for (i = 0; i < X_RULE_NUM; i++)
	{
		sprintf(buf, "%d", ((i - 6) * g_DSO.XGridValue) / 10);
		LCD_DispStrEx(WIN_LEFT - 15 + i * WIN_GRID_X , WIN_TOP + WIN_HEIGHT + 6,  buf, &font, 33, 1);
	}
}

/*
*********************************************************************************************************
*	函 数 名: YAdcToPixel
*	功能说明: Y轴 ADC值转换为像素单位
*	形    参: _adc : ADC值, 有符号数
*	返 回 值: 电压值
*********************************************************************************************************
*/
static int32_t YAdcToPixel(int16_t _adc)
{
	int32_t m;

	m = AdcToVoltage(_adc);
	m = (m *  WIN_GRID_Y) / WIN_Y_GRID_VALUE;
	return m;
}

/*
*********************************************************************************************************
*	函 数 名: XAdcToPixel
*	功能说明: X轴 ADC值转换为像素单位
*	形    参: _adc : ADC值, 有符号数
*	返 回 值: 电压值
*********************************************************************************************************
*/
static int32_t XAdcToPixel(int16_t _adc)
{
	int32_t m;

	m = AdcToCurrent(_adc);
	m = (m *  WIN_GRID_X) / WIN_X_GRID_VALUE;
	return m;
}

/*
*********************************************************************************************************
*	函 数 名: DispXYWave
*	功能说明: 显示xy波形
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void DispXYWave(void)
{
	int16_t i;		/* 有符号数 */
	//uint16_t pos;
	uint16_t *px_new, *py_new;	/* 新波形的坐标点数组 */
	uint16_t *pCount_new;	/* 新波形的坐标点个数 */
	uint16_t *px_old, *py_old;	/* 旧波形的坐标点数组。用于擦除 */
	uint16_t *pCount_old;	/* 旧波形的坐标点个数 */
	int16_t iTempX, iTempY;

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
		pCount_old =  &g_DSO.Ch1InvlidCount2;
	}
	else
	{
		g_DSO.BufUsed = 0;

		px_new = g_DSO.xCh1Buf2;
		py_new = g_DSO.yCh1Buf2;
		pCount_new =  &g_DSO.Ch1InvlidCount2;

		px_old = g_DSO.xCh1Buf1;
		py_old = g_DSO.yCh1Buf1;
		pCount_old =  &g_DSO.Ch1InvlidCount1;
	}

	g_DSO.Ch1VOffset = WIN_HEIGHT / 2;
	g_DSO.Ch2VOffset = WIN_WIDTH / 2;

	*pCount_new = 0;
	for (i = 0; i < 2 * VIEW_POINT_NUM; i++)	/* 使用了2倍的缓冲区来描绘X-Y波形 */
	{
		/* 处理电压(Y) */
		iTempY =  g_DSO.Ch1VOffset + YAdcToPixel(g_DSO.Ch1Buf[i]);

		/* 处理电流(X) */
		iTempX = g_DSO.Ch2VOffset + XAdcToPixel(g_DSO.Ch2Buf[i]);

		if ((iTempX < WIN_WIDTH) && (iTempY < WIN_HEIGHT) && (iTempX > 0) && (iTempY > 0))
		{
			px_new[*pCount_new] = iTempX + WIN_LEFT - 1;

			iTempY = WIN_HEIGHT - iTempY - 1;	/* 绘图函数的坐标左上角是零点（0，0）； 显示坐标做下角是零点. 因此Y轴需倒向 */
			py_new[*pCount_new] = iTempY + WIN_TOP;

			(*pCount_new)++;	/* 只统计窗口内的点个数 */
		}
		else
		{
			;	/* 显示窗口之外的点抛弃 */
		}
	}

	/* 清除旧波形 */
	DrawPoints2(px_old, py_old, px_new, py_new, *pCount_old, *pCount_new, WIN_BACK_COLOR);

	/* 显示新波形 */
	DrawPoints(px_new, py_new, *pCount_new, WIN_WAVE_COLOR);
}

/*
*********************************************************************************************************
*	函 数 名: DispXYWaveAdd
*	功能说明: 显示xy波形, 追加模式显示，不清楚旧波形。
*	形    参: _Count 样本个数
*	返 回 值: 无
*********************************************************************************************************
*/
static void DispXYWaveAdd(uint32_t _Count)
{
	int16_t i;		/* 有符号数 */
	//uint16_t pos;
	uint16_t *px_new, *py_new;	/* 新波形的坐标点数组 */
	uint16_t *pCount_new;	/* 新波形的坐标点个数 */
	int16_t iTempX, iTempY;

	static uint8_t s_DispFirst = 0;		/* 用于第一次调用时刷屏 */

	if (s_DispFirst == 0)
	{
		s_DispFirst = 1;
		//LCD_ClrScr(BACK_COLOR);  			/* 清屏，背景蓝色 */
	}

	px_new = g_DSO.xCh1Buf1;
	py_new = g_DSO.yCh1Buf1;
	pCount_new = &g_DSO.Ch1InvlidCount1;

	g_DSO.Ch1VOffset = WIN_HEIGHT / 2;
	g_DSO.Ch2VOffset = WIN_WIDTH / 2;

	*pCount_new = 0;
	for (i = 0; i < _Count; i++)	/* 使用了2倍的缓冲区来描绘X-Y波形 */
	{
		/* 处理电压(Y) */
		iTempY =  g_DSO.Ch1VOffset + YAdcToPixel(g_DSO.Ch1Buf[i]);

		/* 处理电流(X) */
		iTempX = g_DSO.Ch2VOffset + XAdcToPixel(g_DSO.Ch2Buf[i]);

		if ((iTempX < WIN_WIDTH) && (iTempY < WIN_HEIGHT) && (iTempX > 0) && (iTempY > 0))
		{
			px_new[*pCount_new] = iTempX + WIN_LEFT - 1;

			iTempY = WIN_HEIGHT - iTempY - 1;	/* 绘图函数的坐标左上角是零点（0，0）； 显示坐标做下角是零点. 因此Y轴需倒向 */
			py_new[*pCount_new] = iTempY + WIN_TOP;

			(*pCount_new)++;	/* 只统计窗口内的点个数 */
		}
		else
		{
			;	/* 显示窗口之外的点抛弃 */
		}
	}

	/* 显示新波形 */
	DrawPoints(px_new, py_new, *pCount_new, WIN_WAVE_COLOR);
}

/* 实时显示X-Y波形 */
void DispDsoXY(void)
{
	DispFrameXY();
	DispRuleXY();
	DispXYWave();
}

/* 叠加显示X-Y波形 */
void DispFileXY_0(void)
{
	DispFrameXY();
	DispRuleXY();
}

/* 叠加显示X-Y波形 */
void DispFileXY_1(uint32_t _Count)
{
	DispXYWaveAdd(_Count);
}


/* 清除波形窗口 */
void DispClearWinXY(void)
{
	LCD_Fill_Rect(WIN_LEFT - 1, WIN_TOP - 1, WIN_HEIGHT + 2, WIN_WIDTH + 2, CL_BTN_FACE);
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
