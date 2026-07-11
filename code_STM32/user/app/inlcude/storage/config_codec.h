/**
 * @file    config_codec.h
 * @brief   通道/采集参数 ↔ JSON 纯编解码（不碰文件系统）
 */
#ifndef CONFIG_CODEC_H
#define CONFIG_CODEC_H

#include "config_store.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @return 写入字节数；失败 ≤0 */
int ConfigCodec_ChannelsToJson(const ChannelConfig_t *configs, uint8_t count,
                               char *buf, int buf_size);

/**
 * @param out_count 输出解析到的通道数
 * @return true=至少解析到 1 路
 */
bool ConfigCodec_ChannelsFromJson(const char *json,
                                  ChannelConfig_t *configs, uint8_t max_count,
                                  uint8_t *out_count);

int  ConfigCodec_AcqToJson(const AcqParams_t *params, char *buf, int buf_size);
bool ConfigCodec_AcqFromJson(const char *json, AcqParams_t *params);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_CODEC_H */
