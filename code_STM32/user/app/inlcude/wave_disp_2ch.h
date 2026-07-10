/*
*********************************************************************************************************
*
*	模块名称 : 2CH 示波器波形显示
*	文件名称 : win_disp_2ch.h
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

#ifndef __WAVE_DISP_2CH_H
#define __WAVE_DISP_2CH_H

#define VER_INFO	"X-Y Ver 1.0"

#define WIN_BACK_COLOR   CL_BLACK
#define WIN_GRID_COLOR   CL_WHITE	// CL_GREY	/* 波形窗口栅格颜色 */
#define WIN_WAVE_COLOR CL_RED

/* 画线模式 */
enum
{
	DRAW_DOT = 0,		/* 打点模式 */
	DRAW_LINE = 1		/* 画线模式 */
};

/* 笔画大小 */
enum
{
	PEN_X1 = 0,			/* 单像素 */
	PEN_X2 = 1,			/* 双倍像素 */
	PEN_X3 = 2			/* 三倍像素 */
};

#define VIEW_POINT_NUM		600	//700  					/* 显示点个数，和显示窗口同宽度 */
#define XY_VIEW_POINT_NUM	(2 * VIEW_POINT_NUM)  	/* X-Y界面显示点个数 */

/*
	示波器相关的数据结构
*/
typedef struct
{
	int16_t Ch1Buf[XY_VIEW_POINT_NUM];			
	float Ch2Buf[XY_VIEW_POINT_NUM];

	int16_t Filter;
	int16_t old_speed;				
	uint16_t PeakValue;				/* 速度峰值 */
//	uint16_t PeakToPeak;			/* 峰峰值 */
	uint16_t RmsValue;				/* 有效值 */
	uint16_t FFTMax;				/* FFT最大值 */
	float FFTMaxApd;				/* FFT最大幅值 */
	uint16_t RangeMax;				/* 5-100hz的最大幅值对应的频率 */
	float RangeMaxApd;				/* 5-100hz的最大幅值 */
	
	
	uint32_t TimeBaseIdHold;		/* 暂停时的时基 */

	uint32_t TimeBaseId;			/* 时基索引, 查表可得到 us单位的时基 */
	uint32_t SampleFreq;			/* 采样频率，单位Hz */
	uint32_t TimeBase;				/* 时基 查表可得到 us单位的时基 */

	uint8_t  Ch1AttId;				/* CH1 衰减倍数索引 */
	uint8_t  Ch2AttId;				/* CH2 衰减倍数索引 */
	int32_t  Ch1Attenuation; 		/* 波形1幅度衰减系数(原始数据x10后，再除以这个数)  */
	int32_t  Ch2Attenuation; 		/* 波形2幅度衰减系数(原始数据x10后，再除以这个数)  */

	uint16_t Ch1VScale;				/* 通道1分度值mV单位 */
	uint16_t Ch2VScale;				/* 通道2分度值mV单位 */

	int16_t Ch1VOffset;				/* 通道1 GND线位置, 可以为负数 */
	int16_t Ch2VOffset;				/* 通道1 GND线位置, 可以为负数 */

	uint8_t AdcStep;				/* 取样步距 */

	uint8_t Ch1Zoom;				/* CH1 缩放 */
	uint8_t Ch2Zoom;				/* CH2 缩放 */

#if 1
	/* 注意排列，X-Y波形绘制时，使用了2倍的VIEW_POINT_NUM样本点 */
	uint16_t xCh1Buf1[VIEW_POINT_NUM];		/* 波形数据，坐标数组 */
	uint16_t xCh2Buf1[VIEW_POINT_NUM];		/* 波形数据，坐标数组 */
	uint16_t xCh3Buf1[VIEW_POINT_NUM];		/* 波形数据，坐标数组 */

	uint16_t yCh1Buf1[VIEW_POINT_NUM];		/* 波形数据，坐标数组 */
	uint16_t yCh2Buf1[VIEW_POINT_NUM];		/* 波形数据，坐标数组 */
	uint16_t yCh3Buf1[VIEW_POINT_NUM];		/* 波形数据，坐标数组 */

	uint16_t xCh1Buf2[VIEW_POINT_NUM];		/* 波形数据，坐标数组 */
	uint16_t xCh2Buf2[VIEW_POINT_NUM];		/* 波形数据，坐标数组 */
	uint16_t xCh3Buf2[VIEW_POINT_NUM];		/* 波形数据，坐标数组 */

	uint16_t yCh1Buf2[VIEW_POINT_NUM];		/* 波形数据，坐标数组 */
	uint16_t yCh2Buf2[VIEW_POINT_NUM];		/* 波形数据，坐标数组 */
	uint16_t yCh3Buf2[VIEW_POINT_NUM];		/* 波形数据，坐标数组 */
#else
	/* 使用2个缓冲区完成波形的擦除和重现 */
	uint16_t xCh1Buf1[VIEW_POINT_NUM];		/* 波形数据，坐标数组 */
	uint16_t yCh1Buf1[VIEW_POINT_NUM];		/* 波形数据，坐标数组 */
	uint16_t xCh1Buf2[VIEW_POINT_NUM];		/* 波形数据，坐标数组 */
	uint16_t yCh1Buf2[VIEW_POINT_NUM];		/* 波形数据，坐标数组 */

	uint16_t xCh2Buf1[VIEW_POINT_NUM];		/* 波形数据，坐标数组 */
	uint16_t yCh2Buf1[VIEW_POINT_NUM];		/* 波形数据，坐标数组 */
	uint16_t xCh2Buf2[VIEW_POINT_NUM];		/* 波形数据，坐标数组 */
	uint16_t yCh2Buf2[VIEW_POINT_NUM];		/* 波形数据，坐标数组 */
#endif
	uint8_t BufUsed;			/* 0表示当前用Buf1， 1表示用Buf2 */

	uint16_t Ch1InvlidCount1;		/* 有效绘制点个数 */
	uint16_t Ch1InvlidCount2;		/* 有效绘制点个数 */
	uint16_t Ch2InvlidCount1;		/* 有效绘制点个数 */
	uint16_t Ch2InvlidCount2;		/* 有效绘制点个数 */
	uint16_t Ch3InvlidCount1;		/* 有效绘制点个数 */
	uint16_t Ch3InvlidCount2;		/* 有效绘制点个数 */

	uint8_t DrawMode;	/* 波形绘制模式 */
	uint8_t PenSize;	/* 画笔粗细 */


	uint32_t VoltRate;		/* 电压倍率:  ADC值和电压V之间的关系。 */
	uint32_t CurrRate;		/* 电流倍率:  ADC值和电流A之间的关系。 */

	uint32_t Ch1GridIndex;		/* Y轴一大格对应多少伏 */
	uint32_t Ch2GridIndex;		/* Y轴一大格对应多少伏 */

	int32_t Ch1GridValue;		/* Y轴一大格对应多少伏 */
	int32_t Ch2GridValue;		/* Y轴一大格对应多少伏 */
	int32_t	Ch3GridValue;		/* FFT波形，Y轴一大格对应多少伏 */
	int32_t XGridValue;		/* U-I图，X轴一大格对应多少伏 */
	int32_t YGridValue;		/* U-I图，Y轴一大格对应多少伏 */
}DSO_T;

void InitDSO(void);
void DispDSO(void);

void DispRule1(void);
void DispRule2(void);

int32_t AdcToVoltage(int16_t _adc);
int32_t AdcToCurrent(int16_t _adc);
int32_t AdcToAccele(int16_t _adc);

void DrawPoints(uint16_t *x, uint16_t *y, uint16_t _usSize, uint16_t _usColor);

extern DSO_T g_DSO;
#endif


