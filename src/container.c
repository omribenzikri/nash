#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <sys/stat.h>
#include <uuid/uuid.h>
#include "filesystem.h"
#include "resources.h"
#include "container.h"

int nsh_container_startup(void *data)
{
    nsh_container_config_t *config = data;
    int err;

    err = sethostname(config->hostname, strlen(config->hostname));
    if (err) {
        perror("[x] Failed to set container hostname");
        return  -1;
    }

    umask(0);

    err = nsh_pivot_root(config->image_path);
    if (err)
        return -1;

    err = nsh_mount_essentials();
    if (err)
        return -1;

    err = nsh_populate_dev();
    if (err)
        return -1;

    execve(config->exec_path, config->exec_argv, config->exec_argv);

    perror("[x] Failed to execute container program");
    return -1;
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

int nsh_create_container(nsh_container_config_t *config, nsh_container_t *container)
{
    uint8_t *stack;
    uint8_t *stack_top;
    int flags;
    int err;

    uuid_generate_random(container->id);
    nsh_generate_hostname(container->id, config->hostname);

    err = nsh_create_cgroup(container->id);
    if (err)
        goto error;

    printf("[+] Setting up a stack of size: %d bytes\n", NSH_STACK_SIZE);

    stack = malloc(NSH_STACK_SIZE);
    if (stack == NULL) {
        perror("[x] Stack allocation failed");
        goto error_del_cgroup;
    }
    stack_top = stack + NSH_STACK_SIZE;

    flags = SIGCHLD |
            CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWPID |
            CLONE_NEWIPC;
    container->pid = clone(nsh_container_startup, stack_top, flags, config);
    if (container->pid == -1) {
        perror("[x] Clone failed");
        goto error_free_stack;
    }

    printf("[+] Successfully cloned child\n");

    return 0;

error_free_stack:
    free(stack);
error_del_cgroup:
    nsh_delete_cgroup(container->id);
error:
    return -1;
}

void nsh_cleanup_container_resources(nsh_container_t *container)
{
    free(container->resources.stack);
    nsh_delete_cgroup(container->id);
}
