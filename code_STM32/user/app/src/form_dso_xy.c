/*
*********************************************************************************************************
*
*	模块名称 : X-Y 示波器界面
*	文件名称 : form_dso_xy.c
*	版    本 : V1.0
*	说    明 :
*	修改记录 :
*		版本号  日期       作者    说明
*		v1.0    2013-02-01 armfly  首发
*
*	Copyright (C), 2013-2014, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#include <stdio.h>
#include <stdlib.h>
#include "stm32f4xx.h"
#include "wave_disp_xy.h"		/* usb底层驱动 */
#include "form_dso_xy.h"
#include "wave_disp_2ch.h"
#include "math.h"

#include "bsp_lcd_ra8875.h"
#include "vol_statistics.h"

//#define QUICK_VIEW_EN		/* 定义此行 表示实时刷新 */

/* 框的坐标和大小 */
#define BOX1_X	5
#define BOX1_Y	5
#define BOX1_H	455
#define BOX1_W	(g_LcdWidth -  2 * BOX1_X)
#define BOX1_TEXT	"UI图"

/* 标尺单位 (V) */
#define LabelV_X  	BOX1_X + 43
#define LabelV_Y	BOX1_Y + 10
#define LabelV_TEXT	"(V)"

/* 标尺单位 (A) */
#define LabelA_X  	BOX1_X + 652
#define LabelA_Y	BOX1_Y + 415
#define LabelA_TEXT	"(A)"

/* 状态栏，显示样本文件名 */
#define LabelStatus_X  	655
#define LabelStatus_Y	30

/* 采样频率 */
#define Label1_X  		LabelStatus_X
#define Label1_Y		(LabelStatus_Y + 30)
#define Label1_TEXT		"采样频率:"
#define LabelFreq_X  	(Label1_X + 80)
#define LabelFreq_Y		Label1_Y

/* 采样时间 */
#define Label2_X  		LabelStatus_X
#define Label2_Y		(Label1_Y + 30)
#define Label2_TEXT		"采样时间:"
#define LabelTime_X  	(Label2_X + 80)
#define LabelTime_Y		Label2_Y

/* T1 */
#define Label3_X  		LabelStatus_X
#define Label3_Y		(Label1_Y + 2 * 30)
#define Label3_TEXT		"开始时间T1:"
#define LabelT1_X  		(Label3_X + 96)
#define LabelT1_Y		Label3_Y

/* T2 */
#define Label4_X  		LabelStatus_X
#define Label4_Y		(Label1_Y + 3 * 30)
#define Label4_TEXT		"结束时间T2:"
#define LabelT2_X  		(Label4_X + 96)
#define LabelT2_Y		Label4_Y

/* 电压 */
#define LabelVolt0_X  	LabelStatus_X
#define LabelVolt0_Y	(Label1_Y + 5 * 30)
#define LabelVolt0_TEXT	"电压"

/* 电压有效值 */
#define LabelVolt2_X  	LabelStatus_X
#define LabelVolt2_Y	(LabelVolt0_Y + 20)
#define LabelVolt2_TEXT	"有效值:"

/* 电流 */
#define LabelCurr0_X  	LabelStatus_X
#define LabelCurr0_Y	(LabelVolt2_Y + 40)
#define LabelCurr0_TEXT	"电流"

/* 电流有效值 */
#define LabelCurr2_X  	LabelStatus_X
#define LabelCurr2_Y	(LabelCurr0_Y + 20)
#define LabelCurr2_TEXT	"有效值:"

/* 功率 */
#define LabelPower0_X  	LabelStatus_X
#define LabelPower0_Y	(LabelCurr2_Y + 40)
#define LabelPower0_TEXT	"功率"

#define LabelPower2_X  	LabelStatus_X
#define LabelPower2_Y	(LabelPower0_Y + 20)
#define LabelPower2_TEXT	""

/* 进度栏 */
#define LabelProgress_X  	LabelStatus_X
#define LabelProgress_Y		(Label1_Y + 11 * 30)
#define LabelProgress_TEXT	"..."


static void InitFormXY(void);
static void DispFormXY(void);
static void DispT1T2(void);


FormXY_T *FormXY;

static uint32_t s_file_buf[1000];

void NewAdcToDispBufXY(void);
static void AdcFileToDispBuf(void);
static void DispProgress(char *_str, uint8_t _percent);
static void DispAvgVolt(int32_t _Avg, uint32_t _Rms);
static void DispAvgCurr(int32_t _Avg, uint32_t _Rms);
static void DispPower(uint32_t _power);

FILE_MSG_T g_tFile;

