/**
 * @file    envelope_csv.h
 * @brief   包络 CSV 加载 / 列举（纯业务组件，经 sd_fs）
 */
#ifndef ENVELOPE_CSV_H
#define ENVELOPE_CSV_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*EnvelopeCsvListCb)(const char *filename, void *user);

/**
 * @brief  从相对路径加载基线 Y 列（CSV: x,y；# 注释忽略）
 * @return 有效点数；失败 0
 */
uint16_t EnvelopeCsv_LoadBaseline(const char *rel_path,
                                  float *out_y, uint16_t max_pts);

/** 列举 Envelope/ 下 .csv 文件名（不含目录） */
bool EnvelopeCsv_List(EnvelopeCsvListCb cb, void *user);

#ifdef __cplusplus
}
#endif

#endif /* ENVELOPE_CSV_H */
