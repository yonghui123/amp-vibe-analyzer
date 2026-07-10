/**
 * @file    touch_bsp_calib.h
 * @brief   裸机五点触摸校准（LCD 绘图 + raw ADC，不经过 LVGL）
 */

#ifndef __TOUCH_BSP_CALIB_H
#define __TOUCH_BSP_CALIB_H

#include <stdbool.h>

void TouchBspCalib_Run(void);

/** @return true 五点计算成功（无论 Flash 是否保存成功） */
bool TouchBspCalib_Do(void);

#endif /* __TOUCH_BSP_CALIB_H */