/*
*********************************************************************************************************
*	函 数 名: DSO_XY
*	功能说明: 示波器主界面，实时显示X-Y 通道波形
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
#if 0
void DSO_XY(void)
{
	uint8_t ucKeyCode;		/* 按键代码 */
	FormXY_T form;
	uint8_t fUpdateT1T2 = 0;
	uint8_t fUpdateWave = 0;
	uint8_t f_sd_file_err = 0;

	/* 初始化g_tFile.T1 T2 */
	g_tFile.T1 = 0;
	g_tFile.T2 = 3;
	
	FormXY = &form;

	InitDSO();

	//g_DSO.DrawMode = DRAW_DOT;			/* 波形绘制模式 */
	g_DSO.DrawMode = DRAW_LINE;
	g_DSO.PenSize = PEN_X2;			/* 画笔粗细 */

	InitFormXY();
	DispFormXY();

	/* 显示平均值 */
	DispAvgVolt(0,0);
	DispAvgCurr(0,0);

	if (FindLastFile(&FormXY->SampleFreq, &FormXY->SampleTime) == FL_OK)
//	if (FindNeedFile(&FormXY->SampleFreq, &FormXY->SampleTime) == FL_OK)	/* 读到索引指向的文件 */
	{
		char buf[32];
		
		/* 将采样频率和采样时间存入全局结构体 */
		g_tFile.SampleFreq = FormXY->SampleFreq;
		g_tFile.SampleTime = FormXY->SampleTime;
		
		if (FormXY->SampleTime < g_tFile.T2)
		{
			g_tFile.T2 = FormXY->SampleTime;
		}
		
		sprintf(buf, "%dkHz", FormXY->SampleFreq / 1000);
		FormXY->LabelFreq.pCaption = buf;
		LCD_DrawLabel(&FormXY->LabelFreq);

		sprintf(buf, "%dS", FormXY->SampleTime);
		FormXY->LabelTime.pCaption = buf;
		LCD_DrawLabel(&FormXY->LabelTime);

		FormXY->LabelStatus.pCaption = (char *)g_tFile.AdcFileName;
		LCD_DrawLabel(&FormXY->LabelStatus);
	}
	else
	{
		f_sd_file_err = 1;	/* 没有波形文件 */
		FormXY->LabelStatus.pCaption = "无波形文件";
		LCD_DrawLabel(&FormXY->LabelStatus);
	}

	//s_T1 = 0;
	//s_T2 =  1;

	if ((g_tFile.T1 > g_tFile.T2) || (g_tFile.T1 > FormXY->SampleTime) || (g_tFile.T2 > FormXY->SampleTime))
	{
		g_tFile.T1 = 0;
		g_tFile.T2 = 3;
	}

	fUpdateT1T2 = 1;
	fUpdateWave = 0;

#ifdef QUICK_VIEW_EN
	bsp_StartAutoTimer(1, 200);
#else
	AD7606_StopRecord();	/* 停止自动采集 */
#endif

	DispClearWinXY();	/* 清波形窗口 */
	DispFrameXY();
	while (g_flag == MS_DSO_XY)
	{
		bsp_Idle();

		DealUartComm();	/* 处理串口指令 */

		if (fUpdateT1T2 == 1)
		{
			fUpdateT1T2 = 0;

			DispT1T2();
		}

#ifdef QUICK_VIEW_EN
		if (bsp_CheckTimer(1) || (fUpdateWave == 1))
		{
			fUpdateWave = 0;

			NewAdcToDispBufXY();
			DispDsoXY();
		}
#else
		if (fUpdateWave == 1)
		{
			fUpdateWave = 0;

			if (f_sd_file_err == 0)
			{
				DispClearWinXY();	/* 清波形窗口 */
				DispFrameXY();		/* 显示波形窗口的边框和刻度线 */

				AdcFileToDispBuf();
			}

			/* 清按键FIFO */
			bsp_ClearKey();
		}
#endif

		/* 处理按键事件 */
		ucKeyCode = bsp_GetKey();
		if (ucKeyCode > 0)
		{
			/* 有键按下 */
			switch (ucKeyCode)
			{
				case KEY_DOWN_K1:			/* K1键按下 - 切换到主界面*/
					g_flag = 0;
					g_MainStatus =  MS_DSO_MAIN;
					break;
				
				case KEY_DOWN_K2:			/* K2键按下 - 开始绘制X-Y */
					g_flag = 0;
					g_MainStatus = MS_DSO_XY;
					break;
				
				case JOY_DOWN_U:			/* 摇杆UP键按下 */
					if ((g_tFile.T2 < FormXY->SampleTime) && (g_tFile.T2 < g_tFile.T1 + 10))
					{
						g_tFile.T2++;
					}
					fUpdateT1T2 = 1;
					break;

				case JOY_DOWN_D:			/* 摇杆DOWN键按下 */
					if (g_tFile.T2 > (g_tFile.T1 + 3))
					{
						g_tFile.T2--;
					}
					fUpdateT1T2 = 1;
					break;
				/* T1最小为3s */
				case JOY_DOWN_L:		/* 摇杆LEFT键按下  调节 T1  */
					if (g_tFile.T1 > 0)
					{
						g_tFile.T1--;
					}
					fUpdateT1T2 = 1;
					break;

				case JOY_DOWN_R:		/* 摇杆RIGHT键按下 调节T1  */
					if ((g_tFile.T1 < FormXY->SampleTime - 1) && (g_tFile.T1 < g_tFile.T2 - 3))
					{
						g_tFile.T1++;
					}
					fUpdateT1T2 = 1;
					break;

				case JOY_DOWN_OK:		/* 摇杆OK键按下 */
					{
							fUpdateWave = 1;
					}
					break;

				default:
					break;
			}
		}
	}
