#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <uuid/uuid.h>
#include "filesystem.h"
#include "resources.h"

const int NSH_STACK_SIZE = 1024 * 1024;
const int NSH_HOSTNAME_LEN = 13;

typedef struct  {
    uuid_t id;
    const char *image_path;
    const char *exec_path;
    char **exec_argv;
    char **exec_envp;
} nsh_container_config_t;


int nsh_set_hostname(uuid_t container_id)
{
    assert(NSH_HOSTNAME_LEN < UUID_STR_LEN);

    char uuid_string[UUID_STR_LEN];
    char *hostname = uuid_string + UUID_STR_LEN - NSH_HOSTNAME_LEN;
    int err;

    uuid_unparse_lower(container_id, uuid_string);

    err = sethostname(hostname, strlen(hostname));
    if (err) {
        perror("[x] Failed to set container hostname");
        return  -1;
    }

    return 0;
}

int nsh_container_startup(void *data)
{
    nsh_container_config_t *config = data;
    int err;

    err = nsh_set_hostname(config->id);
    if (err)
        return -1;

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

int nsh_create_container(nsh_container_config_t *config)
{
    uint8_t *stack;
    uint8_t *stack_top;
    int flags;
    int child_tid;
    int ret = 0;
    int err;

    printf("[+] Setting up a stack of size: %d bytes\n", NSH_STACK_SIZE);

    err = nsh_create_cgroup(config->id);
    if (err) {
        ret = -1;
        goto out;
    }

    stack = malloc(NSH_STACK_SIZE);
    if (stack == NULL) {
        perror("[x] Stack allocation failed");
        ret = -1;
        goto out_del_cgroup;
    }
    stack_top = stack + NSH_STACK_SIZE;

    flags = SIGCHLD | CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWIPC;
    child_tid = clone(nsh_container_startup, stack_top, flags, config);
    if (child_tid == -1) {
        perror("[x] Clone failed");
        ret = -1;
        goto out_free_stack;
    }

    printf("[+] Successfully cloned child\n");

    if (waitpid(child_tid, NULL, 0) == -1) {
        perror("[x] Awaiting child failed");
        ret = -1;
        goto out_free_stack;
    }

out_free_stack:
    free(stack);
out_del_cgroup:
    nsh_delete_cgroup(config->id);
out:
    return ret;
}

int main(int argc, char **argv)
{
    nsh_container_config_t config = {0};
    char *envp[] = {NULL};
    int err;

    if (argc < 3) {
        fprintf(stderr, "Usage: nash IMAGE COMMAND [ARG...]\n");
        return EXIT_FAILURE;
    }

    uuid_generate_random(config.id);

    config.image_path = argv[1];
    config.exec_path = argv[2];
    config.exec_argv = argv + 2;
    config.exec_envp = envp;

    err = nsh_create_container(&config);
    if (err)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
