/*
*********************************************************************************************************
*
*	模块名称 : 双通道示波器界面
*	文件名称 : form_dso_2ch.c
*	版    本 : V1.0
*	说    明 :
*	修改记录 :
*		版本号  日期       作者    说明
*		v1.0    2013-02-01 armfly  首发
*
*	Copyright (C), 2014-2015, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#include <stdio.h>
#include <stdlib.h>
#include "arm_const_structs.h"
#include "arm_math.h"
#include "math.h"
#include "stm32f4xx.h"
#include "form_dso_2ch.h"
#include "form_dso_xy.h"
#include "wave_disp_2ch.h"
#include "app_cfg.h"
#include "param.h"
#include "bsp_lcd_ra8875.h"


#include "bsp_tft_lcd.h"

#define ERR_SD_FILE		"Err1"		/* SD卡文件写错误 */
#define ERR_FIFO_FULL	"Err2"		/* FIFO溢出，SD卡写入速度慢 */

#define TEXT_0S		"0s(手动停止采样)"

#define Btn0_X		690
#define Btn0_Y		200		//140
#define Btn0_H		40
#define Btn0_W		110
#define Btn0_Text	"采样频率+"

#define Btn1_X		Btn0_X
#define Btn1_Y		Btn0_Y + 50
#define Btn1_H		Btn0_H
#define Btn1_W		Btn0_W
#define Btn1_Text	"采样频率-"

#define Btn2_X		Btn1_X
#define Btn2_Y		Btn1_Y + 50
#define Btn2_H		Btn1_H
#define Btn2_W		Btn1_W
#define Btn2_Text	"偏移+"

#define Btn3_X		Btn2_X
#define Btn3_Y		Btn2_Y + 50
#define Btn3_H		Btn2_H
#define Btn3_W		Btn2_W
#define Btn3_Text	"偏移-"

#define Btn4_X		Btn3_X
#define Btn4_Y		Btn3_Y + 50
#define Btn4_H		Btn3_H
#define Btn4_W		Btn3_W
#define Btn4_Text	"设置参数"

#define Btn5_X		678
#define Btn5_Y		140
#define Btn5_H		30
#define Btn5_W		100
#define Btn5_Text	"确定"

/* 标签1 */
#define LabelInfo_X		20		//5
#define LabelInfo_Y		2		//0
#define LabelInfo_H		26		//5
#define LabelInfo_W		72		//0

#if 0
/* 时间分度 ms/div */
#define LabelTimeDiv_X  	LabelInfo_X + 80
#define LabelTimeDiv_Y		LabelInfo_Y
#define LabelTimeDiv_TEXT	"200uS/div"
#endif

/* 采样频率值 kHz */
#define LabelFreq_X  	LabelInfo_X + 80
#define LabelFreq_Y		LabelInfo_Y

/* 标签2 */
#define Label2_X		260		
#define Label2_Y		2		
#define Label2_H		26		
#define Label2_W		72		

/* 电压平均值 V */
#define LabelVolt_X  	Label2_X + 80
#define LabelVolt_Y		Label2_Y
#define LabelVolt_TEXT	""

/* 标签3 */
#define Label3_X		500		
#define Label3_Y		2		
#define Label3_H		26		
#define Label3_W		72		

/* 采样时间(设置采样时间) s */
#define LabelTime_X  	Label3_X + 80			
#define LabelTime_Y		Label3_Y

/* 峰值 */
#define LabelPeak_X		20
#define LabelPeak_Y		445
#define LabelPeak_H		26
#define LabelPeak_W		72
/* 峰峰值 */
#define LabelRms_X		LabelPeak_X + 240
#define LabelRms_Y		LabelPeak_Y
#define LabelRms_H		LabelPeak_H
#define	LabelRms_W		LabelPeak_W
/* 振动频率 */
#define LabelValue_X	LabelRms_X + 240
#define LabelValue_Y	LabelRms_Y
#define LabelValue_H	LabelRms_H
#define	LabelValue_W	LabelRms_W

/* 标签4 */
#define Label4_X		680		
#define Label4_Y		30		
#define Label4_H		26		
#define Label4_W		72		

/* 当前速度 */
#define LabelSpeed_X  	Label4_X + 80			
#define LabelSpeed_Y	Label4_Y

/* 电流平均值 A */
#define LabelCurr_X  	LabelInfo_X + 105		
#define LabelCurr_Y		LabelInfo_Y
#define LabelCurr_TEXT	""

/* 运行状态(已采集样本时间) s */
#define LabelStatus_X  	LabelTime_X + 60			
#define LabelStatus_Y	LabelInfo_Y

/* 报警提示 */
#define LabelWarn_X		Label3_X + 170
#define LabelWarn_Y		Label3_Y + 10
#define LabelWarn_H		Label3_H
#define LabelWarn_W		36 
/* 报警频率 */
#define LabelWarnFreq_X		LabelWarn_X
#define LabelWarnFreq_Y		LabelWarn_Y + 40
#define LabelWarnFreq_H		LabelWarn_H
#define LabelWarnFreq_W		LabelWarn_W 
/* 报警峰值 */
#define LabelWarnPeak_X		LabelWarnFreq_X
#define LabelWarnPeak_Y		LabelWarnFreq_Y + 40
#define LabelWarnPeak_H		LabelWarnFreq_H
#define LabelWarnPeak_W		LabelWarnFreq_W 
/* 确认框坐标 */
#define QR_X		250
#define QR_Y		120
#define QR_W		300
#define QR_H		180


static void InitFormAD(void);
static void DispADInitFace(void);

static void DispLableStatus(char *_pBUf);
static void DispLableTimeDiv(void);
static void DispLableVolt(int32_t _Avg, uint32_t _Vpp, uint32_t _Rms);
static void DispLableFreq(uint32_t _value);
static void DispLableTimeLen(uint32_t _value);
static void DispVer(void);
static void DispErr(void);
void AnalyzeWave(void);
static void DispOffset(void);
static void DispLableSpeed(void);
static void Accele_To_Speed(void);
static void Deal_FFT_Wave(void);
static void Fast_FFT(void);
static void DispWarn(void);
static void WarnFrame(void);

#define DSOSCREEN_LENGTH		600 

float32_t SpeedInput_fft[TEST_LENGTH_SAMPLES];		/* fft输入缓存区 */
float32_t SpeedOutput_fft[TEST_LENGTH_SAMPLES];		/* fft输出缓存区 */

FormAD_T *FormAD;
static uint32_t s_UserSetFreq = 1000;	/* 保存用户设置的频率，初始值为1kHz */

