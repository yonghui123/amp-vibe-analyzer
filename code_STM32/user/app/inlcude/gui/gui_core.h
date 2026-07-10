/**
 * @file    gui_core.h
 * @brief   SignalX LVGL GUI 核心管理
 *
 * 管理4个功能界面:
 *   1. 通道设置界面
 *   2. 采集设置界面
 *   3. 主监控画面 (含实时曲线)
 *   4. 报警记录界面
 *
 * 触摸校准见 gui_calib.h / GUI_CalibInit()（config.h GUI_TOUCH_CALIB_MODE）
 */

#ifndef __GUI_CORE_H
#define __GUI_CORE_H

#include <stdint.h>
#include <stdbool.h>

/* ==================== 界面ID ==================== */
typedef enum {
    SCREEN_CHANNEL_SETUP    = 0,
    SCREEN_ACQ_SETTINGS     = 1,
    SCREEN_MAIN_DISPLAY     = 2,
    SCREEN_ALARM_RECORDS    = 3,
    SCREEN_COUNT
} ScreenID_t;

/* ==================== API 函数 ==================== */

/**
 * @brief  初始化 GUI (LVGL + 显示屏 + 触摸)
 */
void GUI_Init(void);

/**
 * @brief  GUI 主循环任务 (需周期性调用, 建议5-10ms)
 */
void GUI_TaskHandler(void);

/**
 * @brief  切换到指定界面
 */
void GUI_SwitchScreen(ScreenID_t screen_id);

/**
 * @brief  获取当前界面
 */
ScreenID_t GUI_GetCurrentScreen(void);

/**
 * @brief  更新实时数据到主监控界面（同步，仅 LVGL 任务上下文安全）
 *
 * 采集任务请使用 GUI_PostRealtimeData()，内部经 lv_async_call 转到 GUI 线程。
 */
void GUI_UpdateRealtimeData(const float *general_vals,
                            const float *vibration_pp,
                            const float *upper_env,
                            const float *lower_env,
                            uint32_t count, uint16_t rpm);

/**
 * @brief  异步投递实时曲线数据（AcqTask → GUI 线程安全）
 *
 * STM32：拷贝 payload 后 lv_async_call，在 lv_timer_handler 中刷新 chart。
 * PC 模拟器：主循环同线程，直接同步刷新。
 *
 * @param  general_vals   通用通道值；general_count=0 时可 NULL
 * @param  general_count  有效通用条数
 * @param  vib_pp         振动峰峰值（0~100 显示坐标）
 * @param  vib_valid      非 0 表示更新振动 PP 序列
 * @param  env_upper      上包络参考点；env_valid=0 时忽略
 * @param  env_lower      下包络参考点
 * @param  env_valid      非 0 表示更新包络 SHIFT 点
 * @param  rpm            转速（预留）
 */
void GUI_PostRealtimeData(const float *general_vals, uint8_t general_count,
                          float vib_pp, uint8_t vib_valid,
                          float env_upper, float env_lower, uint8_t env_valid,
                          uint16_t rpm);

/**
 * @brief  异步投递系统报警状态（AcqTask → GUI 线程安全）
 */
void GUI_PostAlarmStatus(uint8_t status, bool general_alarm,
                         bool vibration_alarm);

/**
 * @brief  异步弹出报警弹窗（AcqTask → GUI 线程安全）
 */
void GUI_PostAlarmPopup(uint8_t alarm_type, const char *channel_name,
                        float current_val, float threshold);

/**
 * @brief  开始采集时清零实时曲线（包络参考线保持静态）
 */
void GUI_ResetMainCharts(void);

/**
 * @brief  按振动 PP 点序号获取包络参考（0~100 显示坐标）
 * @param  seq_index  与采集管线 s_vib_ui_seq 同步
 * @param  upper      上包络输出；可 NULL
 * @param  lower      下包络输出；可 NULL
 * @return true  包络 CSV 已加载
 */
bool GUI_GetVibEnvelopeAtSeq(uint32_t seq_index, float *upper, float *lower);

/**
 * @brief  更新系统状态显示
 * @param  status          系统状态
 * @param  general_alarm   通用报警标志
 * @param  vibration_alarm 振动报警标志
 */
void GUI_UpdateStatus(uint8_t status, bool general_alarm,
                      bool vibration_alarm);

/**
 * @brief  弹出报警弹窗
 * @param  alarm_type  报警类型
 * @param  channel_name 通道名称
 * @param  current_val  当前值
 * @param  threshold    阈值
 */
void GUI_ShowAlarmPopup(uint8_t alarm_type, const char *channel_name,
                        float current_val, float threshold);

/**
 * @brief  刷新通道配置列表
 */
void GUI_RefreshChannelList(void);

/**
 * @brief  显示加载动画
 */
void GUI_ShowLoading(bool show);

/**
 * @brief  显示提示条
 * @param  text  提示文本
 * @param  duration_ms  显示时长(ms)
 */
void GUI_Toast(const char *text, uint32_t duration_ms);

#endif /* __GUI_CORE_H */