#ifdef QUICK_VIEW_EN
	bsp_StopTimer(1);		/* 停止软件定时器 */
#endif
}
#endif

/*
*********************************************************************************************************
*	函 数 名: NewAdcToDispBufXY
*	功能说明: 最新的采样数据 g_tAdcFifo.sBuf[ADC_FIFO_SIZE]; 传送到波形显示缓冲区 g_DSO.Ch1Buf 、
*			  g_DSO.Ch2Buf。 暂时未考虑FIFO不满的情况。请确保FIFO已满后再调用。
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void NewAdcToDispBufXY(void)
{
	#if 0 //TTODO
	uint32_t i;
	uint32_t pos;
	uint32_t read;
	int32_t iTemp;
	signed char ch1,ch2;
	int16_t ch1_max, ch2_max;
	uint16_t step;

	/* 获得ADC fifo 最新的位置 */
	pos = AD7606_GetFifoPos();

	step = 1;

	/* 计算起始 read指针 */
	iTemp = pos - step * XY_VIEW_POINT_NUM;	/* 2倍的缓冲区 */
	if (iTemp < 0)	/* 循环FIFO 折返的情况 */
	{
		iTemp += ADC_FIFO_SIZE;
	}
	read = iTemp;

	/* 从FIFO中取出最近的ADC数据
		 g_tAdcFifo.sBuf[] 数组的每个元素高8bit表示电流  第8bit表示电压
	*/

	ch1_max = 0;
	ch2_max = 0;
	for (i = 0 ; i < XY_VIEW_POINT_NUM; i++)	/* 2倍的缓冲区 */
	{
		ch1 = (g_tAdcFifo.sBuf[read] >> 8) & 0xFF;
		ch2 = g_tAdcFifo.sBuf[read] & 0xFF;

		if (ch1 > ch1_max)	/* 求最大值 */
		{
			ch1_max = ch1;
		}
		if (ch2 > ch2_max)	/* 求最大值 */
		{
			ch2_max = ch2;
		}

		/* 移动读指针 */
		read += step;
		if (read >= ADC_FIFO_SIZE)
		{
			read = read - ADC_FIFO_SIZE;
		}

		g_DSO.Ch1Buf[i] = ch1;
		g_DSO.Ch2Buf[i] = ch2;
	}
	#endif
}