uint32_t g_SampleTime = 10;

uint8_t g_ChangeFlag = 0;

arm_rfft_fast_instance_f32 S;
uint32_t fftSize = 2048; 
uint32_t ifftFlag = 0; 

const uint16_t g_Freq[FREQNUM] = {100, 200, 500, 1000, 2000, 5000, 10000, 20000};		/* 8种采样频率*/
uint8_t g_FreqId = 3;
uint8_t g_ClibFlag = 0;		/* 自校准标志（去除0点飘逸） */
uint8_t g_RefreshWave = 1;	/* 刷新波形标志 */

/*
*********************************************************************************************************
*	函 数 名: DSO_Main
*	功能说明: 示波器主界面，实时显示2通道波形
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
extern void Fill_WaveBack(void);
void DSO_Main(void)
{
	int16_t tpX, tpY;
	uint8_t ucKeyCode;		/* 按键代码 */
	//MSG_T ucMsg;			/* 消息代码 */
	uint8_t ucTouch;		/* 触摸事件 */
	uint8_t Freqbuf[16];
	FormAD_T form;
	
	fftSize = 1024; 
	ifftFlag = 0; 

	FormAD = &form;

//	MountFileSystem();				/* 文件系统启动 */

	g_DSO.DrawMode = DRAW_LINE;		/* 波形绘制模式 */
	g_DSO.PenSize = PEN_X2;			/* 画笔粗细 */
	
	InitFormAD();					/* 初始化界面参数 */
	DispADInitFace();				/* 显示界面 */
	
	FormAD->SampleFreq = g_Freq[g_FreqId];		/* 初始采样频率为1kHz */

	DispLableFreq(FormAD->SampleFreq);			/* 刷新频率显示 */

	DispOffset();								/* 显示加速偏移 */
	
}

/*
*********************************************************************************************************
*	函 数 名: AnalyzeWave
*	功能说明: 解析波形
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void AnalyzeWave(void)			
{
	uint32_t i,k;
	uint32_t pos;
	static uint32_t read;
	int32_t iReadStart;
	int16_t c1;
	int16_t ch1_max, ch2_max;
	int16_t ch1_min, ch2_min;

	int64_t ch1_avg_sum;
	int64_t c1_avg_sum;

	int64_t ch1_rms_sum;
	int64_t c1_rms_sum;

	int16_t c1_max;
	int16_t c1_min;

	uint32_t volt_rms;

	uint32_t m_count;	/* 参与有效值统计的样本个数 */

/* 1.获得ADC fifo 最新的位置 */
	//pos = AD7606_GetFifoPos();

	//step = 1;

	/* 计算起始 read指针 */
	iReadStart = pos - FormAD->TimeStep * TEST_LENGTH_SAMPLES;	//VIEW_POINT_NUM;
	if (iReadStart < 0)	/* 循环FIFO 折返的情况 */
	{
		//iReadStart += ADC_FIFO_SIZE;
	}
	
/* 2.算波形1的参数，如 最大值、有效值等 */
	ch1_avg_sum = 0;

	ch1_rms_sum = 0;

	ch1_max = -32767;
	ch1_min = 32767;

	read = iReadStart;

	//m_count = (10 / FormAD->TimeStep + 4) * VIEW_POINT_NUM;

	if (FormAD->SampleFreq == 10000)
	{
		m_count = TEST_LENGTH_SAMPLES;
	}
	else if (FormAD->SampleFreq == 50000)
	{
		m_count = TEST_LENGTH_SAMPLES;
	}
	else
	{
		m_count = TEST_LENGTH_SAMPLES;
	}
	for (i = 0 ; i  < m_count; i++)
	{
		c1_max = -32767;
		c1_min = 32767;
		c1_avg_sum = 0;

		c1_rms_sum = 0;
		for (k = 0; k < FormAD->TimeStep; k++)
		{
			//c1 = g_tAdcFifo.sBuf[read];

			/* 求累加和，后面计算平均值 -- 客户居然要求绝对值累加后求平均（不理解这样做的具体物理意义) */
			c1_avg_sum += c1;

			c1_rms_sum += c1 * c1;

			/* 移动读指针 */
			read++;
			//if (read >= ADC_FIFO_SIZE)
			{
				//read = read - ADC_FIFO_SIZE;
			}

			/* 求最大值 */
			if (c1 > c1_max) c1_max = c1;

			/*　求最小值　*/
			if (c1 < c1_min) c1_min = c1;
		}

		c1_avg_sum = c1_avg_sum / FormAD->TimeStep;

		c1_rms_sum = c1_rms_sum / FormAD->TimeStep;

		/* 求最大值 */
		if (c1_max > ch1_max) ch1_max = c1_max;

		/*　求最小值　*/
		if (c1_min < ch1_min) ch1_min = c1_min;

		ch1_avg_sum += c1_avg_sum;

		ch1_rms_sum += c1_rms_sum;
	}

	/* 计算峰峰值Vpp */
	FormAD->VoltVpp = AdcToVoltage(ch1_max - ch1_min);

	/* 计算平均值 */
	FormAD->VoltAvg = AdcToVoltage(ch1_avg_sum / m_count);

	/* 计算有效值 */
	ch1_rms_sum = ch1_rms_sum / m_count;
	volt_rms = sqrt(ch1_rms_sum);
	FormAD->VoltRms = AdcToVoltage(volt_rms);
	
/* 3.2014-10-30 根据峰峰值，确定Y刻度 */
	{
		/* 波形1刻度 */
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
		
		g_DSO.Ch1GridValue = (AdcToAccele(ch1_max) / 2);
		g_DSO.Ch1GridValue = (g_DSO.Ch1GridValue * 12) / 10;
		g_DSO.Ch1GridValue = (g_DSO.Ch1GridValue / 2 + 200) * 2;

		if (g_DSO.Ch1GridValue < 1000)
		{
			g_DSO.Ch1GridValue = 1000;
		}
	
		/* 波形2刻度 */
		ch2_max = -32767;
		ch2_min = 32767;
		
		for (i = 0; i < TEST_LENGTH_SAMPLES; i++)
		{
			if (g_DSO.Ch2Buf[i] > ch2_max)
			{
				ch2_max = g_DSO.Ch2Buf[i];
			}
			
			if (g_DSO.Ch2Buf[i] < ch2_min)
			{
				ch2_min = g_DSO.Ch2Buf[i];
			}
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
		
		g_DSO.Ch2GridValue = (ch2_max / 3);
		g_DSO.Ch2GridValue = (g_DSO.Ch2GridValue * 12) / 10;
		g_DSO.Ch2GridValue = (g_DSO.Ch2GridValue / 10) * 10;
		if (g_DSO.Ch2GridValue < 5)
		{
			g_DSO.Ch2GridValue = 5;
		}
		
		/* fft波形刻度 */
		g_DSO.Ch3GridValue = g_DSO.Ch2GridValue * 3 / 5;
	}
}


