#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <ftw.h>
#include <errno.h>
#include <sys/stat.h>
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