/*
*********************************************************************************************************
*	函 数 名: AdcFileToDispBuf
*	功能说明: 读文件到显示缓冲区
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
#if 0
static void AdcFileToDispBuf(void)
{
	uint32_t i,k;
	int16_t ch1, ch2;
	uint16_t m;
	uint32_t file_start;
	uint8_t ucKeyCode;

	int64_t avg_sum_ch1, avg_sum_ch2;
	int64_t avg_sum_c1, avg_sum_c2;
	int16_t ch1_adc_avg, ch2_adc_avg;

	int64_t sql_sum_ch1, sql_sum_ch2;
	int64_t sql_sum_c1, sql_sum_c2;
	int64_t ch1_adc_sql, ch2_adc_sql;
	uint32_t ch1_adc_rms, ch2_adc_rms;

	int16_t ch1_max, ch2_max;
	int16_t ch1_min, ch2_min;

	avg_sum_ch1 = 0;
	avg_sum_ch2 = 0;

	if (g_tFile.T1 >= g_tFile.T2)
	{
		return;
	}

	/* 显示均方差值 */
	//DispSqlVolt(0);
	//DispSqlCurr(0);

	/* 显示平均值 */
	DispAvgVolt(0,0);
	DispAvgCurr(0,0);
	DispPower(0);

	DispFrameXY();	/* 显示栅格框 */

	/* 计算参与统计的样本个数 每次1K样本 */
	m = (g_tFile.T2 - g_tFile.T1) * FormXY->SampleFreq / 1000;
	file_start = (g_tFile.T1 * FormXY->SampleFreq) * 4;	/* 文件偏移,每个样本4字节 */

	/* 第一次遍历文件，计算最大值和最小值，以便于实现自动量程显示 */
	ch1_max = -32767;
	ch2_max = -32767;
	ch1_min = 32767;
	ch2_min = 32767;

	avg_sum_ch1 = 0;
	avg_sum_ch2 = 0;
	
	for (i = 0 ; i < m; i++)	/* 每次分析1K */
	{
		/* 读文件 */
		ReadFileToBuf((char *)g_tFile.AdcFileName, i *  4000 + file_start, (uint8_t *)s_file_buf, 4 * 1000);

		avg_sum_c1 = 0;
		avg_sum_c2 = 0;


		/* 整理缓冲区 */
		for (k = 0; k < 1000; k++)
		{
			ch1 = s_file_buf[k];
			ch2 = s_file_buf[k] >> 16;

			/* 求最大值，后面计算Vpp峰值 */
			if (ch1 > ch1_max)	/* 求最大值 */
			{
				ch1_max = ch1;
			}
			if (ch2 > ch2_max)	/* 求最大值 */
			{
				ch2_max = ch2;
			}

			/*　求最小值　*/
			if (ch1 < ch1_min)	/* 求最大值 */
			{
				ch1_min = ch1;
			}
			if (ch2 < ch2_min)	/* 求最大值 */
			{
				ch2_min = ch2;
			}

			avg_sum_c1 +=  ch1;		/* 用于求均方差值 */
			avg_sum_c2 +=  ch2;
		}
		avg_sum_ch1 += avg_sum_c1 / 1000;
		avg_sum_ch2 += avg_sum_c2 / 1000;

		DispProgress("正在统计:", 100* (i+1) / m);

		/* 处理按键事件 */
		ucKeyCode = bsp_GetKey();
		if (ucKeyCode > 0)
		{
			switch (ucKeyCode)
			{
			case KEY_DOWN_K1:			/* K1键按下 */
			case KEY_DOWN_K2:			/* K2键按下 - 开始绘制X-Y */
			case KEY_DOWN_K3:			/* K3键按下 - 切换到主界面 */
			case JOY_DOWN_U:			/* 摇杆UP键按下 */
			case JOY_DOWN_D:			/* 摇杆DOWN键按下 */
			case JOY_DOWN_L:		/* 摇杆LEFT键按下  调节 T1  */
			case JOY_DOWN_R:		/* 摇杆RIGHT键按下 调节T1  */
			case JOY_DOWN_OK:		/* 摇杆OK键按下 */			
				return;
			}
		}
	}
	/* ADC 平均值 （带正负号） */
	ch1_adc_avg = avg_sum_ch1 / m;
	ch2_adc_avg = avg_sum_ch2 / m;

	FormXY->AvgVolt = AdcToVoltage(ch1_adc_avg);		/* adc值转换成电压值 */
	FormXY->AvgCurr = AdcToCurrent(ch2_adc_avg);		/* adc值转换成电流值 */

	/* 2014-10-30 根据峰峰值，确定刻度 */
	{

		if (ch1_min < 0)
		{
			ch1_min = -ch1_min;
		}
		if (ch1_max < 0)
		{
			ch1_max = -ch1_max;
		}
		if (ch1_min > ch1_max)
		{
			ch1_max = ch1_min;	/* 求出最大绝对值 */
		}
		g_DSO.YGridValue = (AdcToVoltage(ch1_max) / 4);			/* Y轴一大格对应多少V */
		g_DSO.YGridValue = (g_DSO.YGridValue * 12) / 10;
		g_DSO.YGridValue = (g_DSO.YGridValue / 2 + 1) * 2;
		if (g_DSO.YGridValue < 50)
		{
			g_DSO.YGridValue = 50;
		}

		if (ch2_min < 0)
		{
			ch2_min = -ch2_min;
		}
		if (ch2_max < 0)
		{
			ch2_max = -ch2_max;
		}
		if (ch2_min > ch2_max)
		{
			ch2_max = ch2_min;	/* 求出最大绝对值 */
		}
		g_DSO.XGridValue = (AdcToCurrent(ch2_max) / 6);
		g_DSO.XGridValue = (g_DSO.XGridValue * 12) / 10;
		g_DSO.XGridValue = (g_DSO.XGridValue / 10) * 10;
		if (g_DSO.XGridValue < 330)
		{
			g_DSO.XGridValue = 330;
		}

		DispRuleXY();	/* 显示刻度 */
	}

	/* 绘制波形-计算平均值 (重新读文件) */

	/* 客户居然要求显示的平均值按绝对值计算， 没关系，按客户的来做。

		有效值计算过程： 先计算平均值(有正负的)，然后计算均方差值，最后开方得到有效值
	 */
	avg_sum_ch1 = 0;
	avg_sum_ch2 = 0;

	sql_sum_ch1 = 0;
	sql_sum_ch2 = 0;
	for (i = 0 ; i < m; i++)	/* 每次分析1K样本 */
	{
		/* 读文件 */
		ReadFileToBuf((char *)g_tFile.AdcFileName, i *  4000 + file_start, (uint8_t *)s_file_buf, 4*1000);

		avg_sum_c1 = 0;
		avg_sum_c2 = 0;

		sql_sum_c1 = 0;
		sql_sum_c2 = 0;
		/* 整理缓冲区 */
		for (k = 0; k < 1000; k++)
		{
			ch1 = s_file_buf[k];
			ch2 = s_file_buf[k] >> 16;

			g_DSO.Ch1Buf[k] = ch1;
			g_DSO.Ch2Buf[k] = ch2;

			avg_sum_c1 +=  abs(ch1);
			avg_sum_c2 +=  abs(ch2);

			//sql_sum_c1 = sql_sum_c1 + (ch1 - ch1_adc_avg) * (ch1 - ch1_adc_avg);
			//sql_sum_c2 = sql_sum_c2 + (ch2 - ch2_adc_avg) * (ch2 - ch2_adc_avg);
			sql_sum_c1 = sql_sum_c1 + ch1  * ch1;
			sql_sum_c2 = sql_sum_c2 + ch2  * ch2;
		}
		sql_sum_ch1 += sql_sum_c1 / 1000;
		sql_sum_ch2 += sql_sum_c2 / 1000;

		avg_sum_ch1 += avg_sum_c1 / 1000;
		avg_sum_ch2 += avg_sum_c2 / 1000;

		/* 送显示(叠加） */
		DispFileXY_1(1000);			/* 显示xy波形 */

		DispProgress("正在绘制:", 100* (i+1) / m);

		/* 处理按键事件 */
		ucKeyCode = bsp_GetKey();
		if (ucKeyCode > 0)
		{
			switch (ucKeyCode)
			{
			case KEY_DOWN_K1:			/* K1键按下 */
			case KEY_DOWN_K2:			/* K2键按下 - 开始绘制X-Y */
			case KEY_DOWN_K3:			/* K3键按下 - 切换到主界面 */
			case JOY_DOWN_U:			/* 摇杆UP键按下 */
			case JOY_DOWN_D:			/* 摇杆DOWN键按下 */
			case JOY_DOWN_L:		/* 摇杆LEFT键按下  调节 T1  */
			case JOY_DOWN_R:		/* 摇杆RIGHT键按下 调节T1  */
			case JOY_DOWN_OK:		/* 摇杆OK键按下 */			
				return;
			}
		}		
	}

	/* 计算ADC均方差值 */
	ch1_adc_sql = sql_sum_ch1 / m;
	ch2_adc_sql = sql_sum_ch2 / m;

	/* ADC均方差开方计算有效值ADC */
	ch1_adc_rms = sqrt(ch1_adc_sql);
	ch2_adc_rms = sqrt(ch2_adc_sql);

	/* ADC 绝对值的平均值 */
	ch1_adc_avg = avg_sum_ch1 / m;
	ch2_adc_avg = avg_sum_ch2 / m;

	/* ADC值转换为电压 电流单位， 0.1V   0.1A */
	FormXY->AvgVolt = AdcToVoltage(ch1_adc_avg);
	FormXY->AvgCurr = AdcToCurrent(ch2_adc_avg);

	/* ADC值转换为电压 电流单位， 0.1V   0.1A */
	FormXY->RmsVolt = AdcToVoltage(ch1_adc_rms);
	FormXY->RmsCurr = AdcToCurrent(ch2_adc_rms);

	/* 显示平均值 有效值 */
	DispAvgVolt(FormXY->AvgVolt, FormXY->RmsVolt);
	DispAvgCurr(FormXY->AvgCurr, FormXY->RmsCurr);
	
	/* 计算功率 率 = U（有效值）  × I（有效值） ÷ Δt  
		？？ 功率应该就是 U * I,  为何要除以时间
	*/
	FormXY->Power = FormXY->RmsVolt * FormXY->RmsCurr;
	DispPower(FormXY->Power);
	
	bsp_ClearKey();	/* 清除键盘缓冲区 */
}
#endif

