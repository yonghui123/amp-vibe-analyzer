/**
 * @file    envelope_csv.c
 */
#include "storage/envelope_csv.h"
#include "storage/path_layout.h"
#include "storage/sd_fs.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

uint16_t EnvelopeCsv_LoadBaseline(const char *rel_path,
                                  float *out_y, uint16_t max_pts)
{
    SdFsFile f;
    char line[96];
    uint16_t n = 0;
    uint32_t line_len = 0;

    if (rel_path == NULL || rel_path[0] == '\0' || out_y == NULL || max_pts == 0U) {
        return 0U;
    }
    if (!SdFs_IsReady()) {
        return 0U;
    }
    memset(&f, 0, sizeof(f));
    if (!SdFs_Open(&f, rel_path, SD_FS_MODE_READ)) {
        return 0U;
    }

    /* 按字节拼行，避免依赖 f_gets */
    for (;;) {
        char ch = 0;
        int32_t rd = SdFs_Read(&f, &ch, 1U);
        if (rd < 0) {
            break;
        }
        if (rd == 0) {
            if (line_len > 0U) {
                line[line_len] = '\0';
                goto parse_line;
            }
            break;
        }
        if (ch == '\n' || ch == '\r') {
            if (line_len == 0U) {
                continue;
            }
            line[line_len] = '\0';
parse_line:
            if (line[0] != '#' && line[0] != '\0') {
                float x_val = 0.0f, y_val = 0.0f;
                if (sscanf(line, "%f,%f", &x_val, &y_val) == 2 && y_val >= 0.0f) {
                    if (n < max_pts) {
                        out_y[n++] = y_val;
                    }
                }
            }
            line_len = 0U;
            if (rd == 0) {
                break;
            }
            continue;
        }
        if (line_len + 1U < sizeof(line)) {
            line[line_len++] = ch;
        }
    }

    (void)SdFs_Close(&f);
    return n;
}

typedef struct {
    EnvelopeCsvListCb cb;
    void *user;
} list_ctx_t;

static bool list_filter_cb(const char *name, bool is_dir, void *user)
{
    list_ctx_t *ctx = (list_ctx_t *)user;
    size_t len;
    if (is_dir || name == NULL || ctx == NULL || ctx->cb == NULL) {
        return true;
    }
    len = strlen(name);
    if (len > 4U && strcmp(name + len - 4U, ".csv") == 0) {
        return ctx->cb(name, ctx->user);
    }
    return true;
}

bool EnvelopeCsv_List(EnvelopeCsvListCb cb, void *user)
{
    list_ctx_t ctx;
    if (cb == NULL) {
        return false;
    }
    if (!SdFs_IsReady()) {
        return false;
    }
    ctx.cb = cb;
    ctx.user = user;
    return SdFs_ListDir(PATH_DIR_ENVELOPE, ".csv", list_filter_cb, &ctx);
}
