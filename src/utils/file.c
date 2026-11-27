#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <ftw.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include "utils/file.h"

#define FTW_DESCRIPTORS 16
#define FILE_COPY_BUFSIZE 8192

int mkdir_missing(const char *path, mode_t mode)
{
    int err = mkdir(path, mode);

    return (err && errno != EEXIST) ? err : 0;
}

static int remove_cb(const char *path, const struct stat *sb, int type, struct FTW *ftw)
{
    (void)sb;
    (void)type;
    (void)ftw;

    return remove(path);
}

int rmdir_recursive(const char *path)
{
    return nftw(path, remove_cb, FTW_DESCRIPTORS, FTW_DEPTH | FTW_PHYS);
}

int copy_file(const char *src, const char *dst)
{
    uint8_t buffer[FILE_COPY_BUFSIZE];
    FILE *src_stream = NULL;
    FILE *dst_stream = NULL;
    size_t bytes_read;
    size_t bytes_written;
    int errno_copy;
    int err;
    int ret = 0;

    src_stream = fopen(src, "r");
    if (src_stream == NULL) {
        ret = -1;
        goto cleanup;
    }

    dst_stream = fopen(dst, "w");
    if (dst_stream == NULL) {
        ret = -1;
        goto cleanup;
    }

    while (true) {
        bytes_read = fread(buffer, 1, sizeof(buffer), src_stream);
        if (bytes_read > 0) {
            bytes_written = fwrite(buffer, 1, bytes_read, dst_stream);
            if (bytes_written != bytes_read) {
                ret = -1;
                goto cleanup;
            }
        }

        if (bytes_read < sizeof(buffer)) {
            if (ferror(src_stream)) {
                ret = -1;
                goto cleanup;
            }
            if (feof(src_stream))
                break;
        }
    }

    err = fflush(dst_stream);
    if (err) {
        ret = -1;
        goto cleanup;
    }

cleanup:
    errno_copy = errno;
    if (src_stream != NULL) fclose(src_stream);
    if (dst_stream != NULL) fclose(dst_stream);
    errno = errno_copy;
    return ret;
}

void split_path(const char *path,
    char *dirname_buf, size_t dirname_size,
    char *basename_buf, size_t basename_size)
{
    char buf[PATH_MAX];

    if (dirname_buf != NULL) {
        snprintf(buf, sizeof(buf), "%s", path);
        snprintf(dirname_buf, dirname_size, "%s", dirname(buf));
    }

    if (basename_buf != NULL) {
        snprintf(buf, sizeof(buf), "%s", path);
        snprintf(basename_buf, basename_size, "%s", basename(buf));
    }
}
