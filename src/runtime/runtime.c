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
#include "common/utils/array.h"
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

static int nsh_container_exec(nsh_run_config_t *config)
{
    char *exec_path;
    char **exec_argv;
    char **exec_envp;

    if (config->exec_len < 1) {
        fprintf(stderr,
            "[x] Failed to execute container program: "
            "no executable provided\n");
        goto error;
    }

    exec_path = config->exec[0];

    exec_argv = (char **)array_realloc_nullterm_shallow(
        (void **)config->exec, config->exec_len);
    if (exec_argv == NULL) {
        fprintf(stderr, "[x] Failed to allocate argv vector\n");
        goto error;
    }

    exec_envp = (char **)array_realloc_nullterm_shallow(
        (void **)config->env, config->env_len);
    if (exec_envp == NULL) {
        fprintf(stderr, "[x] Failed to allocate envp vector\n");
        goto error_free_argv;
    }

    execve(exec_path, exec_argv, exec_envp);

    perror("[x] Failed to execute container program");
    goto error_free_envp;

error_free_envp:
    free(exec_envp);
error_free_argv:
    free(exec_argv);
error:
    return -1;
}

static void nsh_container_startup(uuid_t id, nsh_run_params_t *params)
{
    int err;

    err = nsh_hostname_set(id, params->config.hostname);
    if (err)
        exit(EXIT_FAILURE);

    err = nsh_fs_pivot_root(params->rootfs_path, params->config.workdir);
    if (err)
        exit(EXIT_FAILURE);

    err = nsh_fs_prepare();
    if (err)
        exit(EXIT_FAILURE);

    err = nsh_caps_drop();
    if (err)
        exit(EXIT_FAILURE);

    err = nsh_creds_set(&params->config.creds);
    if (err)
        exit(EXIT_FAILURE);

    nsh_container_exec(&params->config);
    exit(EXIT_FAILURE);
}

static pid_t nsh_run_clone(
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

static pid_t nsh_run_start(uuid_t id, nsh_run_params_t *params)
{
    int cgroup_fd;
    pid_t pid;

    cgroup_fd = nsh_cgroup_open(id);
    if (cgroup_fd == -1)
        return -1;

    pid = nsh_run_clone(id, params, cgroup_fd);
    close(cgroup_fd);

    return pid;
}

pid_t nsh_run_create(uuid_t id, nsh_run_params_t *params)
{
    pid_t pid;
    int err;

    err = nsh_cgroup_create(id, &params->config.resources);
    if (err)
        goto error;

    pid = nsh_run_start(id, params);
    if (pid == -1)
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
