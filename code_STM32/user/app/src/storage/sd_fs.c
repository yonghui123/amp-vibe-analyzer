/**
 * @file    sd_fs.c
 * @brief   SD / 文件系统工具层（板端 FatFs，PC 本地目录）
 */
#include "storage/sd_fs.h"
#include "storage/path_layout.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#if defined(PC_SIMULATOR)

#include <errno.h>
#include <stdlib.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <sys/stat.h>
#define SD_FS_MKDIR(p) _mkdir(p)
#define SD_FS_ACCESS(p, m) _access(p, m)
#else
#include <sys/stat.h>
#include <unistd.h>
#define SD_FS_MKDIR(p) mkdir((p), 0755)
#define SD_FS_ACCESS(p, m) access(p, m)
#endif
#include <dirent.h>

#else /* !PC_SIMULATOR */

#include "ff.h"
#include "fatfs.h"
#include "cmsis_os.h"
#include "bsp_driver_sd.h"

#endif

#define SD_FS_PATH_MAX  192
#define SD_FS_LOCK_MS   5000U

static bool s_ready;
static int  s_last_err;

#if defined(PC_SIMULATOR)
/* PC 根目录：相对可执行文件工作目录 */
#ifndef SD_FS_PC_ROOT
#define SD_FS_PC_ROOT  "sd_root"
#endif
#else
static osMutexId_t s_mutex;
#endif

/* -------------------------------------------------------------------------- */
/* 内部：错误 / 锁 / 路径                                                      */
/* -------------------------------------------------------------------------- */

static void set_err(int e)
{
    s_last_err = e;
}

#if !defined(PC_SIMULATOR)
static bool lock(void)
{
    if (s_mutex == NULL) {
        return false;
    }
    return (osMutexAcquire(s_mutex, SD_FS_LOCK_MS) == osOK);
}

static void unlock(void)
{
    if (s_mutex != NULL) {
        (void)osMutexRelease(s_mutex);
    }
}

static FIL *fil_of(SdFsFile *f)
{
    return (FIL *)(void *)f->fil_mem;
}

_Static_assert(sizeof(FIL) <= 560, "SdFsFile.fil_mem too small for FIL");
#endif

/**
 * 拼绝对路径到 out。
 * 板端：SDPath + rel；PC：sd_root/ + rel
 * rel 可为 NULL/空 → 仅根。
 */
static bool make_abs(char *out, size_t out_sz, const char *rel)
{
    size_t n;
    const char *r = (rel != NULL) ? rel : "";

    while (*r == '/' || *r == '\\') {
        r++;
    }

#if defined(PC_SIMULATOR)
    if (r[0] == '\0') {
        n = (size_t)snprintf(out, out_sz, "%s", SD_FS_PC_ROOT);
    } else {
        n = (size_t)snprintf(out, out_sz, "%s/%s", SD_FS_PC_ROOT, r);
    }
#else
    if (r[0] == '\0') {
        n = (size_t)snprintf(out, out_sz, "%s", SDPath);
    } else {
        /* SDPath 已是 "0:/" */
        n = (size_t)snprintf(out, out_sz, "%s%s", SDPath, r);
    }
#endif
    return (n > 0U && n < out_sz);
}

/** 从文件路径取出父目录（相对路径），写入 parent；无父目录返回 false */
static bool parent_rel(const char *path, char *parent, size_t parent_sz)
{
    const char *slash;
    size_t len;

    if (path == NULL || path[0] == '\0') {
        return false;
    }
    slash = strrchr(path, '/');
    if (slash == NULL) {
        slash = strrchr(path, '\\');
    }
    if (slash == NULL || slash == path) {
        return false;
    }
    len = (size_t)(slash - path);
    if (len + 1U > parent_sz) {
        return false;
    }
    memcpy(parent, path, len);
    parent[len] = '\0';
    return true;
}