/*
*********************************************************************************************************
*	函 数 名: InitFormXY
*	功能说明: 初始化界面控件
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
static void InitFormXY(void)
{
	/* 分组框标题字体 */
	FormXY->FontBox.FontCode = FC_ST_16;
	FormXY->FontBox.BackColor = CL_BTN_FACE;	/* 和背景色相同 */
	FormXY->FontBox.FrontColor = CL_BLACK;
	FormXY->FontBox.Space = 0;

	/* 字体1 用于静止标签 */
	FormXY->FontBlack.FontCode = FC_ST_16;
	FormXY->FontBlack.BackColor = CL_MASK;		/* 透明色 */
	FormXY->FontBlack.FrontColor = CL_BLACK;
	FormXY->FontBlack.Space = 0;

	/* 字体2 用于变化的文字 */
	FormXY->FontBlue.FontCode = FC_ST_16;
	FormXY->FontBlue.BackColor = CL_BTN_FACE;
	FormXY->FontBlue.FrontColor = CL_BLUE;
	FormXY->FontBlue.Space = 0;

	/* 按钮字体 */
	FormXY->FontBtn.FontCode = FC_ST_16;
	FormXY->FontBtn.BackColor = CL_MASK;		/* 透明背景 */
	FormXY->FontBtn.FrontColor = CL_BLACK;
	FormXY->FontBtn.Space = 0;

	/* 分组框 */
	FormXY->Box1.Left = BOX1_X;
	FormXY->Box1.Top = BOX1_Y;
	FormXY->Box1.Height = BOX1_H;
	FormXY->Box1.Width = BOX1_W;
	FormXY->Box1.pCaption = BOX1_TEXT;
	FormXY->Box1.Font = &FormXY->FontBox;

	/* 标签 */
	{
		FormXY->LabelV.Left = LabelV_X;
		FormXY->LabelV.Top = LabelV_Y;
		FormXY->LabelV.MaxLen = 0;
		FormXY->LabelV.pCaption = LabelV_TEXT;
		FormXY->LabelV.Font = &FormXY->FontBlack;		/* 黑色 */

		FormXY->LabelA.Left = LabelA_X;
		FormXY->LabelA.Top = LabelA_Y;
		FormXY->LabelA.MaxLen = 0;
		FormXY->LabelA.pCaption = LabelA_TEXT;
		FormXY->LabelA.Font = &FormXY->FontBlack;		/* 黑色 */

		FormXY->Label1.Left = Label1_X;
		FormXY->Label1.Top = Label1_Y;
		FormXY->Label1.MaxLen = 0;
		FormXY->Label1.pCaption = Label1_TEXT;
		FormXY->Label1.Font = &FormXY->FontBlack;		/* 黑色 */

		FormXY->Label2.Left = Label2_X;
		FormXY->Label2.Top = Label2_Y;
		FormXY->Label2.MaxLen = 0;
		FormXY->Label2.pCaption = Label2_TEXT;
		FormXY->Label2.Font = &FormXY->FontBlack;		/* 黑色 */

		FormXY->Label3.Left = Label3_X;
		FormXY->Label3.Top = Label3_Y;
		FormXY->Label3.MaxLen = 0;
		FormXY->Label3.pCaption = Label3_TEXT;
		FormXY->Label3.Font = &FormXY->FontBlack;		/* 黑色 */

		FormXY->Label4.Left = Label4_X;
		FormXY->Label4.Top = Label4_Y;
		FormXY->Label4.MaxLen = 0;
		FormXY->Label4.pCaption = Label4_TEXT;
		FormXY->Label4.Font = &FormXY->FontBlack;		/* 黑色 */

		FormXY->LabelFreq.Left = LabelFreq_X;
		FormXY->LabelFreq.Top = LabelFreq_Y;
		FormXY->LabelFreq.MaxLen = 0;
		FormXY->LabelFreq.pCaption = "10kHz";
		FormXY->LabelFreq.Font = &FormXY->FontBlue;		/* 蓝色 */

		FormXY->LabelTime.Left = LabelTime_X;
		FormXY->LabelTime.Top = LabelTime_Y;
		FormXY->LabelTime.MaxLen = 0;
		FormXY->LabelTime.pCaption = "";
		FormXY->LabelTime.Font = &FormXY->FontBlue;		/* 蓝色 */

		FormXY->LabelStatus.Left = LabelStatus_X;
		FormXY->LabelStatus.Top = LabelStatus_Y;
		FormXY->LabelStatus.MaxLen = 0;
		FormXY->LabelStatus.pCaption = "";
		FormXY->LabelStatus.Font = &FormXY->FontBlue;		/* 蓝色 */


		FormXY->LabelT1.Left = LabelT1_X;
		FormXY->LabelT1.Top = LabelT1_Y;
		FormXY->LabelT1.MaxLen = 0;
		FormXY->LabelT1.pCaption = "";
		FormXY->LabelT1.Font = &FormXY->FontBlue;		/* 蓝色 */


		FormXY->LabelT2.Left = LabelT2_X;
		FormXY->LabelT2.Top = LabelT2_Y;
		FormXY->LabelT2.MaxLen = 0;
		FormXY->LabelT2.pCaption = "";
		FormXY->LabelT2.Font = &FormXY->FontBlue;		/* 蓝色 */


		FormXY->LabelProgress.Left = LabelProgress_X;
		FormXY->LabelProgress.Top = LabelProgress_Y;
		FormXY->LabelProgress.MaxLen = 0;
		FormXY->LabelProgress.pCaption = LabelProgress_TEXT;
		FormXY->LabelProgress.Font = &FormXY->FontBlue;		/* 蓝色 */

		FormXY->LabelVolt0.Left = LabelVolt0_X;
		FormXY->LabelVolt0.Top = LabelVolt0_Y;
		FormXY->LabelVolt0.MaxLen = 0;
		FormXY->LabelVolt0.pCaption = LabelVolt0_TEXT;
		FormXY->LabelVolt0.Font = &FormXY->FontBlack;	/* 黑 */

		FormXY->LabelCurr0.Left = LabelCurr0_X;
		FormXY->LabelCurr0.Top = LabelCurr0_Y;
		FormXY->LabelCurr0.MaxLen = 0;
		FormXY->LabelCurr0.pCaption = LabelCurr0_TEXT;
		FormXY->LabelCurr0.Font = &FormXY->FontBlack;	/* 黑 */

		FormXY->LabelPower0.Left = LabelPower0_X;
		FormXY->LabelPower0.Top = LabelPower0_Y;
		FormXY->LabelPower0.MaxLen = 0;
		FormXY->LabelPower0.pCaption = LabelPower0_TEXT;
		FormXY->LabelPower0.Font = &FormXY->FontBlack;	/* 黑 */

		FormXY->LabelVolt2.Left = LabelVolt2_X;
		FormXY->LabelVolt2.Top = LabelVolt2_Y;
		FormXY->LabelVolt2.MaxLen = 0;
		FormXY->LabelVolt2.pCaption = LabelVolt2_TEXT;
		FormXY->LabelVolt2.Font = &FormXY->FontBlue;		/* 蓝色 */

		FormXY->LabelCurr2.Left = LabelCurr2_X;
		FormXY->LabelCurr2.Top = LabelCurr2_Y;
		FormXY->LabelCurr2.MaxLen = 0;
		FormXY->LabelCurr2.pCaption = LabelCurr2_TEXT;
		FormXY->LabelCurr2.Font = &FormXY->FontBlue;		/* 蓝色 */

		FormXY->LabelPower2.Left = LabelPower2_X;
		FormXY->LabelPower2.Top = LabelPower2_Y;
		FormXY->LabelPower2.MaxLen = 0;
		FormXY->LabelPower2.pCaption = LabelPower2_TEXT;
		FormXY->LabelPower2.Font = &FormXY->FontBlue;		/* 蓝色 */
	}
}

