/**
 * @file    sd_fs.h
 * @brief   SD / 文件系统工具层（类 CRUD）
 *
 * 板端：FatFs + SDIO；PC 模拟器：本地目录。
 * 业务代码应只依赖本头文件，不要直接 #include "ff.h"。
 */
#ifndef SD_FS_H
#define SD_FS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 打开模式 */
typedef enum {
    SD_FS_MODE_READ = 0,   /**< 只读，文件须存在 */
    SD_FS_MODE_WRITE,      /**< 覆盖写（不存在则创建） */
    SD_FS_MODE_APPEND,     /**< 追加写（不存在则创建） */
    SD_FS_MODE_READ_WRITE  /**< 读写，不存在则创建 */
} SdFsMode;

/** 流式文件句柄（不透明；勿直接访问内部） */
typedef struct SdFsFile {
#if defined(PC_SIMULATOR)
    void *fp;              /* FILE* */
#else
    uint8_t fil_mem[560];  /* 容纳 FatFs FIL（含 512 扇区缓冲） */
#endif
    uint8_t opened;
} SdFsFile;

/** 目录列举回调：返回 false 可提前结束遍历 */
typedef bool (*SdFsListCb)(const char *name, bool is_dir, void *user);

/* ---------- 生命周期 ---------- */

/**
 * @brief  初始化：互斥量 + 挂载 + 创建目录树
 * @note   须在 MX_FATFS_Init() 之后、建议在 osKernelInitialize() 之后调用
 * @return true=挂载成功且目录就绪；无卡/失败返回 false（不崩溃）
 */
bool SdFs_Init(void);

/** 是否已挂载可用 */
bool SdFs_IsReady(void);

/** 卸载（一般无需调用） */
void SdFs_Deinit(void);

/** 最近一次失败码（板端为 FRESULT 数值；PC 为 errno；0=成功） */
int SdFs_LastError(void);

/* ---------- 目录 / 路径 ---------- */

bool SdFs_EnsureDir(const char *path);
bool SdFs_Exists(const char *path);
bool SdFs_Remove(const char *path);
bool SdFs_Rename(const char *from, const char *to);

/* ---------- 整文件 CRUD ---------- */

/** @return 读到的字节数；失败返回 -1 */
int32_t SdFs_ReadAll(const char *path, void *buf, uint32_t max_len);

/** 覆盖写；@return 写入字节数，失败 -1 */
int32_t SdFs_WriteAll(const char *path, const void *buf, uint32_t len);

/** 追加写；@return 写入字节数，失败 -1 */
int32_t SdFs_Append(const char *path, const void *buf, uint32_t len);

/* ---------- 流式读写 ---------- */

bool    SdFs_Open(SdFsFile *f, const char *path, SdFsMode mode);
int32_t SdFs_Read(SdFsFile *f, void *buf, uint32_t len);
int32_t SdFs_Write(SdFsFile *f, const void *buf, uint32_t len);
bool    SdFs_Close(SdFsFile *f);

/* ---------- 列举 ---------- */

/**
 * @param ext_filter  如 ".csv"；NULL 或空串表示不过滤
 */
bool SdFs_ListDir(const char *path, const char *ext_filter,
                  SdFsListCb cb, void *user);

#ifdef __cplusplus
}
#endif

#endif /* SD_FS_H */
