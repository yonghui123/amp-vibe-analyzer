/**
 * @file    gui_calib.h
 * @brief   RA8875 五点仿射触摸校准向导
 */

#ifndef __GUI_CALIB_H
#define __GUI_CALIB_H

#include <stdbool.h>

void GUI_CalibInit(void);

/** Flash 无有效校准参数时为 true（可自动进入校准） */
bool GUI_CalibNeedsRun(void);

#endif /* __GUI_CALIB_H */