/*
*********************************************************************************************************
*	函 数 名: DispT1T2
*	功能说明: 刷新T1 T2
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void DispT1T2(void)
{
	char buf[32];

	sprintf(buf, "%dS", g_tFile.T1);
	FormXY->LabelT1.pCaption = buf;
	LCD_DrawLabel(&FormXY->LabelT1);

	sprintf(buf, "%dS", g_tFile.T2);
	FormXY->LabelT2.pCaption = buf;
	LCD_DrawLabel(&FormXY->LabelT2);
}

/*
*********************************************************************************************************
*	函 数 名: DispProgress
*	功能说明: 显示读文件的进度
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void DispProgress(char *_str, uint8_t _percent)
{
	char buf[32];

	sprintf(buf, "%s %d%%",_str, _percent);
	FormXY->LabelProgress.pCaption = buf;
	LCD_DrawLabel(&FormXY->LabelProgress);
}

/*
*********************************************************************************************************
*	函 数 名: DispAvgVolt
*	功能说明: 显示电压平均值
*	形    参: _Avg : 平均电流，单位 0.1A
*			  _Rms : 有效值电流，单位 0.1A
*	返 回 值: 无
*********************************************************************************************************
*/
static void DispAvgVolt(int32_t _Avg, uint32_t _Rms)
{
	char buf[32];

#if 0
	if (_Avg < 0)
	{
		_Avg = -_Avg;
		sprintf(buf, "平均值:-%d.%dV", _Avg / 10, _Avg % 10);
	}
	else
	{
		sprintf(buf, "平均值:%d.%dV", _Avg / 10, _Avg % 10);
	}
	FormXY->LabelVolt.pCaption = buf;
	LCD_DrawLabel(&FormXY->LabelVolt);
#endif

	sprintf(buf, "有效值:%d.%dV", _Rms / 10, _Rms % 10);
	FormXY->LabelVolt2.pCaption = buf;
	LCD_DrawLabel(&FormXY->LabelVolt2);
}

