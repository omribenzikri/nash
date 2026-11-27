#ifndef _NASH_UTILS_FILE_H
#define _NASH_UTILS_FILE_H

#include <stddef.h>
#include <sys/types.h>

int mkdir_missing(const char *path, mode_t mode);
int rmdir_recursive(const char *path);

int copy_file(const char *src, const char *dst);

void split_path(const char *path,
    char *dir_buf, size_t dir_size,
    char *base_buf, size_t base_size);

#endif
