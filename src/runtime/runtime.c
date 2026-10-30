#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <linux/sched.h>
#include <uuid/uuid.h>
#include "runtime/filesystem.h"
#include "runtime/resources.h"
#include "runtime/capabilities.h"
#include "runtime/runtime.h"

void nsh_container_startup(nsh_runtime_config_t *config)
{
    int err;

    err = sethostname(config->hostname, strlen(config->hostname));
    if (err) {
        perror("[x] Failed to set container hostname");
        exit(EXIT_FAILURE);
    }

    umask(0);

    err = nsh_pivot_root(config->rootfs_path);
    if (err)
        exit(EXIT_FAILURE);

    err = nsh_mount_essentials();
    if (err)
        exit(EXIT_FAILURE);

    err = nsh_populate_dev();
    if (err)
        exit(EXIT_FAILURE);

    err = nsh_drop_capabilities();
    if (err)
        exit(EXIT_FAILURE);

    execve(config->exec_path, config->exec_argv, config->exec_argv);

    perror("[x] Failed to execute container program");
    exit(EXIT_FAILURE);
}

pid_t nsh_clone_container(nsh_runtime_config_t *config, int cgroup_fd)
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
        nsh_container_startup(config);
        assert(false);
    }

    return pid;
}

pid_t nsh_runtime_container_create(uuid_t id, nsh_runtime_config_t *config)
{
    pid_t pid;
    int cgroup_fd;
    int err;

    cgroup_fd = nsh_create_cgroup(id, &config->resources);
    if (cgroup_fd == -1)
        goto error;

    printf("[+] Successfully created container cgroup\n");

    pid = nsh_clone_container(config, cgroup_fd);
    err = close(cgroup_fd);

    if (err)
        perror("[x] Failed to close cgroup file descriptor");
    if (pid == -1 || err)
        goto error_del_cgroup;

    printf("[+] Successfully cloned child\n");

    return 0;

error_del_cgroup:
    nsh_delete_cgroup(id);
error:
    return -1;
}

void nsh_runtime_container_cleanup(uuid_t id)
{
    nsh_delete_cgroup(id);
}