/*
*********************************************************************************************************
*	函 数 名: DispAvgCurr
*	功能说明: 显示电流平均值,有效值
*	形    参: _Avg : 平均电流，单位 0.1A
*			  _Rms : 有效值电流，单位 0.1A
*	返 回 值: 无
*********************************************************************************************************
*/
static void DispAvgCurr(int32_t _Avg, uint32_t _Rms)
{
	char buf[32];

#if 0
	/* 显示平均值 */
	if (_Avg < 0)
	{
		_Avg = -_Avg;
		sprintf(buf, "平均值:-%d.%dA", _Avg / 10, _Avg % 10);
	}
	else
	{
		sprintf(buf, "平均值:%d.%dA", _Avg / 10, _Avg % 10);
	}
	FormXY->LabelCurr.pCaption = buf;
	LCD_DrawLabel(&FormXY->LabelCurr);
#endif

	/* 显示有效值 */
	sprintf(buf, "有效值:%d.%dA", _Rms / 10, _Rms % 10);
	FormXY->LabelCurr2.pCaption = buf;
	LCD_DrawLabel(&FormXY->LabelCurr2);
}

/*
*********************************************************************************************************
*	函 数 名: DispPower
*	功能说明: 显示功率值
*	形    参:  _power 功率值，单位 0.01W
*	返 回 值: 无
*********************************************************************************************************
*/
static void DispPower(uint32_t _power)
{
	char buf[32];

	sprintf(buf, "%dW", _power / 100);
	FormXY->LabelPower2.pCaption = buf;
	LCD_DrawLabel(&FormXY->LabelPower2);
}

