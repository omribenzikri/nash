#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <linux/sched.h>
#include <uuid/uuid.h>
#include "runtime/filesystem.h"
#include "runtime/resources.h"
#include "runtime/capabilities.h"
#include "runtime/creds.h"
#include "runtime/runtime.h"

#define NSH_GEN_HOSTNAME_LEN 13

static void nsh_hostname_generate(uuid_t container_id, char *buf, size_t size)
{
    char uuid_string[UUID_STR_LEN];
    char *hostname;

    assert(NSH_GEN_HOSTNAME_LEN < HOST_NAME_MAX);
    assert(NSH_GEN_HOSTNAME_LEN < UUID_STR_LEN);
    uuid_unparse_lower(container_id, uuid_string);

    hostname = uuid_string + sizeof(uuid_string) - NSH_GEN_HOSTNAME_LEN;
    snprintf(buf, size, "%s", hostname);
}

static int nsh_hostname_set(uuid_t container_id, const char *name)
{
    int err;
    char hostname[HOST_NAME_MAX];

    if (name == NULL)
        nsh_hostname_generate(container_id, hostname, sizeof(hostname));
    else
        snprintf(hostname, sizeof(hostname), "%s", name);

    err = sethostname(hostname, strlen(hostname));
    if (err) {
        perror("[x] Failed to set container hostname");
        exit(EXIT_FAILURE);
    }

    return 0;
}

static void nsh_container_startup(uuid_t id, nsh_run_params_t *params)
{
    int err;

    err = nsh_hostname_set(id, params->config.hostname);
    if (err)
        exit(EXIT_FAILURE);

    umask(0);

    err = nsh_fs_pivot_root(params->rootfs_path, params->config.cwd);
    if (err)
        exit(EXIT_FAILURE);

    err = nsh_fs_mount();
    if (err)
        exit(EXIT_FAILURE);

    err = nsh_fs_populate();
    if (err)
        exit(EXIT_FAILURE);

    err = nsh_caps_drop();
    if (err)
        exit(EXIT_FAILURE);

    err = nsh_creds_set(&params->config.creds);
    if (err)
        exit(EXIT_FAILURE);

    execve(params->config.exec_path,
        params->config.exec_argv, params->config.exec_argv);

    perror("[x] Failed to execute container program");
    exit(EXIT_FAILURE);
}

static pid_t nsh_container_clone(
    uuid_t id, nsh_run_params_t *params, int cgroup_fd)
{
    pid_t pid;

    const int namespace_flags = CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWPID |
                                CLONE_NEWIPC | CLONE_NEWCGROUP | CLONE_NEWTIME;
    struct clone_args args = {
        .flags  = namespace_flags | CLONE_INTO_CGROUP,
        .exit_signal = SIGCHLD,
        .cgroup = cgroup_fd
    };

    pid = syscall(SYS_clone3, &args, sizeof(args));
    if (pid == -1) {
        perror("[x] Clone failed");
        return -1;
    }

    if (pid == 0) {
        nsh_container_startup(id, params);
        assert(false);
    }

    return pid;
}

pid_t nsh_run_create(uuid_t id, nsh_run_params_t *params)
{
    pid_t pid;
    int cgroup_fd;
    int err;

    cgroup_fd = nsh_cgroup_create(id, &params->config.resources);
    if (cgroup_fd == -1)
        goto error;

    pid = nsh_container_clone(id, params, cgroup_fd);
    err = close(cgroup_fd);

    if (err)
        perror("[x] Failed to close cgroup file descriptor");
    if (pid == -1 || err)
        goto error_del_cgroup;

    return 0;

error_del_cgroup:
    nsh_cgroup_delete(id);
error:
    return -1;
}

void nsh_run_cleanup(uuid_t id)
{
    nsh_cgroup_delete(id);
}
