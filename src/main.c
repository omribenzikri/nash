#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <linux/magic.h>
#include <uuid/uuid.h>
#include "container.h"
#include "filesystem.h"
#include "resources.h"

#define NSH_STACK_SIZE 1024 * 1024
#define NSH_HOSTNAME_LEN 13

typedef struct {
    char hostname[NSH_HOSTNAME_LEN];
    const char *image_path;
    const char *exec_path;
    char **exec_argv;
    char **exec_envp;
} nsh_container_config_t;

int nsh_verify_cgroup_version(bool *compat)
{
    struct statfs st;
    int err;

    err = statfs("/sys/fs/cgroup", &st);
    if (err) {
        perror("[x] Failed to fetch cgroup filesystem type");
        return -1;
    }
    *compat = st.f_type == CGROUP2_SUPER_MAGIC;

    return 0;
}

int nsh_verify_system(bool *compat)
{
    int err;

    err = nsh_verify_cgroup_version(compat);
    if (err)
        return -1;
    if (!*compat)
        fprintf(stderr, "[x] Unsupported cgroup version detected\n");

    return 0;
}

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

// Assuming the buffer is at least NSH_HOSTNAME_LEN bytes
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
    int ret = 0;
    int err;

    uuid_generate_random(container->id);
    nsh_generate_hostname(container->id, config->hostname);

    err = nsh_create_cgroup(container->id);
    if (err) {
        ret = -1;
        goto out;
    }

    printf("[+] Setting up a stack of size: %d bytes\n", NSH_STACK_SIZE);

    stack = malloc(NSH_STACK_SIZE);
    if (stack == NULL) {
        perror("[x] Stack allocation failed");
        ret = -1;
        goto out_del_cgroup;
    }
    stack_top = stack + NSH_STACK_SIZE;

    flags = SIGCHLD |
            CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWPID |
            CLONE_NEWIPC;
    container->pid = clone(nsh_container_startup, stack_top, flags, config);
    if (container->pid == -1) {
        perror("[x] Clone failed");
        ret = -1;
        goto out_free_stack;
    }

    printf("[+] Successfully cloned child\n");

out_free_stack:
    free(stack);
out_del_cgroup:
    nsh_delete_cgroup(container->id);
out:
    return ret;
}

int main(int argc, char **argv)
{
    nsh_container_config_t config = {0};
    nsh_container_t container = {0};
    char *envp[] = {NULL};
    bool system_compat;
    int err;

    if (argc < 3) {
        fprintf(stderr, "Usage: nash IMAGE COMMAND [ARG...]\n");
        return EXIT_FAILURE;
    }

    err = nsh_verify_system(&system_compat);
    if (err | !system_compat) {
        fprintf(stderr, "[x] Failed to verify system compatibility, exiting\n");
        return EXIT_FAILURE;
    }

    config.image_path = argv[1];
    config.exec_path = argv[2];
    config.exec_argv = argv + 2;
    config.exec_envp = envp;

    err = nsh_create_container(&config, &container);
    if (err) {
        fprintf(stderr, "[x] Failed to create container, exiting\n");
        return EXIT_FAILURE;
    }

    if (waitpid(container.pid, NULL, 0) == -1) {
        perror("[x] Awaiting child failed");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
