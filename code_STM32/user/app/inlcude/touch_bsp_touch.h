/**
 * @file    touch_bsp_touch.h
 * @brief   裸机 RA8875 触摸采样（供 BSP 校准/测试共用）
 */

#ifndef __TOUCH_BSP_TOUCH_H
#define __TOUCH_BSP_TOUCH_H

#include <stdint.h>
#include <stdbool.h>

bool TouchBsp_PenDown(void);

/** 非阻塞读一次 ADC；RA8875 忙时返回 false */
bool TouchBsp_TryReadAdc(uint16_t *adc_x, uint16_t *adc_y);

/** 等待触笔完全抬起（v5 TOUCH_WaitRelease） */
void TouchBsp_WaitPenRelease(void);

/** 等待稳定按压后采集中值（不含松手滑动，用于校准） */
bool TouchBsp_CaptureCalibAdc(uint16_t *adc_x, uint16_t *adc_y);

/** 先等松手再 CaptureCalibAdc（兼容旧接口） */
bool TouchBsp_WaitReleaseAdcSample(uint16_t *adc_x, uint16_t *adc_y);

/** 按压期间多次采样取中值（用于测试页） */
bool TouchBsp_ReadAdcFiltered(uint16_t *adc_x, uint16_t *adc_y);

bool TouchBsp_AdcToScreen(uint16_t adc_x, uint16_t adc_y, int32_t *px, int32_t *py);

#endif /* __TOUCH_BSP_TOUCH_H */
