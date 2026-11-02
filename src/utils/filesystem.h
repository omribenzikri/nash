#ifndef _NASH_UTILS_FILESYSTEM_H
#define _NASH_UTILS_FILESYSTEM_H

#include <stddef.h>
#include <sys/stat.h>

int mkdir_if_missing(const char *path, mode_t mode);
int rmdir_recursive(const char *path);

void split_path(const char *path,
    char *dir_buf, size_t dir_size,
    char *base_buf, size_t base_size);

// Atomicity is guaranteed
// File cannot be created without the xattr
int create_with_xattr(const char *path, mode_t mode,
    const char *xname, const void *xvalue, size_t xsize, int xflags);

int fd_to_pathname(int fd, char *buf, size_t size);

#endif
