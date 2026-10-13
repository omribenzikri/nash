#include <stdio.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <unistd.h>
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
    int err;

    nsh_resolve_cgroup_path(container_id, path, sizeof(path));

    err = mkdir(path, S_IRWXU);
    if (err) {
        perror("[x] Failed to create container cgroup");
        return -1;
    }

    return 0;
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