// 下面的函数时按照绝对值求平均的
void __Old_NewAdcToDispBuf(void)
{
	uint32_t i,k;
	uint32_t pos;
	static uint32_t read;
	int32_t iTemp;
	int16_t c1,c2;
	int16_t ch1_max, ch2_max;
	int16_t ch1_min, ch2_min;
	int64_t ch1_avg_sum, ch2_avg_sum;
	int64_t c1_avg_sum, c2_avg_sum;
	int16_t c1_max, c2_max;
	int16_t c1_min, c2_min;
	uint8_t flag = 0;

	/* 获得ADC fifo 最新的位置 */
	//pos = AD7606_GetFifoPos();

	//step = 1;

	/* 计算起始 read指针 */
	iTemp = pos - FormAD->TimeStep * VIEW_POINT_NUM;
	if (iTemp < 0)	/* 循环FIFO 折返的情况 */
	{
		//iTemp += ADC_FIFO_SIZE;
	}
	read = iTemp;

	/* 从FIFO中取出最近的ADC数据
		 g_tAdcFifo.sBuf[] 数组的每个元素高8bit表示电流  第8bit表示电压
	*/

	ch1_avg_sum = 0;
	ch2_avg_sum = 0;

	ch1_max = -32767;
	ch2_max = -32767;
	ch1_min = 32767;
	ch2_min = 32767;

	//read = 0;
	for (i = 0 ; i < 4 * VIEW_POINT_NUM; i++)
	{
		c1_max = -32767;
		c2_max = -32767;
		c1_min = 32767;
		c2_min = 32767;
		c1_avg_sum = 0;
		c2_avg_sum = 0;
		for (k = 0; k < FormAD->TimeStep; k++)
		{
			//c1 = g_tAdcFifo.sBuf[read];
			//c2 = g_tAdcFifo.sBuf[read] >> 16;

			/* 求累加和，后面计算平均值 -- 客户居然要求绝对值累加后求平均（不理解这样做的具体物理意义) */
			c1_avg_sum += abs(c1);
			c2_avg_sum += abs(c2);

			/* 移动读指针 */
			read++;
			//if (read >= ADC_FIFO_SIZE)
			{
				//read = read - ADC_FIFO_SIZE;
			}

			/* 求最大值 */
			if (c1 > c1_max) c1_max = c1;
			if (c2 > c2_max) c2_max = c2;

			/*　求最小值　*/
			if (c1 < c1_min) c1_min = c1;
			if (c2 < c2_min) c2_min = c2;
		}

		if (i < VIEW_POINT_NUM)	/* 只取前面的700个点用来绘制波形 */
		{
			/* 取小段波形中的峰值用来绘制波形
			波形绘制时，每个X像素点只有一个y点。
			*/
			if (flag == 0)
			{
				g_DSO.Ch1Buf[i] = c1_max;
				g_DSO.Ch2Buf[i] = c2_max;
				flag = 1;
			}
			else
			{
				g_DSO.Ch1Buf[i] = c1_min;
				g_DSO.Ch2Buf[i] = c2_min;
				flag = 0;
			}
		}

		c1_avg_sum = c1_avg_sum / FormAD->TimeStep;
		c2_avg_sum = c2_avg_sum / FormAD->TimeStep;

		/* 求最大值 */
		if (c1_max > ch1_max) ch1_max = c1_max;
		if (c2_max > ch2_max) ch2_max = c2_max;

		/*　求最小值　*/
		if (c1_min < ch1_min) ch1_min = c1_min;
		if (c2_min < ch2_min) ch2_min = c2_min;

		ch1_avg_sum += c1_avg_sum;
		ch2_avg_sum += c2_avg_sum;
	}

	/* 计算峰峰值Vpp */
	//FormAD->VoltVpp = AdcToVoltage3(ch1_max - ch1_min);
	//FormAD->CurrVpp = AdcToCurrent3(ch2_max - ch2_min);

	/* 计算平均值 */
	FormAD->VoltAvg = AdcToVoltage(ch1_avg_sum / (4 * VIEW_POINT_NUM));
	FormAD->CurrAvg = AdcToCurrent(ch2_avg_sum / (4 * VIEW_POINT_NUM));

	/* 2014-10-30 根据峰峰值，确定Y刻度 */
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
		g_DSO.Ch1GridValue = (AdcToVoltage(ch1_max) / 3);
		g_DSO.Ch1GridValue = (g_DSO.Ch1GridValue * 12) / 10;
		g_DSO.Ch1GridValue = (g_DSO.Ch1GridValue / 2 + 1) * 2;
		if (g_DSO.Ch1GridValue < 60)
		{
			g_DSO.Ch1GridValue = 60;
		}

		#if 0
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
		g_DSO.Ch2GridValue = (AdcToCurrent(ch2_max) / 3);
		g_DSO.Ch2GridValue = (g_DSO.Ch2GridValue * 12) / 10;
		g_DSO.Ch2GridValue = (g_DSO.Ch2GridValue / 10) * 10;
		if (g_DSO.Ch2GridValue < 660)
		{
			g_DSO.Ch2GridValue = 660;
		}
		#endif
	}
}

