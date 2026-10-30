#ifndef _NASH_UTILS_FILESYSTEM_H
#define _NASH_UTILS_FILESYSTEM_H

#include <stddef.h>
#include <sys/stat.h>

int mkdir_if_missing(const char *path, mode_t mode);
int rmdir_recursive(const char *path);

int fd_to_pathname(int fd, char *buf, size_t size);

#endif
