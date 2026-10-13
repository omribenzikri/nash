#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <uuid/uuid.h>
#include "resources.h"

void nsh_resolve_cgroup_path(uuid_t container_id, char *buf, size_t size)
{
    char uuid_string[UUID_STR_LEN];

    uuid_unparse_lower(container_id, uuid_string);
    snprintf(buf, size, "/sys/fs/cgroup/system.slice/nash-%s.scope", uuid_string);
}

int nsh_create_cgroup(uuid_t container_id)
{
    char path[PATH_MAX];
    int fd;
    int err;

    nsh_resolve_cgroup_path(container_id, path, sizeof(path));

    err = mkdir(path, S_IRWXU);
    if (err) {
        perror("[x] Failed to create container cgroup");
        goto error;
    }

    fd = open(path, O_DIRECTORY | O_PATH);
    if (fd == -1) {
        perror("[x] Failed to open container cgroup");
        goto error_del_cgroup;
    }

    return fd;

error_del_cgroup:
    nsh_delete_cgroup(container_id);
error:
    return -1;
}

int nsh_delete_cgroup(uuid_t container_id)
{
    char path[PATH_MAX];
    int err;

    nsh_resolve_cgroup_path(container_id, path, sizeof(path));

    err = rmdir(path);
    if (err) {
        perror("[x] Failed to delete container cgroup");
        return -1;
    }

    return 0;
}
