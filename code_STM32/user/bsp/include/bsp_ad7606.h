/**
 * @file    bsp_ad7606.h
 * @brief   AD7606 八通道同步 ADC 驱动 API
 *
 * 硬件：FSMC + TIM12(PB15) CONVST + BUSY(PB14)。
 * 缓冲：每通道 ADC_DATA_SIZE 点环形历史（静态 .bss，非栈/堆）。
 * 业务：GetLatestRaw / SampleHook 供 acq_pipeline 使用。
 */

#ifndef __BSP_AD7606_H
#define __BSP_AD7606_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

#define AD7606_CHANNEL_COUNT    8

#ifndef AD7606_DEFAULT_SAMPLE_HZ
#define AD7606_DEFAULT_SAMPLE_HZ    1000U
#endif

#define AD7606_SAMPLE_HZ_MIN        100U
#define AD7606_SAMPLE_HZ_MAX        20000U

/** 每通道环形缓冲深度（与 code_source 实际值一致；注释里的 1024 未启用） */
#ifndef ADC_DATA_SIZE
#define ADC_DATA_SIZE               128
#endif

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

void bsp_InitAD7606(void);
void AD7606_ReadNowAdc(void);
void AD7606_SetInputRange(uint8_t range);
void AD7606_Reset(void);
void AD7606_SetOS(uint8_t os_mode);
void AD7606_EnterAutoMode(uint32_t sample_hz);
void AD7606_StartRecord(uint32_t sample_hz);
void AD7606_StopRecord(void);

bool AD7606_GetLatestRaw(int16_t out[AD7606_CHANNEL_COUNT]);
uint32_t AD7606_CopyChannelHistory(uint8_t channel_id, int16_t *out, uint32_t count);
uint32_t AD7606_GetFifoDepth(void);
uint32_t AD7606_GetSampleCount(void);
bool AD7606_IsRecording(void);
uint32_t AD7606_GetSampleHz(void);
void AD7606_SampleHook(const int16_t raw[AD7606_CHANNEL_COUNT]);

#endif /* __BSP_AD7606_H */
