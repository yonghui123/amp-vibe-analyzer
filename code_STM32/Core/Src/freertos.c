/* USER CODE BEGIN Header */

/**

  ******************************************************************************

  * File Name          : freertos.c

  * Description        : Code for freertos applications

  ******************************************************************************

  * @attention

  *

  * Copyright (c) 2026 STMicroelectronics.

  * All rights reserved.

  *

  * This software is licensed under terms that can be found in the LICENSE file

  * in the root directory of this software component.

  * If no LICENSE file comes with this software, it is provided AS-IS.

  *

  ******************************************************************************

  */

/* USER CODE END Header */



/* Includes ------------------------------------------------------------------*/

#include "FreeRTOS.h"

#include "task.h"

#include "main.h"



/* Private includes ----------------------------------------------------------*/

/* USER CODE BEGIN Includes */

#include "cmsis_os.h"

#include "gui_core.h"

#include "gui_calib.h"

#include "config.h"

#include "lvgl.h"

#include "disp_ra8875_lvgl.h"

#include "bsp_lcd_ra8875.h"

#include "bsp_tft_lcd.h"

#if !defined(PC_SIMULATOR)

#include "touch_bsp_calib.h"

#include "touch_bsp_test.h"

#include "touch_calib.h"

#endif

#include "storage/sd_fs.h"

/* USER CODE END Includes */



/* Private typedef -----------------------------------------------------------*/

/* USER CODE BEGIN PTD */



/* USER CODE END PTD */



/* Private define ------------------------------------------------------------*/

/* USER CODE BEGIN PD */



/* USER CODE END PD */



/* Private macro -------------------------------------------------------------*/

/* USER CODE BEGIN PM */



/* USER CODE END PM */



/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN Variables */



/* USER CODE END Variables */



/* Private function prototypes -----------------------------------------------*/

/* USER CODE BEGIN FunctionPrototypes */

void StartGUITask(void *argument);

#if !defined(PC_SIMULATOR)
static void gui_run_lvgl_app(void);
#endif

/* USER CODE END FunctionPrototypes */



/* Private application code --------------------------------------------------*/

/* USER CODE BEGIN Application */

/*
 * AcqTask 由 AcqTask_Init()（GUI_Init 内调用）创建，栈 2KB、优先级 Normal。
 * 采集循环见 user/app/src/acq_task.c。
 */

#if !defined(PC_SIMULATOR)

static void gui_run_lvgl_app(void)
{
  lv_init();
  DISP_RA8875_LVGL_Init();
  DISP_ResetTouchState();
  GUI_Init();
  lv_refr_now(NULL);

  for (;;) {
    GUI_TaskHandler();
    osDelay(5);
  }
}

#endif

/**

  * @brief  LVGL GUI 任务线程

  * @note   lv_init / 显示驱动 / GUI_Init 均在此任务内执行（main 栈仅 1KB）。

  * @param  argument: 未使用

  */

void StartGUITask(void *argument)

{

  (void)argument;



#if defined(__GNUC__)

  /* CCMRAM NOLOAD 段：启动文件不清零，须在 lv_init 前清零 */

  {

    extern uint32_t _sccmram;

    extern uint32_t _eccmram;

    uint32_t *p = (uint32_t *)&_sccmram;

    uint32_t *end = (uint32_t *)&_eccmram;

    while (p < end) {

      *p++ = 0U;

    }

  }

#endif



  /* SD 工具层：无卡时返回 false，不阻塞 GUI */

  (void)SdFs_Init();



#if GUI_TOUCH_BSP_TEST_MODE

  RA8875_TouchInit();

  TouchCalib_Init();

  TouchBspTest_Run();

  for (;;) {

    osDelay(1000);

  }

#elif GUI_TOUCH_CALIB_MODE

  RA8875_TouchInit();

  TouchCalib_Init();

#if !defined(PC_SIMULATOR)

  if (!TouchCalib_IsValid()) {

    (void)TouchBspCalib_Do();

  }

  gui_run_lvgl_app();

#endif

  for (;;) {

    osDelay(1000);

  }

#else

#if !defined(PC_SIMULATOR)

  TouchCalib_Init();

  if (!TouchCalib_IsValid()) {

    RA8875_TouchInit();

    (void)TouchBspCalib_Do();

  }

#endif

  gui_run_lvgl_app();

#endif /* GUI_TOUCH_BSP_TEST_MODE || GUI_TOUCH_CALIB_MODE */

}



/* USER CODE END Application */



