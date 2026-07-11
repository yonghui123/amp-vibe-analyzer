/**
 * @file    path_layout.c
 * @brief   SD 卡逻辑目录布局
 */
#include "storage/path_layout.h"
#include "storage/sd_fs.h"

#include <stdio.h>
#include <stddef.h>

bool PathLayout_EnsureAll(void)
{
    static const char *const dirs[] = {
        PATH_DIR_CONFIG,
        PATH_DIR_ENVELOPE,
        PATH_DIR_REALDATA,
        PATH_DIR_ALARMDATA,
        PATH_DIR_EXPORT,
    };
    uint32_t i;

    for (i = 0; i < (uint32_t)(sizeof(dirs) / sizeof(dirs[0])); i++) {
        if (!SdFs_EnsureDir(dirs[i])) {
            return false;
        }
    }
    return true;
}

bool PathLayout_RealDataMonth(uint16_t year, uint8_t month,
                              char *out, uint32_t out_sz)
{
    int n;

    if (out == NULL || out_sz < 20U || month < 1U || month > 12U) {
        return false;
    }
    n = snprintf(out, (size_t)out_sz, "%s/%04u/%02u",
                 PATH_DIR_REALDATA, (unsigned)year, (unsigned)month);
    return (n > 0 && (uint32_t)n < out_sz);
}
