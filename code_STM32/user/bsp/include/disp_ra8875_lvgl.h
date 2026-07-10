/**
 * @file disp_ra8875_lvgl.h
 * RA8875 显示/触摸 LVGL 驱动适配层
 */

#ifndef __DISP_RA8875_LVGL_H
#define __DISP_RA8875_LVGL_H

#include "lvgl.h"
#include <stdbool.h>

/**
 * @brief  初始化 LVGL 显示和触摸驱动
 *         注册 RA8875 flush 回调和触摸 read 回调
 * @note   调用前需确保 RA8875_InitHard() 和 RA8875_TouchInit() 已完成
 */
void DISP_RA8875_LVGL_Init(void);

/** 校准向导：开启后仅采集 raw ADC 平均值，不做坐标映射 */
void DISP_SetTouchCalibCollectMode(bool enable);

/** 退出校准采集后清零触摸滤波/坐标，避免映射切换后首点漂移 */
void DISP_ResetTouchState(void);

/** 校准向导：取走一次抬手后的 raw ADC 均值样本 */
bool DISP_TakeCalibSample(uint16_t *adc_x, uint16_t *adc_y);

#if !defined(PC_SIMULATOR)
/** 最近一次有效触摸的 RA8875 原始 ADC 值（校准界面用） */
void DISP_GetLastTouchRaw(uint16_t *adc_x, uint16_t *adc_y);

/** 获取滤波后的屏幕坐标（与 LVGL indev 一致，校准界面推荐用此接口） */
bool DISP_GetTouchPoint(int32_t *x, int32_t *y, lv_indev_state_t *state);
#endif

#endif /* __DISP_RA8875_LVGL_H */
