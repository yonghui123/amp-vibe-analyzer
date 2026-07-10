/**
 * @file    alarm_manager.h
 * @brief   SignalX 报警管理模块（阶段 8）
 *
 * 一级通用 / 二级振动包络 / 双报警合并 / 三态状态机。
 * 判定纯函数 + ProcessFrame 内状态编排；GUI 经 GUI_PostAlarm* 线程安全投递。
 */

#ifndef __ALARM_MANAGER_H
#define __ALARM_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

/** 前向声明，完整结构见 acq_pipeline.h 中 AcqDisplayFrame_t */
typedef struct AcqDisplayFrame_s AcqDisplayFrame_t;

/* ==================== 枚举定义 ==================== */

typedef enum {
    ALARM_TYPE_NONE      = 0,
    ALARM_TYPE_GENERAL   = 1,
    ALARM_TYPE_VIBRATION = 2,
    ALARM_TYPE_DUAL      = 3
} AlarmType_t;

typedef enum {
    STATUS_NORMAL     = 0,
    STATUS_TRIGGERING = 1,
    STATUS_ALARMING   = 2
} SystemStatus_t;

typedef struct {
    uint32_t    id;
    uint32_t    timestamp;
    AlarmType_t type;
    uint8_t     channel_num;
    char        channel_name[21];
    float       current_value;
    float       threshold_value;
    float       over_duration_s;
    float       duration_s;
    char        process_status[10];
    char        notes[501];
} AlarmRecord_t;

/* ==================== 全局状态（GUI 与采集任务可读） ==================== */

extern SystemStatus_t g_system_status;
extern bool g_buzzer_enabled;
extern bool g_general_alarm_active;
extern bool g_vibration_alarm_active;

/* ==================== API ==================== */

void Alarm_Init(void);

/** 通道/采集配置变更后清零防抖计时（不解除已锁存报警） */
void Alarm_NotifyConfigChanged(void);

void Alarm_ProcessFrame(const AcqDisplayFrame_t *frame, uint32_t dt_ms);

bool Alarm_CheckGeneral(float current_value, float threshold,
                        uint32_t over_duration_ms, uint32_t required_ms);

bool Alarm_CheckVibration(const float *peak_to_peak,
                          const float *upper_envelope,
                          const float *lower_envelope,
                          uint32_t count, uint32_t threshold_percent);

float Alarm_CalcOverLimitRatio(const float *peak_to_peak,
                               const float *upper_envelope,
                               const float *lower_envelope, uint32_t count);

SystemStatus_t Alarm_GetStatus(void);
void Alarm_Reset(void);
void Alarm_SetBuzzer(bool enabled);

void Alarm_CreateRecord(AlarmRecord_t *record, AlarmType_t type,
                        uint8_t ch_num, const char *ch_name,
                        float cur_val, float thresh);

#endif /* __ALARM_MANAGER_H */
