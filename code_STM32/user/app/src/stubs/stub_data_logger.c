/**
 * @file    stub_data_logger.c
 * @brief   DataLogger 桩模块 - 用于 PC 模拟器和 STM32 初期集成
 */

#include "data_logger.h"
#include <string.h>

void DataLogger_Init(void)
{
    /* 桩: 无操作 */
}

bool DataLogger_SavePoint(const DataPoint_t *dp)
{
    (void)dp;
    return true;  /* 桩: 假装成功 */
}

uint32_t DataLogger_SavePointsBulk(const DataPoint_t *dp, uint32_t count)
{
    (void)dp;
    return count;  /* 桩: 假装全部成功 */
}

uint32_t DataLogger_QueryHistory(uint8_t channel_num,
                                 uint32_t start_time, uint32_t end_time,
                                 DataPoint_t *buffer, uint32_t max_count)
{
    (void)channel_num;
    (void)start_time;
    (void)end_time;
    (void)buffer;
    (void)max_count;
    return 0;  /* 桩: 无历史数据 */
}

bool DataLogger_SaveAlarm(const AlarmRecord_t *record)
{
    (void)record;
    return true;
}

uint32_t DataLogger_QueryAlarms(uint8_t type,
                                uint32_t start_time, uint32_t end_time,
                                AlarmRecord_t *buffer, uint32_t max_count)
{
    (void)type;
    (void)start_time;
    (void)end_time;
    (void)buffer;
    (void)max_count;
    return 0;  /* 桩: 无报警记录 */
}

bool DataLogger_ExportCSV(uint8_t channel_num,
                          uint32_t start_time, uint32_t end_time,
                          const char *filename)
{
    (void)channel_num;
    (void)start_time;
    (void)end_time;
    (void)filename;
    return false;  /* 桩: 导出失败 */
}

void DataLogger_Cleanup(uint32_t retention_days)
{
    (void)retention_days;
}

uint32_t DataLogger_GetAlarmCount(void)
{
    return 0;
}
