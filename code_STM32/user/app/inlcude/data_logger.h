/**
 * @file    data_logger.h
 * @brief   SignalX 数据记录模块
 *
 * 支持:
 *   - SPI Flash 循环缓冲区 (实时数据, 30天滚动)
 *   - SD Card CSV 导出
 *   - 报警记录持久化 (≥10万条)
 *   - 历史数据查询
 */

#ifndef __DATA_LOGGER_H
#define __DATA_LOGGER_H

#include <stdint.h>
#include <stdbool.h>
#include "alarm_manager.h"

/* ==================== 数据点结构 ==================== */

/** 紧凑存储的信号数据点 (16 bytes) */
typedef struct {
    float   signal_value;       /**< 信号值 (校准后) */
    float   peak_to_peak;       /**< 峰峰值 (振动通道) */
    uint32_t timestamp;         /**< Unix时间戳 */
    uint8_t  channel_num;       /**< 通道号 */
    uint8_t  flags;             /**< 标志位: bit0=超限 bit1=报警 */
} __attribute__((packed)) DataPoint_t;

/* ==================== API 函数 ==================== */

/**
 * @brief  初始化数据记录器
 */
void DataLogger_Init(void);

/**
 * @brief  保存实时数据点
 * @param  dp  数据点指针
 * @return true=成功
 */
bool DataLogger_SavePoint(const DataPoint_t *dp);

/**
 * @brief  批量保存数据点 (高效写入)
 * @param  dp     数据点数组
 * @param  count  数量
 * @return 成功写入的数量
 */
uint32_t DataLogger_SavePointsBulk(const DataPoint_t *dp, uint32_t count);

/**
 * @brief  查询历史数据
 * @param  channel_num 通道号
 * @param  start_time  起始时间戳
 * @param  end_time    结束时间戳
 * @param  buffer      输出缓冲区
 * @param  max_count   最大数量
 * @return 实际查询到的数量
 */
uint32_t DataLogger_QueryHistory(uint8_t channel_num,
                                 uint32_t start_time, uint32_t end_time,
                                 DataPoint_t *buffer, uint32_t max_count);

/**
 * @brief  保存报警记录
 * @param  record  报警记录
 * @return true=成功
 */
bool DataLogger_SaveAlarm(const AlarmRecord_t *record);

/**
 * @brief  查询报警记录
 * @param  type       报警类型 (0=全部)
 * @param  start_time 起始时间
 * @param  end_time   结束时间
 * @param  buffer     输出缓冲区
 * @param  max_count  最大数量
 * @return 实际数量
 */
uint32_t DataLogger_QueryAlarms(uint8_t type,
                                uint32_t start_time, uint32_t end_time,
                                AlarmRecord_t *buffer, uint32_t max_count);

/**
 * @brief  导出数据到 SD Card CSV
 * @param  channel_num  通道号
 * @param  start_time   起始时间
 * @param  end_time     结束时间
 * @param  filename     CSV文件名
 * @return true=成功
 */
bool DataLogger_ExportCSV(uint8_t channel_num,
                          uint32_t start_time, uint32_t end_time,
                          const char *filename);

/**
 * @brief  清理过期数据
 * @param  retention_days  保留天数
 */
void DataLogger_Cleanup(uint32_t retention_days);

/**
 * @brief  获取已存储报警记录数
 */
uint32_t DataLogger_GetAlarmCount(void);

#endif /* __DATA_LOGGER_H */
