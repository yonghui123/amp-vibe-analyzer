/**
 * @file    stub_alarm_manager.c
 * @brief   Alarm_Manager 桩模块 - 用于 PC 模拟器和 STM32 初期集成
 */

#include "alarm_manager.h"
#include <string.h>

/* 全局状态变量 */
SystemStatus_t g_system_status = STATUS_NORMAL;
bool g_buzzer_enabled = true;
bool g_general_alarm_active = false;
bool g_vibration_alarm_active = false;

void Alarm_Init(void)
{
    g_system_status = STATUS_NORMAL;
    g_buzzer_enabled = true;
    g_general_alarm_active = false;
    g_vibration_alarm_active = false;
}

bool Alarm_CheckGeneral(float current_value, float threshold,
                        uint32_t over_duration_ms)
{
    (void)current_value;
    (void)threshold;
    (void)over_duration_ms;
    return false;  /* 桩: 永不触发报警 */
}

bool Alarm_CheckVibration(const float *peak_to_peak,
                          const float *upper_envelope,
                          const float *lower_envelope,
                          uint32_t count, uint32_t threshold_percent)
{
    (void)peak_to_peak;
    (void)upper_envelope;
    (void)lower_envelope;
    (void)count;
    (void)threshold_percent;
    return false;  /* 桩: 永不触发报警 */
}

float Alarm_CalcOverLimitRatio(const float *peak_to_peak,
                               const float *upper_envelope,
                               const float *lower_envelope, uint32_t count)
{
    (void)peak_to_peak;
    (void)upper_envelope;
    (void)lower_envelope;
    (void)count;
    return 0.0f;
}

SystemStatus_t Alarm_GetStatus(void)
{
    return g_system_status;
}

void Alarm_Reset(void)
{
    g_system_status = STATUS_NORMAL;
    g_general_alarm_active = false;
    g_vibration_alarm_active = false;
}

void Alarm_SetBuzzer(bool enabled)
{
    g_buzzer_enabled = enabled;
}

void Alarm_CreateRecord(AlarmRecord_t *record, AlarmType_t type,
                        uint8_t ch_num, const char *ch_name,
                        float cur_val, float thresh)
{
    if (record == NULL) return;
    memset(record, 0, sizeof(AlarmRecord_t));
    record->type = type;
    record->channel_num = ch_num;
    record->current_value = cur_val;
    record->threshold_value = thresh;
    if (ch_name) {
        strncpy(record->channel_name, ch_name, sizeof(record->channel_name) - 1);
    }
}
