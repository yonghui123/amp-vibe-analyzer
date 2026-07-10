/**
 * @file    channel_data.h
 * @brief   SignalX 共享通道配置数据模块
 *
 * 所有 Tab 页面共享的通道配置数据，支持 JSON 持久化存储。
 * 通道通过 channel_id (1~8) 唯一标识，即使名称修改也不影响引用。
 *
 * 使用方式:
 *   - GUI_Init() 时调用 ChannelData_Init() 加载数据
 *   - 任意 Tab 通过 ChannelData_Get(id) / ChannelData_GetByIndex(i) 读取
 *   - 修改后调用 ChannelData_Save() 写入 JSON 文件
 */

#ifndef __CHANNEL_DATA_H
#define __CHANNEL_DATA_H

#include <stdint.h>
#include <stdbool.h>
#include "config_store.h"

/* ==================== 常量 ==================== */
#define CHANNEL_DATA_MAX    8
#define CHANNEL_ID_BASE     1   /* ID 从 1 开始 */

/* 单位表 (按 channel_type 索引) */
extern const char *g_units_general[];     /* {"A", "kW", NULL} */
extern const char *g_units_vibration[];   /* {"um", "mm/s", "m/s", "m/s2", NULL} */

/* ==================== API ==================== */

/**
 * @brief  初始化通道数据 (加载 JSON 或使用默认值)
 *         应在 GUI_Init() 中最早调用
 */
void ChannelData_Init(void);

/**
 * @brief  获取通道数量
 */
uint8_t ChannelData_GetCount(void);

/**
 * @brief  获取通道配置 (按索引 0~N-1)
 * @return 指针, 可直接修改; 返回 NULL 表示索引无效
 */
ChannelConfig_t *ChannelData_GetByIndex(uint8_t index);

/**
 * @brief  获取通道配置 (按 channel_id 1~8)
 * @return 指针, 可直接修改; 返回 NULL 表示 ID 不存在
 */
ChannelConfig_t *ChannelData_GetById(uint8_t id);

/**
 * @brief  获取所有通道数据数组 (直接访问)
 */
ChannelConfig_t *ChannelData_GetAll(void);

/**
 * @brief  保存通道配置到 JSON 文件
 * @return true=成功
 */
bool ChannelData_Save(void);

/**
 * @brief  恢复出厂默认 (重新生成默认数据, 不自动保存)
 */
void ChannelData_ResetDefaults(void);

/**
 * @brief  获取通道的单位字符串
 */
const char *ChannelData_GetUnit(const ChannelConfig_t *ch);

#endif /* __CHANNEL_DATA_H */
