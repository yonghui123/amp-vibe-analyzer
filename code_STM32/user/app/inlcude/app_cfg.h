/*
*********************************************************************************************************
*
*	模块名称 : 配置文件
*	文件名称 : app_cfg.h
*	版    本 : V1.0
*	说    明 : 头文件
*	修改记录 :
*		版本号  日期       作者    说明
*		v1.0    2011-07-01 armfly  创建
*
*	Copyright (C), 2010-2011, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __APP_CFG_H
#define __APP_CFG_H

/* 供外部调用的函数声明 */
#define DEV_MODEL			"DH-2708A"		/* 设备型号 */
#define DEV_SN				"110701"		/* 设备编号 */
#define SOFT_VER			"V2.0a"			/* 固件版本 */

#define MAX_RECORD_NUMBER	999


#define SYS_DIR				"/DAS"			/* 数据采集文件存放路径 */
#define SYS_DIR_1			"DAS"			/* 用于判断目录 */

#define DAS_FILE_COUT_MAX	50				/* DAS目录下最多文件个数 */

#define PRE_SIZE_OFFSET		171				/* 预采集区起始地址 */
#define PRE_POS_OFFSET		(PRE_POS_OFFSET + 4)	/* 预采集区起始地址 */
#define PRE_DATA_OFFSET		(PRE_POS_OFFSET + 4)	/* 预采集区起始地址 */

#define MAX_SAMPLE_TIME		600				/* 最大采样时间，单位秒 */
#define MIN_SAMPLE_TIME		1				/* 最小采样时间，单位秒 */


#define ADC_PRE_SIZE_MAX	(32 * 1024)		/* 预采集数据最大数量(样本数) */
//#define ADC_FIFO_SIZE		(ADC_PRE_SIZE_MAX + 4 * 1024)		/* 采集数据缓冲区 */

#define ADC_TRIG_COUNT_MAX	3				/* ADC通道触发最大次数 */


#endif


