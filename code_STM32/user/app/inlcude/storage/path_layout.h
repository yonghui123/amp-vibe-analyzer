/**
 * @file    path_layout.h
 * @brief   SD 卡逻辑目录布局（对照 PRD §6.1）
 *
 * 业务层只使用这些相对路径常量，盘符由 sd_fs 封装。
 */
#ifndef PATH_LAYOUT_H
#define PATH_LAYOUT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 根下一级目录（相对路径，无盘符） */
#define PATH_DIR_CONFIG     "Config"
#define PATH_DIR_ENVELOPE   "Envelope"
#define PATH_DIR_REALDATA   "RealData"
#define PATH_DIR_ALARMDATA  "AlarmData"
#define PATH_DIR_EXPORT     "Export"

/* 常用配置文件 */
#define PATH_FILE_CHANNEL_CONFIG  "Config/channel_config.json"
#define PATH_FILE_ACQ_SETTINGS    "Config/acq_settings.json"
#define PATH_FILE_TOUCH_CALIB     "Config/touch_calib.bin"

/**
 * @brief  确保 PRD 约定的一级目录存在
 * @return true=全部就绪（或已存在）
 */
bool PathLayout_EnsureAll(void);

/**
 * @brief  拼出 RealData/年/月 相对路径
 * @param  year   如 2026
 * @param  month  1~12
 * @param  out    输出缓冲
 * @param  out_sz 缓冲大小
 * @return true=成功
 */
bool PathLayout_RealDataMonth(uint16_t year, uint8_t month,
                              char *out, uint32_t out_sz);

#ifdef __cplusplus
}
#endif

#endif /* PATH_LAYOUT_H */
