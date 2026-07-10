/**
 * @file    bsp_ad7606.h
 * @brief   AD7606 八通道同步 ADC 驱动（FSMC + TIM12 CONVST + BUSY 中断）
 *
 * 物理映射：第 n 次 FSMC 读（n=1~8）对应 channel_id = n。
 * 阶段 1：完整保存 8 路 raw，提供线程安全快照 API。
 */

#ifndef __BSP_AD7606_H
#define __BSP_AD7606_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

/** AD7606 同步采样通道数 */
#define AD7606_CHANNEL_COUNT    8

/** 默认采样率 (Hz)，与 config.h ACQ_DEFAULT_ADC_HZ 一致 */
#ifndef AD7606_DEFAULT_SAMPLE_HZ
#define AD7606_DEFAULT_SAMPLE_HZ    1000U
#endif

/** 允许配置的最小/最大采样率 (Hz) */
#define AD7606_SAMPLE_HZ_MIN        100U
#define AD7606_SAMPLE_HZ_MAX        20000U

/** 过采样倍率（OS2:OS1:OS0 组合） */
typedef enum {
    AD_OS_NO  = 0,
    AD_OS_X2  = 1,
    AD_OS_X4  = 2,
    AD_OS_X8  = 3,
    AD_OS_X16 = 4,
    AD_OS_X32 = 5,
    AD_OS_X64 = 6
} AD7606_OS_E;

extern TIM_HandleTypeDef htim12;

/** 初始化 AD7606：量程、过采样、硬件复位 */
void bsp_InitAD7606(void);

/** BUSY 下降沿中断中调用：顺序读取 8 路 FSMC 结果并更新最新快照 */
void AD7606_ReadNowAdc(void);

void AD7606_SetInputRange(uint8_t range);
void AD7606_Reset(void);
void AD7606_SetOS(uint8_t os_mode);

/** 配置 TIM12 PWM 触发 CONVST，并使能 BUSY 外部中断 */
void AD7606_EnterAutoMode(uint32_t sample_hz);

/** 开始连续采集（复位硬件、清零计数、启动 PWM） */
void AD7606_StartRecord(uint32_t sample_hz);

/** 停止连续采集（停 PWM、关闭 BUSY 中断） */
void AD7606_StopRecord(void);

/**
 * @brief  拷贝最近一次 8 路原始采样值（任务上下文安全）
 * @param  out  长度至少 AD7606_CHANNEL_COUNT 的 int16 数组
 * @return true=已成功至少采样一帧；false=out 为空
 */
bool AD7606_GetLatestRaw(int16_t out[AD7606_CHANNEL_COUNT]);

/** 累计完成的转换组数（每组 8 通道），Stop 后清零 */
uint32_t AD7606_GetSampleCount(void);

/** 当前是否处于 StartRecord 运行状态 */
bool AD7606_IsRecording(void);

/** 当前配置的采样率 (Hz)，未启动时为 0 */
uint32_t AD7606_GetSampleHz(void);

/**
 * @brief  每次 8 路 ADC 采样完成后的用户钩子（弱符号，可在 app 层重载）
 *
 * 在 BUSY 中断上下文调用；默认空实现。
 * 采集管线在 acq_pipeline.c 中重载为 AcqPipeline_OnAd7606Sample。
 *
 * @param  raw  本次转换的 8 路 int16 原始值
 */
void AD7606_SampleHook(const int16_t raw[AD7606_CHANNEL_COUNT]);

#endif /* __BSP_AD7606_H */
