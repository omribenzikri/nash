#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <uuid/uuid.h>
#include "container/resources.h"

#define CNTR_CGROUP_PAYLOAD_MAX_SIZE 128

static int cntr_cgroup_write_file(int cgroup_fd, const char *name, const char *payload)
{
    int fd;
    ssize_t nwritten;
    int ret = 0;

    fd = openat(cgroup_fd, name, O_WRONLY);
    if (fd == -1) {
        perror("[x] Failed to open cgroup interface file");
        ret = -1;
        goto out;
    }

    nwritten = write(fd, payload, strlen(payload));
    if (nwritten == -1) {
        perror("[x] Failed to write to cgroup interface file");
        ret = -1;
        goto out_close;
    }

out_close:
    close(fd);
out:
    return ret;
}

static void cntr_cgroup_cpu_max_payload_format(float value, char *buf, size_t size)
{
    const int duration_us = 100000;
    int max_us;

    assert(0 < value && value <= 100);

    max_us = duration_us * (value / 100);
    snprintf(buf, size, "%d %d", max_us, duration_us);
}

static int cntr_cgroup_apply_limits(int cgroup_fd, cntr_resources_t *resources)
{
    char payload[CNTR_CGROUP_PAYLOAD_MAX_SIZE];
    int err;

    if (resources->cpu_time > 0) {
        cntr_cgroup_cpu_max_payload_format(resources->cpu_time, payload, sizeof(payload));

        err = cntr_cgroup_write_file(cgroup_fd, "cpu.max", payload);
        if (err)
            return -1;
    }

    if (resources->procs > 0) {
        snprintf(payload, sizeof(payload), "%u", resources->procs);

        err = cntr_cgroup_write_file(cgroup_fd, "pids.max", payload);
        if (err)
            return -1;
    }

    if (resources->memory_bytes > 0) {
        snprintf(payload, sizeof(payload), "%lu", resources->memory_bytes);

        err = cntr_cgroup_write_file(cgroup_fd, "memory.max", payload);
        if (err)
            return -1;
    }

    if (resources->memory_swap_bytes > 0) {
        snprintf(payload, sizeof(payload), "%lu", resources->memory_swap_bytes);

        err = cntr_cgroup_write_file(cgroup_fd, "memory.swap.max", payload);
        if (err)
            return -1;
    }

    return 0;
}

static void cntr_cgroup_path_resolve(uuid_t id, char *buf, size_t size)
{
    char uuid_string[UUID_STR_LEN];

    uuid_unparse_lower(id, uuid_string);
    snprintf(buf, size, "/sys/fs/cgroup/system.slice/nash-%s.scope", uuid_string);
}

static int cntr_cgroup_init(uuid_t id, cntr_resources_t *resources)
{
    int fd;
    int err;
    int ret = 0;

    fd = cntr_cgroup_open(id);
    if (fd == -1) {
        ret = -1;
        goto out;
    }

    err = cntr_cgroup_apply_limits(fd, resources);
    if (err) {
        ret = -1;
        goto out_close;
    }

out_close:
    close(fd);
out:
    return ret;
}

int cntr_cgroup_create(uuid_t id, cntr_resources_t *resources)
{
    char path[PATH_MAX];
    int err;

    cntr_cgroup_path_resolve(id, path, sizeof(path));

    err = mkdir(path, S_IRWXU);
    if (err) {
        perror("[x] Failed to create container cgroup");
        goto error;
    }

    err = cntr_cgroup_init(id, resources);
    if (err)
        goto error_del_cgroup;

    return 0;

error_del_cgroup:
    cntr_cgroup_delete(id);
error:
    return -1;
}

int cntr_cgroup_open(uuid_t id)
{
    char path[PATH_MAX];
    int fd;

    cntr_cgroup_path_resolve(id, path, sizeof(path));

    fd = open(path, O_DIRECTORY | O_PATH);
    if (fd == -1) {
        perror("[x] Failed to open container cgroup");
        return -1;
    }

    return fd;
}

int cntr_cgroup_delete(uuid_t id)
{
    char path[PATH_MAX];
    int err;

    cntr_cgroup_path_resolve(id, path, sizeof(path));

    err = rmdir(path);
    if (err) {
        perror("[x] Failed to delete container cgroup");
        return -1;
    }

    return 0;
}
