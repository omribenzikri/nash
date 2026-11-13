#ifndef _NASH_RUNTIME_FILESYSTEM_H
#define _NASH_RUNTIME_FILESYSTEM_H

#include <sys/stat.h>

typedef struct {
    const char *source;
    const char *destination;
    const char *type;
    const void *data;
    int flags;
    mode_t mode;
} nsh_mount_t;

typedef struct {
    const char *path;
    mode_t mode;
    dev_t dev_major;
    dev_t dev_minor;
} nsh_node_t;

typedef struct {
    const char *from;
    const char *to;
} nsh_symlink_t;

int nsh_fs_prepare(void);
int nsh_fs_pivot_root(const char *root, const char *cwd);

#endif
