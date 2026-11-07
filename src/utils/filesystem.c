#define _GNU_SOURCE
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <ftw.h>
#include <errno.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <linux/limits.h>
#include "utils/filesystem.h"

#define NTFW_MAXFDS 16

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

// We wouldn't want to error on cleanup
// since the file is already created by now
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
