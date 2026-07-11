/**
 * @file    data_logger.h
 * @brief   数据记录：RealData / AlarmData / Export（经 sd_fs）
 */
#ifndef __DATA_LOGGER_H
#define __DATA_LOGGER_H

#include <stdint.h>
#include <stdbool.h>
#include "alarm_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 紧凑存储的信号数据点 (16 bytes) */
typedef struct {
    float   signal_value;
    float   peak_to_peak;
    uint32_t timestamp;
    uint8_t  channel_num;
    uint8_t  flags;
} __attribute__((packed)) DataPoint_t;

void DataLogger_Init(void);

/**
 * @brief  开始采集会话：创建 RealData/年/月/时间-通道名.csv 并写表头
 * @param  channel_num 主通道号（用于文件名；0 则用 "multi"）
 * @param  channel_name 通道名（可空）
 */
bool DataLogger_BeginSession(uint8_t channel_num, const char *channel_name);

/** 结束会话：刷盘并关闭当前文件 */
void DataLogger_EndSession(void);

/** 强制刷写缓冲 */
bool DataLogger_Flush(void);

bool     DataLogger_SavePoint(const DataPoint_t *dp);
uint32_t DataLogger_SavePointsBulk(const DataPoint_t *dp, uint32_t count);

uint32_t DataLogger_QueryHistory(uint8_t channel_num,
                                 uint32_t start_time, uint32_t end_time,
                                 DataPoint_t *buffer, uint32_t max_count);

bool     DataLogger_SaveAlarm(const AlarmRecord_t *record);

/** 按 id 更新处理状态/备注（重写 alarms.csv） */
bool     DataLogger_UpdateAlarm(const AlarmRecord_t *record);

uint32_t DataLogger_QueryAlarms(uint8_t type,
                                uint32_t start_time, uint32_t end_time,
                                AlarmRecord_t *buffer, uint32_t max_count);

/**
 * @param channel_num 0=导出报警；非 0=导出该通道历史到 Export/
 * @param filename    仅文件名，落在 Export/
 */
bool DataLogger_ExportCSV(uint8_t channel_num,
                          uint32_t start_time, uint32_t end_time,
                          const char *filename);

void     DataLogger_Cleanup(uint32_t retention_days);
uint32_t DataLogger_GetAlarmCount(void);

#ifdef __cplusplus
}
#endif

#endif /* __DATA_LOGGER_H */