/*
*********************************************************************************************************
*	函 数 名: InitFormAD
*	功能说明: 初始化GPS初始界面控件
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
static void InitFormAD(void)
{
	/* 分组框标题字体 */
	FormAD->FontBox.FontCode = FC_ST_16;
	FormAD->FontBox.BackColor = CL_MASK;	/* 和背景色相同 */
	FormAD->FontBox.FrontColor = CL_RED;
	FormAD->FontBox.Space = 0;
	
	/* 字体1 用于静止标签 */
	FormAD->FontBlack.FontCode = FC_ST_16;
	FormAD->FontBlack.BackColor = CL_MASK;		/* 透明色 */
	FormAD->FontBlack.FrontColor = CL_BLACK;
	FormAD->FontBlack.Space = 0;

	//	RA_FONT_16 = 0,		/* RA8875 字体 16点阵 */
	//RA_FONT_24 = 1,		/* RA8875 字体 24点阵 */
	//RA_FONT_32 = 2		/* RA8875 字体 32点阵 */

	/* 字体2 用于变化的文字 */
	#if 0
	FormAD->FontBlue.FontCode = RA_FONT_16;
	FormAD->FontBlue.BackColor = CL_BTN_FACE;
	FormAD->FontBlue.FrontColor = CL_BLUE;
	FormAD->FontBlue.Space = 0;
	#endif

	/* 按钮字体 */
	FormAD->FontBtn.FontCode = FC_ST_16;
	FormAD->FontBtn.BackColor = CL_MASK;		/* 透明背景 */
	FormAD->FontBtn.FrontColor = CL_BLACK;	//CL_BLACK;
	FormAD->FontBtn.Space = 0;


	/* 按键 */
	{	
		FormAD->Btn0.Left = Btn0_X;
		FormAD->Btn0.Top = Btn0_Y;
		FormAD->Btn0.Height = Btn0_H;
		FormAD->Btn0.Width = Btn0_W;
		FormAD->Btn0.Focus = 0;
		FormAD->Btn0.pCaption = Btn0_Text;
		FormAD->Btn0.Font = &FormAD->FontBtn;	
		
		FormAD->Btn1.Left = Btn1_X;
		FormAD->Btn1.Top = Btn1_Y;
		FormAD->Btn1.Height = Btn1_H;
		FormAD->Btn1.Width = Btn1_W;
		FormAD->Btn1.Focus = 0;
		FormAD->Btn1.pCaption = Btn1_Text;
		FormAD->Btn1.Font = &FormAD->FontBtn;	
		
		FormAD->Btn2.Left = Btn2_X;
		FormAD->Btn2.Top = Btn2_Y;
		FormAD->Btn2.Height = Btn2_H;
		FormAD->Btn2.Width = Btn2_W;
		FormAD->Btn2.Focus = 0;
		FormAD->Btn2.pCaption = Btn2_Text;
		FormAD->Btn2.Font = &FormAD->FontBtn;			
		
		FormAD->Btn3.Left = Btn3_X;
		FormAD->Btn3.Top = Btn3_Y;
		FormAD->Btn3.Height = Btn3_H;
		FormAD->Btn3.Width = Btn3_W;
		FormAD->Btn3.Focus = 0;
		FormAD->Btn3.pCaption = Btn3_Text;
		FormAD->Btn3.Font = &FormAD->FontBtn;		
		
		FormAD->Btn4.Left = Btn4_X;
		FormAD->Btn4.Top = Btn4_Y;
		FormAD->Btn4.Height = Btn4_H;
		FormAD->Btn4.Width = Btn4_W;
		FormAD->Btn4.Focus = 0;
		FormAD->Btn4.pCaption = Btn4_Text;
		FormAD->Btn4.Font = &FormAD->FontBtn;		

		FormAD->Btn5.Left = Btn5_X;
		FormAD->Btn5.Top = Btn5_Y;
		FormAD->Btn5.Height = Btn5_H;
		FormAD->Btn5.Width = Btn5_W;
		FormAD->Btn5.Focus = 0;
		FormAD->Btn5.pCaption = Btn5_Text;
		FormAD->Btn5.Font = &FormAD->FontBox;	
		
		FormAD->LabelStatus.Left = LabelStatus_X;
		FormAD->LabelStatus.Top = LabelStatus_Y;
		FormAD->LabelStatus.MaxLen = 0;
		FormAD->LabelStatus.pCaption = "";
		FormAD->LabelStatus.Font = &FormAD->FontBlue;		
#if 0
		FormAD->Div.Left = LabelInfo_X;
		FormAD->Div.Top = LabelInfo_Y + 2;
		FormAD->Div.Height = LabelInfo_H;
		FormAD->Div.Width = LabelInfo_W;
		FormAD->Div.MaxLen = 0;
		FormAD->Div.pCaption = "采样分度";
		FormAD->Div.Font = &FormAD->FontBtn;		
#endif		
		FormAD->Freq.Left = LabelInfo_X;
		FormAD->Freq.Top = LabelInfo_Y + 2;
		FormAD->Freq.Height = LabelInfo_H;
		FormAD->Freq.Width = LabelInfo_W;
		FormAD->Freq.MaxLen = 0;
		FormAD->Freq.pCaption = "采样频率";
		FormAD->Freq.Font = &FormAD->FontBtn;		
		
		FormAD->Volt.Left = Label2_X;
		FormAD->Volt.Top = Label2_Y + 2;
		FormAD->Volt.Height = Label2_H;
		FormAD->Volt.Width = Label2_W;
		FormAD->Volt.MaxLen = 0;
		FormAD->Volt.pCaption = "当前电压";
		FormAD->Volt.Font = &FormAD->FontBtn;		

		FormAD->Offset.Left = Label3_X;
		FormAD->Offset.Top = Label3_Y + 2;
		FormAD->Offset.Height = Label3_H;
		FormAD->Offset.Width = Label3_W;
		FormAD->Offset.MaxLen = 0;
		FormAD->Offset.pCaption = "零位偏移";
		FormAD->Offset.Font = &FormAD->FontBtn;		
		
		FormAD->PeakValue.Left = LabelPeak_X;
		FormAD->PeakValue.Top = LabelPeak_Y +  5;
		FormAD->PeakValue.Height = LabelPeak_H;
		FormAD->PeakValue.Width = LabelPeak_W;
		FormAD->PeakValue.MaxLen = 0;
		FormAD->PeakValue.pCaption = "最大速度";
		FormAD->PeakValue.Font = &FormAD->FontBtn;	
		
		FormAD->RmsValue.Left = LabelRms_X;
		FormAD->RmsValue.Top = LabelRms_Y +  5;
		FormAD->RmsValue.Height = LabelRms_H;
		FormAD->RmsValue.Width = LabelRms_W + 16;
		FormAD->RmsValue.MaxLen = 0;
		FormAD->RmsValue.pCaption = "速度有效值";
		FormAD->RmsValue.Font = &FormAD->FontBtn;	
	
		FormAD->Average.Left = LabelValue_X;
		FormAD->Average.Top = LabelValue_Y +  5;
		FormAD->Average.Height = LabelValue_H;
		FormAD->Average.Width = LabelValue_W;
		FormAD->Average.MaxLen = 0;
		FormAD->Average.pCaption = "振动频率";
		FormAD->Average.Font = &FormAD->FontBtn;	
		
		FormAD->Warn.Left = LabelWarn_X;
		FormAD->Warn.Top = LabelWarn_Y + 2;
		FormAD->Warn.Height = LabelWarn_H;
		FormAD->Warn.Width = LabelWarn_W;
		FormAD->Warn.MaxLen = 0;
		FormAD->Warn.pCaption = "状态:";
		FormAD->Warn.Font = &FormAD->FontBtn;	
		
		FormAD->WarnFreq.Left = LabelWarnFreq_X;
		FormAD->WarnFreq.Top = LabelWarnFreq_Y + 2;
		FormAD->WarnFreq.Height = LabelWarnFreq_H;
		FormAD->WarnFreq.Width = LabelWarnFreq_W;
		FormAD->WarnFreq.MaxLen = 0;
		FormAD->WarnFreq.pCaption = "频率:";
		FormAD->WarnFreq.Font = &FormAD->FontBtn;
		
	
		FormAD->WarnPeak.Left = LabelWarnPeak_X;
		FormAD->WarnPeak.Top = LabelWarnPeak_Y + 2;
		FormAD->WarnPeak.Height = LabelWarnPeak_H;
		FormAD->WarnPeak.Width = LabelWarnPeak_W;
		FormAD->WarnPeak.MaxLen = 0;
		FormAD->WarnPeak.pCaption = "幅值:";
		FormAD->WarnPeak.Font = &FormAD->FontBtn;
	}
}

