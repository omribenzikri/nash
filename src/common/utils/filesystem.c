#define _GNU_SOURCE
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <ftw.h>
#include <errno.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <linux/limits.h>
#include "common/utils/filesystem.h"

#define NTFW_MAXFDS 16
#define FILE_COPY_BUF_SIZE 8192

int mkdir_if_missing(const char *path, mode_t mode)
{
    int err = mkdir(path, mode);

    return (err && errno != EEXIST) ? err : 0;
}

static int remove_callback(const char *path, const struct stat *sb, int type, struct FTW *ftw)
{
    (void)sb;
    (void)type;
    (void)ftw;

    return remove(path);
}

int rmdir_recursive(const char *path)
{
    return nftw(path, remove_callback, NTFW_MAXFDS, FTW_DEPTH | FTW_PHYS);
}

int copy_file(const char *src, const char *dst)
{
    uint8_t buffer[FILE_COPY_BUF_SIZE];
    FILE *src_stream = NULL;
    FILE *dst_stream = NULL;
    size_t bytes_read;
    size_t bytes_written;
    int err;
    int errcode;
    int ret = 0;

    src_stream = fopen(src, "rb");
    if (src_stream == NULL) {
        ret = -1;
        goto cleanup;
    }

    dst_stream = fopen(dst, "wb");
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
    errcode = errno;
    if (src_stream != NULL) fclose(src_stream);
    if (dst_stream != NULL) fclose(dst_stream);
    errno = errcode;
    return ret;
}

void split_path(const char *path,
    char *dir_buf, size_t dir_size,
    char *base_buf, size_t base_size)
{
    char buf[PATH_MAX];

    if (dir_buf != NULL) {
        snprintf(buf, sizeof(buf), "%s", path);
        snprintf(dir_buf, dir_size, "%s", dirname(buf));
    }

    if (base_buf != NULL) {
        snprintf(buf, sizeof(buf), "%s", path);
        snprintf(base_buf, base_size, "%s", basename(buf));
    }
}

int create_with_xattr(const char *path, mode_t mode,
    const char *xname, const void *xvalue, size_t xsize, int xflags)
{
    char dir_name[PATH_MAX];
    char base_name[PATH_MAX];
    int dirfd;
    int fd;

    int ret = 0;
    int errcode = errno;
    int err;

    split_path(path, dir_name, sizeof(dir_name),
        base_name, sizeof(base_name));

    dirfd = open(dir_name, O_RDONLY | O_DIRECTORY);
    if (dirfd == -1) {
        errcode = errno;
        ret = -1;
        goto out;
    }

    fd = openat(dirfd, ".", O_TMPFILE | O_WRONLY, mode);
    if (dirfd == -1) {
        errcode = errno;
        ret = -1;
        goto out_close_dirfd;
    }

    err = fsetxattr(fd, xname, xvalue, xsize, xflags);
    if (err) {
        errcode = errno;
        ret = -1;
        goto out_close_fd;
    }

    err = linkat(fd, "", dirfd, base_name, AT_EMPTY_PATH);
    if (err) {
        errcode = errno;
        ret = -1;
        goto out_close_fd;
    }

out_close_fd:
    close(fd);
out_close_dirfd:
    close(dirfd);
out:
    errno = errcode;
    return ret;
}

int fd_to_pathname(int fd, char *buf, size_t size)
{
    char fd_path[PATH_MAX];
    ssize_t len;

    snprintf(fd_path, sizeof(fd_path), "/proc/self/fd/%d", fd);

    len = readlink(fd_path, buf, size - 1);
    if (len == -1)
        return -1;

    buf[len] = '\0';

    return 0;
}
