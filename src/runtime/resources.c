#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <uuid/uuid.h>
#include "runtime/resources.h"

#define NSH_CGROUP_PAYLOAD_SIZE 128

static int nsh_write_to_cgroup_file(int cgroup_fd, const char *name, const char *payload)
{
    char log[128];
    int fd;
    ssize_t nwritten;

    fd = openat(cgroup_fd, name, O_WRONLY);
    if (fd == -1) {
        snprintf(log, sizeof(log), "[x] Failed to open %s in container cgroup\n", name);
        perror(log);
        return -1;
    }

    nwritten = write(fd, payload, strlen(payload));
    if (nwritten == -1) {
        snprintf(log, sizeof(log), "[x] Failed to write to %s in container cgroup\n", name);
        perror(log);
        return -1;
    }

    return 0;
}

static void nsh_format_cpu_max_payload(char *buf, size_t size, float value)
{
    const int duration_us = 100000;
    int max_us;

    assert(0 < value && value <= 100);

    max_us = duration_us * (value / 100);
    snprintf(buf, size, "%d %d", max_us, duration_us);
}

static int nsh_apply_resource_limits(int cgroup_fd, nsh_resources_t *resources)
{
    int err;
    char payload[NSH_CGROUP_PAYLOAD_SIZE];

    if (resources->cpu_time > 0) {
        nsh_format_cpu_max_payload(payload, sizeof(payload), resources->cpu_time);

        err = nsh_write_to_cgroup_file(cgroup_fd, "cpu.max", payload);
        if (err) {
            fprintf(stderr, "[x] Failed to set CPU time limit\n");
            return -1;
        }
    }

    if (resources->memory_bytes > 0) {
        snprintf(payload, sizeof(payload), "%lu", resources->memory_bytes);

        err = nsh_write_to_cgroup_file(cgroup_fd, "memory.max", payload);
        if (err) {
            fprintf(stderr, "[x] Failed to set memory limit\n");
            return -1;
        }
    }

    if (resources->memory_swap_bytes > 0) {
        snprintf(payload, sizeof(payload), "%lu", resources->memory_swap_bytes);

        err = nsh_write_to_cgroup_file(cgroup_fd, "memory.swap.max", payload);
        if (err) {
            fprintf(stderr, "[x] Failed to set swap limit\n");
            return -1;
        }
    }

    if (resources->procs > 0) {
        snprintf(payload, sizeof(payload), "%u", resources->procs);

        err = nsh_write_to_cgroup_file(cgroup_fd, "pids.max", payload);
        if (err) {
            fprintf(stderr, "[x] Failed to set processes limit\n");
            return -1;
        }
    }

    return 0;
}

static void nsh_resolve_cgroup_path(uuid_t container_id, char *buf, size_t size)
{
    char uuid_string[UUID_STR_LEN];

    uuid_unparse_lower(container_id, uuid_string);
    snprintf(buf, size, "/sys/fs/cgroup/system.slice/nash-%s.scope", uuid_string);
}

int nsh_cgroup_create(uuid_t container_id, nsh_resources_t *resources)
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

    err = nsh_apply_resource_limits(fd, resources);
    if (err)
        goto error_del_cgroup;

    return fd;

error_del_cgroup:
    nsh_cgroup_delete(container_id);
error:
    return -1;
}

int nsh_cgroup_delete(uuid_t container_id)
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