/*
*********************************************************************************************************
*	函 数 名: DispLableStatus
*	功能说明: 显示状态标签
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void DispLableStatus(char *_pBUf)
{
	FormAD->LabelStatus.pCaption = _pBUf;

#if 1
	RA8875_SetFont(RA_FONT_24, 0, 0);	/* 设置32点阵字体，行间距=0，字间距=0 */

	RA8875_SetBackColor(CL_BTN_FACE);
	RA8875_SetFrontColor(CL_BLUE);

	RA8875_DispStr(LabelStatus_X, LabelStatus_Y, "          ");
	RA8875_DispStr(LabelStatus_X, LabelStatus_Y, _pBUf);
#else
	LCD_DrawLabel(&FormAD->LabelStatus);
#endif
}

/*
*********************************************************************************************************
*	函 数 名: DispLableVolt
*	功能说明: 显示电压标签
*	形    参: _Avg 平均值  _Vpp 峰值,_Rms 有效值  (单位 0.1v)
*	返 回 值: 无
*********************************************************************************************************
*/
static void DispLableVolt(int32_t _Avg, uint32_t _Vpp, uint32_t _Rms)
{
	char buf[64];
	
	RA8875_SetBackColor(CL_BLUE5);
	RA8875_SetFont(RA_FONT_24, 0, 0);	/* 设置32点阵字体，行间距=0，字间距=0 */
	RA8875_SetFrontColor(CL_WHITE);
	{
		sprintf(buf, "%d.%d%d%dV   ", _Rms / 1000, _Rms % 1000 / 100, _Rms % 100 / 10, _Rms % 10);
	}
	RA8875_DispStr(LabelVolt_X, LabelVolt_Y, buf);
	#if 0
	sprintf(buf, "%dm/(s*s)     ", ACCELE * _Rms / (5 * 1000));
	RA8875_DispStr(LabelVolt_X, LabelVolt_Y + 28, buf);
	#endif
}



/*
*********************************************************************************************************
*	函 数 名: DispOffset
*	功能说明: 显示加速偏移
*	形    参: _value : 频率值，Hz
*	返 回 值: 无
*********************************************************************************************************
*/
static void DispOffset(void)
/* 刷新标签显示 */
{
	char buf[12];

	sprintf(buf, "%-4d  ", g_tParam.OffsetAdc);

#if 1
	RA8875_SetFont(RA_FONT_24, 0, 0);	/* 设置32点阵字体，行间距=0，字间距=0 */
	RA8875_SetBackColor(CL_BLUE5);

	RA8875_SetFrontColor(CL_WHITE);
	RA8875_DispStr(LabelTime_X, LabelTime_Y, buf);
#else
	FormAD->LabelTime.pCaption = buf;
	LCD_DrawLabel(&FormAD->LabelTime);
#endif
}

/*
*********************************************************************************************************
*	函 数 名: DispLableSpeed
*	功能说明: 显示速度标签
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void DispLableSpeed(void)
{
	char buf[32];
	
	RA8875_SetBackColor(CL_BLUE5);
	RA8875_SetFont(RA_FONT_24, 0, 0);	/* 设置32点阵字体，行间距=0，字间距=0 */
	RA8875_SetFrontColor(CL_WHITE);
	
	sprintf(buf, "%4d mm/s  ", g_DSO.PeakValue);
	RA8875_DispStr(LabelPeak_X + 80, LabelPeak_Y, buf);
	
	sprintf(buf, "%4d mm/s ", g_DSO.RmsValue);
	RA8875_DispStr(LabelRms_X + 96, LabelRms_Y, buf);
	
	sprintf(buf, "%4d.%d Hz  ", g_DSO.FFTMax / 10, g_DSO.FFTMax % 10);
	RA8875_DispStr(LabelValue_X + 80, LabelValue_Y, buf);
}

/*
*********************************************************************************************************
*	函 数 名: DispWarn
*	功能说明: 显示报警状态标签
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void DispWarn(void)
{
	char buf[24];
	FONT_T font;

	font.FontCode = FC_ST_16;
	font.BackColor = CL_WHITE;	/* 和背景色相同 */
	font.FrontColor = CL_BLACK;
	font.Space = 0;
	
	RA8875_SetFont(RA_FONT_16, 0, 0);	/* 设置32点阵字体，行间距=0，字间距=0 */
	RA8875_SetBackColor(CL_WHITE);
	RA8875_SetFrontColor(CL_GREEN);
	
	if (g_tParam.Mode == 0x02)			/* 单机模式 */
	{	
		sprintf (buf, "%3d.%dHz  ", g_DSO.RangeMax / 10, g_DSO.RangeMax % 10);
		LCD_DispStrEx(LabelWarnFreq_X + 40 , LabelWarnFreq_Y + 8,  buf, &font, 80, 1);
		sprintf (buf, "%.2f  ", g_DSO.RangeMaxApd);
		LCD_DispStrEx(LabelWarnPeak_X + 40 , LabelWarnPeak_Y + 8,  buf, &font, 80, 1);
	}
	else if (g_tParam.Mode == 0x01)			/* 在线模式 */
	{
		//if (g_tSData.NowSpeed1 > g_tData.Speed1)		/* g_tSData表示要发送的数据，g_tData表示接收到的标准 */
		{
			RA8875_SetFrontColor(CL_RED);
		}
		
		//if (g_tSData.SpeedFreq1 > g_tData.Accel1)
		{
			RA8875_SetFrontColor(CL_RED);
		}
		
		//if (g_tSData.NowAccel1 > g_tData.Offset1)
		{
			RA8875_SetFrontColor(CL_RED);
		}
	}

	if (g_DSO.RangeMaxApd >= g_tParam.Alarm)		/* 5-100Hz 信号的最大幅值大于报警阈值 */
	{
		RA8875_SetFrontColor(CL_RED);
		RA8875_DispStr(LabelWarn_X + 45, LabelWarn_Y + 8, "报警");
		
		g_RefreshWave = 0;		/* 不刷新波形 */
	}
	else
	{
		RA8875_SetFrontColor(CL_BLACK);
		RA8875_DispStr(LabelWarn_X + 45, LabelWarn_Y + 8, "正常");
		
		g_RefreshWave = 1;		/* 刷新波形 */
	}
}

