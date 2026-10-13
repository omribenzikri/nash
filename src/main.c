#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/statfs.h>
#include <linux/magic.h>
#include "container.h"

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

    // Temporary
    config.resources.cpu_time = 0.5;
    config.resources.procs = 100;
    config.resources.memory_bytes = 536870912;
    config.resources.memory_swap_bytes = 134217728;

    err = nsh_create_container(&config, &container);
    if (err) {
        fprintf(stderr, "[x] Failed to create container, exiting\n");
        return EXIT_FAILURE;
    }

    if (waitpid(container.pid, NULL, 0) == -1) {
        perror("[x] Awaiting child failed");
        return EXIT_FAILURE;
    }

    nsh_cleanup_container_resources(&container);

    return EXIT_SUCCESS;
}
