/**
 * @file    stub_fatfs.c
 * @brief   FatFs 函数桩实现 - 供 STM32 板端链接通过
 *
 * 所有函数返回 "不可用" 状态，待实际 SD 卡 FatFs 驱动接入后替换。
 */
#ifndef PC_SIMULATOR

#include "ff.h"
#include <stddef.h>

FRESULT f_open(FIL *fp, const char *path, BYTE mode)
{
    (void)fp;
    (void)path;
    (void)mode;
    return FR_NOT_READY;
}

FRESULT f_close(FIL *fp)
{
    (void)fp;
    return FR_OK;
}

char *f_gets(char *buff, int len, FIL *fp)
{
    (void)fp;
    (void)len;
    if (buff) buff[0] = '\0';
    return NULL;
}

FRESULT f_read(FIL *fp, void *buff, UINT btr, UINT *br)
{
    (void)fp;
    (void)buff;
    (void)btr;
    if (br) *br = 0;
    return FR_NOT_READY;
}

FRESULT f_write(FIL *fp, const void *buff, UINT btw, UINT *bw)
{
    (void)fp;
    (void)buff;
    (void)btw;
    if (bw) *bw = 0;
    return FR_NOT_READY;
}

FRESULT f_opendir(DIR *dp, const char *path)
{
    (void)dp;
    (void)path;
    return FR_NOT_READY;  /* SD 卡驱动未就绪 */
}

FRESULT f_readdir(DIR *dp, FILINFO *fno)
{
    (void)dp;
    if (fno) {
        fno->fname[0] = '\0';  /* 无文件 */
    }
    return FR_NOT_READY;
}

FRESULT f_closedir(DIR *dp)
{
    (void)dp;
    return FR_OK;
}

#endif /* !PC_SIMULATOR */