/*
*********************************************************************************************************
*	函 数 名: DispFormXY
*	功能说明: 显示初始界面
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void DispFormXY(void)
{
	LCD_ClrScr(CL_BTN_FACE);
	
	/* 分组框 */
	LCD_DrawGroupBox(&FormXY->Box1);

	RA8875_SetFont(RA_FONT_24, 0, 0);	/* 设置32点阵字体，行间距=0，字间距=0 */
	RA8875_SetBackColor(CL_BTN_FACE);
	RA8875_SetFrontColor(CL_BLACK);

	RA8875_DispStr(5, 0, "II");
	/* 标签 */
	{
		LCD_DrawLabel(&FormXY->Label1);
		LCD_DrawLabel(&FormXY->Label2);
		LCD_DrawLabel(&FormXY->Label3);
		LCD_DrawLabel(&FormXY->Label4);

		LCD_DrawLabel(&FormXY->LabelFreq);
		LCD_DrawLabel(&FormXY->LabelTime);
		LCD_DrawLabel(&FormXY->LabelT1);
		LCD_DrawLabel(&FormXY->LabelT2);

		LCD_DrawLabel(&FormXY->LabelStatus);

		LCD_DrawLabel(&FormXY->LabelProgress);

		LCD_DrawLabel(&FormXY->LabelVolt0);
		LCD_DrawLabel(&FormXY->LabelCurr0);

		//LCD_DrawLabel(&FormXY->LabelVolt);
		//LCD_DrawLabel(&FormXY->LabelCurr);

		LCD_DrawLabel(&FormXY->LabelV);
		LCD_DrawLabel(&FormXY->LabelA);

		//LCD_DrawLabel(&FormXY->LabelVolt2);
		//LCD_DrawLabel(&FormXY->LabelCurr2);
		
		LCD_DrawLabel(&FormXY->LabelPower0);
		LCD_DrawLabel(&FormXY->LabelPower2);
	}
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