/*
*********************************************************************************************************
*	函 数 名: DispLableFreq
*	功能说明: 显示采样频率
*	形    参: _value : 频率值，Hz
*	返 回 值: 无
*********************************************************************************************************
*/
static void DispLableFreq(uint32_t _value)
{
	char buf[20];

	if (_value >= 1000)
	{
		sprintf(buf, "%-4dkHz  ", _value / 1000);
	}
	else
	{
		sprintf(buf, "%-4dHz  ", _value);
	}

#if 1
	RA8875_SetFont(RA_FONT_24, 0, 0);	/* 设置32点阵字体，行间距=0，字间距=0 */
	RA8875_SetBackColor(CL_BLUE5);

	RA8875_SetFrontColor(CL_WHITE);
	RA8875_DispStr(LabelFreq_X, LabelFreq_Y, buf);
#else
	FormAD->LabelFreq.pCaption = buf;
	LCD_DrawLabel(&FormAD->LabelFreq);
#endif
}


/*
*********************************************************************************************************
*	函 数 名: DispLableTimeLen
*	功能说明: 显示采样时间长度
*	形    参: _value : 频率值，Hz
*	返 回 值: 无
*********************************************************************************************************
*/
static void DispLableTimeLen(uint32_t _value)
/* 刷新标签显示 */
{
	char buf[12];

	sprintf(buf, "%4ds  ", g_SampleTime);

#if 1
	RA8875_SetFont(RA_FONT_24, 0, 0);	/* 设置32点阵字体，行间距=0，字间距=0 */
	RA8875_SetBackColor(CL_BLUE5);

	RA8875_SetFrontColor(CL_WHITE);
	RA8875_DispStr(LabelTime_X, LabelTime_Y, buf);
#else
	FormAD->LabelTime.pCaption = buf;
	LCD_DrawLabel(&FormAD->LabelTime);
#endif
}

/*
*********************************************************************************************************
*	函 数 名: DispADInitFace
*	功能说明: 显示初始界面
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void DispADInitFace(void)
{
	LCD_ClrScr(CL_BLUE5);

	/* 按键 */
	LCD_DrawButton2(&FormAD->Btn0, 3);
	LCD_DrawButton2(&FormAD->Btn1, 3);
	LCD_DrawButton2(&FormAD->Btn2, 3);
	LCD_DrawButton2(&FormAD->Btn3, 3);
	LCD_DrawButton2(&FormAD->Btn4, 3);
	
	/* 画框 */
	//LCD_DrawLabel2(&FormAD->Div, CL_YELLOW2, 3);
	LCD_DrawLabel2(&FormAD->Freq, CL_YELLOW, 3);
	LCD_DrawLabel2(&FormAD->Volt, CL_YELLOW, 3);
	//LCD_DrawLabel2(&FormAD->Accele, CL_YELLOW2, 3);
	LCD_DrawLabel2(&FormAD->Offset, CL_YELLOW, 3);
	LCD_DrawLabel2(&FormAD->PeakValue, CL_YELLOW, 3);
	LCD_DrawLabel2(&FormAD->RmsValue, CL_YELLOW, 3);	
	LCD_DrawLabel2(&FormAD->Average, CL_YELLOW, 3);		
	//LCD_DrawLabel2(&FormAD->Warn, CL_YELLOW, 3);		
	//LCD_DrawLabel2(&FormAD->WarnFreq, CL_YELLOW, 3);		
	//LCD_DrawLabel2(&FormAD->WarnPeak, CL_YELLOW, 3);	
	
	//WarnFrame();		/* 显示报警框 */
	
	#if 0
	LCD_Fill_Rect(Label4_X - 10, Label4_Y - 1, 80 + 2, 790 - Label4_X + 8, CL_BLACK);
	LCD_DrawRect(Label4_X - 9, Label4_Y, 80, 790 - Label4_X + 6, CL_WHITE);
	LCD_DrawRect(Label4_X - 10, Label4_Y - 1, 80 + 2, 790 - Label4_X + 8, CL_WHITE);
	LCD_DrawLabel2(&FormAD->Speed, CL_YELLOW, 3);
	#endif
}

/*
*********************************************************************************************************
*	函 数 名: DispVer
*	功能说明: 显示版本号
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void DispVer(void)
{
	FONT_T font;

	font.FontCode = FC_ST_16;
	font.BackColor = CL_BLUE5;	/* 和背景色相同 */
	font.FrontColor = CL_WHITE;
	font.Space = 0;

	{
		LCD_DispStrEx(758 , 460,  SOFT_VER, &font, 33, 2);
	}
}

/*
*********************************************************************************************************
*	函 数 名：DispErr
*	功能说明：显示输入无效小窗口，延迟2秒
*	形    参：无
*	返 回 值：无
*********************************************************************************************************
*/
static void DispErr(void)
{
	WIN_T tWin;
	LABEL_T tLable;
	FONT_T tFont;

	tFont.FontCode = FC_ST_16;			/* 字体代码 16点阵 */
	tFont.FrontColor = CL_WHITE;			/* 字体颜色 */
	tFont.BackColor = CL_BLUE3;			/* 文字背景颜色 */
	tFont.Space = 0;					/* 文字间距，单位 = 像素 */

	/* 绘制主窗体 */
	tWin.Font = &tFont;
	
	tWin.Left = QR_X;
	tWin.Top = QR_Y;
	tWin.Height = QR_H;
	tWin.Width = QR_W;
	tWin.pCaption = "Warning";
	LCD_DrawWin(&tWin);

	tFont.BackColor = WIN_BODY_COLOR;
	tLable.Font = &tFont;
	tLable.Left = QR_X + 5;
	tLable.Top = QR_Y + 38;
	tLable.MaxLen = 0;
	tLable.pCaption = "请到页面II设置参数";
	LCD_DrawLabel(&tLable);
	
	//bsp_DelayMS(2000);
	/* 清FIFO */
	//bsp_ClearKey();
	
}