#if defined(PC_SIMULATOR)
static bool ensure_dir_abs(const char *abs)
{
    char tmp[SD_FS_PATH_MAX];
    size_t i;
    size_t len;

    if (abs == NULL || abs[0] == '\0') {
        return false;
    }
    len = strlen(abs);
    if (len >= sizeof(tmp)) {
        return false;
    }
    memcpy(tmp, abs, len + 1U);

    for (i = 1; i < len; i++) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            char c = tmp[i];
            tmp[i] = '\0';
            if (SD_FS_ACCESS(tmp, 0) != 0) {
                if (SD_FS_MKDIR(tmp) != 0 && errno != EEXIST) {
                    set_err(errno);
                    return false;
                }
            }
            tmp[i] = c;
        }
    }
    if (SD_FS_ACCESS(tmp, 0) != 0) {
        if (SD_FS_MKDIR(tmp) != 0 && errno != EEXIST) {
            set_err(errno);
            return false;
        }
    }
    set_err(0);
    return true;
}
#else
static bool ensure_dir_abs(const char *abs)
{
    char tmp[SD_FS_PATH_MAX];
    size_t i;
    size_t len;
    FRESULT fr;

    if (abs == NULL || abs[0] == '\0') {
        return false;
    }
    len = strlen(abs);
    if (len >= sizeof(tmp)) {
        return false;
    }
    memcpy(tmp, abs, len + 1U);

    /* 跳过盘符 "0:/" */
    i = 0;
    if (len >= 3U && tmp[1] == ':' && (tmp[2] == '/' || tmp[2] == '\\')) {
        i = 3;
    }

    for (; i < len; i++) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            char c = tmp[i];
            if (i == 0U || (i == 2U && tmp[1] == ':')) {
                continue;
            }
            tmp[i] = '\0';
            fr = f_mkdir(tmp);
            if (fr != FR_OK && fr != FR_EXIST) {
                set_err((int)fr);
                return false;
            }
            tmp[i] = c;
        }
    }
    fr = f_mkdir(tmp);
    if (fr != FR_OK && fr != FR_EXIST) {
        set_err((int)fr);
        return false;
    }
    set_err(0);
    return true;
}
#endif

static bool ensure_parent_of_file(const char *rel_path)
{
    char parent[SD_FS_PATH_MAX];

    if (!parent_rel(rel_path, parent, sizeof(parent))) {
        return true; /* 无父目录，写在根下 */
    }
    return SdFs_EnsureDir(parent);
}

