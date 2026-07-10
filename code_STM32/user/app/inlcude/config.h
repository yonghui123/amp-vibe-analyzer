/**
 * @file    config.h
 * @brief   SignalX STM32 系统配置 - 硬件引脚定义、参数宏、GUI 布局宏
 * @note    针对 STM32F407VGT6 + RA8875 7寸屏 (800x480)
 */

#ifndef __CONFIG_H
#define __CONFIG_H

/* ==================== 系统时钟配置 ==================== */
#define SYS_CLOCK_FREQ_HZ       168000000UL

/* ==================== 通道配置 ==================== */
#define MAX_GENERAL_CHANNELS    4
#define MAX_VIBRATION_CHANNELS  4
#define MAX_TOTAL_CHANNELS      (MAX_GENERAL_CHANNELS + MAX_VIBRATION_CHANNELS)

/* ADC 参考电压和分辨率 */
#define ADC_VREF_MV             3300
#define ADC_RESOLUTION_BITS     12
#define ADC_MAX_VALUE           ((1 << ADC_RESOLUTION_BITS) - 1)

/* ==================== 存储接口 ==================== */
#define FLASH_SECTOR_SIZE       4096
#define FLASH_TOTAL_SIZE        (8 * 1024 * 1024)

#define CONFIG_FLASH_ADDR       0x00000000
#define CONFIG_FLASH_SIZE       (16 * FLASH_SECTOR_SIZE)
#define ALARM_FLASH_ADDR        (CONFIG_FLASH_ADDR + CONFIG_FLASH_SIZE)
#define ALARM_FLASH_SIZE        (64 * FLASH_SECTOR_SIZE)
#define DATA_FLASH_ADDR         (ALARM_FLASH_ADDR + ALARM_FLASH_SIZE)
#define DATA_FLASH_SIZE         (FLASH_TOTAL_SIZE - DATA_FLASH_ADDR)

/* ==================== LCD 显示屏 (RA8875 7寸) ==================== */
#define LCD_WIDTH               800
#define LCD_HEIGHT              480

/* ==================== 采样参数默认值 ==================== */
#define DEFAULT_GENERAL_FREQ_HZ     30
#define DEFAULT_VIBRATION_FREQ_HZ   1000
#define DEFAULT_ALARM_DELAY_MS      1000
#define DEFAULT_CYCLE_MULTIPLIER    20
#define DEFAULT_MOVING_AVG_WINDOW   10

/* ==================== 采集任务配置（审阅 v1.2 已确认） ==================== */
/** GUI 与采集管线向界面推送数据的周期（毫秒），默认 20ms ≈ 50Hz */
#define ACQ_UI_UPDATE_MS            20
/** 主监控折线图显示时间窗（秒）；每点对应 ACQ_UI_UPDATE_MS */
#define ACQ_DISPLAY_WINDOW_SEC      30
/** 折线图点数 = 时间窗 / 推送周期（PC 30s→1500 点；STM32 受内存限制见下） */
#define ACQ_CHART_POINTS            ((ACQ_DISPLAY_WINDOW_SEC * 1000) / ACQ_UI_UPDATE_MS)
/** AD7606 默认硬件采样率（Hz）；振动通道写入环形缓冲，通用通道可降采样显示 */
#define ACQ_DEFAULT_ADC_HZ          1000
/** 选定振动通道环形缓冲区长度（样本个数），应 ≥ 单 UI 周期内样本数 */
#define ACQ_VIB_RING_SIZE           512
/** 单次 ProcessFrame 峰峰值计算窗口最大样本数（20ms@1kHz≈20，留余量） */
#define ACQ_VIB_WINDOW_MAX            64
/** 定义后主监控创建 chart 时填充静态演示正弦；正常运行时不定义，由 AcqTask 推送实时数据 */
/* #define ACQ_USE_DEMO_DATA        1 */

/* ==================== GUI 配置 ==================== */
#define LVGL_TICK_PERIOD_MS     1
#define LVGL_BUF_SIZE           (LCD_WIDTH * 50)

/* LVGL 折线图点数：PC 30s@20ms=1500 点；STM32 受 LV_MEM 限制 100 点≈2s 滚动窗 */
#if defined(PC_SIMULATOR)
#define GUI_CHART_POINTS_MAX    ACQ_CHART_POINTS
#else
#define GUI_CHART_POINTS_MAX    100
#endif
/** 振动图横轴点数（与主监控折线图一致，供包络序号映射） */
#define ACQ_VIB_CHART_POINTS        GUI_CHART_POINTS_MAX

/* 触摸校准模式：1=仅调试（校准后进测试页）；0=正常产品（无 Flash 校准时先校准再进正式界面） */
#define GUI_TOUCH_CALIB_MODE        0

/** 裸机 BSP 触摸测试；=1 跳过校准直接进 BSP 测试页 */
#define GUI_TOUCH_BSP_TEST_MODE     0

/** LVGL 触摸测试页；=1 且 CALIB_MODE=1 时校准后进测试页；正式产品应设 0 */
#define GUI_TOUCH_TEST_LVGL_ENABLE  0

/** =1：Flash 无校准时尝试 v5 出厂 PARAM(0x0800C000) */
#define GUI_TOUCH_IMPORT_V5_PARAM   0

/** LVGL 触摸 IIR 滤波；正式界面建议 1 */
#define GUI_TOUCH_LVGL_FILTER       1

#define TAB_BAR_H               36

/* ==================== GUI 布局宏 (集中管理) ==================== */
#define GUI_CONTENT_W           780
#define GUI_CONTENT_H           430
#define GUI_MARGIN               10
#define GUI_TABLE_W             780
#define GUI_TABLE_H             340
#define GUI_TABLE_ROW_H          40
#define GUI_CHART_W             780
#define GUI_CHART_H             170
#define GUI_BTN_W               120
#define GUI_BTN_H                42
#define GUI_GROUP_W             780
#define GUI_GROUP_H              65
#define GUI_GROUP_GAP             8
#define GUI_STATUS_BAR_H         35
#define GUI_BOTTOM_BAR_H         50
#define GUI_HEADER_BAR_H         50

#endif /* __CONFIG_H */