/* 过滤加速度波形 */
static void CalcuOffset(void);
/*
*********************************************************************************************************
*	函 数 名：Accele_To_Speed
*	功能说明：将求得的加速度值放入g_DSO.Ch1Buf，求得的速度值放入g_DSO.Ch2Buf，并进行快速fft变换得到波形放入SpeedInput_fft
*	形    参：无
*	返 回 值：无
*********************************************************************************************************
*/
static void Accele_To_Speed(void)
{
	int32_t iReadStart;
	static uint32_t read;
	uint32_t pos;
	uint32_t k, m_count;
	uint16_t i;
	int32_t Speed_sum = 0;
	int32_t Filter;
	int32_t Accel_sum = 0;		/* 加速度去直流用 */
	int32_t Accel_Filter;
	int16_t c1;
	int16_t c1_max;
	int16_t c1_min;
	uint8_t flag = 0;
	
	int32_t Adc_clib = 0;			/* adc自校准参数 */
	int16_t Adc_Offset = 0;			/* adc自校准偏移值 */
	static int32_t lasttime = 0;
	static uint8_t timeflag = 1;
	
	int16_t ch1_max, ch1_min;
	

/* 1.获得ADC fifo 最新的位置 */
	//pos = AD7606_GetFifoPos();

	//step = 1;
	/* 计算起始 read指针 */
	iReadStart = pos - FormAD->TimeStep * TEST_LENGTH_SAMPLES;	//TEST_LENGTH_SAMPLES = 1024
	if (iReadStart < 0)			/* 循环FIFO 折返的情况 */
	{
		//iReadStart += ADC_FIFO_SIZE;
	}
	
/* 2.从FIFO中取出最近的ADC数据 */
	read = iReadStart;

	if (FormAD->SampleFreq == 10000)
	{
		m_count = TEST_LENGTH_SAMPLES;
	}
	else if (FormAD->SampleFreq == 50000)
	{
		m_count = TEST_LENGTH_SAMPLES;
	}
	else
	{
		m_count = TEST_LENGTH_SAMPLES;
	}
	for (i = 0 ; i  < m_count; i++)		/* 将最后得到的1024个数据放入g_DSO.Ch1Buf */
	{
		c1_max = -32767;
		c1_min = 32767;
		
		for (k = 0; k < FormAD->TimeStep; k++)
		{
			//c1 = g_tAdcFifo.sBuf[read];

			/* 移动读指针 */
			read++;
			//if (read >= ADC_FIFO_SIZE)
			{
			//	read = read - ADC_FIFO_SIZE;
			}

			/* 求最大值 */
			if (c1 > c1_max) c1_max = c1;

			/*　求最小值　*/
			if (c1 < c1_min) c1_min = c1;
		}

		if (i < TEST_LENGTH_SAMPLES)	/* 只取前面的1024个点用来绘制波形 */
		{
			/* 取小段波形中的峰值用来绘制波形
			波形绘制时，每个X像素点只有一个y点。
			*/
			if (flag == 0)
			{
				g_DSO.Ch1Buf[i] = c1_max;
			//	g_DSO.Ch2Buf[i] = c2_max;
				flag = 1;
			}
			else
			{
				g_DSO.Ch1Buf[i] = c1_min;
			//	g_DSO.Ch2Buf[i] = c2_min;
				flag = 0;
			}
		}
	}
	
/* 2016-05-19 这里偏移值自适应，传感器静止时，会自动得到偏移量 */
	for (i = 0; i < TEST_LENGTH_SAMPLES; i++)
	{
		Adc_clib += g_DSO.Ch1Buf[i];
	}
	
	Adc_Offset = Adc_clib / TEST_LENGTH_SAMPLES;
	
	for (i = 0; i < TEST_LENGTH_SAMPLES; i++)
	{
		if (g_DSO.Ch1Buf[i] <= (Adc_Offset + 5) && g_DSO.Ch1Buf[i] >= (Adc_Offset - 5))		/* 之前位+-5 */
		{
			g_ClibFlag = 1;					/* 此时，为传感器静止状态，可以得到偏移值 */
		}
		else
		{
			g_ClibFlag = 0;					/* 此时，为传感器非静止状态 */
			break;
		}
	}
		
	//if (bsp_CheckRunTime(lasttime) > 20000)		/* 每次存参数有20秒的最小间隔 */
	{
		timeflag = 1;
	}
			
	if (g_ClibFlag == 1)					/* 得到了偏移值。则判断是否保存 */
	{	
		if (g_tParam.OffsetAdc != (-Adc_Offset))	/* 如果当前计算偏移量和之前的不同，则保存 */
		{
			g_tParam.OffsetAdc = (-Adc_Offset);
			
			if (timeflag == 1)				/* 最小存储间隔为10秒 */
			{
				timeflag = 0;
				SaveParam();		
				//lasttime = bsp_GetRunTime();
			}
			
			DispOffset();					/* 显示零点偏移值 */
		}
	}
	else									/* 传感器非静止状态，偏移量保持不变 */
	{
		;
	}	
		
	for (i = 0; i < TEST_LENGTH_SAMPLES; i++)	/* 根据偏移值得到去飘移后的值 */
	{
		g_DSO.Ch1Buf[i] += g_tParam.OffsetAdc;
	}
/******** 自适应结束 ********/
	
	read = iReadStart;

	//if (g_tAdcFifo.Count >= TEST_LENGTH_SAMPLES)
	{	
		{	/* 2015-12-08 只为求最大加速度，无其他作用 */
			ch1_max = -32767;
			ch1_min = 32767;
			for (i = 0; i < TEST_LENGTH_SAMPLES; i++)
			{
				if (g_DSO.Ch1Buf[i] > ch1_max)
				{
					ch1_max = g_DSO.Ch1Buf[i];		/* 求加速度最大值 */
				}
				
				if (g_DSO.Ch1Buf[i] < ch1_min)
				{
					ch1_min = g_DSO.Ch1Buf[i];		/* 求加速度最小值 */
				}
				
				if (ch1_min < -10000)
				{
					ch1_min++;
				}
			}
			
			//g_tSData.NowAccel = AdcToAccele(ch1_max);
		}

/* 3.将加速度换算成速度，并去直流成分 */
		g_DSO.Ch2Buf[0] = (float)AdcToAccele(g_DSO.Ch1Buf[0]) / FormAD->SampleFreq;		/* 单位为0.001m / s */
		
		for (i = 0; i < TEST_LENGTH_SAMPLES; i++)		/* 求1024个加速度样本每个点对应的速度(默认初始速度为0) */
		{
			if (AdcToAccele(g_DSO.Ch1Buf[i]) >= 0)
			{
				g_DSO.Ch2Buf[i + 1] = g_DSO.Ch2Buf[i] + (float)AdcToAccele(g_DSO.Ch1Buf[i]) / FormAD->SampleFreq;
			}
			else 
			{
				g_DSO.Ch2Buf[i + 1] = g_DSO.Ch2Buf[i] - (float)(-AdcToAccele(g_DSO.Ch1Buf[i])) / FormAD->SampleFreq;
			}
		}

		/* 2015-12-24 计算位移 g_tSData.Offset1 */
		CalcuOffset();
		
		/* 此时得到1024个速度,因为速度初始为0，所以存在直流成分，再次通过傅立叶变换滤波*/
		
		#if 1	/* 求平均值滤波(滤除直流成分),这种方法代替傅立叶变换滤波 */
			for (i = 0; i < TEST_LENGTH_SAMPLES; i++)
			{
				Speed_sum += g_DSO.Ch2Buf[i];
			}
			Filter = Speed_sum / TEST_LENGTH_SAMPLES;
			
			for (i = 0; i < TEST_LENGTH_SAMPLES; i++)
			{
				if (Filter >= 0)
				{
					g_DSO.Ch2Buf[i] = g_DSO.Ch2Buf[i] - Filter;
				}
				else
				{
					g_DSO.Ch2Buf[i] = g_DSO.Ch2Buf[i] + (-Filter);
				}
			}
		#endif
			
		//if (g_DSO.Mode == 1)		/* 2015-12-08 联级模式 */
		{
			//g_tSData.OverSpeed = 0;		/* 超出速度清零 */
		//	if (g_DSO.Ch2Buf[i + 1] > g_tData.Speed1)		/* 如果大于速度标准值 */
			{
			//	g_tSData.OverSpeed = g_DSO.Ch2Buf[i + 1];			/* 超出标准的速度值 */
			//	g_tSData.OverSpeedFreq = g_DSO.FFTMax / 10;			/* g_DSO.FFTMax精确到0.1HZ这里怎么弄？ */
				//bsp_PutMsg(MSG_SPEED_OVERTOP, 0);
			}
		}

/* 4.FFT傅立叶变化换，求变换后的波形。 */	
		Fast_FFT();
	}
}

