/**
 * @file    config_store.h
 * @brief   SignalX Flash配置存储模块
 *
 * 使用 SPI Flash (W25Q64) 存储通道配置和采集参数
 * 配置格式: 二进制结构体 + CRC16 校验
 */

#ifndef __CONFIG_STORE_H
#define __CONFIG_STORE_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

/* ==================== 配置数据结构 ==================== */

/** 通道类型 */
typedef enum {
    CH_TYPE_GENERAL     = 0,
    CH_TYPE_VIBRATION   = 1
} ChType_t;

/** 单通道配置 */
typedef struct {
    uint8_t     channel_id;             /* 唯一标识 (1~8, 不可变) */
    uint8_t     channel_num;            /* 通道序号 */
    uint8_t     enabled : 1;
    uint8_t     ch_type : 1;
    uint8_t     unit_index : 3;         /* 单位索引 (通用:0=A,1=kW; 振动:0=um,1=mm/s,2=m/s,3=m/s2) */
    uint8_t     reserved : 3;
    char        channel_name[21];       /* 通道名称, ≤20字符 */
    uint16_t    sample_freq_hz;         /* 采样频率 */
    float       coefficient;            /* 校准系数 */
    float       offset;                 /* 校准偏移 */
    union {
        struct {                        /* 通用通道 */
            float   alarm_threshold;    /* 报警阈值 */
            uint16_t alarm_delay_ms;    /* 报警延迟(固定1000ms) */
        } general;
        struct {                        /* 振动通道 */
            float   sensitivity;        /* 灵敏度 */
            float   suggested_value;    /* 动平衡建议值 */
            float   alarm_value;        /* 动平衡报警值 */
        } vibration;
    } params;
} ChannelConfig_t;

/** 采集参数 (64 bytes) */
typedef struct {
    uint16_t    min_cycle_duration_s;   /* 修整最小循环时长 */
    uint8_t     vibration_ch_num;       /* 振动通道选择 */
    uint8_t     general_ch_num;         /* 通用通道选择 */
    uint8_t     filter_mode;            /* 0=不滤波 1=平滑 2=转速 */
    uint16_t    rpm;                    /* 转速 */
    uint8_t     cycle_multiplier;       /* 周期倍数 */
    uint8_t     moving_average;         /* 滑动平均 */
    float       general_threshold;      /* 通用阈值 */
    float       threshold_time_s;       /* 阈值时间 */
    uint8_t     alarm_threshold_pct;    /* 报警阈值% */
    uint8_t     envelope_upper_pct;     /* 包络线上限% */
    uint8_t     envelope_lower_pct;     /* 包络线下限% */
    uint8_t     reserved;
    char        envelope_file_path[32]; /* 包络线文件路径 */
} __attribute__((packed)) AcqParams_t;

/* ==================== API 函数 ==================== */

/**
 * @brief  初始化配置存储 (初始化 SPI Flash)
 */
void Config_Init(void);

/**
 * @brief  加载通道配置
 * @param  configs   输出: 通道配置数组
 * @param  max_count 最大通道数
 * @return 实际加载的通道数
 */
uint8_t Config_LoadChannels(ChannelConfig_t *configs, uint8_t max_count);

/**
 * @brief  保存通道配置
 * @param  configs 通道配置数组
 * @param  count   通道数量
 * @return true=成功
 */
bool Config_SaveChannels(const ChannelConfig_t *configs, uint8_t count);

/**
 * @brief  加载采集参数
 */
AcqParams_t Config_LoadAcqParams(void);

/**
 * @brief  保存采集参数
 */
bool Config_SaveAcqParams(const AcqParams_t *params);

/**
 * @brief  恢复出厂默认配置
 */
void Config_RestoreDefaults(void);

/**
 * @brief  获取默认通道配置
 */
void Config_GetDefaultChannels(ChannelConfig_t *configs, uint8_t *count);

/**
 * @brief  获取默认采集参数
 */
void Config_GetDefaultAcqParams(AcqParams_t *params);

#endif /* __CONFIG_STORE_H */