#if !defined(PC_SIMULATOR)
static BYTE mode_to_fa(SdFsMode mode)
{
    switch (mode) {
    case SD_FS_MODE_READ:
        return FA_READ;
    case SD_FS_MODE_WRITE:
        return (BYTE)(FA_WRITE | FA_CREATE_ALWAYS);
    case SD_FS_MODE_APPEND:
        return (BYTE)(FA_WRITE | FA_OPEN_APPEND);
    case SD_FS_MODE_READ_WRITE:
        return (BYTE)(FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
    default:
        return FA_READ;
    }
}
#else
static const char *mode_to_fopen(SdFsMode mode)
{
    switch (mode) {
    case SD_FS_MODE_READ:
        return "rb";
    case SD_FS_MODE_WRITE:
        return "wb";
    case SD_FS_MODE_APPEND:
        return "ab";
    case SD_FS_MODE_READ_WRITE:
        return "r+b";
    default:
        return "rb";
    }
}
#endif

static bool name_has_ext(const char *name, const char *ext_filter)
{
    size_t nlen;
    size_t elen;
    const char *p;

    if (ext_filter == NULL || ext_filter[0] == '\0') {
        return true;
    }
    nlen = strlen(name);
    elen = strlen(ext_filter);
    if (nlen < elen) {
        return false;
    }
    p = name + (nlen - elen);
    while (*ext_filter) {
        if (tolower((unsigned char)*p) != tolower((unsigned char)*ext_filter)) {
            return false;
        }
        p++;
        ext_filter++;
    }
    return true;
}

/* -------------------------------------------------------------------------- */
/* 生命周期                                                                    */
/* -------------------------------------------------------------------------- */

bool SdFs_Init(void)
{
#if defined(PC_SIMULATOR)
    char abs[SD_FS_PATH_MAX];

    s_ready = false;
    set_err(0);

    if (!make_abs(abs, sizeof(abs), NULL)) {
        return false;
    }
    if (!ensure_dir_abs(abs)) {
        return false;
    }
    s_ready = true;
    if (!PathLayout_EnsureAll()) {
        s_ready = false;
        return false;
    }
    return true;
#else
    FRESULT fr;

    s_ready = false;
    set_err(0);

    if (s_mutex == NULL) {
        s_mutex = osMutexNew(NULL);
        if (s_mutex == NULL) {
            set_err(-1);
            return false;
        }
    }

    if (!lock()) {
        set_err(-2);
        return false;
    }

    if (BSP_SD_IsDetected() != SD_PRESENT) {
        set_err((int)FR_NOT_READY);
        unlock();
        return false;
    }

    fr = f_mount(&SDFatFS, SDPath, 1);
    set_err((int)fr);
    if (fr != FR_OK) {
        unlock();
        return false;
    }

    s_ready = true;
    unlock();

    if (!PathLayout_EnsureAll()) {
        s_ready = false;
        (void)f_mount(NULL, SDPath, 0);
        return false;
    }
    return true;
#endif
}

bool SdFs_IsReady(void)
{
    return s_ready;
}

void SdFs_Deinit(void)
{
#if defined(PC_SIMULATOR)
    s_ready = false;
    set_err(0);
#else
    if (lock()) {
        if (s_ready) {
            (void)f_mount(NULL, SDPath, 0);
        }
        s_ready = false;
        unlock();
    } else {
        s_ready = false;
    }
#endif
}

int SdFs_LastError(void)
{
    return s_last_err;
}

/* -------------------------------------------------------------------------- */
/* 目录 / 路径                                                                 */
/* -------------------------------------------------------------------------- */

bool SdFs_EnsureDir(const char *path)
{
    char abs[SD_FS_PATH_MAX];
    bool ok;

    if (!s_ready || path == NULL || path[0] == '\0') {
        return false;
    }
    if (!make_abs(abs, sizeof(abs), path)) {
        return false;
    }

#if defined(PC_SIMULATOR)
    ok = ensure_dir_abs(abs);
#else
    if (!lock()) {
        return false;
    }
    ok = ensure_dir_abs(abs);
    unlock();
#endif
    return ok;
}

bool SdFs_Exists(const char *path)
{
    char abs[SD_FS_PATH_MAX];
    bool ok;

    if (!s_ready || path == NULL) {
        return false;
    }
    if (!make_abs(abs, sizeof(abs), path)) {
        return false;
    }

#if defined(PC_SIMULATOR)
    ok = (SD_FS_ACCESS(abs, 0) == 0);
    set_err(ok ? 0 : errno);
#else
    {
        FILINFO fno;
        FRESULT fr;
        if (!lock()) {
            return false;
        }
        fr = f_stat(abs, &fno);
        set_err((int)fr);
        ok = (fr == FR_OK);
        unlock();
    }
#endif
    return ok;
}

bool SdFs_Remove(const char *path)
{
    char abs[SD_FS_PATH_MAX];
    bool ok;

    if (!s_ready || path == NULL) {
        return false;
    }
    if (!make_abs(abs, sizeof(abs), path)) {
        return false;
    }

#if defined(PC_SIMULATOR)
    ok = (remove(abs) == 0);
    set_err(ok ? 0 : errno);
#else
    {
        FRESULT fr;
        if (!lock()) {
            return false;
        }
        fr = f_unlink(abs);
        set_err((int)fr);
        ok = (fr == FR_OK);
        unlock();
    }
#endif
    return ok;
}

bool SdFs_Rename(const char *from, const char *to)
{
    char abs_from[SD_FS_PATH_MAX];
    char abs_to[SD_FS_PATH_MAX];
    bool ok;

    if (!s_ready || from == NULL || to == NULL) {
        return false;
    }
    if (!make_abs(abs_from, sizeof(abs_from), from) ||
        !make_abs(abs_to, sizeof(abs_to), to)) {
        return false;
    }

#if defined(PC_SIMULATOR)
    ok = (rename(abs_from, abs_to) == 0);
    set_err(ok ? 0 : errno);
#else
    {
        FRESULT fr;
        if (!lock()) {
            return false;
        }
        fr = f_rename(abs_from, abs_to);
        set_err((int)fr);
        ok = (fr == FR_OK);
        unlock();
    }
#endif
    return ok;
}

/* -------------------------------------------------------------------------- */
/* 整文件 CRUD                                                                 */
/* -------------------------------------------------------------------------- */

int32_t SdFs_ReadAll(const char *path, void *buf, uint32_t max_len)
{
    SdFsFile f;
    int32_t n;

    if (!s_ready || path == NULL || buf == NULL || max_len == 0U) {
        return -1;
    }
    memset(&f, 0, sizeof(f));
    if (!SdFs_Open(&f, path, SD_FS_MODE_READ)) {
        return -1;
    }
    n = SdFs_Read(&f, buf, max_len);
    (void)SdFs_Close(&f);
    return n;
}

int32_t SdFs_WriteAll(const char *path, const void *buf, uint32_t len)
{
    SdFsFile f;
    int32_t n;

    if (!s_ready || path == NULL || (buf == NULL && len > 0U)) {
        return -1;
    }
    if (!ensure_parent_of_file(path)) {
        return -1;
    }
    memset(&f, 0, sizeof(f));
    if (!SdFs_Open(&f, path, SD_FS_MODE_WRITE)) {
        return -1;
    }
    n = SdFs_Write(&f, buf, len);
    if (!SdFs_Close(&f)) {
        return -1;
    }
    return n;
}

int32_t SdFs_Append(const char *path, const void *buf, uint32_t len)
{
    SdFsFile f;
    int32_t n;

    if (!s_ready || path == NULL || (buf == NULL && len > 0U)) {
        return -1;
    }
    if (!ensure_parent_of_file(path)) {
        return -1;
    }
    memset(&f, 0, sizeof(f));
    if (!SdFs_Open(&f, path, SD_FS_MODE_APPEND)) {
        return -1;
    }
    n = SdFs_Write(&f, buf, len);
    if (!SdFs_Close(&f)) {
        return -1;
    }
    return n;
}

/* -------------------------------------------------------------------------- */
/* 流式读写                                                                    */
/* -------------------------------------------------------------------------- */

bool SdFs_Open(SdFsFile *f, const char *path, SdFsMode mode)
{
    char abs[SD_FS_PATH_MAX];

    if (!s_ready || f == NULL || path == NULL) {
        return false;
    }
    if (!make_abs(abs, sizeof(abs), path)) {
        return false;
    }

    if (mode == SD_FS_MODE_WRITE || mode == SD_FS_MODE_APPEND ||
        mode == SD_FS_MODE_READ_WRITE) {
        if (!ensure_parent_of_file(path)) {
            return false;
        }
    }

    memset(f, 0, sizeof(*f));

#if defined(PC_SIMULATOR)
    {
        FILE *fp;
        const char *m = mode_to_fopen(mode);

        if (mode == SD_FS_MODE_READ_WRITE) {
            fp = fopen(abs, "r+b");
            if (fp == NULL) {
                fp = fopen(abs, "w+b");
            }
        } else {
            fp = fopen(abs, m);
        }
        if (fp == NULL) {
            set_err(errno);
            return false;
        }
        f->fp = fp;
        f->opened = 1U;
        set_err(0);
        return true;
    }
#else
    {
        FRESULT fr;
        if (!lock()) {
            return false;
        }
        fr = f_open(fil_of(f), abs, mode_to_fa(mode));
        set_err((int)fr);
        if (fr != FR_OK) {
            unlock();
            return false;
        }
        f->opened = 1U;
        unlock();
        return true;
    }
#endif
}

int32_t SdFs_Read(SdFsFile *f, void *buf, uint32_t len)
{
    if (f == NULL || !f->opened || buf == NULL) {
        return -1;
    }

#if defined(PC_SIMULATOR)
    {
        size_t n = fread(buf, 1, (size_t)len, (FILE *)f->fp);
        if (n == 0U && ferror((FILE *)f->fp)) {
            set_err(errno);
            return -1;
        }
        set_err(0);
        return (int32_t)n;
    }
#else
    {
        UINT br = 0;
        FRESULT fr;
        if (!lock()) {
            return -1;
        }
        fr = f_read(fil_of(f), buf, (UINT)len, &br);
        set_err((int)fr);
        unlock();
        if (fr != FR_OK) {
            return -1;
        }
        return (int32_t)br;
    }
#endif
}

int32_t SdFs_Write(SdFsFile *f, const void *buf, uint32_t len)
{
    if (f == NULL || !f->opened || (buf == NULL && len > 0U)) {
        return -1;
    }
    if (len == 0U) {
        return 0;
    }

#if defined(PC_SIMULATOR)
    {
        size_t n = fwrite(buf, 1, (size_t)len, (FILE *)f->fp);
        if (n != (size_t)len) {
            set_err(errno);
            return -1;
        }
        set_err(0);
        return (int32_t)n;
    }
#else
    {
        UINT bw = 0;
        FRESULT fr;
        if (!lock()) {
            return -1;
        }
        fr = f_write(fil_of(f), buf, (UINT)len, &bw);
        set_err((int)fr);
        unlock();
        if (fr != FR_OK || bw != len) {
            return -1;
        }
        return (int32_t)bw;
    }
#endif
}

bool SdFs_Close(SdFsFile *f)
{
    bool ok = true;

    if (f == NULL || !f->opened) {
        return false;
    }

#if defined(PC_SIMULATOR)
    if (fflush((FILE *)f->fp) != 0) {
        set_err(errno);
        ok = false;
    }
    if (fclose((FILE *)f->fp) != 0) {
        set_err(errno);
        ok = false;
    } else if (ok) {
        set_err(0);
    }
    f->fp = NULL;
#else
    {
        FRESULT fr;
        if (!lock()) {
            return false;
        }
        fr = f_close(fil_of(f));
        set_err((int)fr);
        ok = (fr == FR_OK);
        unlock();
    }
#endif
    f->opened = 0U;
    return ok;
}

/* -------------------------------------------------------------------------- */
/* 列举                                                                        */
/* -------------------------------------------------------------------------- */

bool SdFs_ListDir(const char *path, const char *ext_filter,
                  SdFsListCb cb, void *user)
{
    char abs[SD_FS_PATH_MAX];

    if (!s_ready || path == NULL || cb == NULL) {
        return false;
    }
    if (!make_abs(abs, sizeof(abs), path)) {
        return false;
    }

#if defined(PC_SIMULATOR)
    {
        DIR *d = opendir(abs);
        struct dirent *ent;

        if (d == NULL) {
            set_err(errno);
            return false;
        }
        while ((ent = readdir(d)) != NULL) {
            bool is_dir;
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
                continue;
            }
#ifdef DT_DIR
            is_dir = (ent->d_type == DT_DIR);
#else
            {
                char child[SD_FS_PATH_MAX];
                struct stat st;
                snprintf(child, sizeof(child), "%s/%s", abs, ent->d_name);
                is_dir = (stat(child, &st) == 0 && (st.st_mode & S_IFDIR) != 0);
            }
#endif
            if (!is_dir && !name_has_ext(ent->d_name, ext_filter)) {
                continue;
            }
            if (!cb(ent->d_name, is_dir, user)) {
                break;
            }
        }
        closedir(d);
        set_err(0);
        return true;
    }
#else
    {
        DIR dir;
        FILINFO fno;
        FRESULT fr;

        if (!lock()) {
            return false;
        }
        fr = f_opendir(&dir, abs);
        set_err((int)fr);
        if (fr != FR_OK) {
            unlock();
            return false;
        }
        for (;;) {
            fr = f_readdir(&dir, &fno);
            if (fr != FR_OK) {
                set_err((int)fr);
                (void)f_closedir(&dir);
                unlock();
                return false;
            }
            if (fno.fname[0] == 0) {
                break;
            }
            {
                bool is_dir = ((fno.fattrib & AM_DIR) != 0);
                if (!is_dir && !name_has_ext(fno.fname, ext_filter)) {
                    continue;
                }
                if (!cb(fno.fname, is_dir, user)) {
                    break;
                }
            }
        }
        (void)f_closedir(&dir);
        set_err(0);
        unlock();
        return true;
    }
#endif
}
