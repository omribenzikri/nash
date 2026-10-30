#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <unistd.h>
#include <ftw.h>
#include <errno.h>
#include <sys/stat.h>
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
