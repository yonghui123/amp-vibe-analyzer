/**
 * @file    ff.h
 * @brief   FatFs 最小桩头文件 - 供 STM32 板端编译通过
 *
 * 本文件仅提供 FatFs 类型定义和函数声明的桩，
 * 使依赖 FatFs API 的代码能在 STM32 端编译通过。
 * 实际 SD 卡 FatFs 驱动接入后应替换为完整的 FatFs 头文件。
 */
#ifndef FF_H_STUB
#define FF_H_STUB

#include <stdint.h>

/* ==================== 基本类型 ==================== */
typedef unsigned int    UINT;
typedef unsigned char   BYTE;
typedef uint16_t        WORD;
typedef uint32_t        DWORD;

/* ==================== 枚举: FRESULT ==================== */
typedef enum {
    FR_OK = 0,          /* 成功 */
    FR_DISK_ERR,
    FR_INT_ERR,
    FR_NOT_READY,
    FR_NO_FILE,
    FR_NO_PATH,
    FR_INVALID_NAME,
    FR_DENIED,
    FR_EXIST,
    FR_INVALID_OBJECT,
    FR_WRITE_PROTECTED,
    FR_INVALID_DRIVE,
    FR_NOT_ENABLED,
    FR_NO_FILESYSTEM,
    FR_MKFS_ABORTED,
    FR_TIMEOUT,
    FR_LOCKED,
    FR_NOT_ENOUGH_CORE,
    FR_TOO_MANY_OPEN_FILES,
    FR_INVALID_PARAMETER
} FRESULT;

/* ==================== 文件属性标志 ==================== */
#define AM_RDO  0x01    /* 只读 */
#define AM_HID  0x02    /* 隐藏 */
#define AM_SYS  0x04    /* 系统 */
#define AM_DIR  0x10    /* 目录 */
#define AM_ARC  0x20    /* 归档 */

/* ==================== 打开模式标志 ==================== */
#define FA_READ         0x01
#define FA_WRITE        0x02
#define FA_OPEN_ALWAYS  0x10
#define FA_CREATE_NEW   0x04

/* ==================== 结构体: FIL ==================== */
typedef struct {
    BYTE    dummy[128]; /* 占位 (实际 FatFs FIL 结构更大) */
} FIL;

/* ==================== 结构体: DIR ==================== */
typedef struct {
    BYTE    dummy[64];  /* 占位 (实际 FatFs DIR 结构更大) */
} DIR;

/* ==================== 结构体: FILINFO ==================== */
typedef struct {
    DWORD   fsize;      /* 文件大小 */
    WORD    fdate;      /* 修改日期 */
    WORD    ftime;      /* 修改时间 */
    BYTE    fattrib;    /* 文件属性 */
    char    fname[256]; /* 文件名 */
} FILINFO;

/* ==================== 函数声明 (桩实现) ==================== */
#ifdef __cplusplus
extern "C" {
#endif

/* 文件操作 */
FRESULT f_open(FIL *fp, const char *path, BYTE mode);
FRESULT f_close(FIL *fp);
char   *f_gets(char *buff, int len, FIL *fp);
FRESULT f_read(FIL *fp, void *buff, UINT btr, UINT *br);
FRESULT f_write(FIL *fp, const void *buff, UINT btw, UINT *bw);

/* 目录操作 */
FRESULT f_opendir(DIR *dp, const char *path);
FRESULT f_readdir(DIR *dp, FILINFO *fno);
FRESULT f_closedir(DIR *dp);

#ifdef __cplusplus
}
#endif

#endif /* FF_H_STUB */
