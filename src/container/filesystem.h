#ifndef _NASH_CONTAINER_FILESYSTEM_H
#define _NASH_CONTAINER_FILESYSTEM_H

#include <sys/types.h>

typedef struct {
    const char *source;
    const char *destination;
    const char *type;
    const void *data;
    int flags;
    mode_t mode;
} cntr_mount_t;

typedef struct {
    const char *path;
    mode_t mode;
    dev_t dev_major;
    dev_t dev_minor;
} cntr_node_t;

typedef struct {
    const char *from;
    const char *to;
} cntr_symlink_t;

int cntr_filesystem_init(void);
int cntr_pivot_root(const char *root, const char *cwd);

#endif