/* 2015-12-24 计算振动位移，这里默认g_DSO.Ch2Buf[0]时的速度为0,这样会导致得到的数据不准确,但是找不到更好的方法求位移 */
static void CalcuOffset(void)
{
	int32_t Ssum = 0;
	uint16_t i;
	
	for (i = 0; i < TEST_LENGTH_SAMPLES; i++)
	{
		Ssum += g_DSO.Ch2Buf[i];				/* 求的平均速度 */
	}
	
	if (Ssum >= 0)
	{
		//g_tSData.Offset1 = Ssum / FormAD->SampleFreq;		/* 位移1 = 平均速度 * T */
	}
	else 
	{
		//g_tSData.Offset1 = (-Ssum) / FormAD->SampleFreq;
	}
}

/*
*********************************************************************************************************
*	函 数 名: Fast_FFT
*	功能说明: 快速fft傅立叶变换
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
static void Fast_FFT(void)
{
	uint16_t i;

	/* 实数序列FFT长度 */
	fftSize = 1024; 
	/* 正变换 */
    ifftFlag = 0; 
	
	for (i = 0; i < TEST_LENGTH_SAMPLES; i++)
	{
		SpeedInput_fft[i] = g_DSO.Ch2Buf[i];
	}
	
/* 1024点实序列快速FFT */ 
	arm_rfft_fast_f32(&S, SpeedInput_fft, SpeedOutput_fft, ifftFlag);
	
/* 为了方便跟函数arm_cfft_f32计算的结果做对比，这里求解了1024组模值，
			实际函数arm_rfft_fast_f32只求解出了512组  */ 
	arm_cmplx_mag_f32(SpeedOutput_fft, SpeedInput_fft, DSOSCREEN_LENGTH);
}


/*
*********************************************************************************************************
*	函 数 名: Deal_FFT_Wave
*	功能说明: 处理fft傅立叶波形
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
static void Deal_FFT_Wave(void)
{
	uint16_t i;
		/* 第4步：在LCD上显示FFT幅频**********************************************************************/
	/* 在自动触发模式，采样率为2.8Msps到5Ksps时(单通道)才显示FFT波形 */
//	if((g_ucFFTDispFlag == 0)&&(TriggerFlag == 0)&&(TimeBaseId < 10))
	
	/* 求FFT变换结果的幅值 */
	for(i = 0; i < DSOSCREEN_LENGTH; i++)
	{
		SpeedInput_fft[i] = SpeedInput_fft[i]/512; 
	}
	
	/* 直流分量大小 */
	SpeedInput_fft[0] = SpeedInput_fft[0]/2;

#if 0
	/* 求得幅频做一下转换，方便在LCD上显示 */
	for(i = 0; i < DSOSCREEN_LENGTH-2; i++)
	{
		testInput_fft_2048[i] = testInput_fft_2048[i]/5.0f;
		aPointsRe[i+2].y = 370 - testInput_fft_2048[i] ;
	}
	
	GUI_SetColor(GUI_RED);
	GUI_DrawPolyLine(&aPointsRe[2], 598, DSOSCREEN_STARTX, DSOSCREEN_STARTY);
#endif
}

/*
*********************************************************************************************************
*	函 数 名: WarnFrame
*	功能说明: 报警框
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
static void WarnFrame(void)
{
	LCD_Fill_Rect(660, 5, 180, 135, CL_WHITE);
	LCD_DrawRect(660 - 1, 5 - 1, 180 + 2, 135 + 2, CL_RED);
	LCD_DrawRect(660 - 2, 5 - 2, 180 + 4, 135 + 4, CL_RED);
	
	LCD_DrawLabel2(&FormAD->Warn, CL_WHITE, 3);		
	LCD_DrawLabel2(&FormAD->WarnFreq, CL_WHITE, 3);		
	LCD_DrawLabel2(&FormAD->WarnPeak, CL_WHITE, 3);	
	
	LCD_DrawButton2(&FormAD->Btn5, 3);		/* 确定按钮 */
}


/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
