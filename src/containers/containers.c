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
#include "containers/filesystem.h"
#include "containers/resources.h"
#include "containers/capabilities.h"
#include "containers/containers.h"

void nsh_container_startup(nsh_container_config_t *config)
{
    int err;

    err = sethostname(config->hostname, strlen(config->hostname));
    if (err) {
        perror("[x] Failed to set container hostname");
        exit(EXIT_FAILURE);
    }

    umask(0);

    err = nsh_pivot_root(config->image_path);
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

// Assuming the buffer is at least NSH_HOSTNAME_LEN bytes long
void nsh_generate_hostname(uuid_t container_id, char *buf)
{
    char uuid_string[UUID_STR_LEN];
    char *hostname = uuid_string + UUID_STR_LEN - NSH_HOSTNAME_LEN;

    assert(NSH_HOSTNAME_LEN < UUID_STR_LEN);
    uuid_unparse_lower(container_id, uuid_string);
    strncpy(buf, hostname, NSH_HOSTNAME_LEN - 1);
}

int nsh_clone_container(nsh_container_config_t *config, nsh_container_t *container)
{
    pid_t pid;

    const int namespace_flags = CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWPID |
                                CLONE_NEWIPC | CLONE_NEWCGROUP | CLONE_NEWTIME;
    struct clone_args args = {
        .flags  = namespace_flags | CLONE_INTO_CGROUP,
        .exit_signal = SIGCHLD,
        .cgroup = config->cgroup_fd
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

    container->pid = pid;
    return 0;
}

int nsh_create_container(nsh_container_config_t *config, nsh_container_t *container)
{
    int clone_err;
    int close_err;

    uuid_generate_random(container->id);
    nsh_generate_hostname(container->id, config->hostname);

    config->cgroup_fd = nsh_create_cgroup(container->id, &config->resources);
    if (config->cgroup_fd == -1)
        goto error;

    printf("[+] Successfully created container cgroup\n");

    clone_err = nsh_clone_container(config, container);
    close_err = close(config->cgroup_fd);

    if (close_err)
        perror("[x] Failed to close cgroup file descriptor");
    if (clone_err || close_err)
        goto error_del_cgroup;

    printf("[+] Successfully cloned child\n");

    return 0;

error_del_cgroup:
    nsh_delete_cgroup(container->id);
error:
    return -1;
}

void nsh_cleanup_container_resources(nsh_container_t *container)
{
    nsh_delete_cgroup(container->id);
}
