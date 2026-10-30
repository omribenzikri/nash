#ifndef _NASH_UTILS_FILESYSTEM_H
#define _NASH_UTILS_FILESYSTEM_H

#include <sys/stat.h>

int mkdir_if_missing(const char *path, mode_t mode);
int rmdir_recursive(const char *path);

#endif
